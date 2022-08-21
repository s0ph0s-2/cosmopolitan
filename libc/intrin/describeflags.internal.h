#ifndef COSMOPOLITAN_LIBC_INTRIN_DESCRIBEFLAGS_INTERNAL_H_
#define COSMOPOLITAN_LIBC_INTRIN_DESCRIBEFLAGS_INTERNAL_H_
#include "libc/mem/alloca.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

struct thatispacked DescribeFlags {
  unsigned flag;
  const char *name;
};

const char *DescribeFlags(char *, size_t, struct DescribeFlags *, size_t,
                          const char *, unsigned);

const char *DescribeCapability(char[20], int);
const char *DescribeClockName(char[32], int);
const char *DescribeDirfd(char[12], int);
const char *DescribeFrame(char[32], int);
const char *DescribeFutexResult(char[12], int);
const char *DescribeHow(char[12], int);
const char *DescribeMapFlags(char[64], int);
const char *DescribeMapping(char[8], int, int);
const char *DescribeNtConsoleInFlags(char[256], uint32_t);
const char *DescribeNtConsoleOutFlags(char[128], uint32_t);
const char *DescribeNtCreationDisposition(uint32_t);
const char *DescribeNtFileAccessFlags(char[512], uint32_t);
const char *DescribeNtFileFlagAttr(char[256], uint32_t);
const char *DescribeNtFileMapFlags(char[64], uint32_t);
const char *DescribeNtFileShareFlags(char[64], uint32_t);
const char *DescribeNtFiletypeFlags(char[64], uint32_t);
const char *DescribeNtMovFileInpFlags(char[256], uint32_t);
const char *DescribeNtPageFlags(char[64], uint32_t);
const char *DescribeNtPipeModeFlags(char[64], uint32_t);
const char *DescribeNtPipeOpenFlags(char[64], uint32_t);
const char *DescribeNtProcAccessFlags(char[256], uint32_t);
const char *DescribeNtStartFlags(char[128], uint32_t);
const char *DescribeNtSymlinkFlags(char[64], uint32_t);
const char *DescribeOpenFlags(char[128], int);
const char *DescribePersonalityFlags(char[128], int);
const char *DescribePollFlags(char[64], int);
const char *DescribePrctlOperation(int);
const char *DescribeProtFlags(char[48], int);
const char *DescribePtrace(char[12], int);
const char *DescribePtraceEvent(char[32], int);
const char *DescribeRemapFlags(char[48], int);
const char *DescribeRlimitName(char[20], int);
const char *DescribeSchedPolicy(char[48], int);
const char *DescribeSeccompOperation(int);
const char *DescribeSockLevel(char[12], int);
const char *DescribeSockOptname(char[32], int, int);
const char *DescribeSocketFamily(char[12], int);
const char *DescribeSocketProtocol(char[12], int);
const char *DescribeSocketType(char[64], int);
const char *DescribeWhence(char[12], int);

#define DescribeCapability(x)        DescribeCapability(alloca(20), x)
#define DescribeClockName(x)         DescribeClockName(alloca(32), x)
#define DescribeDirfd(x)             DescribeDirfd(alloca(12), x)
#define DescribeFrame(x)             DescribeFrame(alloca(32), x)
#define DescribeFutexResult(x)       DescribeFutexResult(alloca(12), x)
#define DescribeHow(x)               DescribeHow(alloca(12), x)
#define DescribeMapFlags(x)          DescribeMapFlags(alloca(64), x)
#define DescribeMapping(x, y)        DescribeMapping(alloca(8), x, y)
#define DescribeNtConsoleInFlags(x)  DescribeNtConsoleInFlags(alloca(256), x)
#define DescribeNtConsoleOutFlags(x) DescribeNtConsoleOutFlags(alloca(128), x)
#define DescribeNtFileAccessFlags(x) DescribeNtFileAccessFlags(alloca(512), x)
#define DescribeNtFileFlagAttr(x)    DescribeNtFileFlagAttr(alloca(256), x)
#define DescribeNtFileMapFlags(x)    DescribeNtFileMapFlags(alloca(64), x)
#define DescribeNtFileShareFlags(x)  DescribeNtFileShareFlags(alloca(64), x)
#define DescribeNtFiletypeFlags(x)   DescribeNtFiletypeFlags(alloca(64), x)
#define DescribeNtMovFileInpFlags(x) DescribeNtMovFileInpFlags(alloca(256), x)
#define DescribeNtPageFlags(x)       DescribeNtPageFlags(alloca(64), x)
#define DescribeNtPipeModeFlags(x)   DescribeNtPipeModeFlags(alloca(64), x)
#define DescribeNtPipeOpenFlags(x)   DescribeNtPipeOpenFlags(alloca(64), x)
#define DescribeNtProcAccessFlags(x) DescribeNtProcAccessFlags(alloca(256), x)
#define DescribeNtStartFlags(x)      DescribeNtStartFlags(alloca(128), x)
#define DescribeNtSymlinkFlags(x)    DescribeNtSymlinkFlags(alloca(64), x)
#define DescribeOpenFlags(x)         DescribeOpenFlags(alloca(128), x)
#define DescribePersonalityFlags(p)  DescribePersonalityFlags(alloca(128), p)
#define DescribePollFlags(p)         DescribePollFlags(alloca(64), p)
#define DescribeProtFlags(x)         DescribeProtFlags(alloca(48), x)
#define DescribePtrace(i)            DescribePtrace(alloca(12), i)
#define DescribePtraceEvent(x)       DescribePtraceEvent(alloca(32), x)
#define DescribeRemapFlags(x)        DescribeRemapFlags(alloca(48), x)
#define DescribeRlimitName(rl)       DescribeRlimitName(alloca(20), rl)
#define DescribeSchedPolicy(x)       DescribeSchedPolicy(alloca(48), x)
#define DescribeSockLevel(x)         DescribeSockLevel(alloca(12), x)
#define DescribeSockOptname(x, y)    DescribeSockOptname(alloca(32), x, y)
#define DescribeSocketFamily(x)      DescribeSocketFamily(alloca(12), x)
#define DescribeSocketProtocol(x)    DescribeSocketProtocol(alloca(12), x)
#define DescribeSocketType(x)        DescribeSocketType(alloca(64), x)
#define DescribeWhence(x)            DescribeWhence(alloca(12), x)

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_INTRIN_DESCRIBEFLAGS_INTERNAL_H_ */
