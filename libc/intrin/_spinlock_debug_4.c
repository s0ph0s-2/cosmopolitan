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
#include "libc/bits/weaken.h"
#include "libc/calls/calls.h"
#include "libc/intrin/kprintf.h"
#include "libc/intrin/lockcmpxchgp.h"
#include "libc/intrin/spinlock.h"
#include "libc/log/log.h"
#include "libc/nexgen32e/rdtsc.h"
#include "libc/runtime/internal.h"
#include "libc/time/clockstonanos.internal.h"

privileged void _spinlock_debug_4(int *lock, const char *lockname,
                                  const char *file, int line,
                                  const char *func) {
  unsigned i;
  int me, owner;
  uint64_t ts1, ts2;
  me = gettid();
  owner = 0;
  if (!_lockcmpxchgp(lock, &owner, me)) {
    if (owner == me) {
      kprintf("%s:%d: error: lock re-entry on %s in %s()\n", file, line,
              lockname, func);
      if (weaken(__die)) weaken(__die)();
      __restorewintty();
      _Exit(1);
    }
    i = 0;
    ts1 = rdtsc();
    for (;;) {
      owner = 0;
      if (_lockcmpxchgp(lock, &owner, me)) break;
      ts2 = rdtsc();
      if (ClocksToNanos(ts1, ts2) > 1000000000ul) {
        ts1 = ts2;
        kprintf("%s:%d: warning: slow lock on %s in %s()\n", file, line,
                lockname, func);
      }
      if (++i & 7) {
        __builtin_ia32_pause();
      } else {
        sched_yield();
      }
    }
  }
}
