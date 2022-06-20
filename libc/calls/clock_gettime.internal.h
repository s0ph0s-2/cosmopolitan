#ifndef COSMOPOLITAN_LIBC_CALLS_CLOCK_GETTIME_H_
#define COSMOPOLITAN_LIBC_CALLS_CLOCK_GETTIME_H_
#include "libc/calls/struct/timespec.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

typedef int clock_gettime_f(int, struct timespec *);

extern clock_gettime_f *__clock_gettime;
clock_gettime_f *__clock_gettime_get(bool *) hidden;
int __clock_gettime_init(int, struct timespec *) hidden;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_CALLS_CLOCK_GETTIME_H_ */
