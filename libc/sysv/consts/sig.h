#ifndef COSMOPOLITAN_LIBC_SYSV_CONSTS_SIG_H_
#define COSMOPOLITAN_LIBC_SYSV_CONSTS_SIG_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

extern const int SIGABRT;
extern const int SIGALRM;
extern const int SIGBUS;
extern const int SIGTHR;
extern const int SIGCHLD;
extern const int SIGCONT;
extern const int SIGEMT;
extern const int SIGFPE;
extern const int SIGHUP;
extern const int SIGILL;
extern const int SIGINFO;
extern const int SIGINT;
extern const int SIGIO;
extern const int SIGIOT;
extern const int SIGKILL;
extern const int SIGPIPE;
extern const int SIGPOLL;
extern const int SIGPROF;
extern const int SIGPWR;
extern const int SIGQUIT;
extern const int SIGRTMAX;
extern const int SIGRTMIN;
extern const int SIGSEGV;
extern const int SIGSTKFLT;
extern const int SIGSTOP;
extern const int SIGSYS;
extern const int SIGTERM;
extern const int SIGTRAP;
extern const int SIGTSTP;
extern const int SIGTTIN;
extern const int SIGTTOU;
extern const int SIGUNUSED;
extern const int SIGURG;
extern const int SIGUSR1;
extern const int SIGUSR2;
extern const int SIGVTALRM;
extern const int SIGWINCH;
extern const int SIGXCPU;
extern const int SIGXFSZ;

extern const int SIG_BLOCK;
extern const int SIG_SETMASK;
extern const int SIG_UNBLOCK;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */

#define SIGABRT   6
#define SIGALRM   14
#define SIGFPE    8
#define SIGHUP    1
#define SIGILL    4
#define SIGINT    2
#define SIGIOT    6
#define SIGKILL   9
#define SIGPIPE   13
#define SIGPROF   27
#define SIGQUIT   3
#define SIGSEGV   11
#define SIGTERM   15
#define SIGTRAP   5
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGVTALRM 26
#define SIGWINCH  28
#define SIGXCPU   24
#define SIGXFSZ   25

#define SIGBUS    SIGBUS
#define SIGTHR    SIGTHR
#define SIGCHLD   SIGCHLD
#define SIGCONT   SIGCONT
#define SIGEMT    SIGEMT
#define SIGINFO   SIGINFO
#define SIGIO     SIGIO
#define SIGPOLL   SIGPOLL
#define SIGPWR    SIGPWR
#define SIGRTMAX  SIGRTMAX
#define SIGRTMIN  SIGRTMIN
#define SIGSTKFLT SIGSTKFLT
#define SIGSTOP   SIGSTOP
#define SIGSYS    SIGSYS
#define SIGTSTP   SIGTSTP
#define SIGUNUSED SIGUNUSED
#define SIGURG    SIGURG
#define SIGUSR1   SIGUSR1
#define SIGUSR2   SIGUSR2

#define SIG_BLOCK   SIG_BLOCK
#define SIG_SETMASK SIG_SETMASK
#define SIG_UNBLOCK SIG_UNBLOCK



#endif /* COSMOPOLITAN_LIBC_SYSV_CONSTS_SIG_H_ */
