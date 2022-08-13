#ifndef COSMOPOLITAN_LIBC_CALLS_STRUCT_SYSINFO_INTERNAL_H_
#define COSMOPOLITAN_LIBC_CALLS_STRUCT_SYSINFO_INTERNAL_H_
#include "libc/calls/struct/sysinfo.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

int sys_sysinfo(struct sysinfo *) hidden;
int sys_sysinfo_nt(struct sysinfo *) hidden;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_CALLS_STRUCT_SYSINFO_INTERNAL_H_ */
