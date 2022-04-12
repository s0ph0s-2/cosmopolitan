/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2021 Justine Alexandra Roberts Tunney                              │
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
#include "libc/intrin/lockxadd.h"
#include "libc/runtime/runtime.h"

/**
 * Compares and exchanges w/ lock prefix.
 *
 * @param ifthing is uint𝑘_t[hasatleast 1] where 𝑘 ∈ {8,16,32,64}
 * @param size is automatically supplied by macro wrapper
 * @return value at location `*ifthing` *before* addition
 * @see InterlockedAdd() for a very similar API
 * @see xadd() if only written by one thread
 */
intptr_t(_lockxadd)(void *ifthing, intptr_t replaceitwithme, size_t size) {
  switch (size) {
    case 1:
      return _lockxadd((int8_t *)ifthing, (int8_t)replaceitwithme);
    case 2:
      return _lockxadd((int16_t *)ifthing, (int16_t)replaceitwithme);
    case 4:
      return _lockxadd((int32_t *)ifthing, (int32_t)replaceitwithme);
    case 8:
      return _lockxadd((int64_t *)ifthing, (int64_t)replaceitwithme);
    default:
      abort();
  }
}
