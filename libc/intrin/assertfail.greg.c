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
#include "libc/calls/strace.internal.h"
#include "libc/intrin/cmpxchg.h"
#include "libc/intrin/kprintf.h"
#include "libc/intrin/lockcmpxchg.h"
#include "libc/log/log.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/runtime.h"

/**
 * Handles failure of assert() macro.
 */
relegated wontreturn void __assert_fail(const char *expr, const char *file,
                                        int line) {
  int rc;
  static bool noreentry;
  __strace = 0;
  g_ftrace = 0;
  kprintf("%s:%d: assert(%s) failed\n", file, line, expr);
  if (_lockcmpxchg(&noreentry, false, true)) {
    if (weaken(__die)) {
      weaken(__die)();
    } else {
      kprintf("can't backtrace b/c `__die` not linked\n");
    }
    rc = 23;
  } else {
    rc = 24;
  }
  if (weaken(__die)) weaken(__die)();
  __restorewintty();
  _Exit(rc);
}
