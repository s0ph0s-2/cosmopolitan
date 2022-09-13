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
#include "libc/dce.h"
#include "libc/fmt/fmt.h"
#include "libc/intrin/asan.internal.h"
#include "libc/log/libfatal.internal.h"
#include "libc/log/log.h"
#include "libc/mem/mem.h"
#include "libc/mem/gc.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/testlib/ezbench.h"
#include "libc/testlib/testlib.h"

TEST(asan, test) {
  char *p;
  if (!IsAsan()) return;
  p = gc(malloc(3));
  EXPECT_TRUE(__asan_is_valid(p, 3));
  EXPECT_FALSE(__asan_is_valid(p, 4));
  EXPECT_TRUE(__asan_is_valid(p + 1, 2));
  EXPECT_FALSE(__asan_is_valid(p + 1, 3));
  p = gc(malloc(8 + 3));
  EXPECT_TRUE(__asan_is_valid(p, 8 + 3));
  EXPECT_FALSE(__asan_is_valid(p, 8 + 4));
  EXPECT_TRUE(__asan_is_valid(p + 1, 8 + 2));
  EXPECT_FALSE(__asan_is_valid(p + 1, 8 + 3));
  p = gc(malloc(64 + 3));
  EXPECT_TRUE(__asan_is_valid(p, 64 + 3));
  EXPECT_FALSE(__asan_is_valid(p, 64 + 4));
  EXPECT_TRUE(__asan_is_valid(p + 1, 64 + 2));
  EXPECT_FALSE(__asan_is_valid(p + 1, 64 + 3));
  EXPECT_FALSE(__asan_is_valid(p - 1, 64));
}

TEST(asan, test2) {
  char *p;
  if (!IsAsan()) return;
  p = gc(memalign(16, 64));
  // asan design precludes this kind of poisoning
  __asan_poison(p + 1, 1, kAsanProtected);
  EXPECT_TRUE(__asan_is_valid(p, 2));
  EXPECT_TRUE(__asan_is_valid(p + 1, 2));
  // but we can do this
  __asan_poison(p + 7, 1, kAsanProtected);
  EXPECT_TRUE(__asan_is_valid(p + 6, 1));
  EXPECT_FALSE(__asan_is_valid(p + 7, 1));
  EXPECT_TRUE(__asan_is_valid(p + 8, 1));
  EXPECT_FALSE(__asan_is_valid(p + 6, 2));
  EXPECT_FALSE(__asan_is_valid(p + 7, 2));
  EXPECT_FALSE(__asan_is_valid(p + 6, 3));
  __asan_unpoison(p + 7, 1);
  EXPECT_TRUE(__asan_is_valid(p + 6, 3));
}

TEST(asan, testEmptySize_isAlwaysValid) {
  if (!IsAsan()) return;
  EXPECT_TRUE(__asan_is_valid(0, 0));
  EXPECT_TRUE(__asan_is_valid((void *)(intptr_t)-2, 0));
  EXPECT_TRUE(__asan_is_valid((void *)(intptr_t)9999, 0));
}

TEST(asan, testBigSize_worksFine) {
  char *p;
  if (!IsAsan()) return;
  p = malloc(64 * 1024);
  EXPECT_TRUE(__asan_is_valid(p, 64 * 1024));
  EXPECT_FALSE(__asan_is_valid(p - 1, 64 * 1024));
  EXPECT_FALSE(__asan_is_valid(p + 1, 64 * 1024));
  EXPECT_TRUE(__asan_is_valid(p + 1, 64 * 1024 - 1));
  free(p);
}

TEST(asan, testUnmappedShadowMemory_doesntSegfault) {
  if (!IsAsan()) return;
  EXPECT_FALSE(__asan_is_valid(0, 1));
  EXPECT_FALSE(__asan_is_valid((void *)(intptr_t)-1, 1));
  EXPECT_FALSE(__asan_is_valid((void *)(intptr_t)-2, 1));
  EXPECT_FALSE(__asan_is_valid((void *)(intptr_t)9999, 1));
  EXPECT_FALSE(__asan_is_valid(0, 7));
  EXPECT_FALSE(__asan_is_valid((void *)(intptr_t)-1, 7));
  EXPECT_FALSE(__asan_is_valid((void *)(intptr_t)-2, 7));
  EXPECT_FALSE(__asan_is_valid((void *)(intptr_t)9999, 7));
}

BENCH(asan, bench) {
  char *p;
  size_t n, m;
  if (!IsAsan()) return;
  m = 4 * 1024 * 1024;
  p = gc(malloc(m));
  EZBENCH_N("__asan_check", 0, EXPROPRIATE(__asan_check(p, 0).kind));
  for (n = 2; n <= m; n *= 2) {
    EZBENCH_N("__asan_check", n, EXPROPRIATE(__asan_check(p, n).kind));
  }
}
