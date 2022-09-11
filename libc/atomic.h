#ifndef COSMOPOLITAN_LIBC_ATOMIC_H_
#define COSMOPOLITAN_LIBC_ATOMIC_H_
#include "libc/inttypes.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

/**
 * @fileoverview C11 Atomic Types
 *
 * We supoprt C++ and old C compilers. It's recommended you use macros
 * like `_Atomic(int)` rather than `_Atomic int` or `atomic_int` since
 * we only define a portability macro for the syntax `_Atomic(T)`.
 *
 * @see libc/integral/c.inc
 * @see libc/intrin/atomic.h
 */

#define atomic_bool           _Atomic(_Bool)
#define atomic_bool32         atomic_int32
#define atomic_char           _Atomic(char)
#define atomic_schar          _Atomic(signed char)
#define atomic_uchar          _Atomic(unsigned char)
#define atomic_short          _Atomic(short)
#define atomic_ushort         _Atomic(unsigned short)
#define atomic_int            _Atomic(int)
#define atomic_uint           _Atomic(unsigned int)
#define atomic_long           _Atomic(long)
#define atomic_ulong          _Atomic(unsigned long)
#define atomic_llong          _Atomic(long long)
#define atomic_ullong         _Atomic(unsigned long long)
#define atomic_char16_t       _Atomic(char16_t)
#define atomic_char32_t       _Atomic(char32_t)
#define atomic_wchar_t        _Atomic(wchar_t)
#define atomic_intptr_t       _Atomic(intptr_t)
#define atomic_uintptr_t      _Atomic(uintptr_t)
#define atomic_size_t         _Atomic(size_t)
#define atomic_ptrdiff_t      _Atomic(ptrdiff_t)
#define atomic_int_fast8_t    _Atomic(int_fast8_t)
#define atomic_uint_fast8_t   _Atomic(uint_fast8_t)
#define atomic_int_fast16_t   _Atomic(int_fast16_t)
#define atomic_uint_fast16_t  _Atomic(uint_fast16_t)
#define atomic_int_fast32_t   _Atomic(int_fast32_t)
#define atomic_uint_fast32_t  _Atomic(uint_fast32_t)
#define atomic_int_fast64_t   _Atomic(int_fast64_t)
#define atomic_uint_fast64_t  _Atomic(uint_fast64_t)
#define atomic_int_least8_t   _Atomic(int_least8_t)
#define atomic_uint_least8_t  _Atomic(uint_least8_t)
#define atomic_int_least16_t  _Atomic(int_least16_t)
#define atomic_uint_least16_t _Atomic(uint_least16_t)
#define atomic_int_least32_t  _Atomic(int_least32_t)
#define atomic_uint_least32_t _Atomic(uint_least32_t)
#define atomic_int_least64_t  _Atomic(int_least64_t)
#define atomic_uint_least64_t _Atomic(uint_least64_t)

#ifdef __CLANG_ATOMIC_BOOL_LOCK_FREE
#define ATOMIC_BOOL_LOCK_FREE     __CLANG_ATOMIC_BOOL_LOCK_FREE
#define ATOMIC_CHAR_LOCK_FREE     __CLANG_ATOMIC_CHAR_LOCK_FREE
#define ATOMIC_CHAR16_T_LOCK_FREE __CLANG_ATOMIC_CHAR16_T_LOCK_FREE
#define ATOMIC_CHAR32_T_LOCK_FREE __CLANG_ATOMIC_CHAR32_T_LOCK_FREE
#define ATOMIC_WCHAR_T_LOCK_FREE  __CLANG_ATOMIC_WCHAR_T_LOCK_FREE
#define ATOMIC_SHORT_LOCK_FREE    __CLANG_ATOMIC_SHORT_LOCK_FREE
#define ATOMIC_INT_LOCK_FREE      __CLANG_ATOMIC_INT_LOCK_FREE
#define ATOMIC_LONG_LOCK_FREE     __CLANG_ATOMIC_LONG_LOCK_FREE
#define ATOMIC_LLONG_LOCK_FREE    __CLANG_ATOMIC_LLONG_LOCK_FREE
#define ATOMIC_POINTER_LOCK_FREE  __CLANG_ATOMIC_POINTER_LOCK_FREE
#else
#define ATOMIC_BOOL_LOCK_FREE     __GCC_ATOMIC_BOOL_LOCK_FREE
#define ATOMIC_CHAR_LOCK_FREE     __GCC_ATOMIC_CHAR_LOCK_FREE
#define ATOMIC_CHAR16_T_LOCK_FREE __GCC_ATOMIC_CHAR16_T_LOCK_FREE
#define ATOMIC_CHAR32_T_LOCK_FREE __GCC_ATOMIC_CHAR32_T_LOCK_FREE
#define ATOMIC_WCHAR_T_LOCK_FREE  __GCC_ATOMIC_WCHAR_T_LOCK_FREE
#define ATOMIC_SHORT_LOCK_FREE    __GCC_ATOMIC_SHORT_LOCK_FREE
#define ATOMIC_INT_LOCK_FREE      __GCC_ATOMIC_INT_LOCK_FREE
#define ATOMIC_LONG_LOCK_FREE     __GCC_ATOMIC_LONG_LOCK_FREE
#define ATOMIC_LLONG_LOCK_FREE    __GCC_ATOMIC_LLONG_LOCK_FREE
#define ATOMIC_POINTER_LOCK_FREE  __GCC_ATOMIC_POINTER_LOCK_FREE
#endif

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_ATOMIC_H_ */
