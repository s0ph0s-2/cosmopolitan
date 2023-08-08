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
#include "libc/calls/state.internal.h"
#include "libc/calls/syscall_support-nt.internal.h"
#include "libc/dce.h"
#include "libc/intrin/describeflags.internal.h"
#include "libc/intrin/kprintf.h"
#include "libc/intrin/strace.internal.h"
#include "libc/intrin/weaken.h"
#include "libc/log/libfatal.internal.h"
#include "libc/macros.internal.h"
#include "libc/nexgen32e/rdtsc.h"
#include "libc/nt/console.h"
#include "libc/nt/createfile.h"
#include "libc/nt/enum/accessmask.h"
#include "libc/nt/enum/consolemodeflags.h"
#include "libc/nt/enum/creationdisposition.h"
#include "libc/nt/enum/fileflagandattributes.h"
#include "libc/nt/enum/filemapflags.h"
#include "libc/nt/enum/filesharemode.h"
#include "libc/nt/enum/pageflags.h"
#include "libc/nt/files.h"
#include "libc/nt/memory.h"
#include "libc/nt/pedef.internal.h"
#include "libc/nt/process.h"
#include "libc/nt/runtime.h"
#include "libc/nt/signals.h"
#include "libc/nt/struct/ntexceptionpointers.h"
#include "libc/nt/struct/teb.h"
#include "libc/nt/thunk/msabi.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/memtrack.internal.h"
#include "libc/runtime/stack.h"
#include "libc/runtime/winargs.internal.h"
#include "libc/sock/internal.h"
#include "libc/sysv/consts/prot.h"

#ifdef __x86_64__

// clang-format off
__msabi extern typeof(AddVectoredExceptionHandler) *const __imp_AddVectoredExceptionHandler;
__msabi extern typeof(CloseHandle) *const __imp_CloseHandle;
__msabi extern typeof(CreateFile) *const __imp_CreateFileW;
__msabi extern typeof(CreateFileMapping) *const __imp_CreateFileMappingW;
__msabi extern typeof(DuplicateHandle) *const __imp_DuplicateHandle;
__msabi extern typeof(ExitProcess) *const __imp_ExitProcess;
__msabi extern typeof(FreeEnvironmentStrings) *const __imp_FreeEnvironmentStringsW;
__msabi extern typeof(GetConsoleMode) *const __imp_GetConsoleMode;
__msabi extern typeof(GetCurrentProcess) *const __imp_GetCurrentProcess;
__msabi extern typeof(GetCurrentProcessId) *const __imp_GetCurrentProcessId;
__msabi extern typeof(GetEnvironmentStrings) *const __imp_GetEnvironmentStringsW;
__msabi extern typeof(GetStdHandle) *const __imp_GetStdHandle;
__msabi extern typeof(MapViewOfFileEx) *const __imp_MapViewOfFileEx;
__msabi extern typeof(SetConsoleCP) *const __imp_SetConsoleCP;
__msabi extern typeof(SetConsoleMode) *const __imp_SetConsoleMode;
__msabi extern typeof(SetConsoleOutputCP) *const __imp_SetConsoleOutputCP;
__msabi extern typeof(SetStdHandle) *const __imp_SetStdHandle;
__msabi extern typeof(VirtualProtect) *const __imp_VirtualProtect;
__msabi extern typeof(WriteFile) *const __imp_WriteFile;
// clang-format on

/*
 * TODO: Why can't we allocate addresses above 4GB on Windows 7 x64?
 * TODO: How can we ensure we never overlap with KERNEL32.DLL?
 */

extern int64_t __wincrashearly;
extern const signed char kNtConsoleHandles[3];
extern void cosmo(int, char **, char **, long (*)[2]) wontreturn;

static const short kConsoleModes[3] = {
    kNtEnableProcessedInput | kNtEnableLineInput | kNtEnableEchoInput |
        kNtEnableMouseInput | kNtEnableQuickEditMode | kNtEnableExtendedFlags |
        kNtEnableAutoPosition | kNtEnableInsertMode |
        kNtEnableVirtualTerminalInput,
    kNtEnableProcessedOutput | kNtEnableWrapAtEolOutput |
        kNtEnableVirtualTerminalProcessing,
    kNtEnableProcessedOutput | kNtEnableWrapAtEolOutput |
        kNtEnableVirtualTerminalProcessing,
};

