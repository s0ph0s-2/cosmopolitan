/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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
#include "ape/sections.internal.h"
#include "libc/assert.h"
#include "libc/calls/calls.h"
#include "libc/calls/struct/aarch64.internal.h"
#include "libc/calls/struct/rusage.internal.h"
#include "libc/calls/struct/siginfo.h"
#include "libc/calls/struct/sigset.h"
#include "libc/calls/struct/sigset.internal.h"
#include "libc/calls/struct/utsname.h"
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/calls/ucontext.h"
#include "libc/errno.h"
#include "libc/intrin/kprintf.h"
#include "libc/log/internal.h"
#include "libc/log/log.h"
#include "libc/macros.internal.h"
#include "libc/mem/mem.h"
#include "libc/nexgen32e/stackframe.h"
#include "libc/runtime/runtime.h"
#include "libc/runtime/stack.h"
#include "libc/runtime/symbols.internal.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/sig.h"
#include "libc/thread/thread.h"
#ifdef __aarch64__

STATIC_YOINK("strerror_wr");  // for kprintf %m
STATIC_YOINK("strsignal_r");  // for kprintf %G

#define RESET   "\e[0m"
#define BOLD    "\e[1m"
#define STRONG  "\e[30;101m"
#define RED     "\e[31;1m"
#define GREEN   "\e[32;1m"
#define BLUE    "\e[34;1m"
#define YELLOW  "\e[33;1m"
#define MAGENTA "\e[35;1m"

struct Buffer {
  char *p;
  int n;
  int i;
};

static bool IsCode(uintptr_t p) {
  return __executable_start <= (uint8_t *)p && (uint8_t *)p < _etext;
}

static void Append(struct Buffer *b, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  b->i += kvsnprintf(b->p + b->i, b->n - b->i, fmt, va);
  va_end(va);
}

static const char *ColorRegister(int r) {
  if (__nocolor) return "";
  switch (r) {
    case 0:  // arg / res
    case 1:  // arg / res
    case 2:  // arg / res
    case 3:  // arg / res
    case 4:  // arg / res
    case 5:  // arg / res
    case 6:  // arg / res
    case 7:  // arg / res
      return GREEN;
    case 9:   // volatile
    case 10:  // volatile
    case 11:  // volatile
    case 12:  // volatile
    case 13:  // volatile
    case 14:  // volatile
    case 15:  // volatile
      return BLUE;
    case 19:  // saved
    case 20:  // saved
    case 21:  // saved
    case 22:  // saved
    case 23:  // saved
    case 24:  // saved
    case 25:  // saved
    case 26:  // saved
    case 27:  // saved
      return MAGENTA;
    case 18:  // platform register
    case 28:  // our tls register
    case 29:  // frame pointer
    case 30:  // return pointer
    case 31:  // stack pointer
      return RED;
    default:  // miscellaneous registers
      return YELLOW;
  }
}

