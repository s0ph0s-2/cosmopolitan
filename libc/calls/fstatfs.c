/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
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
#include "libc/calls/cp.internal.h"
#include "libc/calls/internal.h"
#include "libc/calls/struct/statfs-meta.internal.h"
#include "libc/calls/struct/statfs.internal.h"
#include "libc/dce.h"
#include "libc/intrin/strace.internal.h"
#include "libc/runtime/stack.h"
#include "libc/sysv/errfuns.h"

/**
 * Returns information about filesystem.
 * @return 0 on success, or -1 w/ errno
 * @cancellationpoint
 */
int fstatfs(int fd, struct statfs *sf) {
  int rc;
  union statfs_meta m;
  BEGIN_CANCELLATION_POINT;
  CheckLargeStackAllocation(&m, sizeof(m));

  if (!IsWindows()) {
    if ((rc = sys_fstatfs(fd, &m)) != -1) {
      statfs2cosmo(sf, &m);
    }
  } else if (__isfdopen(fd)) {
    rc = sys_fstatfs_nt(g_fds.p[fd].handle, sf);
  } else {
    rc = ebadf();
  }

  END_CANCELLATION_POINT;
  STRACE("fstatfs(%d, [%s]) → %d% m", fd, DescribeStatfs(rc, sf));
  return rc;
}
