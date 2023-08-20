/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
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
#include "libc/calls/sig.internal.h"
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/calls/syscall_support-nt.internal.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/atomic.h"
#include "libc/intrin/dll.h"
#include "libc/intrin/strace.internal.h"
#include "libc/nt/enum/threadaccess.h"
#include "libc/nt/runtime.h"
#include "libc/nt/thread.h"
#include "libc/sysv/consts/sicode.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/errfuns.h"
#include "libc/thread/posixthread.internal.h"
#include "libc/thread/thread.h"
#include "libc/thread/tls.h"

static dontinline textwindows int __tkill_nt(int tid, int sig,
                                             struct CosmoTib *tib) {

  // check to see if this is a cosmo posix thread
  int rc = 0;
  struct Dll *e;
  bool found = false;
  pthread_spin_lock(&_pthread_lock);
  for (e = dll_first(_pthread_list); e; e = dll_next(_pthread_list, e)) {
    enum PosixThreadStatus status;
    struct PosixThread *pt = POSIXTHREAD_CONTAINER(e);
    if (tib && tib != pt->tib) continue;
    int other = atomic_load_explicit(&pt->tib->tib_tid, memory_order_acquire);
    if (!other || tid != other) continue;
    status = atomic_load_explicit(&pt->status, memory_order_acquire);
    found = true;
    if (status < kPosixThreadTerminated) {
      if (sig == SIGKILL) {
        intptr_t h;
        if ((h = OpenThread(kNtThreadTerminate, false, tid))) {
          TerminateThread(h, sig);
          CloseHandle(h);
        }
        atomic_store_explicit(&pt->status, kPosixThreadTerminated,
                              memory_order_release);
      } else {
        rc = __sig_add(tid, sig, SI_TKILL);
      }
    } else {
      // already dead but not joined
    }
    break;
  }
  pthread_spin_unlock(&_pthread_lock);
  if (found) {
    return rc;
  }

  // otherwise try our luck sigkilling a manually made thread
  if (!tib) {
    intptr_t h;
    if ((h = OpenThread(kNtThreadTerminate, false, tid))) {
      if (TerminateThread(h, sig)) {
        return 0;
      } else {
        return __winerr();
      }
      CloseHandle(h);
    } else {
      return __winerr();
    }
  } else {
    return esrch();
  }
}

// OpenBSD has an optional `tib` parameter for extra safety.
int __tkill(int tid, int sig, void *tib) {
  int rc;
  if (IsLinux() || IsXnu() || IsFreebsd() || IsOpenbsd() || IsNetbsd()) {
    rc = sys_tkill(tid, sig, tib);
  } else if (IsWindows()) {
    rc = __tkill_nt(tid, sig, tib);
  } else {
    rc = enosys();
  }
  STRACE("tkill(%d, %G) → %d% m", tid, sig, rc);
  return rc;
}

/**
 * Kills thread.
 *
 * @param tid is thread id
 * @param sig does nothing on xnu
 * @return 0 on success, or -1 w/ errno
 * @raise EAGAIN if `RLIMIT_SIGPENDING` was exceeded
 * @raise EINVAL if `tid` or `sig` were invalid
 * @raise ESRCH if no such `tid` existed
 * @raise EPERM if permission was denied
 * @asyncsignalsafe
 */
int tkill(int tid, int sig) {
  return __tkill(tid, sig, 0);
}