static bool AppendFileLine(struct Buffer *b, const char *addr2line,
                           const char *debugbin, long addr) {
  ssize_t rc;
  char *p, *q, buf[128];
  int j, k, ws, pid, pfd[2];
  if (!debugbin || !*debugbin) return false;
  if (!addr2line || !*addr2line) return false;
  if (sys_pipe(pfd)) return false;
  ksnprintf(buf, sizeof(buf), "%lx", addr);
  if ((pid = vfork()) == -1) {
    sys_close(pfd[1]);
    sys_close(pfd[0]);
    return false;
  }
  if (!pid) {
    sys_close(pfd[0]);
    sys_dup2(pfd[1], 1, 0);
    sys_close(2);
    __sys_execve(addr2line,
                 (char *const[]){addr2line, "-pifCe", debugbin, buf, 0},
                 (char *const[]){0});
    _Exit(127);
  }
  sys_close(pfd[1]);
  // copy addr2line stdout to buffer. normally it is "file:line\n".
  // however additional lines can get created for inline functions.
  j = b->i;
  Append(b, "in ");
  k = b->i;
  while ((rc = sys_read(pfd[0], buf, sizeof(buf))) > 0) {
    Append(b, "%.*s", (int)rc, buf);
  }
  // remove the annoying `foo.c:123 (discriminator 3)` suffixes, because
  // they break emacs, and who on earth knows what those things mean lol
  while ((p = memmem(b->p + k, b->i - k, " (discriminator ", 16)) &&
         (q = memchr(p + 16, '\n', (b->p + b->i) - (p + 16)))) {
    memmove(p, q, (b->p + b->i) - q);
    b->i -= q - p;
  }
  // mop up after the process and sanity check captured text
  sys_close(pfd[0]);
  if (sys_wait4(pid, &ws, 0, 0) != -1 && !ws && b->p[k] != ':' &&
      b->p[k] != '?' && b->p[b->i - 1] == '\n') {
    --b->i;  // chomp last newline
    return true;
  } else {
    b->i = j;  // otherwise reset the buffer
    return false;
  }
}

static char *GetSymbolName(struct SymbolTable *st, int symbol, char **mem,
                           size_t *memsz) {
  char *s, *t;
  if ((s = __get_symbol_name(st, symbol)) &&  //
      s[0] == '_' && s[1] == 'Z' &&           //
      (t = __cxa_demangle(s, *mem, memsz, 0))) {
    *mem = s = t;
  }
  return s;
}

