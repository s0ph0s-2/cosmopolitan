#ifndef COSMOPOLITAN_LIBC_RUNTIME_WINARGS_INTERNAL_H_
#define COSMOPOLITAN_LIBC_RUNTIME_WINARGS_INTERNAL_H_
#include "libc/limits.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

struct WinArgs {
  char *argv[4096];
  char *envp[4060];
  intptr_t auxv[2][2];
  char argblock[ARG_MAX / 2];
  char envblock[ARG_MAX / 2];
  char argv0buf[256];
};

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_RUNTIME_WINARGS_INTERNAL_H_ */
