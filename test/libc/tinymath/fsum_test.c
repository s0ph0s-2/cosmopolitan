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
#include "libc/macros.internal.h"
#include "libc/math.h"
#include "libc/mem/gc.h"
#include "libc/testlib/ezbench.h"
#include "libc/testlib/testlib.h"
#include "libc/x/xasprintf.h"

#define N 100000

float F[N];
double D[N];

void SetUp(void) {
  int i;
  for (i = 0; i < N / 2; ++i) {
    D[i * 2 + 0] = 1000000000.1;
    D[i * 2 + 1] = 1.1;
  }
  for (i = 0; i < N / 2; ++i) {
    F[i * 2 + 0] = 1000.1;
    F[i * 2 + 1] = 1.1;
  }
}

TEST(fsum, test) {
  EXPECT_STREQ("500000000.6", _gc(xasprintf("%.15g", fsum(D, N) / N)));
}

TEST(fsumf, test) {
  EXPECT_STREQ("500.6", _gc(xasprintf("%.7g", fsumf(F, N) / N)));
}

BENCH(fsum, bench) {
  EZBENCH2("fsum", donothing, fsum(D, N));
  EZBENCH2("fsumf", donothing, fsumf(F, N));
}
