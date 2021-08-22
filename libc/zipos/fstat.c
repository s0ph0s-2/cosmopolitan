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
#include "libc/sysv/errfuns.h"
#include "libc/zip.h"
#include "libc/zipos/zipos.internal.h"

/**
 * Reads file metadata from αcτµαlly pδrταblε εxεcµταblε object store.
 *
 * @param uri is obtained via __zipos_parseuri()
 * @asyncsignalsafe
 */
int __zipos_fstat(const struct ZiposHandle *h, struct stat *st) {
  int rc;
  if (!st) return efault();
  if (!(rc = __zipos_stat_impl(__zipos_get(), h->cfile, st))) {
    ZTRACE("__zipos_fstat(%`'.*s) → %ld",
           ZIP_CFILE_NAMESIZE(__zipos_get()->map + h->cfile),
           ZIP_CFILE_NAME(__zipos_get()->map + h->cfile), st->st_size);
    return 0;
  } else {
    return rc;
  }
}
