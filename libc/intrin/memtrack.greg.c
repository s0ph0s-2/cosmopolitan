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
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/asan.internal.h"
#include "libc/intrin/bits.h"
#include "libc/intrin/directmap.internal.h"
#include "libc/intrin/likely.h"
#include "libc/intrin/strace.internal.h"
#include "libc/log/libfatal.internal.h"
#include "libc/log/log.h"
#include "libc/macros.internal.h"
#include "libc/mem/mem.h"
#include "libc/runtime/memtrack.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/errfuns.h"

#if IsModeDbg()
#define ASSERT_MEMTRACK()          \
  if (!AreMemoryIntervalsOk(mm)) { \
    PrintMemoryIntervals(2, mm);   \
    notpossible;                   \
  }
#else
#define ASSERT_MEMTRACK()
#endif

static void *MoveMemoryIntervals(struct MemoryInterval *d,
                                 const struct MemoryInterval *s, int n) {
  int i;
  _unassert(n >= 0);
  if (d > s) {
    for (i = n; i--;) {
      d[i] = s[i];
    }
  } else {
    for (i = 0; i < n; ++i) {
      d[i] = s[i];
    }
  }
  return d;
}

static void RemoveMemoryIntervals(struct MemoryIntervals *mm, int i, int n) {
  _unassert(i >= 0);
  _unassert(i + n <= mm->i);
  MoveMemoryIntervals(mm->p + i, mm->p + i + n, mm->i - (i + n));
  mm->i -= n;
}

static bool ExtendMemoryIntervals(struct MemoryIntervals *mm) {
  int prot, flags;
  char *base, *shad;
  size_t gran, size;
  struct DirectMap dm;
  gran = kMemtrackGran;
  base = (char *)kMemtrackStart;
  prot = PROT_READ | PROT_WRITE;
  flags = MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED;
  // TODO(jart): These map handles should not leak across NT fork()
  if (mm->p == mm->s) {
    // TODO(jart): How can we detect ASAN mode under GREG?
    if (1 || IsAsan()) {
      shad = (char *)(((intptr_t)base >> 3) + 0x7fff8000);
      dm = sys_mmap(shad, gran >> 3, prot, flags, -1, 0);
      if (!dm.addr) return false;
    }
    dm = sys_mmap(base, gran, prot, flags, -1, 0);
    if (!dm.addr) return false;
    MoveMemoryIntervals(dm.addr, mm->p, mm->i);
    mm->p = dm.addr;
    mm->n = gran / sizeof(*mm->p);
  } else {
    size = ROUNDUP(mm->n * sizeof(*mm->p), gran);
    base += size;
    if (IsAsan()) {
      shad = (char *)(((intptr_t)base >> 3) + 0x7fff8000);
      dm = sys_mmap(shad, gran >> 3, prot, flags, -1, 0);
      if (!dm.addr) return false;
    }
    dm = sys_mmap(base, gran, prot, flags, -1, 0);
    if (!dm.addr) return false;
    mm->n = (size + gran) / sizeof(*mm->p);
  }
  ASSERT_MEMTRACK();
  return true;
}

int CreateMemoryInterval(struct MemoryIntervals *mm, int i) {
  _unassert(i >= 0);
  _unassert(i <= mm->i);
  _unassert(mm->n >= 0);
  if (UNLIKELY(mm->i == mm->n) && !ExtendMemoryIntervals(mm)) return enomem();
  MoveMemoryIntervals(mm->p + i + 1, mm->p + i, mm->i++ - i);
  return 0;
}

static int PunchHole(struct MemoryIntervals *mm, int x, int y, int i) {
  if (CreateMemoryInterval(mm, i) == -1) return -1;
  mm->p[i + 0].size -= (size_t)(mm->p[i + 0].y - (x - 1)) * FRAMESIZE;
  mm->p[i + 0].y = x - 1;
  mm->p[i + 1].size -= (size_t)((y + 1) - mm->p[i + 1].x) * FRAMESIZE;
  mm->p[i + 1].x = y + 1;
  return 0;
}

