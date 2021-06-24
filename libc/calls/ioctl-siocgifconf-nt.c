/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/calls.h"
#include "libc/calls/internal.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/af.h"
#include "libc/sock/sock.h"
#include "libc/sock/internal.h"
#include "libc/sysv/errfuns.h"
#include "libc/sysv/consts/iff.h"
#include "libc/nt/winsock.h"
#include "libc/nt/errors.h"
#include "libc/nt/iphlpapi.h"
#include "libc/nt/struct/ipadapteraddresses.h"
#include "libc/bits/weaken.h"
#include "libc/str/str.h"
#include "libc/assert.h"
//#include "libc/nt/windows.h"        /* Needed for WideCharToMultiByte */

/* Maximum number of unicast addresses handled for each interface */
#define MAX_UNICAST_ADDR    32
#define MAX_NAME_CLASH      ((int)('z'-'a'))      /* Allow a..z */

static int insertAdapterName(NtIpAdapterAddresses *aa);

struct HostAdapterInfoNode {
  struct HostAdapterInfoNode * next;
  char name[IFNAMSIZ];    /* Obtained from FriendlyName */
  struct sockaddr unicast;
  struct sockaddr netmask;
  struct sockaddr broadcast;
  short flags;
} *__hostInfo;

/* Frees all the nodes of the _hostInfo */
static void freeHostInfo() {
  struct HostAdapterInfoNode *next, *node = __hostInfo;
  while(node) {
    next = node->next;
    weaken(free)(node);
    node = next;
  }
  __hostInfo = NULL;
}

/* Given a short adapter name, look into __hostInfo to see if there is
 * an adapter with the same name. Returns the pointer to the HostAdapterInfoNode
 * if found, or NULL if not found
 */
static struct HostAdapterInfoNode *findAdapterByName(const char *name) {
  struct HostAdapterInfoNode *node = __hostInfo;
  while(node) {
    if (!strncmp(name, node->name, IFNAMSIZ)) {
      /* Found */
      return node;
    }
    node=node->next;
  }
  /* Not found */
  return NULL;
}

/* Creates a new HostAdapterInfoNode object, initializes it from
 * the given adapter, unicast address and address prefixes
 * and insert it in the __hostInfo.
 * Increments the pointers to the unicast addresses and
 * the address prefixes
 * Returns NULL if an error occurred or the newly created
 * HostAdapterInfoNode object (last in the list)
 */
