#ifndef COSMOPOLITAN_LIBC_CALLS_STRUCT_TERMIOS_INTERNAL_H_
#define COSMOPOLITAN_LIBC_CALLS_STRUCT_TERMIOS_INTERNAL_H_
#include "libc/calls/struct/termios.h"
#include "libc/mem/alloca.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

const char *DescribeTermios(char[1024], ssize_t, struct termios *);

#define DescribeTermios(rc, tio) DescribeTermios(alloca(1024), rc, tio)

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_CALLS_STRUCT_TERMIOS_INTERNAL_H_ */
