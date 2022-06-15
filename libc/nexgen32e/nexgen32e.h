#ifndef COSMOPOLITAN_LIBC_NEXGEN32E_NEXGEN32E_H_
#define COSMOPOLITAN_LIBC_NEXGEN32E_NEXGEN32E_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

extern long kHalfCache3;
extern const uint64_t kTens[20];

void imapxlatab(void *);
void insertionsort(int32_t *, size_t);
void CheckStackIsAligned(void);

int64_t div10int64(int64_t) libcesque pureconst;
int64_t div100int64(int64_t) libcesque pureconst;
int64_t div1000int64(int64_t) libcesque pureconst;
int64_t div10000int64(int64_t) libcesque pureconst;
int64_t div1000000int64(int64_t) libcesque pureconst;
int64_t div1000000000int64(int64_t) libcesque pureconst;

int64_t rem10int64(int64_t) libcesque pureconst;
int64_t rem100int64(int64_t) libcesque pureconst;
int64_t rem1000int64(int64_t) libcesque pureconst;
int64_t rem10000int64(int64_t) libcesque pureconst;
int64_t rem1000000int64(int64_t) libcesque pureconst;
int64_t rem1000000000int64(int64_t) libcesque pureconst;

char sbb(uint64_t *, const uint64_t *, const uint64_t *, size_t);
char adc(uint64_t *, const uint64_t *, const uint64_t *, size_t);

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_NEXGEN32E_NEXGEN32E_H_ */
