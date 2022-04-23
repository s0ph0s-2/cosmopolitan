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
#include "libc/bits/safemacros.internal.h"
#include "libc/intrin/cmpxchg.h"
#include "libc/intrin/kprintf.h"
#include "libc/log/libfatal.internal.h"
#include "libc/macros.internal.h"
#include "libc/nexgen32e/rdtsc.h"
#include "libc/nexgen32e/rdtscp.h"
#include "libc/nexgen32e/stackframe.h"
#include "libc/nexgen32e/x86feature.h"
#include "libc/runtime/memtrack.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/runtime/symbols.internal.h"
#include "libc/stdio/stdio.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/o.h"
#include "libc/time/clockstonanos.internal.h"

#pragma weak stderr

#define MAX_NESTING 512

/**
 * @fileoverview Plain-text function call logging.
 *
 * Able to log ~2 million function calls per second, which is mostly
 * bottlenecked by system call overhead. Log size is reasonable if piped
 * into gzip.
 */

void ftrace_hook(void);

bool ftrace_enabled;
static int g_skew;
static int64_t g_lastaddr;
static uint64_t g_laststamp;

static privileged noinstrument noasan noubsan int GetNestingLevelImpl(
    struct StackFrame *frame) {
  int nesting = -2;
  while (frame) {
    ++nesting;
    frame = frame->next;
  }
  return MAX(0, nesting);
}

static privileged noinstrument noasan noubsan int GetNestingLevel(
    struct StackFrame *frame) {
  int nesting;
  nesting = GetNestingLevelImpl(frame);
  if (nesting < g_skew) g_skew = nesting;
  nesting -= g_skew;
  return MIN(MAX_NESTING, nesting);
}

/**
 * Prints name of function being called.
 *
 * We insert CALL instructions that point to this function, in the
 * prologues of other functions. We assume those functions behave
 * according to the System Five NexGen32e ABI.
 */
privileged noinstrument noasan noubsan void ftracer(void) {
  /* asan runtime depends on this function */
  uint64_t stamp;
  static bool noreentry;
  struct StackFrame *frame;
  if (!_cmpxchg(&noreentry, 0, 1)) return;
  if (ftrace_enabled) {
    stamp = rdtsc();
    frame = __builtin_frame_address(0);
    frame = frame->next;
    if (frame->addr != g_lastaddr) {
      kprintf("+ %*s%t %d\r\n", GetNestingLevel(frame) * 2, "", frame->addr,
              ClocksToNanos(stamp, g_laststamp));
      g_laststamp = X86_HAVE(RDTSCP) ? rdtscp(0) : rdtsc();
      g_lastaddr = frame->addr;
    }
  }
  noreentry = 0;
}

textstartup int ftrace_install(void) {
  if (GetSymbolTable()) {
    g_lastaddr = -1;
    g_laststamp = kStartTsc;
    g_skew = GetNestingLevelImpl(__builtin_frame_address(0));
    ftrace_enabled = 1;
    return __hook(ftrace_hook, GetSymbolTable());
  } else {
    kprintf("error: --ftrace failed to open symbol table\r\n");
    return -1;
  }
}
