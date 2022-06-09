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
#include "ape/relocations.h"
#include "libc/assert.h"
#include "libc/bits/weaken.h"
#include "libc/calls/calls.h"
#include "libc/calls/internal.h"
#include "libc/calls/state.internal.h"
#include "libc/calls/struct/sigset.h"
#include "libc/calls/struct/stat.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/spinlock.h"
#include "libc/macros.internal.h"
#include "libc/mem/mem.h"
#include "libc/nexgen32e/crc32.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/errfuns.h"
#include "libc/zip.h"
#include "libc/zipos/zipos.internal.h"
#include "third_party/zlib/zlib.h"

static int __zipos_inflate(struct ZiposHandle *h, uint8_t *data, size_t size) {
  return !__inflate(h->freeme, h->size, data, size) ? 0 : eio();
}

static int __zipos_load(struct Zipos *zipos, size_t cf, unsigned flags,
                        int mode) {
  size_t lf;
  int rc, fd;
  size_t size;
  uint8_t *data;
  struct ZiposHandle *h;
  lf = GetZipCfileOffset(zipos->map + cf);
  if (!IsTiny() &&
      (ZIP_LFILE_MAGIC(zipos->map + lf) != kZipLfileHdrMagic ||
       (ZIP_LFILE_COMPRESSIONMETHOD(zipos->map + lf) != kZipCompressionNone &&
        ZIP_LFILE_COMPRESSIONMETHOD(zipos->map + lf) !=
            kZipCompressionDeflate))) {
    return eio();
  }
  if (!(h = calloc(1, sizeof(*h)))) return -1;
  h->cfile = cf;
  h->size = GetZipLfileUncompressedSize(zipos->map + lf);
  if (ZIP_LFILE_COMPRESSIONMETHOD(zipos->map + lf)) {
    if ((h->freeme = malloc(h->size))) {
      data = ZIP_LFILE_CONTENT(zipos->map + lf);
      size = GetZipLfileCompressedSize(zipos->map + lf);
      if ((rc = __zipos_inflate(h, data, size)) != -1) {
        h->mem = h->freeme;
      }
    }
  } else {
    h->mem = ZIP_LFILE_CONTENT(zipos->map + lf);
  }
  if (!IsTiny() && h->mem &&
      crc32_z(0, h->mem, h->size) != ZIP_LFILE_CRC32(zipos->map + lf)) {
    errno = EIO;
    h->mem = NULL;
  }
  if (h->mem) {
    if ((fd = IsWindows() ? __reservefd(-1) : dup(2)) != -1) {
      if (__ensurefds(fd) != -1) {
        __fds_lock();
        h->handle = g_fds.p[fd].handle;
        g_fds.p[fd].kind = kFdZip;
        g_fds.p[fd].handle = (intptr_t)h;
        g_fds.p[fd].flags = flags | O_CLOEXEC;
        g_fds.p[fd].mode = mode;
        g_fds.p[fd].extra = 0;
        __fds_unlock();
        return fd;
      }
      close(fd);
    }
  }
  free(h->freeme);
  free(h);
  return -1;
}

/**
 * Loads compressed file from αcτµαlly pδrταblε εxεcµταblε object store.
 *
 * @param uri is obtained via __zipos_parseuri()
 * @note don't call open() from signal handlers
 */
int __zipos_open(const struct ZiposUri *name, unsigned flags, int mode) {
  int rc;
  ssize_t cf;
  sigset_t oldmask;
  struct Zipos *zipos;
  if ((flags & O_ACCMODE) == O_RDONLY) {
    if ((zipos = __zipos_get())) {
      if ((cf = __zipos_find(zipos, name)) != -1) {
        rc = __zipos_load(zipos, cf, flags, mode);
      } else {
        rc = enoent();
      }
    } else {
      rc = enoexec();
    }
  } else {
    rc = einval();
  }
  return rc;
}