int ReleaseMemoryIntervals(struct MemoryIntervals *mm, int x, int y,
                           void wf(struct MemoryIntervals *, int, int)) {
  unsigned l, r;
  ASSERT_MEMTRACK();
  _unassert(y >= x);
  if (!mm->i) return 0;
  // binary search for the lefthand side
  l = FindMemoryInterval(mm, x);
  if (l == mm->i) return 0;
  if (y < mm->p[l].x) return 0;

  // binary search for the righthand side
  r = FindMemoryInterval(mm, y);
  if (r == mm->i || (r > l && y < mm->p[r].x)) --r;
  _unassert(r >= l);
  _unassert(x <= mm->p[r].y);

  // remove the middle of an existing map
  //
  // ----|mmmmmmmmmmmmmmmm|--------- before
  //           xxxxx
  // ----|mmmm|-----|mmmmm|--------- after
  //
  // this isn't possible on windows because we track each
  // 64kb segment on that platform using a separate entry
  if (l == r && x > mm->p[l].x && y < mm->p[l].y) {
    return PunchHole(mm, x, y, l);
  }

  // trim the right side of the lefthand map
  //
  // ----|mmmmmmm|-------------- before
  //           xxxxx
  // ----|mmmm|----------------- after
  //
  if (x > mm->p[l].x && x <= mm->p[l].y) {
    _unassert(y >= mm->p[l].y);
    if (IsWindows()) return einval();
    mm->p[l].size -= (size_t)(mm->p[l].y - (x - 1)) * FRAMESIZE;
    mm->p[l].y = x - 1;
    _unassert(mm->p[l].x <= mm->p[l].y);
    ++l;
  }

  // trim the left side of the righthand map
  //
  // ------------|mmmmm|-------- before
  //           xxxxx
  // ---------------|mm|-------- after
  //
  if (y >= mm->p[r].x && y < mm->p[r].y) {
    _unassert(x <= mm->p[r].x);
    if (IsWindows()) return einval();
    mm->p[r].size -= (size_t)((y + 1) - mm->p[r].x) * FRAMESIZE;
    mm->p[r].x = y + 1;
    _unassert(mm->p[r].x <= mm->p[r].y);
    --r;
  }

  if (l <= r) {
    if (IsWindows() && wf) {
      wf(mm, l, r);
    }
    RemoveMemoryIntervals(mm, l, r - l + 1);
  }
  return 0;
}

int TrackMemoryInterval(struct MemoryIntervals *mm, int x, int y, long h,
                        int prot, int flags, bool readonlyfile, bool iscow,
                        long offset, long size) {
  unsigned i;
  ASSERT_MEMTRACK();
  _unassert(y >= x);
  i = FindMemoryInterval(mm, x);

  // try to extend the righthand side of the lefthand entry
  // we can't do that if we're tracking independent handles
  // we can't do that if it's a file map with a small size!
  if (i && x == mm->p[i - 1].y + 1 && h == mm->p[i - 1].h &&
      prot == mm->p[i - 1].prot && flags == mm->p[i - 1].flags &&
      mm->p[i - 1].size ==
          (size_t)(mm->p[i - 1].y - mm->p[i - 1].x) * FRAMESIZE + FRAMESIZE) {
    mm->p[i - 1].size += (size_t)(y - mm->p[i - 1].y) * FRAMESIZE;
    mm->p[i - 1].y = y;
    // if we filled the hole then merge the two mappings
    if (i < mm->i && y + 1 == mm->p[i].x && h == mm->p[i].h &&
        prot == mm->p[i].prot && flags == mm->p[i].flags) {
      mm->p[i - 1].y = mm->p[i].y;
      mm->p[i - 1].size += mm->p[i].size;
      RemoveMemoryIntervals(mm, i, 1);
    }
  }

  // try to extend the lefthand side of the righthand entry
  // we can't do that if we're creating a smaller file map!
  else if (i < mm->i && y + 1 == mm->p[i].x && h == mm->p[i].h &&
           prot == mm->p[i].prot && flags == mm->p[i].flags &&
           size == (size_t)(y - x) * FRAMESIZE + FRAMESIZE) {
    mm->p[i].size += (size_t)(mm->p[i].x - x) * FRAMESIZE;
    mm->p[i].x = x;
  }

  // otherwise, create a new entry and memmove the items
  else {
    if (CreateMemoryInterval(mm, i) == -1) return -1;
    mm->p[i].x = x;
    mm->p[i].y = y;
    mm->p[i].h = h;
    mm->p[i].prot = prot;
    mm->p[i].flags = flags;
    mm->p[i].offset = offset;
    mm->p[i].size = size;
    mm->p[i].iscow = iscow;
    mm->p[i].readonlyfile = readonlyfile;
  }

  return 0;
}