struct HostAdapterInfoNode *appendHostInfo(
    struct HostAdapterInfoNode *parentInfoNode,
    const char *baseName,               /* Max length = IFNAMSIZ-1 */
    const NtIpAdapterAddresses *aa,     /* Top level adapter object being processed */
    NtIpAdapterUnicastAddress **ptrUA,  /* Ptr to ptr to unicast address list node */
    NtIpAdapterPrefix **ptrAP,          /* Ptr to ptr to Adapter prefix list node */
    int count) {                        /* count is used to create a unique name in case of alias */

  struct HostAdapterInfoNode *temp;
  struct HostAdapterInfoNode *node;
  struct sockaddr_in tempAddr;
  int attemptNum;

  node = weaken(calloc)(1, sizeof(*node));
  if (!node) {
    errno = ENOMEM;
    return NULL;
  }

  memcpy(node->name, baseName, IFNAMSIZ);

  /* Are there more than a single unicast address ? */
  if (count > 0 || ((*ptrUA)->Next != NULL)) {
    /* Yes, compose it using <baseName>:<count> */
    size_t nameLen = strlen(node->name);
    if (nameLen+2 > IFNAMSIZ-2) {
      /* Appending the ":x" will exceed the size, need to chop the end */
      nameLen -= 2;
    }
    node->name[nameLen-2] = ':';
    node->name[nameLen-1] = '0'+count;
    node->name[nameLen] = '\0';
  }

  /* Is there a name clash with other interfaces? */
  for (attemptNum=0; attemptNum < MAX_NAME_CLASH; ++attemptNum) {
    temp = findAdapterByName(node->name);
    if (!temp) {
      break;

    } else {
      /* Yes, this name has been already used, append an extra 
       * character to resolve conflict. Note since the max length
       * of the string now is IFNAMSIZ-2, we have just enough space for this.
       * E.g. 'Ethernet_1' -> 'Ethernet_1a'
       */

      size_t pos = strlen(node->name);
      node->name[pos] = 'a' + attemptNum;
      node->name[pos+1] = '\0';
      /* Try again */
    }
  }
  if (attemptNum == MAX_NAME_CLASH) {
    /* Cannot resolve the conflict */
    weaken(free)(node);
    errno = EEXIST;
    return NULL;
  }

  /* Finally we got a unique short and friendly name */
  node->unicast = *((*ptrUA)->Address.lpSockaddr);
  if (*ptrUA == aa->FirstUnicastAddress) {
    short flags;
    /* This is the first unicast address of this interface
     * calculate the flags for this adapter. Flags to consider:
     * IFF_UP
     * IFF_BROADCAST        ** TODO: We need to validate
     * IFF_LOOPBACK
     * IFF_POINTOPOINT
     * IFF_MULTICAST
     * IFF_RUNNING          ** Same as IFF_UP for now
     * IFF_PROMISC          ** NOT SUPPORTED, unknown how to retrieve it
     */
    flags = 0;
    if (aa->OperStatus == kNtIfOperStatusUp) flags |= IFF_UP | IFF_RUNNING;
    if (aa->IfType == kNtIfTypePpp) flags |= IFF_POINTOPOINT;
    //if (aa->TunnelType != TUNNEL_TYPE_NONE) flags |= IFF_POINTOPOINT;
    if (aa->NoMulticast == 0) flags |= IFF_MULTICAST;
    if (aa->IfType == kNtIfTypeSoftwareLoopback) flags |= IFF_LOOPBACK;
    if (aa->FirstPrefix != NULL) flags |= IFF_BROADCAST;
    node->flags = flags;
  } else {
    /* Copy from previous node */
    node->flags = parentInfoNode->flags;
  }

  /* Process the prefix and extract the netmask and broadcast */
  /* According to the doc:
   *  ... On Windows Vista and later, the linked IP_ADAPTER_PREFIX structures pointed to 
   *  by the FirstPrefix member include three IP adapter prefixes for each IP address 
   *  assigned to the adapter. These include the host IP address prefix, the subnet IP 
   *  address prefix, and the subnet broadcast IP address prefix. In addition, for each 
   *  adapter there is a multicast address prefix and a broadcast address prefix.
   *
   * For example, interface "Ethernet", with 2 unicast addresses:
   *  - 192.168.1.84 
   *  - 192.168.5.99
   * The Prefix list has 8 elements:
   *  #1: 192.168.1.0/24      <- Network, use the PrefixLength for netmask
   *  #2: 192.168.1.84/32     <- Host IP
   *  #3: 192.168.1.255/32    <- Subnet broadcast
   *
   *  #4: 192.168.5.0/24      <- Network
   *  #5: 192.168.5.99/32     <- Host IP
   *  #6: 192.168.5.255/32    <- Subnet broadcast
   *
   *  #7: 224.0.0.0/4         <- Multicast
   *  #8: 255.255.255.255/32  <- Broadcast
   */

  /* Netmask */
  memset(&tempAddr, 0, sizeof(tempAddr));
  tempAddr.sin_family = AF_INET;
  tempAddr.sin_addr.s_addr = (uint32_t)((1LLU << (*ptrAP)->PrefixLength) - 1LLU);
  memcpy(&node->netmask, &tempAddr, sizeof(tempAddr));

  *ptrAP = (*ptrAP)->Next;
  *ptrAP = (*ptrAP)->Next;    /* Skip over Host IP */

  /* Broadcast */
  node->broadcast = *((*ptrAP)->Address.lpSockaddr);
  *ptrAP = (*ptrAP)->Next;

  /* Move pointer to Unicast Address record */
  *ptrUA = (*ptrUA)->Next;

  /* Append this node to the last node (if any) */
  if (parentInfoNode) {
    parentInfoNode->next = node;
  }

  /* Success */
  return node;
}

/* Returns -1 in case of failure */
static int createHostInfo(NtIpAdapterAddresses *firstAdapter) {
  NtIpAdapterAddresses *aa;
  NtIpAdapterUnicastAddress *ua;
  NtIpAdapterPrefix *ap;
  struct HostAdapterInfoNode *node = NULL;
  char baseName[IFNAMSIZ];
  char name[IFNAMSIZ];
  int count, i;

  /* __hostInfo must be empty */
  assert(__hostInfo == NULL);
  assert(weaken(tprecode16to8));

  for (aa = firstAdapter; aa; aa = aa->Next) {
    /* Skip all the interfaces with no address and the ones that are not AF_INET */
    if (!aa->FirstUnicastAddress || 
        aa->FirstUnicastAddress->Address.lpSockaddr->sa_family != AF_INET) {
      continue;
    }

    /* Use max IFNAMSIZ-1 chars, leave the last char for eventual conficts */
    tprecode16to8(baseName, IFNAMSIZ-1, aa->FriendlyName);
    baseName[IFNAMSIZ-2] = '\0';
    /* Replace any space with a '_' */
    for (i = 0; i < IFNAMSIZ-2; ++i) {
      if (baseName[i] == ' ') baseName[i] = '_';
      if (!baseName[i]) break;
    }
    for (count = 0, ua = aa->FirstUnicastAddress, ap = aa->FirstPrefix; 
        (ua != NULL) && (count < MAX_UNICAST_ADDR); 
        ++count) {
      node = appendHostInfo(node, baseName, aa, &ua, &ap, count);
      if (!node) {
        goto err;
      }
      if (!__hostInfo) __hostInfo = node;
    }

    /* Note: do we need to process the remaining adapter prefix? 
     *      ap       - points to broadcast addr
     *      ap->Next - points to interface multicast addr
     * Ignoring them for now
     */
  }
  return 0;

err:
  freeHostInfo(__hostInfo);
  return -1;
}

