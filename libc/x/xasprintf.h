#ifndef COSMOPOLITAN_LIBC_X_XASPRINTF_H_
#define COSMOPOLITAN_LIBC_X_XASPRINTF_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

char *xasprintf(const char *, ...) printfesque(1) paramsnonnull((1))
    returnspointerwithnoaliases dontthrow nocallback dontdiscard returnsnonnull;
char *xvasprintf(const char *, va_list) paramsnonnull()
    returnspointerwithnoaliases dontthrow nocallback dontdiscard returnsnonnull;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_X_XASPRINTF_H_ */
