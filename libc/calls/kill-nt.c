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
#include "libc/calls/getconsolectrlevent.h"
#include "libc/calls/internal.h"
#include "libc/dce.h"
#include "libc/macros.internal.h"
#include "libc/nt/console.h"
#include "libc/nt/enum/ctrlevent.h"
#include "libc/nt/enum/processaccess.h"
#include "libc/nt/errors.h"
#include "libc/nt/process.h"
#include "libc/nt/runtime.h"
#include "libc/sysv/errfuns.h"

textwindows int sys_kill_nt(int pid, int sig) {
  bool ok;
  int64_t handle;
  int event, ntpid;
  if (pid) {
    pid = ABS(pid);
    if ((event = GetConsoleCtrlEvent(sig)) != -1) {
      /* kill(pid, SIGINT|SIGHUP|SIGQUIT) */
      if (__isfdkind(pid, kFdProcess)) {
        ntpid = GetProcessId(g_fds.p[pid].handle);
      } else if (!__isfdopen(pid)) {
        /* XXX: this is sloppy (see fork-nt.c) */
        ntpid = pid;
      } else {
        return esrch();
      }
      ok = !!GenerateConsoleCtrlEvent(event, ntpid);
    } else if (__isfdkind(pid, kFdProcess)) {
      ok = !!TerminateProcess(g_fds.p[pid].handle, 128 + sig);
      if (!ok && GetLastError() == kNtErrorAccessDenied) ok = true;
    } else if ((handle = OpenProcess(kNtProcessTerminate, false, pid))) {
      ok = !!TerminateProcess(handle, 128 + sig);
      if (!ok && GetLastError() == kNtErrorAccessDenied) ok = true;
      CloseHandle(handle);
    } else {
      ok = false;
    }
    return ok ? 0 : __winerr();
  } else {
    return raise(sig);
  }
}
