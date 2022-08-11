#ifndef COSMOPOLITAN_LIBC_STDIO_INTERNAL_H_
#define COSMOPOLITAN_LIBC_STDIO_INTERNAL_H_
#include "libc/stdio/stdio.h"

#define PUSHBACK 12

#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

extern char g_stdinbuf[BUFSIZ];
extern char g_stdoutbuf[BUFSIZ];
extern char g_stderrbuf[BUFSIZ];

hidden extern uint64_t g_rando;

int __fflush_impl(FILE *) hidden;
int __fflush_register(FILE *) hidden;
void __fflush_unregister(FILE *) hidden;
bool __stdio_isok(FILE *) hidden;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_STDIO_INTERNAL_H_ */
