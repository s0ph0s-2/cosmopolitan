/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=8 sts=2 sw=2 fenc=utf-8                                :vi│
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
#include "libc/assert.h"
#include "libc/calls/internal.h"
#include "libc/calls/strace.internal.h"
#include "libc/calls/struct/timeval.h"
#include "libc/dce.h"
#include "libc/fmt/conv.h"
#include "libc/intrin/asan.internal.h"
#include "libc/intrin/describeflags.internal.h"
#include "libc/nt/synchronization.h"
#include "libc/sysv/errfuns.h"

static typeof(sys_clock_gettime) *__clock_gettime = sys_clock_gettime;

/**
 * Returns nanosecond time.
 *
 * This is a high-precision timer that supports multiple definitions of
 * time. Among the more popular is CLOCK_MONOTONIC. This function has a
 * zero syscall implementation of that on modern x86.
 *
 * @param clockid can be CLOCK_REALTIME, CLOCK_MONOTONIC, etc.
 * @param ts is where the result is stored
 * @return 0 on success, or -1 w/ errno
 * @error EINVAL if clockid isn't supported on this system
 * @see strftime(), gettimeofday()
 * @asyncsignalsafe
 */
noinstrument int clock_gettime(int clockid, struct timespec *ts) {
  int rc, e;
  axdx_t ad;
  char buf[45];
  if (!ts) {
    rc = efault();
  } else if (IsAsan() && !__asan_is_valid(ts, sizeof(*ts))) {
    rc = efault();
  } else if (clockid == -1) {
    rc = einval();
  } else if (!IsWindows()) {
    e = errno;
    if ((rc = __clock_gettime(clockid, ts))) {
      errno = e;
      ad = sys_gettimeofday((struct timeval *)ts, NULL, NULL);
      assert(ad.ax != -1);
      if (SupportsXnu() && ad.ax) {
        ts->tv_sec = ad.ax;
        ts->tv_nsec = ad.dx;
      }
      ts->tv_nsec *= 1000;
      rc = 0;
    }
  } else {
    rc = sys_clock_gettime_nt(clockid, ts);
  }
  if (!__time_critical) {
    STRACE("clock_gettime(%d, [%s]) → %d% m", clockid,
           DescribeTimespec(buf, sizeof(buf), rc, ts), rc);
  }
  return rc;
}

/**
 * Returns fast system clock_gettime() if it exists.
 */
void *__get_clock_gettime(void) {
  void *vdso;
  static bool once;
  static void *result;
  if (!once) {
    if ((vdso = __vdsofunc("__vdso_clock_gettime"))) {
      __clock_gettime = result = vdso;
    }
    once = true;
  }
  return result;
}

const void *const __clock_gettime_ctor[] initarray = {
    __get_clock_gettime,
};
