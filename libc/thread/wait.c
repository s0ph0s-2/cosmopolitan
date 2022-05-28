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
#include "libc/bits/atomic.h"
#include "libc/calls/calls.h"
#include "libc/dce.h"
#include "libc/sysv/consts/clock.h"
#include "libc/sysv/consts/futex.h"
#include "libc/thread/freebsd.internal.h"
#include "libc/thread/thread.h"

int cthread_memory_wait32(uint32_t* addr, uint32_t val,
                          const struct timespec* timeout) {
  size_t size;
  struct _umtx_time *put, ut;
  if (IsLinux() || IsOpenbsd()) {
    return futex(addr, FUTEX_WAIT, val, timeout, 0);

#if 0
  } else if (IsFreebsd()) {
    if (!timeout) {
      put = 0;
      size = 0;
    } else {
      ut._flags = 0;
      ut._clockid = CLOCK_REALTIME;
      ut._timeout = *timeout;
      put = &ut;
      size = sizeof(ut);
    }
    return _umtx_op(addr, UMTX_OP_MUTEX_WAIT, 0, &size, put);
#endif

  } else {
    unsigned tries;
    for (tries = 1; atomic_load(addr) == val; ++tries) {
      if (tries & 7) {
        __builtin_ia32_pause();
      } else {
        sched_yield();
      }
    }
    return 0;
  }
}

int cthread_memory_wake32(uint32_t* addr, int n) {
  if (IsLinux() || IsOpenbsd()) {
    return futex(addr, FUTEX_WAKE, n, 0, 0);
#if 0
  } else if (IsFreebsd()) {
    return _umtx_op(addr, UMTX_OP_MUTEX_WAKE, n, 0, 0);
#endif
  }
  return -1;
}
