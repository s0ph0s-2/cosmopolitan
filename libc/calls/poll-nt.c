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
#include "libc/assert.h"
#include "libc/calls/calls.h"
#include "libc/calls/internal.h"
#include "libc/calls/sig.internal.h"
#include "libc/calls/state.internal.h"
#include "libc/calls/struct/sigaction.h"
#include "libc/calls/struct/timespec.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/atomic.h"
#include "libc/intrin/bits.h"
#include "libc/intrin/strace.internal.h"
#include "libc/macros.internal.h"
#include "libc/mem/mem.h"
#include "libc/nt/console.h"
#include "libc/nt/enum/filetype.h"
#include "libc/nt/errors.h"
#include "libc/nt/files.h"
#include "libc/nt/ipc.h"
#include "libc/nt/runtime.h"
#include "libc/nt/struct/pollfd.h"
#include "libc/nt/synchronization.h"
#include "libc/nt/thread.h"
#include "libc/nt/thunk/msabi.h"
#include "libc/nt/winsock.h"
#include "libc/sock/internal.h"
#include "libc/sock/struct/pollfd.h"
#include "libc/sock/struct/pollfd.internal.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/poll.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/errfuns.h"
#include "libc/thread/posixthread.internal.h"
#include "libc/thread/tls.h"

#ifdef __x86_64__

