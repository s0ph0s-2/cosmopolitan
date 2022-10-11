#ifndef COSMOPOLITAN_LIBC_SOCK_STRUCT_MSGHDR_H_
#define COSMOPOLITAN_LIBC_SOCK_STRUCT_MSGHDR_H_
#include "libc/calls/struct/iovec.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

struct msghdr {            /* Linux+NT ABI */
  void *msg_name;          /* optional address */
  uint32_t msg_namelen;    /* size of msg_name */
  struct iovec *msg_iov;   /* scatter/gather array */
  int msg_iovlen;          /* iovec count */
  void *msg_control;       /* credentials and stuff */
  uint32_t msg_controllen; /* size of msg_control */
  uint32_t __pad0;         /* reconcile abi */
  int msg_flags;           /* MSG_XXX */
};

ssize_t recvmsg(int, struct msghdr *, int);
ssize_t sendmsg(int, const struct msghdr *, int);

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_SOCK_STRUCT_MSGHDR_H_ */
