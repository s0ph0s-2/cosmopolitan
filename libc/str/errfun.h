#ifndef COSMOPOLITAN_LIBC_STR_ERRFUN_H_
#define COSMOPOLITAN_LIBC_STR_ERRFUN_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

char *strerrno(int) nosideeffect libcesque;
char *strerdoc(int) nosideeffect libcesque;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_STR_ERRFUN_H_ */
