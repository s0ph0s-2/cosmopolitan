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
#include "libc/intrin/asan.internal.h"
#include "libc/intrin/bsr.h"
#include "libc/intrin/kprintf.h"
#include "libc/sock/select.h"
#include "libc/sock/select.internal.h"

#define N 100

#define append(...) o += ksnprintf(buf + o, N - o, __VA_ARGS__)

const char *(DescribeFdSet)(char buf[N], ssize_t rc, int nfds, fd_set *fds) {
  int o = 0;

  if (!fds) return "NULL";
  if ((!IsAsan() && kisdangerous(fds)) ||
      (IsAsan() && !__asan_is_valid(fds, sizeof(*fds) * nfds))) {
    ksnprintf(buf, N, "%p", fds);
    return buf;
  }

  append("{");

  bool gotsome = false;
  for (int fd = 0; fd < nfds; fd += 64) {
    uint64_t w = fds->fds_bits[fd >> 6];
    while (w) {
      unsigned o = _bsr(w);
      w &= ~((uint64_t)1 << o);
      if (fd + o < nfds) {
        if (!gotsome) {
          gotsome = true;
          append(", ");
          append("%d", fd);
        }
      }
    }
  }

  append("}");

  return buf;
}
