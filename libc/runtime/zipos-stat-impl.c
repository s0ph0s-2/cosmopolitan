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
#include "libc/calls/struct/stat.h"
#include "libc/intrin/safemacros.internal.h"
#include "libc/runtime/zipos.internal.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/s.h"
#include "libc/sysv/errfuns.h"
#include "libc/zip.internal.h"

int __zipos_stat_impl(struct Zipos *zipos, size_t cf, struct stat *st) {
  size_t lf;
  if (zipos && st) {
    bzero(st, sizeof(*st));
    if (cf == ZIPOS_SYNTHETIC_DIRECTORY) {
      st->st_mode = S_IFDIR | 0555;
    } else {
      lf = GetZipCfileOffset(zipos->map + cf);
      st->st_mode = GetZipCfileMode(zipos->map + cf);
      st->st_size = GetZipLfileUncompressedSize(zipos->map + lf);
      st->st_blocks =
          roundup(GetZipLfileCompressedSize(zipos->map + lf), 512) / 512;
      GetZipCfileTimestamps(zipos->map + cf, &st->st_mtim, &st->st_atim,
                            &st->st_ctim, 0);
      st->st_birthtim = st->st_ctim;
    }
    return 0;
  } else {
    return einval();
  }
}