// https://nullprogram.com/blog/2022/02/18/
__msabi static inline char16_t *MyCommandLine(void) {
  void *cmd;
  asm("mov\t%%gs:(0x60),%0\n"
      "mov\t0x20(%0),%0\n"
      "mov\t0x78(%0),%0\n"
      : "=r"(cmd));
  return cmd;
}

__msabi static inline size_t StrLen16(const char16_t *s) {
  size_t n;
  for (n = 0;; ++n) {
    if (!s[n]) {
      return n;
    }
  }
}

__msabi static textwindows int OnEarlyWinCrash(struct NtExceptionPointers *ep) {
  uint32_t wrote;
  char buf[64], *p = buf;
  *p++ = 'c';
  *p++ = 'r';
  *p++ = 'a';
  *p++ = 's';
  *p++ = 'h';
  *p++ = ' ';
  *p++ = '0';
  *p++ = 'x';
  p = __fixcpy(p, ep->ExceptionRecord->ExceptionCode, 32);
  *p++ = ' ';
  *p++ = 'r';
  *p++ = 'i';
  *p++ = 'p';
  *p++ = ' ';
  p = __fixcpy(p, ep->ContextRecord ? ep->ContextRecord->Rip : -1, 32);
  *p++ = '\r';
  *p++ = '\n';
  __imp_WriteFile(__imp_GetStdHandle(kNtStdErrorHandle), buf, p - buf, &wrote,
                  0);
  __imp_ExitProcess(11);
  __builtin_unreachable();
}

// this makes it possible for our read() implementation to periodically
// poll for signals while performing a blocking overlapped io operation
__msabi static textwindows void ReopenConsoleForOverlappedIo(void) {
  uint32_t mode;
  int64_t hOld, hNew;
  hOld = __imp_GetStdHandle(kNtStdInputHandle);
  if (__imp_GetConsoleMode(hOld, &mode)) {
    hNew = __imp_CreateFileW(u"CONIN$", kNtGenericRead | kNtGenericWrite,
                             kNtFileShareRead | kNtFileShareWrite,
                             &kNtIsInheritable, kNtOpenExisting,
                             kNtFileFlagOverlapped, 0);
    if (hNew != kNtInvalidHandleValue) {
      __imp_SetStdHandle(kNtStdInputHandle, hNew);
      __imp_CloseHandle(hOld);
    }
  }
}

// this ensures close(1) won't accidentally close(2) for example
__msabi static textwindows void DeduplicateStdioHandles(void) {
  long i, j;
  int64_t h1, h2, h3, proc;
  for (i = 0; i < 3; ++i) {
    h1 = __imp_GetStdHandle(kNtConsoleHandles[i]);
    for (j = i + 1; j < 3; ++j) {
      h3 = h2 = __imp_GetStdHandle(kNtConsoleHandles[j]);
      if (h1 == h2) {
        proc = __imp_GetCurrentProcess();
        __imp_DuplicateHandle(proc, h2, proc, &h3, 0, true,
                              kNtDuplicateSameAccess);
        __imp_SetStdHandle(kNtConsoleHandles[j], h3);
      }
    }
  }
}

