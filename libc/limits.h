#ifndef COSMOPOLITAN_LIBC_LIMITS_H_
#define COSMOPOLITAN_LIBC_LIMITS_H_
#define __STDC_LIMIT_MACROS

#define UCHAR_MIN 0
#define UCHAR_MAX 255

#if '\200' < 0
#define CHAR_MIN '\200'
#define CHAR_MAX '\177'
#else
#define CHAR_MIN '\0'
#define CHAR_MAX '\377'
#endif

#define SCHAR_MAX     __SCHAR_MAX__
#define SHRT_MAX      __SHRT_MAX__
#define INT_MAX       __INT_MAX__
#define LONG_MAX      __LONG_MAX__
#define LLONG_MAX     LONG_LONG_MAX
#define LONG_LONG_MAX __LONG_LONG_MAX__
#define SIZE_MAX      __SIZE_MAX__
#define INT8_MAX      __INT8_MAX__
#define INT16_MAX     __INT16_MAX__
#define INT32_MAX     __INT32_MAX__
#define INT64_MAX     __INT64_MAX__
#define WINT_MAX      __WCHAR_MAX__
#define WCHAR_MAX     __WCHAR_MAX__
#define INTPTR_MAX    __INTPTR_MAX__
#define PTRDIFF_MAX   __PTRDIFF_MAX__
#define UINTPTR_MAX   __UINTPTR_MAX__
#define UINT8_MAX     __UINT8_MAX__
#define UINT16_MAX    __UINT16_MAX__
#define UINT32_MAX    __UINT32_MAX__
#define UINT64_MAX    __UINT64_MAX__
#define INTMAX_MAX    __INTMAX_MAX__
#define UINTMAX_MAX   __UINTMAX_MAX__

extern int __got_long_min;
#define SCHAR_MIN     (-SCHAR_MAX - 1)
#define SHRT_MIN      (-SHRT_MAX - 1)
#define INT_MIN       (-INT_MAX - 1)
#define LONG_MIN      (-LONG_MAX - 1)
#define LLONG_MIN     (-LLONG_MAX - 1)
#define LONG_LONG_MIN (-LONG_LONG_MAX - 1)
#define SIZE_MIN      (-SIZE_MAX - 1)
#define INT8_MIN      (-INT8_MAX - 1)
#define INT16_MIN     (-INT16_MAX - 1)
#define INT32_MIN     (-INT32_MAX - 1)
#define INT64_MIN     (-INT64_MAX - 1)
#define INTMAX_MIN    (-INTMAX_MAX - 1)
#define INTPTR_MIN    (-INTPTR_MAX - 1)
#define WINT_MIN      (-WINT_MAX - 1)
#define WCHAR_MIN     (-WCHAR_MAX - 1)
#define PTRDIFF_MIN   (-PTRDIFF_MAX - 1)

#define USHRT_MAX 65535
#define UINT_MAX  0xffffffffu
#if __SIZEOF_LONG__ == 8
#define ULONG_MAX 0xfffffffffffffffful
#else
#define ULONG_MAX 0xfffffffful
#endif
#define ULLONG_MAX     0xffffffffffffffffull
#define ULONG_LONG_MAX 0xffffffffffffffffull

#define USHRT_MIN      0
#define UINT_MIN       0u
#define ULONG_MIN      0ul
#define ULLONG_MIN     0ull
#define ULONG_LONG_MIN 0ull
#define UINT8_MIN      0
#define UINT16_MIN     0
#define UINT32_MIN     0u
#define UINT64_MIN     0ull
#define UINTPTR_MIN    0ull
#define UINTMAX_MIN    ((uintmax_t)0)

#define MB_CUR_MAX 4
#define MB_LEN_MAX 4

#if __GNUC__ * 100 + __GNUC_MINOR__ >= 406 || defined(__llvm__)
#define INT128_MIN  (-INT128_MAX - 1)
#define UINT128_MIN ((uint128_t)0)
#define INT128_MAX \
  ((int128_t)0x7fffffffffffffff << 64 | (int128_t)0xffffffffffffffff)
#define UINT128_MAX \
  ((uint128_t)0xffffffffffffffff << 64 | (uint128_t)0xffffffffffffffff)
#endif /* GCC 4.6+ */

#define SIG_ATOMIC_MIN INT32_MIN
#define SIG_ATOMIC_MAX INT32_MAX

#define FILESIZEBITS   64
#define SYMLOOP_MAX    40
#define TTY_NAME_MAX   32
#define HOST_NAME_MAX  255
#define TZNAME_MAX     6
#define WORD_BIT       32
#define SEM_VALUE_MAX  0x7fffffff
#define SEM_NSEMS_MAX  256
#define DELAYTIMER_MAX 0x7fffffff
#define MQ_PRIO_MAX    32768
#define LOGIN_NAME_MAX 256

#define NL_ARGMAX  9
#define NL_MSGMAX  32767
#define NL_SETMAX  255
#define NL_TEXTMAX 2048

#endif /* COSMOPOLITAN_LIBC_LIMITS_H_ */
