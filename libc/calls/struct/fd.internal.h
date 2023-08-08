#ifndef COSMOPOLITAN_LIBC_CALLS_STRUCT_FD_INTERNAL_H_
#define COSMOPOLITAN_LIBC_CALLS_STRUCT_FD_INTERNAL_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

#define kFdEmpty    0
#define kFdFile     1
#define kFdSocket   2
#define kFdProcess  3
#define kFdConsole  4
#define kFdSerial   5
#define kFdZip      6
#define kFdEpoll    7
#define kFdReserved 8

#define kFdTtyEchoing 1 /* read()→write() (ECHO && !ICANON) */
#define kFdTtyEchoRaw 2 /* don't ^X visualize control codes */
#define kFdTtyMunging 4 /* enable input / output remappings */
#define kFdTtyNoCr2Nl 8 /* don't map \r → \n (a.k.a !ICRNL) */

struct Fd {
  char kind;
  bool zombie;
  char ttymagic;
  unsigned flags;
  unsigned mode;
  int64_t handle;
  int64_t extra;
};

struct Fds {
  _Atomic(int) f; /* lowest free slot */
  size_t n;
  struct Fd *p, *e;
};

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_CALLS_STRUCT_FD_INTERNAL_H_ */