__msabi static textwindows wontreturn void WinMainNew(const char16_t *cmdline) {
  bool32 rc;
  int64_t h, hand;
  uint32_t oldprot;
  struct WinArgs *wa;
  char inflagsbuf[256];
  char outflagsbuf[128];
  const char16_t *env16;
  int i, prot, count, version;
  size_t allocsize, stacksize;
  intptr_t stackaddr, allocaddr;
  version = NtGetPeb()->OSMajorVersion;
  __oldstack = (intptr_t)__builtin_frame_address(0);
  ReopenConsoleForOverlappedIo();
  DeduplicateStdioHandles();
  if ((intptr_t)v_ntsubsystem == kNtImageSubsystemWindowsCui && version >= 10) {
    rc = __imp_SetConsoleCP(kNtCpUtf8);
    NTTRACE("SetConsoleCP(kNtCpUtf8) → %hhhd", rc);
    rc = __imp_SetConsoleOutputCP(kNtCpUtf8);
    NTTRACE("SetConsoleOutputCP(kNtCpUtf8) → %hhhd", rc);
    for (i = 0; i < 3; ++i) {
      hand = __imp_GetStdHandle(kNtConsoleHandles[i]);
      rc = __imp_GetConsoleMode(hand, __ntconsolemode + i);
      NTTRACE("GetConsoleMode(%p, [%s]) → %hhhd", hand,
              i ? (DescribeNtConsoleOutFlags)(outflagsbuf, __ntconsolemode[i])
                : (DescribeNtConsoleInFlags)(inflagsbuf, __ntconsolemode[i]),
              rc);
      rc = __imp_SetConsoleMode(hand, kConsoleModes[i]);
      NTTRACE("SetConsoleMode(%p, %s) → %hhhd", hand,
              i ? (DescribeNtConsoleOutFlags)(outflagsbuf, kConsoleModes[i])
                : (DescribeNtConsoleInFlags)(inflagsbuf, kConsoleModes[i]),
              rc);
    }
    (void)rc;
  }
  _Static_assert(sizeof(struct WinArgs) % FRAMESIZE == 0, "");
  _mmi.p = _mmi.s;
  _mmi.n = ARRAYLEN(_mmi.s);
  stackaddr = GetStaticStackAddr(0);
  stacksize = GetStackSize();
  allocaddr = stackaddr;
  allocsize = stacksize + sizeof(struct WinArgs);
  NTTRACE("WinMainNew() mapping %'zu byte stack at %p", allocsize, allocaddr);
  __imp_MapViewOfFileEx((_mmi.p[0].h = __imp_CreateFileMappingW(
                             -1, &kNtIsInheritable, kNtPageExecuteReadwrite,
                             allocsize >> 32, allocsize, NULL)),
                        kNtFileMapWrite | kNtFileMapExecute, 0, 0, allocsize,
                        (void *)allocaddr);
  prot = (intptr_t)ape_stack_prot;
  if (~prot & PROT_EXEC) {
    __imp_VirtualProtect((void *)allocaddr, allocsize, kNtPageReadwrite,
                         &oldprot);
  }
  _mmi.p[0].x = allocaddr >> 16;
  _mmi.p[0].y = (allocaddr >> 16) + ((allocsize - 1) >> 16);
  _mmi.p[0].prot = prot;
  _mmi.p[0].flags = 0x00000026;  // stack+anonymous
  _mmi.p[0].size = allocsize;
  _mmi.i = 1;
  wa = (struct WinArgs *)(allocaddr + stacksize);
  NTTRACE("WinMainNew() loading arg block");
  count = GetDosArgv(cmdline, wa->argblock, ARRAYLEN(wa->argblock), wa->argv,
                     ARRAYLEN(wa->argv));
  for (i = 0; wa->argv[0][i]; ++i) {
    if (wa->argv[0][i] == '\\') {
      wa->argv[0][i] = '/';
    }
  }
  env16 = __imp_GetEnvironmentStringsW();
  NTTRACE("WinMainNew() loading environment");
  GetDosEnviron(env16, wa->envblock, ARRAYLEN(wa->envblock) - 8, wa->envp,
                ARRAYLEN(wa->envp) - 1);
  __imp_FreeEnvironmentStringsW(env16);
  NTTRACE("WinMainNew() switching stacks");
  _jmpstack((char *)(stackaddr + stacksize - (intptr_t)ape_stack_align), cosmo,
            count, wa->argv, wa->envp, wa->auxv);
}

__msabi textwindows int64_t WinMain(int64_t hInstance, int64_t hPrevInstance,
                                    const char *lpCmdLine, int64_t nCmdShow) {
  const char16_t *cmdline;
  extern char os asm("__hostos");
  os = _HOSTWINDOWS; /* madness https://news.ycombinator.com/item?id=21019722 */
  kStartTsc = rdtsc();
  __pid = __imp_GetCurrentProcessId();
#if !IsTiny()
  __wincrashearly =
      __imp_AddVectoredExceptionHandler(1, (void *)OnEarlyWinCrash);
#endif
  cmdline = MyCommandLine();
#ifdef SYSDEBUG
  /* sloppy flag-only check for early initialization */
  if (__strstr16(cmdline, u"--strace")) ++__strace;
#endif
  NTTRACE("WinMain()");
  if (_weaken(WinSockInit)) {
    _weaken(WinSockInit)();
  }
  if (_weaken(WinMainForked)) {
    _weaken(WinMainForked)();
  }
  WinMainNew(cmdline);
}

#endif /* __x86_64__ */
