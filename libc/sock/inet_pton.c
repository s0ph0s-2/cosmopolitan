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
#include "libc/fmt/fmt.h"
#include "libc/macros.internal.h"
#include "libc/sock/internal.h"
#include "libc/sock/sock.h"
#include "libc/sysv/consts/af.h"
#include "libc/sysv/consts/inaddr.h"
#include "libc/sysv/errfuns.h"

/**
 * Converts internet address string to binary.
 *
 * @param af can be AF_INET
 * @param src is the ASCII-encoded address
 * @param dst is where the binary-encoded net-order address goes
 * @return 1 on success, 0 on src malformed, or -1 w/ errno
 */
int inet_pton(int af, const char *src, void *dst) {
  uint8_t *p;
  int b, c, j;
  if (af != AF_INET) return eafnosupport();
  j = 0;
  p = dst;
  p[0] = 0;
  while ((c = *src++)) {
    if (isdigit(c)) {
      b = c - '0' + p[j] * 10;
      p[j] = MIN(255, b);
      if (b > 255) return 0;
    } else if (c == '.') {
      if (++j == 4) return 0;
      p[j] = 0;
    } else {
      return 0;
    }
  }
  return j == 3 ? 1 : 0;
}
