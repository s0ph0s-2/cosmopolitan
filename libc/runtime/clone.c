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
#include "libc/assert.h"
#include "libc/calls/calls.h"
#include "libc/calls/struct/ucontext-netbsd.internal.h"
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/intrin/asan.internal.h"
#include "libc/intrin/describeflags.internal.h"
#include "libc/intrin/strace.internal.h"
#include "libc/limits.h"
#include "libc/macros.internal.h"
#include "libc/nt/runtime.h"
#include "libc/nt/synchronization.h"
#include "libc/nt/thread.h"
#include "libc/nt/thunk/msabi.h"
#include "libc/runtime/clone.internal.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/runtime.h"
#include "libc/sock/internal.h"
#include "libc/stdalign.internal.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/clone.h"
#include "libc/sysv/consts/futex.h"
#include "libc/sysv/consts/nr.h"
#include "libc/sysv/consts/nrlinux.h"
#include "libc/sysv/errfuns.h"
#include "libc/thread/freebsd.internal.h"
#include "libc/thread/openbsd.internal.h"
#include "libc/thread/thread.h"
#include "libc/thread/tls.h"
#include "libc/thread/tls2.h"
#include "libc/thread/xnu.internal.h"

#define __NR_thr_new                      455
#define __NR_clone_linux                  56
#define __NR__lwp_create                  309
#define __NR_getcontext_netbsd            307
#define __NR_bsdthread_create             0x02000168
#define __NR_thread_fast_set_cthread_self 0x03000003
#define PTHREAD_START_CUSTOM_XNU          0x01000000
#define LWP_DETACHED                      0x00000040
#define LWP_SUSPENDED                     0x00000080

__msabi extern typeof(TlsSetValue) *const __imp_TlsSetValue;
__msabi extern typeof(ExitThread) *const __imp_ExitThread;
__msabi extern typeof(WakeByAddressAll) *const __imp_WakeByAddressAll;

struct CloneArgs {
  union {
    int tid;
    uint32_t utid;
    int64_t tid64;
  };
  int *ptid;
  int *ctid;
  int *ztid;
  char *tls;
  int (*func)(void *, int);
  void *arg;
};

////////////////////////////////////////////////////////////////////////////////
// THE NEW TECHNOLOGY

int WinThreadLaunch(void *arg,                 // rdi
                    int tid,                   // rsi
                    int (*func)(void *, int),  // rdx
                    intptr_t rsp);             // rcx

// we can't log this function because:
//   1. windows owns the backtrace pointer right now
//   2. ftrace unwinds rbp to determine depth
//   3. tid in tls for ftrace isn't set yet
// we can't use address sanitizer because:
//   1. __asan_handle_no_return wipes stack
//   2. windows owns the stack memory right now
// we need win32 raw imports because:
//   1. generated thunks are function logged
noasan noinstrument static textwindows wontreturn void  //
WinThreadEntry(int rdi,                                 // rcx
               int rsi,                                 // rdx
               int rdx,                                 // r8
               struct CloneArgs *wt) {                  // r9
  int rc;
  if (wt->tls) __set_tls_win32(wt->tls);
  *wt->ctid = wt->tid;
  rc = WinThreadLaunch(wt->arg, wt->tid, wt->func, (intptr_t)wt & -16);
  // we can now clear ctid directly since we're no longer using our own
  // stack memory, which can now be safely free'd by the parent thread.
  *wt->ztid = 0;
  __imp_WakeByAddressAll(wt->ztid);
  // since we didn't indirect this function through NT2SYSV() it's not
  // safe to simply return, and as such, we need ExitThread().
  __imp_ExitThread(rc);
  notpossible;
}

static textwindows errno_t CloneWindows(int (*func)(void *, int), char *stk,
                                        size_t stksz, int flags, void *arg,
                                        void *tls, int *ptid, int *ctid) {
  int64_t h;
  struct CloneArgs *wt;
  wt = (struct CloneArgs *)(((intptr_t)(stk + stksz) -
                             sizeof(struct CloneArgs)) &
                            -alignof(struct CloneArgs));
  wt->ctid = flags & CLONE_CHILD_SETTID ? ctid : &wt->tid;
  wt->ztid = flags & CLONE_CHILD_CLEARTID ? ctid : &wt->tid;
  wt->func = func;
  wt->arg = arg;
  wt->tls = flags & CLONE_SETTLS ? tls : 0;
  if ((h = CreateThread(0, 4096, (void *)WinThreadEntry, wt, 0, &wt->utid))) {
    CloseHandle(h);
    if (flags & CLONE_PARENT_SETTID) {
      *ptid = wt->tid;
    }
    return 0;
  } else {
    return __dos2errno(GetLastError());
  }
}

