#ifndef COSMOPOLITAN_LIBC_ASSERT_H_
#define COSMOPOLITAN_LIBC_ASSERT_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

extern bool __assert_disable;
void __assert_fail(const char *, const char *, int) _Hide relegated;

#ifdef NDEBUG
#define assert(x) ((void)0)
#else
#define assert(x) ((void)((x) || (__assert_fail(#x, __FILE__, __LINE__), 0)))
#endif

#ifndef __cplusplus
#define static_assert _Static_assert
#endif

#ifndef NDEBUG
#define _unassert(x) __assert_macro(x, #x)
#define _npassert(x) __assert_macro(x, #x)
#define __assert_macro(x, s)                \
  ({                                        \
    if (__builtin_expect(!(x), 0)) {        \
      __assert_fail(s, __FILE__, __LINE__); \
      __builtin_trap();                     \
    }                                       \
    (void)0;                                \
  })
#else
#define _npassert(x)                 \
  ({                                 \
    if (__builtin_expect(!(x), 0)) { \
      __builtin_trap();              \
    }                                \
    (void)0;                         \
  })
#define _unassert(x)                 \
  ({                                 \
    if (__builtin_expect(!(x), 0)) { \
      __builtin_unreachable();       \
    }                                \
    (void)0;                         \
  })
#endif

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_ASSERT_H_ */
