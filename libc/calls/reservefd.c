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
#include "libc/bits/weaken.h"
#include "libc/calls/calls.h"
#include "libc/calls/internal.h"
#include "libc/calls/state.internal.h"
#include "libc/calls/strace.internal.h"
#include "libc/intrin/kprintf.h"
#include "libc/intrin/spinlock.h"
#include "libc/macros.internal.h"
#include "libc/mem/mem.h"
#include "libc/runtime/runtime.h"
#include "libc/str/str.h"
#include "libc/sysv/errfuns.h"

// XXX: until we can add read locks to all the code that uses g_fds.p
//      (right now we only have write locks) we need to keep old copies
//      of g_fds.p around after it's been extended, so that threads
//      which are using an fd they de facto own can continue reading
static void FreeOldFdsArray(void *p) {
  weaken(free)(p);
}

/**
 * Grows file descriptor array memory if needed.
 */
int __ensurefds_unlocked(int fd) {
  size_t n1, n2;
  struct Fd *p1, *p2;
  if (fd < g_fds.n) return fd;
  STRACE("__ensurefds(%d) extending", fd);
  if (!weaken(malloc)) return emfile();
  p1 = g_fds.p;
  n1 = g_fds.n;
  if (p1 == g_fds.__init_p) {
    if (!(p2 = weaken(malloc)(sizeof(g_fds.__init_p)))) return -1;
    memcpy(p2, p1, sizeof(g_fds.__init_p));
    g_fds.p = p1 = p2;
  }
  n2 = n1;
  while (n2 <= fd) n2 *= 2;
  if (!(p2 = weaken(malloc)(n2 * sizeof(*p1)))) return -1;
  __cxa_atexit(FreeOldFdsArray, p1, 0);
  memcpy(p2, p1, n1 * sizeof(*p1));
  bzero(p2 + n1, (n2 - n1) * sizeof(*p1));
  g_fds.p = p2;
  g_fds.n = n2;
  return fd;
}

/**
 * Grows file descriptor array memory if needed.
 */
int __ensurefds(int fd) {
  __fds_lock();
  fd = __ensurefds_unlocked(fd);
  __fds_unlock();
  return fd;
}

/**
 * Finds open file descriptor slot.
 */
int __reservefd_unlocked(int start) {
  int fd;
  for (fd = MAX(start, g_fds.f); fd < g_fds.n; ++fd) {
    if (!g_fds.p[fd].kind) {
      break;
    }
  }
  fd = __ensurefds_unlocked(fd);
  bzero(g_fds.p + fd, sizeof(*g_fds.p));
  g_fds.p[fd].kind = kFdReserved;
  return fd;
}

/**
 * Finds open file descriptor slot.
 */
int __reservefd(int start) {
  int fd;
  __fds_lock();
  fd = __reservefd_unlocked(start);
  __fds_unlock();
  return fd;
}

/**
 * Closes non-stdio file descriptors to free dynamic memory.
 */
static void FreeFds(void) {
  int i, keep = 3;
  STRACE("FreeFds()");
  __fds_lock();
  for (i = keep; i < g_fds.n; ++i) {
    if (g_fds.p[i].kind) {
      __fds_unlock();
      close(i);
      __fds_lock();
    }
  }
  if (g_fds.p != g_fds.__init_p) {
    bzero(g_fds.__init_p, sizeof(g_fds.__init_p));
    memcpy(g_fds.__init_p, g_fds.p, sizeof(*g_fds.p) * keep);
    if (weaken(free)) {
      weaken(free)(g_fds.p);
    }
    g_fds.p = g_fds.__init_p;
    g_fds.n = ARRAYLEN(g_fds.__init_p);
  }
  __fds_unlock();
}

static textstartup void FreeFdsInit(void) {
  atexit(FreeFds);
}

const void *const FreeFdsCtor[] initarray = {
    FreeFdsInit,
};
