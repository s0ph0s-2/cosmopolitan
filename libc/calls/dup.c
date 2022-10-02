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
#include "libc/calls/calls.h"
#include "libc/calls/internal.h"
#include "libc/calls/syscall-nt.internal.h"
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/dce.h"
#include "libc/intrin/strace.internal.h"
#include "libc/sysv/errfuns.h"

/**
 * Duplicates file descriptor.
 *
 * The `O_CLOEXEC` flag shall be cleared from the resulting file
 * descriptor; see dup3() to preserve it.
 *
 * @param fd remains open afterwards
 * @return some arbitrary new number for fd
 * @raise EPERM if pledge() is in play without stdio
 * @raise ENOTSUP if `fd` is a zip file descriptor
 * @raise EBADF if `fd` is negative or not open
 * @asyncsignalsafe
 * @vforksafe
 */
int dup(int fd) {
  int rc;
  if (__isfdkind(fd, kFdZip)) {
    rc = enotsup();
  } else if (!IsWindows()) {
    rc = sys_dup(fd);
  } else {
    rc = sys_dup_nt(fd, -1, 0, -1);
  }
  STRACE("%s(%d) → %d% m", "dup", fd, rc);
  return rc;
}
