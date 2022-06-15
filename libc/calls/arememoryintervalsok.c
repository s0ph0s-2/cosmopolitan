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
#include "libc/calls/strace.internal.h"
#include "libc/intrin/kprintf.h"
#include "libc/runtime/memtrack.internal.h"

noasan bool AreMemoryIntervalsOk(const struct MemoryIntervals *mm) {
  /* asan runtime depends on this function */
  int i;
  for (i = 0; i < mm->i; ++i) {
    if (mm->p[i].y < mm->p[i].x) {
      STRACE("AreMemoryIntervalsOk() y should be >= x!");
      return false;
    }
    if (!(mm->p[i].size <=
              (size_t)(mm->p[i].y - mm->p[i].x) * FRAMESIZE + FRAMESIZE &&
          mm->p[i].size > (size_t)(mm->p[i].y - mm->p[i].x) * FRAMESIZE)) {
      STRACE("AreMemoryIntervalsOk() size is wrong!");
      return false;
    }
    if (i) {
      if (mm->p[i].h != -1 || mm->p[i - 1].h != -1) {
        if (mm->p[i].x <= mm->p[i - 1].y) {
          return false;
        }
      } else {
        if (!(mm->p[i - 1].y + 1 <= mm->p[i].x)) {
          STRACE("AreMemoryIntervalsOk() out of order or overlap!");
          return false;
        }
      }
    }
  }
  return true;
}
