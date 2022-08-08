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
#include "libc/bits/likely.h"
#include "libc/bits/weaken.h"
#include "libc/calls/calls.h"
#include "libc/calls/pledge.h"
#include "libc/calls/pledge.internal.h"
#include "libc/calls/strace.internal.h"
#include "libc/calls/syscall-nt.internal.h"
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/dce.h"
#include "libc/intrin/asan.internal.h"
#include "libc/intrin/kprintf.h"
#include "libc/intrin/promises.internal.h"
#include "libc/log/libfatal.internal.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/errfuns.h"

/**
 * Replaces current process with program.
 *
 * @param program will not be PATH searched, see commandv()
 * @param argv[0] is the name of the program to run
 * @param argv[1,n-2] optionally specify program arguments
 * @param argv[n-1] is NULL
 * @param envp[0,n-2] specifies "foo=bar" environment variables
 * @param envp[n-1] is NULL
 * @return doesn't return, or -1 w/ errno
 * @asyncsignalsafe
 * @vforksafe
 */
int execve(const char *prog, char *const argv[], char *const envp[]) {
  int rc;
  size_t i;
  if (!prog || !argv || !envp ||
      (IsAsan() &&
       (!__asan_is_valid(prog, 1) || !__asan_is_valid_strlist(argv) ||
        !__asan_is_valid_strlist(envp)))) {
    rc = efault();
  } else {
#ifdef SYSDEBUG
    if (UNLIKELY(__strace > 0)) {
      kprintf(STRACE_PROLOGUE "execve(%#s, {", prog);
      for (i = 0; argv[i]; ++i) {
        if (i) kprintf(", ");
        kprintf("%#s", argv[i]);
      }
      kprintf("}, {");
      for (i = 0; envp[i]; ++i) {
        if (i) kprintf(", ");
        kprintf("%#s", envp[i]);
      }
      kprintf("})\n");
    }
#endif
    if (!IsWindows()) {
      rc = 0;
      if (IsLinux() && __execpromises && weaken(sys_pledge_linux)) {
        rc = weaken(sys_pledge_linux)(__execpromises, __pledge_mode, false);
      }
      if (!rc) {
        rc = sys_execve(prog, argv, envp);
      }
    } else {
      rc = sys_execve_nt(prog, argv, envp);
    }
  }
  STRACE("execve(%#s) failed %d% m", prog, rc);
  return rc;
}