////////////////////////////////////////////////////////////////////////////////
// XNU'S NOT UNIX

void XnuThreadThunk(void *pthread,          // rdi
                    int machport,           // rsi
                    void *(*func)(void *),  // rdx
                    void *arg,              // rcx
                    intptr_t *stack,        // r8
                    unsigned xnuflags);     // r9
asm("XnuThreadThunk:\n\t"
    "xor\t%ebp,%ebp\n\t"
    "mov\t%r8,%rsp\n\t"
    "and\t$-16,%rsp\n\t"
    "push\t%rax\n\t"
    "jmp\tXnuThreadMain\n\t"
    ".size\tXnuThreadThunk,.-XnuThreadThunk");
__attribute__((__used__, __no_reorder__))

static wontreturn void
XnuThreadMain(void *pthread,                    // rdi
              int tid,                          // rsi
              int (*func)(void *arg, int tid),  // rdx
              void *arg,                        // rcx
              struct CloneArgs *wt,             // r8
              unsigned xnuflags) {              // r9
  int ax;
  wt->tid = tid;
  *wt->ctid = tid;
  *wt->ptid = tid;

  if (wt->tls) {
    // XNU uses the same 0x30 offset as the WIN32 TIB x64. They told the
    // Go team at Google that they Apply stands by our ability to use it
    // https://github.com/golang/go/issues/23617#issuecomment-376662373
    asm volatile("syscall"
                 : "=a"(ax)
                 : "0"(__NR_thread_fast_set_cthread_self), "D"(wt->tls - 0x30)
                 : "rcx", "rdx", "r8", "r9", "r10", "r11", "memory", "cc");
  }

  func(arg, tid);

  // we no longer use the stack after this point
  // %rax = int bsdthread_terminate(%rdi = void *stackaddr,
  //                                %rsi = size_t freesize,
  //                                %rdx = uint32_t port,
  //                                %r10 = uint32_t sem);
  asm volatile("movl\t$0,%0\n\t"         // *wt->ztid = 0
               "xor\t%%r10d,%%r10d\n\t"  // sem = 0
               "syscall\n\t"             // __bsdthread_terminate()
               "ud2"
               : "=m"(*wt->ztid)
               : "a"(0x2000000 | 361), "D"(0), "S"(0), "d"(0)
               : "rcx", "r10", "r11", "memory");
  notpossible;
}

static errno_t CloneXnu(int (*fn)(void *), char *stk, size_t stksz, int flags,
                        void *arg, void *tls, int *ptid, int *ctid) {
  int rc;
  bool failed;
  static bool once;
  struct CloneArgs *wt;
  if (!once) {
    _npassert(sys_bsdthread_register(XnuThreadThunk, 0, 0, 0, 0, 0, 0) != -1);
    once = true;
  }
  wt = (struct CloneArgs *)(((intptr_t)(stk + stksz) -
                             sizeof(struct CloneArgs)) &
                            -alignof(struct CloneArgs));
  wt->ptid = flags & CLONE_PARENT_SETTID ? ptid : &wt->tid;
  wt->ctid = flags & CLONE_CHILD_SETTID ? ctid : &wt->tid;
  wt->ztid = flags & CLONE_CHILD_CLEARTID ? ctid : &wt->tid;
  wt->tls = flags & CLONE_SETTLS ? tls : 0;
  return sys_clone_xnu(fn, arg, wt, 0, PTHREAD_START_CUSTOM_XNU);
}

////////////////////////////////////////////////////////////////////////////////
// FREE BESIYATA DISHMAYA

