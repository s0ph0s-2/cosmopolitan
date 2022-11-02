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
#include "libc/intrin/directmap.internal.h"
#include "libc/nt/memory.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/memtrack.internal.h"

#define ADDR(x) ((char *)((int64_t)((uint64_t)(x) << 32) >> 16))

textwindows int sys_mprotect_nt(void *addr, size_t size, int prot) {
  int rc = 0;
  unsigned i;
  uint32_t op;
  char *a, *b, *x, *y, *p;
  __mmi_lock();
  p = addr;
  i = FindMemoryInterval(&_mmi, (intptr_t)p >> 16);
  if (i == _mmi.i || (!i && p + size <= ADDR(_mmi.p[0].x))) {
    // memory isn't in memtrack
    // let's just trust the user then
    // it's probably part of the executable
    if (!VirtualProtect(addr, size, __prot2nt(prot, false), &op)) {
      rc = -1;
    }
  } else {
    // memory is in memtrack, so use memtrack, to do dimensioning
    // we unfortunately must do something similar to this for cow
    for (; i < _mmi.i; ++i) {
      x = ADDR(_mmi.p[i].x);
      y = x + _mmi.p[i].size;
      if ((x <= p && p < y) || (x < p + size && p + size <= y) ||
          (p < x && y < p + size)) {
        a = MIN(MAX(p, x), y);
        b = MAX(MIN(p + size, y), x);
        if (!VirtualProtect(a, b - a, __prot2nt(prot, _mmi.p[i].iscow), &op)) {
          rc = -1;
          break;
        }
      } else {
        break;
      }
    }
  }
  __mmi_unlock();
  return rc;
}
