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
#include "libc/intrin/weaken.h"
#include "libc/calls/internal.h"
#include "libc/calls/sig.internal.h"
#include "libc/calls/strace.internal.h"
#include "libc/calls/syscall_support-nt.internal.h"
#include "libc/errno.h"
#include "libc/nt/errors.h"
#include "libc/nt/runtime.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/runtime.h"
#include "libc/sysv/consts/sicode.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/errfuns.h"

static textwindows ssize_t sys_write_nt_impl(int fd, void *data, size_t size,
                                             ssize_t offset) {
  uint32_t err, sent;
  struct NtOverlapped overlap;
  if (WriteFile(g_fds.p[fd].handle, data, _clampio(size), &sent,
                _offset2overlap(g_fds.p[fd].handle, offset, &overlap))) {
    return sent;
  }
  switch (GetLastError()) {
    // case kNtErrorInvalidHandle:
    //   return ebadf(); /* handled by consts.sh */
    // case kNtErrorNotEnoughQuota:
    //   return edquot(); /* handled by consts.sh */
    case kNtErrorBrokenPipe:  // broken pipe
    case kNtErrorNoData:      // closing named pipe
      if (weaken(__sig_raise)) {
        weaken(__sig_raise)(SIGPIPE, SI_KERNEL);
        return epipe();
      } else {
        STRACE("broken pipe");
        __restorewintty();
        _Exit(128 + EPIPE);
      }
    case kNtErrorAccessDenied:  // write doesn't return EACCESS
      return ebadf();           //
    default:
      return __winerr();
  }
}

textwindows ssize_t sys_write_nt(int fd, const struct iovec *iov, size_t iovlen,
                                 ssize_t opt_offset) {
  ssize_t rc;
  size_t i, total;
  uint32_t size, wrote;
  struct NtOverlapped overlap;
  while (iovlen && !iov[0].iov_len) iov++, iovlen--;
  if (iovlen) {
    for (total = i = 0; i < iovlen; ++i) {
      if (!iov[i].iov_len) continue;
      rc = sys_write_nt_impl(fd, iov[i].iov_base, iov[i].iov_len, opt_offset);
      if (rc == -1) return -1;
      total += rc;
      if (opt_offset != -1) opt_offset += rc;
      if (rc < iov[i].iov_len) break;
    }
    return total;
  } else {
    return sys_write_nt_impl(fd, NULL, 0, opt_offset);
  }
}