static int readAdapterAddresses(void) {
  uint32_t size, rc;
  NtIpAdapterAddresses * aa = NULL;

  assert(weaken(GetAdaptersAddresses));

  /* Calculate the required data size
   * Note: alternatively you can use AF_UNSPEC to also return IPv6 interfaces 
   */
  rc = weaken(GetAdaptersAddresses)(AF_INET,  
      kNtGaaFlagSkipAnycast | kNtGaaFlagSkipMulticast | kNtGaaFlagSkipDnsServer | kNtGaaFlagIncludePrefix,
      NULL,  /* Reserved */
      NULL,  /* Ptr */
      &size);
  if (rc != kNtErrorBufferOverflow) {
    ebadf();
    goto err;
  }

  aa = (NtIpAdapterAddresses *)weaken(malloc)(size);
  if (!aa) {
    enomem();
    goto err;
  }

  /* Re-run GetAdaptersAddresses this time with a valid buffer */
  rc = weaken(GetAdaptersAddresses)(AF_INET, 
      kNtGaaFlagSkipAnycast | kNtGaaFlagSkipMulticast | kNtGaaFlagSkipDnsServer | kNtGaaFlagIncludePrefix,
      //kNtGaaFlagIncludePrefix,
      NULL, 
      aa, 
      &size);
  if (rc != kNtErrorSuccess) {
    efault();
    goto err;
  }
  if (createHostInfo(aa) == -1) {
    goto err;
  }

  weaken(free)(aa);
  return 0;

err:
  if (aa) {
    weaken(free)(aa);
  }
  freeHostInfo();
  return -1;
}

textwindows int ioctl_siocgifconf_nt(int fd, struct ifconf *ifc) {
  NtIpAdapterAddresses *aa;
  struct HostAdapterInfoNode *node;
  struct ifreq *ptr;

  if (__hostInfo) {
    freeHostInfo();
  }

  if (readAdapterAddresses() == -1) {
    return -1;
  }

  for (ptr = ifc->ifc_req, node = __hostInfo;
      (((char *)(ptr+1) - ifc->ifc_buf) < ifc->ifc_len) && node;
      ptr++, node = node->next) {
    memcpy(ptr->ifr_name, node->name, IFNAMSIZ);
    memcpy(&ptr->ifr_addr, &node->unicast, sizeof(struct sockaddr));
  }
  ifc->ifc_len = (char *)ptr - ifc->ifc_buf;

  return 0;
}

/* Performs the SIOCGIFADDR operation */
int ioctl_siocgifaddr_nt(int fd, struct ifreq *ifr) {
  struct HostAdapterInfoNode *node;

  node = findAdapterByName(ifr->ifr_name);
  if (!node) {
    return ebadf();
  }
  memcpy(&ifr->ifr_addr, &node->unicast, sizeof(struct sockaddr));
  return 0;
}

/* Performs the SIOCGIFFLAGS operation */
int ioctl_siocgifflags_nt(int fd, struct ifreq *ifr) {
  struct HostAdapterInfoNode *node;

  node = findAdapterByName(ifr->ifr_name);
  if (!node) {
    return ebadf();
  }
  ifr->ifr_flags = node->flags;
  return 0;
}


/* Performs the SIOCGIFNETMASK operation */
int ioctl_siocgifnetmask_nt(int fd, struct ifreq *ifr) {
  struct HostAdapterInfoNode *node;

  node = findAdapterByName(ifr->ifr_name);
  if (!node) {
    return ebadf();
  }
  memcpy(&ifr->ifr_netmask, &node->netmask, sizeof(struct sockaddr));
  return 0;
}

/* Performs the SIOCGIFBRDADDR operation */
int ioctl_siocgifbrdaddr_nt(int fd, struct ifreq *ifr) {
  struct HostAdapterInfoNode *node;

  node = findAdapterByName(ifr->ifr_name);
  if (!node) {
    return ebadf();
  }
  memcpy(&ifr->ifr_broadaddr, &node->broadcast, sizeof(struct sockaddr));
  return 0;
}

