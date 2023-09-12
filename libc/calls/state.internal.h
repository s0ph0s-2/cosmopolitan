#ifndef COSMOPOLITAN_LIBC_CALLS_STATE_INTERNAL_H_
#define COSMOPOLITAN_LIBC_CALLS_STATE_INTERNAL_H_
#include "libc/thread/thread.h"
#include "libc/thread/tls.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

extern int __vforked;
extern bool __time_critical;
extern pthread_mutex_t __fds_lock_obj;
extern pthread_mutex_t __sig_lock_obj;
extern unsigned __sighandrvas[NSIG + 1];
extern unsigned __sighandflags[NSIG + 1];
extern const struct NtSecurityAttributes kNtIsInheritable;

void __fds_lock(void);
void __fds_unlock(void);
void __fds_funlock(void);
void __sig_lock(void);
void __sig_unlock(void);
void __sig_funlock(void);

#define __vforked (__tls_enabled && (__get_tls()->tib_flags & TIB_FLAG_VFORKED))

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_CALLS_STATE_INTERNAL_H_ */
