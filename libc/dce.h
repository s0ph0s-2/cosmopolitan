#ifndef COSMOPOLITAN_LIBC_DCE_H_
#define COSMOPOLITAN_LIBC_DCE_H_
/*─────────────────────────────────────────────────────────────────────────────╗
│ cosmopolitan § autotune » dead code elimination                              │
╚─────────────────────────────────────────────────────────────────────────────*/

#ifndef SUPPORT_VECTOR
/**
 * Supported Platforms Tuning Knob (Runtime & Compile-Time)
 * Tuning this bitmask will remove platform polyfills at compile-time.
 */
#define SUPPORT_VECTOR 255
#endif

#define LINUX   1
#define METAL   2
#define WINDOWS 4
#define XNU     8
#define OPENBSD 16
#define FREEBSD 32
#define NETBSD  64

#ifdef NDEBUG
#define NoDebug() 1
#else
#define NoDebug() 0
#endif

#ifdef MODE_DBG
#define IsModeDbg() 1
#else
#define IsModeDbg() 0
#endif

#ifdef __MFENTRY__
#define HaveFentry() 1
#else
#define HaveFentry() 0
#endif

#ifdef TRUSTWORTHY
#define IsTrustworthy() 1
#else
#define IsTrustworthy() 0
#endif

#ifdef TINY
#define IsTiny() 1
#else
#define IsTiny() 0
#endif

#ifdef __OPTIMIZE__
#define IsOptimized() 1
#else
#define IsOptimized() 0
#endif

#ifdef __SANITIZE_ADDRESS__
#define IsAsan() 1
#else
#define IsAsan() 0
#endif

#if defined(__PIE__) || defined(__PIC__)
#define IsPositionIndependent() 1
#else
#define IsPositionIndependent() 0
#endif

#define SupportsLinux()   ((SUPPORT_VECTOR & LINUX) == LINUX)
#define SupportsMetal()   ((SUPPORT_VECTOR & METAL) == METAL)
#define SupportsWindows() ((SUPPORT_VECTOR & WINDOWS) == WINDOWS)
#define SupportsXnu()     ((SUPPORT_VECTOR & XNU) == XNU)
#define SupportsFreebsd() ((SUPPORT_VECTOR & FREEBSD) == FREEBSD)
#define SupportsOpenbsd() ((SUPPORT_VECTOR & OPENBSD) == OPENBSD)
#define SupportsNetbsd()  ((SUPPORT_VECTOR & NETBSD) == NETBSD)
#define SupportsBsd()     (!!(SUPPORT_VECTOR & (XNU | FREEBSD | OPENBSD | NETBSD)))
#define SupportsSystemv() \
  (!!(SUPPORT_VECTOR & (LINUX | XNU | OPENBSD | FREEBSD | NETBSD)))

#ifndef __ASSEMBLER__
#define IsLinux()   (SupportsLinux() && (__hostos & LINUX))
#define IsMetal()   (SupportsMetal() && (__hostos & METAL))
#define IsWindows() (SupportsWindows() && (__hostos & WINDOWS))
#define IsXnu()     (SupportsXnu() && (__hostos & XNU))
#define IsFreebsd() (SupportsFreebsd() && (__hostos & FREEBSD))
#define IsOpenbsd() (SupportsOpenbsd() && (__hostos & OPENBSD))
#define IsNetbsd()  (SupportsNetbsd() && (__hostos & NETBSD))
#define IsBsd()     (IsXnu() || IsFreebsd() || IsOpenbsd() || IsNetbsd())
#else
/* clang-format off */
#define IsLinux() $LINUX,__hostos(%rip)
#define IsMetal() $METAL,__hostos(%rip)
#define IsWindows() $WINDOWS,__hostos(%rip)
#define IsBsd() $XNU|FREEBSD|OPENBSD|NETBSD,__hostos(%rip)
#define IsXnu() $XNU,__hostos(%rip)
#define IsFreebsd() $FREEBSD,__hostos(%rip)
#define IsOpenbsd() $OPENBSD,__hostos(%rip)
#define IsNetbsd() $NETBSD,__hostos(%rip)
/* clang-format on */
#endif

#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

extern const int __hostos;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_DCE_H_ */
