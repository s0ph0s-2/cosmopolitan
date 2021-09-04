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
#include "libc/bits/weaken.h"
#include "libc/calls/calls.h"
#include "libc/calls/internal.h"
#include "libc/calls/struct/metastat.internal.h"
#include "libc/calls/struct/stat.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/asan.internal.h"
#include "libc/nt/files.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/at.h"
#include "libc/sysv/errfuns.h"
#include "libc/zipos/zipos.internal.h"

/**
 * Returns true if file exists at path.
 *
 * This function is equivalent to:
 *
 *     struct stat st;
 *     return stat(path, &st) != -1;
 *
 * Please note that things which aren't strictly files, e.g. directories
 * or sockets, could be considered files for the purposes of this
 * function. The stat() function may be used to differentiate them.
 */
bool fileexists(const char *path) {
  int e;
  union metastat st;
  struct ZiposUri zipname;
  uint16_t path16[PATH_MAX];
  if (IsAsan() && !__asan_is_valid(path, 1)) return efault();
  if (weaken(__zipos_open) && weaken(__zipos_parseuri)(path, &zipname) != -1) {
    e = errno;
    if (weaken(__zipos_stat)(&zipname, &st.cosmo) != -1) {
      return true;
    } else {
      errno = e;
      return false;
    }
  } else if (IsMetal()) {
    return false;
  } else if (!IsWindows()) {
    e = errno;
    if (__sys_fstatat(AT_FDCWD, path, &st, 0) != -1) {
      return true;
    } else {
      errno = e;
      return false;
    }
  } else {
    if (__mkntpath(path, path16) == -1) return -1;
    return GetFileAttributes(path16) != -1u;
  }
}
