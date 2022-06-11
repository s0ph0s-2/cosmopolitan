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
#include "libc/bits/bits.h"
#include "libc/bits/safemacros.internal.h"
#include "libc/calls/calls.h"
#include "libc/calls/dprintf.h"
#include "libc/calls/strace.internal.h"
#include "libc/calls/struct/stat.h"
#include "libc/calls/struct/timeval.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/fmt/conv.h"
#include "libc/fmt/fmt.h"
#include "libc/intrin/spinlock.h"
#include "libc/log/internal.h"
#include "libc/log/log.h"
#include "libc/math.h"
#include "libc/nexgen32e/nexgen32e.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/fileno.h"
#include "libc/time/struct/tm.h"
#include "libc/time/time.h"

#define kNontrivialSize (8 * 1000 * 1000)

static struct timespec vflogf_ts;

/**
 * Takes corrective action if logging is on the fritz.
 */
static void vflogf_onfail(FILE *f) {
  errno_t err;
  int64_t size;
  if (IsTiny()) return;
  err = ferror_unlocked(f);
  if (fileno_unlocked(f) != -1 &&
      (err == ENOSPC || err == EDQUOT || err == EFBIG) &&
      ((size = getfiledescriptorsize(fileno_unlocked(f))) == -1 ||
       size > kNontrivialSize)) {
    ftruncate(fileno_unlocked(f), 0);
    fseeko_unlocked(f, SEEK_SET, 0);
    f->beg = f->end = 0;
    clearerr_unlocked(f);
    (fprintf_unlocked)(f, "performed emergency log truncation: %s\n",
                       strerror(err));
  }
}

/**
 * Writes formatted message w/ timestamp to log.
 *
 * Timestamps are hyphenated out when multiple events happen within the
 * same second in the same process. When timestamps are crossed out, it
 * will display microseconsd as a delta elapsed time. This is useful if
 * you do something like:
 *
 *     INFOF("connecting to foo");
 *     connect(...)
 *     INFOF("connected to foo");
 *
 * In that case, the second log entry will always display the amount of
 * time that it took to connect. This is great in forking applications.
 *
 * @asyncsignalsafe
 * @threadsafe
 */
void(vflogf)(unsigned level, const char *file, int line, FILE *f,
             const char *fmt, va_list va) {
  int bufmode;
  struct tm tm;
  long double t2;
  const char *prog;
  bool issamesecond;
  char buf32[32];
  int64_t secs, nsec, dots;
  if (!f) f = __log_file;
  if (!f) return;
  flockfile(f);
  --__strace;

  t2 = nowl();
  secs = t2;
  nsec = (t2 - secs) * 1e9L;
  issamesecond = secs == vflogf_ts.tv_sec;
  dots = issamesecond ? nsec - vflogf_ts.tv_nsec : nsec;
  vflogf_ts.tv_sec = secs;
  vflogf_ts.tv_nsec = nsec;

  localtime_r(&secs, &tm);
  strcpy(iso8601(buf32, &tm), issamesecond ? "+" : ".");
  prog = basename(firstnonnull(program_invocation_name, "unknown"));
  bufmode = f->bufmode;
  if (bufmode == _IOLBF) f->bufmode = _IOFBF;

  if ((fprintf_unlocked)(f, "%r%c%s%06ld:%s:%d:%.*s:%d] ",
                         "FEWIVDNT"[level & 7], buf32,
                         rem1000000int64(div1000int64(dots)), file, line,
                         strchrnul(prog, '.') - prog, prog, getpid()) <= 0) {
    vflogf_onfail(f);
  }
  (vfprintf_unlocked)(f, fmt, va);
  fputc_unlocked('\n', f);
  if (bufmode == _IOLBF) {
    f->bufmode = _IOLBF;
    fflush_unlocked(f);
  }

  if (level == kLogFatal) {
    __start_fatal(file, line);
    strcpy(buf32, "unknown");
    gethostname(buf32, sizeof(buf32));
    (dprintf)(STDERR_FILENO, "fatality %s pid %d\n", buf32, getpid());
    __die();
    unreachable;
  }

  ++__strace;
  funlockfile(f);
}
