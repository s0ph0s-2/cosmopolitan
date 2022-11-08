#ifndef COSMOPOLITAN_LIBC_THREAD_POSIXTHREAD_INTERNAL_H_
#define COSMOPOLITAN_LIBC_THREAD_POSIXTHREAD_INTERNAL_H_
#include "libc/calls/struct/sched_param.h"
#include "libc/calls/struct/sigset.h"
#include "libc/runtime/runtime.h"
#include "libc/thread/thread.h"
#include "libc/thread/tls.h"

#define PT_OWNSTACK       1
#define PT_MAINTHREAD     2
#define PT_ASYNC          4
#define PT_NOCANCEL       8
#define PT_MASKED         16
#define PT_INCANCEL       32
#define PT_OPENBSD_KLUDGE 64

#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

// LEGAL TRANSITIONS             ┌──> TERMINATED ─┐
// pthread_create ─┬─> JOINABLE ─┴┬─> DETACHED ───┴─> ZOMBIE
//                 └──────────────┘
enum PosixThreadStatus {

  // this is a running thread that needs pthread_join()
  //
  // the following transitions are possible:
  //
  // - kPosixThreadJoinable -> kPosixThreadTerminated if start_routine()
  //   returns, or is longjmp'd out of by pthread_exit(), and the thread
  //   is waiting to be joined.
  //
  // - kPosixThreadJoinable -> kPosixThreadDetached if pthread_detach()
  //   is called on this thread.
  kPosixThreadJoinable,

  // this is a managed thread that'll be cleaned up by the library.
  //
  // the following transitions are possible:
  //
  // - kPosixThreadDetached -> kPosixThreadZombie if start_routine()
  //   returns, or is longjmp'd out of by pthread_exit(), and the thread
  //   is waiting to be joined.
  kPosixThreadDetached,

  // this is a joinable thread that terminated.
  //
  // the following transitions are possible:
  //
  // - kPosixThreadTerminated -> _pthread_free() will happen when
  //   pthread_join() is called by the user.
  // - kPosixThreadTerminated -> kPosixThreadZombie will happen when
  //   pthread_detach() is called by the user.
  kPosixThreadTerminated,

  // this is a detached thread that terminated.
  //
  // the following transitions are possible:
  //
  // - kPosixThreadZombie -> _pthread_free() will happen whenever
  //   convenient, e.g. pthread_create() entry or atexit handler.
  kPosixThreadZombie,
};

struct PosixThread {
  int flags;               // 0x00: see PT_* constants
  _Atomic(int) cancelled;  // 0x04: thread has bad beliefs
  _Atomic(enum PosixThreadStatus) status;
  _Atomic(int) ptid;       // transitions 0 → tid
  void *(*start)(void *);  // creation callback
  void *arg;               // start's parameter
  void *rc;                // start's return value
  char *altstack;          // thread sigaltstack
  char *tls;               // bottom of tls allocation
  struct CosmoTib *tib;    // middle of tls allocation
  jmp_buf exiter;          // for pthread_exit
  pthread_attr_t attr;
  sigset_t sigmask;
  struct _pthread_cleanup_buffer *cleanup;
};

typedef void (*atfork_f)(void);

extern struct PosixThread _pthread_main;
extern _Atomic(pthread_key_dtor) _pthread_key_dtor[PTHREAD_KEYS_MAX] _Hide;

int _pthread_atfork(atfork_f, atfork_f, atfork_f) _Hide;
int _pthread_reschedule(struct PosixThread *) _Hide;
int _pthread_setschedparam_freebsd(int, int, const struct sched_param *) _Hide;
int _pthread_wait(struct PosixThread *) _Hide;
void _pthread_free(struct PosixThread *) _Hide;
void _pthread_cleanup(struct PosixThread *) _Hide;
void _pthread_ungarbage(void) _Hide;
void _pthread_zombies_add(struct PosixThread *) _Hide;
void _pthread_zombies_purge(void) _Hide;
void _pthread_zombies_decimate(void) _Hide;
void _pthread_zombies_harvest(void) _Hide;
void _pthread_key_destruct(void) _Hide;
void _pthread_onfork_prepare(void) _Hide;
void _pthread_onfork_parent(void) _Hide;
void _pthread_onfork_child(void) _Hide;
int _pthread_cancel_sys(void) _Hide;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_THREAD_POSIXTHREAD_INTERNAL_H_ */
