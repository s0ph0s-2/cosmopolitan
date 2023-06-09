#ifndef COSMOPOLITAN_LIBC_SYSV_CONSTS_FIO_H_
#define COSMOPOLITAN_LIBC_SYSV_CONSTS_FIO_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

extern const uint32_t FIOASYNC;
extern const uint32_t FIOCLEX;
extern const uint32_t FIONBIO;
extern const uint32_t FIONCLEX;
extern const uint32_t FIONREAD;

#define FIOASYNC FIOASYNC
#define FIOCLEX  FIOCLEX
#define FIONBIO  FIONBIO
#define FIONCLEX FIONCLEX
#define FIONREAD FIONREAD

#define __tmpcosmo_FIOASYNC -484047213
#define __tmpcosmo_FIOCLEX  -1198942590
#define __tmpcosmo_FIONBIO  2035363853
#define __tmpcosmo_FIONCLEX -25760400
#define __tmpcosmo_FIONREAD 1333957726

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_SYSV_CONSTS_FIO_H_ */