// Polls on the New Technology.
//
// This function is used to implement poll() and select(). You may poll
// on both sockets and files at the same time. We also poll for signals
// while poll is polling.
textwindows int sys_poll_nt(struct pollfd *fds, uint64_t nfds, uint32_t *ms,
                            const sigset_t *sigmask) {
  bool ok;
  uint64_t m;
  bool interrupted;
  sigset_t oldmask;
  uint32_t cm, avail, waitfor;
  struct sys_pollfd_nt pipefds[8];
  struct sys_pollfd_nt sockfds[64];
  int pipeindices[ARRAYLEN(pipefds)];
  int sockindices[ARRAYLEN(sockfds)];
  int i, rc, sn, pn, gotinvals, gotpipes, gotsocks;

#if IsModeDbg()
  struct timespec noearlier =
      timespec_add(timespec_real(), timespec_frommillis(ms ? *ms : -1u));
#endif

  // do the planning
  // we need to read static variables
  // we might need to spawn threads and open pipes
  m = atomic_exchange(&__get_tls()->tib_sigmask, -1);
  __fds_lock();
  for (gotinvals = rc = sn = pn = i = 0; i < nfds; ++i) {
    if (fds[i].fd < 0) continue;
    if (__isfdopen(fds[i].fd)) {
      if (__isfdkind(fds[i].fd, kFdSocket)) {
        if (sn < ARRAYLEN(sockfds)) {
          // the magnums for POLLIN/OUT/PRI on NT include the other ones too
          // we need to clear ones like POLLNVAL or else WSAPoll shall whine
          sockindices[sn] = i;
          sockfds[sn].handle = g_fds.p[fds[i].fd].handle;
          sockfds[sn].events = fds[i].events & (POLLPRI | POLLIN | POLLOUT);
          sockfds[sn].revents = 0;
          ++sn;
        } else {
          // too many socket fds
          rc = enomem();
          break;
        }
      } else if (pn < ARRAYLEN(pipefds)) {
        pipeindices[pn] = i;
        pipefds[pn].handle = g_fds.p[fds[i].fd].handle;
        pipefds[pn].events = 0;
        pipefds[pn].revents = 0;
        switch (g_fds.p[fds[i].fd].flags & O_ACCMODE) {
          case O_RDONLY:
            pipefds[pn].events = fds[i].events & POLLIN;
            break;
          case O_WRONLY:
            pipefds[pn].events = fds[i].events & POLLOUT;
            break;
          case O_RDWR:
            pipefds[pn].events = fds[i].events & (POLLIN | POLLOUT);
            break;
          default:
            __builtin_unreachable();
        }
        ++pn;
      } else {
        // too many non-socket fds
        rc = enomem();
        break;
      }
    } else {
      ++gotinvals;
    }
  }
  __fds_unlock();
  atomic_store_explicit(&__get_tls()->tib_sigmask, m, memory_order_release);
  if (rc) {
    // failed to create a polling solution
    goto Finished;
  }

  // perform the i/o and sleeping and looping
  for (;;) {
    // see if input is available on non-sockets
    for (gotpipes = i = 0; i < pn; ++i) {
      if (pipefds[i].events & POLLOUT) {
        // we have no way of polling if a non-socket is writeable yet
        // therefore we assume that if it can happen, it shall happen
        pipefds[i].revents |= POLLOUT;
      }
      if (pipefds[i].events & POLLIN) {
        if (GetFileType(pipefds[i].handle) == kNtFileTypePipe) {
          ok = PeekNamedPipe(pipefds[i].handle, 0, 0, 0, &avail, 0);
          POLLTRACE("PeekNamedPipe(%ld, 0, 0, 0, [%'u], 0) → %hhhd% m",
                    pipefds[i].handle, avail, ok);
          if (ok) {
            if (avail) {
              pipefds[i].revents |= POLLIN;
            }
          } else {
            pipefds[i].revents |= POLLERR;
          }
        } else if (GetConsoleMode(pipefds[i].handle, &cm)) {
          if (CountConsoleInputBytes(pipefds[i].handle)) {
            pipefds[i].revents |= POLLIN;
          }
        } else {
          // we have no way of polling if a non-socket is readable yet
          // therefore we assume that if it can happen it shall happen
          pipefds[i].revents |= POLLIN;
        }
      }
      if (pipefds[i].revents) {
        ++gotpipes;
      }
    }
    // if we haven't found any good results yet then here we
    // compute a small time slice we don't mind sleeping for
    if (sn) {
      if ((gotsocks = WSAPoll(sockfds, sn, 0)) == -1) {
        return __winsockerr();
      }
    } else {
      gotsocks = 0;
    }
    waitfor = MIN(__SIG_POLL_INTERVAL_MS, *ms);
    if (!gotinvals && !gotsocks && !gotpipes && waitfor) {
      POLLTRACE("poll() sleeping for %'d out of %'u ms", waitfor, *ms);
      struct PosixThread *pt = _pthread_self();
      pt->abort_errno = 0;
      if (sigmask) __sig_mask(SIG_SETMASK, sigmask, &oldmask);
      interrupted = _check_interrupts(0) || __pause_thread(waitfor);
      if (sigmask) __sig_mask(SIG_SETMASK, &oldmask, 0);
      if (interrupted) return -1;
      if (*ms != -1u) {
        if (waitfor < *ms) {
          *ms -= waitfor;
        } else {
          *ms = 0;
        }
      }
    }
    // we gave all the sockets and all the named pipes a shot
    // if we found anything at all then it's time to end work
    if (gotinvals || gotpipes || gotsocks || !*ms) {
      break;
    }
  }

  // the system call is going to succeed
  // it's now ok to start setting the output memory
  for (i = 0; i < nfds; ++i) {
    if (fds[i].fd < 0 || __isfdopen(fds[i].fd)) {
      fds[i].revents = 0;
    } else {
      fds[i].revents = POLLNVAL;
    }
  }
  for (i = 0; i < pn; ++i) {
    fds[pipeindices[i]].revents = pipefds[i].revents;
  }
  for (i = 0; i < sn; ++i) {
    fds[sockindices[i]].revents = sockfds[i].revents;
  }

  // and finally return
  rc = gotinvals + gotpipes + gotsocks;

Finished:

#if IsModeDbg()
  struct timespec ended = timespec_real();
  if (!rc && timespec_cmp(ended, noearlier) < 0) {
    STRACE("poll() ended %'ld ns too soon!",
           timespec_tonanos(timespec_sub(noearlier, ended)));
  }
#endif

  return rc;
}

#endif /* __x86_64__ */