relegated void __oncrash_arm64(int sig, struct siginfo *si, void *arg) {
  char buf[10000];
  ucontext_t *ctx = arg;
  static _Thread_local bool once;
  struct Buffer b[1] = {{buf, sizeof(buf)}};
  b->p[b->i++] = '\n';
  if (!once) {
    int i, j;
    const char *kind;
    const char *reset;
    const char *strong;
    char host[64] = "unknown";
    struct utsname names = {0};
    once = true;
    ftrace_enabled(-1);
    strace_enabled(-1);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
    __restore_tty();
    uname(&names);
    gethostname(host, sizeof(host));
    reset = !__nocolor ? RESET : "";
    strong = !__nocolor ? STRONG : "";
    if (ctx && (ctx->uc_mcontext.sp & (GetStackSize() - 1)) <= APE_GUARDSIZE) {
      kind = "Stack Overflow";
    } else {
      kind = GetSiCodeName(sig, si->si_code);
    }
    Append(b,
           "%serror%s: Uncaught %G (%s) on %s pid %d tid %d\n"
           " %s\n"
           " %m\n"
           " %s %s %s %s\n",
           strong, reset, sig, kind, host, getpid(), gettid(),
           program_invocation_name, names.sysname, names.version,
           names.nodename, names.release);
    if (ctx) {
      long pc;
      char line[256];
      char *mem = 0;
      size_t memsz = 0;
      int addend, symbol;
      const char *debugbin;
      const char *addr2line;
      struct StackFrame *fp;
      struct SymbolTable *st;
      struct fpsimd_context *vc;
      st = GetSymbolTable();
      debugbin = FindDebugBinary();
      addr2line = GetAddr2linePath();

      if (ctx->uc_mcontext.fault_address) {
        Append(b, " fault_address = %#lx\n", ctx->uc_mcontext.fault_address);
      }

      // PRINT REGISTERS
      for (i = 0; i < 8; ++i) {
        Append(b, " ");
        for (j = 0; j < 4; ++j) {
          int r = 8 * j + i;
          if (j) Append(b, " ");
          Append(b, "%s%016lx%s x%d%s", ColorRegister(r),
                 ctx->uc_mcontext.regs[r], reset, r,
                 r == 8 || r == 9 ? " " : "");
        }
        Append(b, "\n");
      }

      // PRINT VECTORS
      vc = (struct fpsimd_context *)ctx->uc_mcontext.__reserved;
      if (vc->head.magic == FPSIMD_MAGIC) {
        int n = 16;
        while (n && !vc->vregs[n - 1] && !vc->vregs[n - 2]) n -= 2;
        for (i = 0; i * 2 < n; ++i) {
          Append(b, " ");
          for (j = 0; j < 2; ++j) {
            int r = j + 2 * i;
            if (j) Append(b, " ");
            Append(b, "%016lx ..%s %016lx v%d%s", (long)(vc->vregs[r] >> 64),
                   !j ? "" : ".", (long)vc->vregs[r], r, r < 10 ? " " : "");
          }
          Append(b, "\n");
        }
      }

      // PRINT CURRENT LOCATION
      //
      // We can get the address of the currently executing function by
      // simply examining the program counter.
      pc = ctx->uc_mcontext.pc;
      Append(b, " %016lx sp %lx pc", ctx->uc_mcontext.sp, pc);
      if (pc && st && (symbol = __get_symbol(st, pc))) {
        addend = pc - st->addr_base;
        addend -= st->symbols[symbol].x;
        Append(b, " %s", GetSymbolName(st, symbol, &mem, &memsz));
        if (addend) Append(b, "%+d", addend);
      }
      Append(b, "\n");

      // PRINT LINKED LOCATION
      //
      // The x30 register can usually tell us the address of the parent
      // function. This can help us determine the caller in cases where
      // stack frames aren't being generated by the compiler; but if we
      // have stack frames, then we need to ensure this won't duplicate
      // the first element of the frame pointer backtrace below.
      fp = (struct StackFrame *)ctx->uc_mcontext.regs[29];
      if (IsCode((pc = ctx->uc_mcontext.regs[30]))) {
        Append(b, " %016lx sp %lx lr", ctx->uc_mcontext.sp, pc);
        if (pc && st && (symbol = __get_symbol(st, pc))) {
          addend = pc - st->addr_base;
          addend -= st->symbols[symbol].x;
          Append(b, " ");
          if (!AppendFileLine(b, addr2line, debugbin, pc)) {
            Append(b, "%s", GetSymbolName(st, symbol, &mem, &memsz));
            if (addend) Append(b, "%+d", addend);
          }
        }
        Append(b, "\n");
        if (fp && !kisdangerous(fp) && pc == fp->addr) {
          fp = fp->next;
        }
      }

      // PRINT FRAME POINTERS
      //
      // The prologues and epilogues of non-leaf functions should save
      // the frame pointer (x29) and return address (x30) to the stack
      // and then set x29 to sp, which is the address of the new frame
      // effectively creating a daisy chain letting us trace back into
      // the origin of execution, e.g. _start(), or sys_clone_linux().
      for (i = 0; fp; fp = fp->next) {
        if (kisdangerous(fp)) {
          Append(b, " %016lx <dangerous fp>\n", fp);
          break;
        }
        if (++i == 100) {
          Append(b, " <truncated backtrace>\n");
          break;
        }
        if (st && (pc = fp->addr)) {
          if ((symbol = __get_symbol(st, pc))) {
            addend = pc - st->addr_base;
            addend -= st->symbols[symbol].x;
          } else {
            addend = 0;
          }
        } else {
          symbol = 0;
          addend = 0;
        }
        Append(b, " %016lx fp %lx lr ", fp, pc);
        if (!AppendFileLine(b, addr2line, debugbin, pc) && st) {
          Append(b, "%s", GetSymbolName(st, symbol, &mem, &memsz));
          if (addend) Append(b, "%+d", addend);
        }
        Append(b, "\n");
      }
      free(mem);
    }
  } else {
    Append(b, "got %G while crashing! pc %lx lr %lx\n", sig,
           ctx->uc_mcontext.pc, ctx->uc_mcontext.regs[30]);
  }
  sys_write(2, b->p, MIN(b->i, b->n));
  __print_maps();
  _Exit(128 + sig);
}

#endif /* __aarch64__ */