static wontreturn void FreebsdThreadMain(void *p) {
  struct CloneArgs *wt = p;
  *wt->ctid = wt->tid;
  wt->func(wt->arg, wt->tid);
  // we no longer use the stack after this point
  // void thr_exit(%rdi = long *state);
  asm volatile("movl\t$0,%0\n\t"       // *wt->ztid = 0
               "syscall\n\t"           // _umtx_op(wt->ztid, WAKE, INT_MAX)
               "movl\t$431,%%eax\n\t"  // thr_exit(long *nonzeroes_and_wake)
               "xor\t%%edi,%%edi\n\t"  // sad we can't use this free futex op
               "syscall\n\t"           // exit1() fails if thread is orphaned
               "movl\t$1,%%eax\n\t"    // exit()
               "syscall"               //
               : "=m"(*wt->ztid)
               : "a"(454), "D"(wt->ztid), "S"(UMTX_OP_WAKE), "d"(INT_MAX)
               : "rcx", "r8", "r9", "r10", "r11", "memory");
  notpossible;
}

static errno_t CloneFreebsd(int (*func)(void *, int), char *stk, size_t stksz,
                            int flags, void *arg, void *tls, int *ptid,
                            int *ctid) {
  int ax;
  bool failed;
  int64_t tid;
  struct CloneArgs *wt;
  wt = (struct CloneArgs *)(((intptr_t)(stk + stksz) -
                             sizeof(struct CloneArgs)) &
                            -alignof(struct CloneArgs));
  wt->ctid = flags & CLONE_CHILD_SETTID ? ctid : &wt->tid;
  wt->ztid = flags & CLONE_CHILD_CLEARTID ? ctid : &wt->tid;
  wt->tls = tls;
  wt->func = func;
  wt->arg = arg;
  struct thr_param params = {
      .start_func = FreebsdThreadMain,
      .arg = wt,
      .stack_base = stk,
      .stack_size = (((intptr_t)wt - (intptr_t)stk) & -16) - 8,
      .tls_base = flags & CLONE_SETTLS ? tls : 0,
      .tls_size = 64,
      .child_tid = &wt->tid64,
      .parent_tid = &tid,
  };
  asm volatile(CFLAG_ASM("syscall")
               : CFLAG_CONSTRAINT(failed), "=a"(ax)
               : "1"(__NR_thr_new), "D"(&params), "S"(sizeof(params))
               : "rcx", "rdx", "r8", "r9", "r10", "r11", "memory");
  if (failed) return ax;
  if (flags & CLONE_PARENT_SETTID) *ptid = tid;
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// OPEN BESIYATA DISHMAYA

// we can't use address sanitizer because:
//   1. __asan_handle_no_return wipes stack [todo?]
noasan static wontreturn void OpenbsdThreadMain(void *p) {
  struct CloneArgs *wt = p;
  *wt->ctid = wt->tid;
  wt->func(wt->arg, wt->tid);
  asm volatile("mov\t%2,%%rsp\n\t"     // so syscall can validate stack exists
               "movl\t$0,(%%rdi)\n\t"  // *wt->ztid = 0 (old stack now free'd)
               "syscall\n\t"           // futex(int*, op, val) will wake wait0
               "xor\t%%edi,%%edi\n\t"  // so kernel doesn't write to old stack
               "mov\t$302,%%eax\n\t"   // __threxit(int *notdead) doesn't wake
               "syscall"
               : "=m"(*wt->ztid)
               : "a"(83), "m"(__oldstack), "D"(wt->ztid),
                 "S"(2 /* FUTEX_WAKE */), "d"(INT_MAX)
               : "rcx", "r11", "memory");
  notpossible;
}

static errno_t CloneOpenbsd(int (*func)(void *, int), char *stk, size_t stksz,
                            int flags, void *arg, void *tls, int *ptid,
                            int *ctid) {
  int rc;
  intptr_t sp;
  struct __tfork *tf;
  struct CloneArgs *wt;
  sp = (intptr_t)stk + stksz;
  sp -= sizeof(struct __tfork);
  sp &= -alignof(struct __tfork);
  tf = (struct __tfork *)sp;
  sp -= sizeof(struct CloneArgs);
  sp &= -MAX(16, alignof(struct CloneArgs));
  wt = (struct CloneArgs *)sp;
  wt->ctid = flags & CLONE_CHILD_SETTID ? ctid : &wt->tid;
  wt->ztid = flags & CLONE_CHILD_CLEARTID ? ctid : &wt->tid;
  wt->arg = arg;
  wt->func = func;
  tf->tf_stack = (char *)wt - 8;
  tf->tf_tcb = flags & CLONE_SETTLS ? tls : 0;
  tf->tf_tid = &wt->tid;
  if ((rc = __tfork_thread(tf, sizeof(*tf), OpenbsdThreadMain, wt)) >= 0) {
    _npassert(rc);
    if (flags & CLONE_PARENT_SETTID) {
      *ptid = rc;
    }
    return 0;
  } else {
    return -rc;
  }
}

////////////////////////////////////////////////////////////////////////////////
// NET BESIYATA DISHMAYA

static wontreturn void NetbsdThreadMain(void *arg,                 // rdi
                                        int (*func)(void *, int),  // rsi
                                        int *tid,                  // rdx
                                        int *ctid,                 // rcx
                                        int *ztid) {               // r9
  int ax, dx;
  // TODO(jart): Why are we seeing flakes where *tid is zero?
  // ax = *tid;
  ax = sys_gettid();
  *ctid = ax;
  func(arg, ax);
  // we no longer use the stack after this point
  // %eax = int __lwp_exit(void);
  asm volatile("movl\t$0,%2\n\t"  // *wt->ztid = 0
               "syscall\n\t"      // __lwp_exit()
               "ud2"
               : "=a"(ax), "=d"(dx), "=m"(*ztid)
               : "0"(310)
               : "rcx", "r11", "memory");
  notpossible;
}

static int CloneNetbsd(int (*func)(void *, int), char *stk, size_t stksz,
                       int flags, void *arg, void *tls, int *ptid, int *ctid) {
  // NetBSD has its own clone() and it works, but it's technically a
  // second-class API, intended to help Linux folks migrate to this.
  bool failed;
  int ax, *tid;
  intptr_t dx, sp;
  static bool once;
  struct ucontext_netbsd *ctx;
  static struct ucontext_netbsd netbsd_clone_template;

  // memoize arbitrary valid processor state structure
  if (!once) {
    asm volatile(CFLAG_ASM("syscall")
                 : CFLAG_CONSTRAINT(failed), "=a"(ax)
                 : "1"(__NR_getcontext_netbsd), "D"(&netbsd_clone_template)
                 : "rcx", "rdx", "r8", "r9", "r10", "r11", "memory");
    _npassert(!failed);
    once = true;
  }
  sp = (intptr_t)(stk + stksz);

  // allocate memory for tid
  sp -= sizeof(int);
  sp = sp & -alignof(int);
  tid = (int *)sp;
  *tid = 0;

  // align the stack
  sp = sp & -16;

  // simulate call to misalign stack and ensure backtrace looks good
  sp -= 8;
  *(intptr_t *)sp = (intptr_t)CloneNetbsd + 1;

  // place the giant 784 byte ucontext structure in the red zone!
  // it only has to live long enough for the thread to come alive
  ctx = (struct ucontext_netbsd *)((sp - sizeof(struct ucontext_netbsd)) &
                                   -alignof(struct ucontext_netbsd));

  // pass parameters in process state
  memcpy(ctx, &netbsd_clone_template, sizeof(*ctx));
  ctx->uc_link = 0;
  ctx->uc_mcontext.rbp = 0;
  ctx->uc_mcontext.rsp = sp;
  ctx->uc_mcontext.rip = (intptr_t)NetbsdThreadMain;
  ctx->uc_mcontext.rdi = (intptr_t)arg;
  ctx->uc_mcontext.rsi = (intptr_t)func;
  ctx->uc_mcontext.rdx = (intptr_t)tid;
  ctx->uc_mcontext.rcx = (intptr_t)(flags & CLONE_CHILD_SETTID ? ctid : tid);
  ctx->uc_mcontext.r8 = (intptr_t)(flags & CLONE_CHILD_CLEARTID ? ctid : tid);
  ctx->uc_flags |= _UC_STACK;
  ctx->uc_stack.ss_sp = stk;
  ctx->uc_stack.ss_size = stksz;
  ctx->uc_stack.ss_flags = 0;
  if (flags & CLONE_SETTLS) {
    ctx->uc_flags |= _UC_TLSBASE;
    ctx->uc_mcontext._mc_tlsbase = (intptr_t)tls;
  }

  // perform the system call
  asm volatile(CFLAG_ASM("syscall")
               : CFLAG_CONSTRAINT(failed), "=a"(ax), "=d"(dx)
               : "1"(__NR__lwp_create), "D"(ctx), "S"(LWP_DETACHED), "2"(tid)
               : "rcx", "r8", "r9", "r10", "r11", "memory");
  if (!failed) {
    _npassert(*tid);
    if (flags & CLONE_PARENT_SETTID) {
      *ptid = *tid;
    }
    return 0;
  } else {
    return ax;
  }
}

////////////////////////////////////////////////////////////////////////////////
// GNU/SYSTEMD

int sys_clone_linux(int flags,   // rdi
                    long sp,     // rsi
                    int *ptid,   // rdx
                    int *ctid,   // rcx
                    void *tls,   // r8
                    void *func,  // r9
                    void *arg);  // 8(rsp)

static int CloneLinux(int (*func)(void *arg, int rc), char *stk, size_t stksz,
                      int flags, void *arg, void *tls, int *ptid, int *ctid) {
  int rc;
  long sp;
  sp = (intptr_t)(stk + stksz);
  if (~flags & CLONE_CHILD_SETTID) {
    flags |= CLONE_CHILD_SETTID;
    sp -= sizeof(int);
    sp = sp & -alignof(int);
    ctid = (int *)sp;
    sp -= 8;  // experiment
  }
  sp = sp & -16;  // align the stack
  if ((rc = sys_clone_linux(flags, sp, ptid, ctid, tls, func, arg)) >= 0) {
    // clone() is documented as setting ptid before return
    return 0;
  } else {
    return -rc;
  }
}

////////////////////////////////////////////////////////////////////////////////
// COSMOPOLITAN

/**
 * Creates thread without malloc being linked.
 *
 * If you use clone() you're on you're own, e.g.
 *
 *     int worker(void *arg) { return 0; }
 *     struct CosmoTib tib = {.tib_self = &tib, .tib_tid = -1};
 *     atomic_int tid;
 *     char *stk = _mapstack();
 *     clone(worker, stk, GetStackSize() - 16,
 *           CLONE_VM | CLONE_THREAD | CLONE_FS | CLONE_FILES |
 *           CLONE_SIGHAND | CLONE_PARENT_SETTID | CLONE_CHILD_SETTID |
 *           CLONE_CHILD_CLEARTID | CLONE_SETTLS,
 *           arg, &tid, &tib, &tib.tib_tid);
 *     while (atomic_load(&tid) == 0) sched_yield();
 *     // thread is known
 *     while (atomic_load(&tib.tib_tid) < 0) sched_yield();
 *     // thread is running
 *     while (atomic_load(&tib.tib_tid) > 0) sched_yield();
 *     // thread has terminated
 *     _freestack(stk);
 *
 * Threads are created in a detached manner. They currently can't be
 * synchronized using wait() or posix signals. Threads created by this
 * function should be synchronized using shared memory operations.
 *
 * Any memory that's required by this system call wrapper is allocated
 * to the top of your stack. This shouldn't be more than 128 bytes.
 *
 * Your function is called from within the stack you specify. A return
 * address is pushed onto your stack, that causes returning to jump to
 * _Exit1() which terminates the thread. Even though the callback says
 * it supports a return code, that'll only work on Linux and Windows.
 *
 * This function follows the same ABI convention as the Linux userspace
 * libraries, with a few small changes. The varargs has been removed to
 * help prevent broken code, and the stack size and tls size parameters
 * are introduced for compatibility with FreeBSD.
 *
 * To keep this system call lightweight, only the thread creation use
 * case is polyfilled across platforms. For example, if you want fork
 * that works on OpenBSD for example, don't do it with clone(SIGCHLD)
 * and please just call fork(). Even if you do that on Linux, it will
 * effectively work around libc features like atfork(), so that means
 * other calls like getpid() may return incorrect values.
 *
 * @param func is your callback function, which this wrapper requires
 *     not be null, otherwise EINVAL is raised. It is passed two args
 *     within the child thread: (1) the caller-supplied `arg` and (2)
 *     the new tid is always passed in the second arg for convenience
 *
 * @param stk points to the bottom of a caller allocated stack, which
 *     must be allocated via mmap() using the MAP_STACK flag, or else
 *     you won't get optimal performance and it won't work on OpenBSD
 *
 * @param stksz is the size of that stack in bytes, we recommend that
 *     that this be set to GetStackSize() or else memory safety tools
 *     like kprintf() can't do as good and quick of a job; this value
 *     must be 16-aligned plus it must be at least 4192 bytes in size
 *     and it's advised to have the bottom-most page, be a guard page
 *
 * @param flags which SHOULD always have all of these flags:
 *
 *     - `CLONE_THREAD`
 *     - `CLONE_VM`
 *     - `CLONE_FS`
 *     - `CLONE_FILES`
 *     - `CLONE_SIGHAND`
 *
 *     This system call wrapper is intended for threads, and as such, we
 *     won't polyfill Linux's ability to simulate unrelated calls (e.g.
 *     fork, vfork) via clone() on other platforms. Please just call
 *     fork() and vfork() when that's what you want.
 *
 *     Your `flags` may also optionally also additionally bitwise-OR any
 *     combination of the following additional flags:
 *
 *     - `CLONE_CHILD_SETTID` must be specified if you intend to set the
 *       `ctid` argument, which will updated with the child tid once the
 *       child has started.
 *
 *     - `CLONE_PARENT_SETTID` must be specified if you intend to set
 *       the `ptid` argument, which is updated at the most opportune
 *       moment. On all platforms except XNU, this happens before
 *       clone() returns. On XNU, it happens once the thread starts.
 *
 *     - `CLONE_CHILD_CLEARTID` causes `*ctid = 0` upon child thread
 *       termination. This is used to implement join so that the parent
 *       may know when it's safe to free the child's stack memory, and
 *       as such, is guaranteed to happen AFTER the child thread has
 *       either terminated or has finished using its stack memory
 *
 *     - `CLONE_SETTLS` is needed if you intend to specify the `tls`
 *       argument, which after thread creation may be accessed using
 *       __get_tls(). Doing this means that `errno`, gettid(), etc.
 *       correctly work. Caveat emptor if you choose not to do this.
 *
 * @param arg is passed as an argument to `func` in the child thread
 * @param tls may be used to set the thread local storage segment;
 *     this parameter is ignored if `CLONE_SETTLS` is not set
 * @param ctid lets the child receive its thread id without having to
 *     call gettid() and is ignored if `CLONE_CHILD_SETTID` isn't set
 * @return 0 on success, or errno on errno
 * @threadsafe
 */
errno_t clone(void *func, void *stk, size_t stksz, int flags, void *arg,
              void *ptid, void *tls, void *ctid) {
  int rc;

  if (flags & CLONE_THREAD) {
    __enable_threads();
  }

  if (!func) {
    rc = EINVAL;
  } else if (!IsTiny() &&
             ((flags & CLONE_VM) && (stksz < PAGESIZE || (stksz & 15)))) {
    rc = EINVAL;
  } else if (IsAsan() &&
             (((flags & CLONE_SETTLS) && !__asan_is_valid(tls, 64)) ||
              ((flags & CLONE_PARENT_SETTID) &&
               !__asan_is_valid(ptid, sizeof(int))) ||
              ((flags & CLONE_CHILD_SETTID) &&
               !__asan_is_valid(ctid, sizeof(int))))) {
    rc = EFAULT;
  } else if (IsLinux()) {
    rc = CloneLinux(func, stk, stksz, flags, arg, tls, ptid, ctid);
  } else if (!IsTiny() &&
             (flags & ~(CLONE_SETTLS | CLONE_PARENT_SETTID |
                        CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID)) !=
                 (CLONE_THREAD | CLONE_VM | CLONE_FS | CLONE_FILES |
                  CLONE_SIGHAND)) {
    STRACE("clone flag unsupported on this platform");
    rc = EINVAL;
  } else if (IsXnu()) {
    rc = CloneXnu(func, stk, stksz, flags, arg, tls, ptid, ctid);
  } else if (IsFreebsd()) {
    rc = CloneFreebsd(func, stk, stksz, flags, arg, tls, ptid, ctid);
  } else if (IsNetbsd()) {
    rc = CloneNetbsd(func, stk, stksz, flags, arg, tls, ptid, ctid);
  } else if (IsOpenbsd()) {
    rc = CloneOpenbsd(func, stk, stksz, flags, arg, tls, ptid, ctid);
  } else if (IsWindows()) {
    rc = CloneWindows(func, stk, stksz, flags, arg, tls, ptid, ctid);
  } else {
    rc = ENOSYS;
  }

  STRACE("clone(%t, %p, %'zu, %#x, %p, %p, %p, %p) → %s", func, stk, stksz,
         flags, arg, ptid, tls, ctid, DescribeErrno(rc));

  return rc;
}
