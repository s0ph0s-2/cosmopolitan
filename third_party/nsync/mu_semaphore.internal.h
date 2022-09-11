#ifndef NSYNC_MU_SEMAPHORE_INTERNAL_H_
#define NSYNC_MU_SEMAPHORE_INTERNAL_H_
#include "third_party/nsync/mu_semaphore.h"
#include "third_party/nsync/time.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

void nsync_mu_semaphore_init_futex(nsync_semaphore *);
void nsync_mu_semaphore_p_futex(nsync_semaphore *);
int nsync_mu_semaphore_p_with_deadline_futex(nsync_semaphore *, nsync_time);
void nsync_mu_semaphore_v_futex(nsync_semaphore *);

void nsync_mu_semaphore_init_win32(nsync_semaphore *);
void nsync_mu_semaphore_p_win32(nsync_semaphore *);
int nsync_mu_semaphore_p_with_deadline_win32(nsync_semaphore *, nsync_time);
void nsync_mu_semaphore_v_win32(nsync_semaphore *);

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* NSYNC_MU_SEMAPHORE_INTERNAL_H_ */
