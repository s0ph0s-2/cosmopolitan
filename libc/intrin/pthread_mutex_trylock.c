/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/assert.h"
#include "libc/calls/calls.h"
#include "libc/errno.h"
#include "libc/intrin/atomic.h"
#include "libc/intrin/likely.h"
#include "libc/intrin/weaken.h"
#include "libc/thread/thread.h"
#include "libc/thread/tls.h"
#include "third_party/nsync/mu.h"

/**
 * Locks mutex if it isn't locked already.
 *
 * Unlike pthread_mutex_lock() this function won't block and instead
 * returns an error immediately if the lock couldn't be acquired.
 *
 * @return 0 on success, or errno on error
 * @raise EBUSY if lock is already held
 * @raise ENOTRECOVERABLE if `mutex` is corrupted
 */
int pthread_mutex_trylock(pthread_mutex_t *mutex) {
  int c, d, t;

  if (LIKELY(__tls_enabled &&                               //
             mutex->_type == PTHREAD_MUTEX_NORMAL &&        //
             mutex->_pshared == PTHREAD_PROCESS_PRIVATE &&  //
             weaken(nsync_mu_trylock))) {
    if (weaken(nsync_mu_trylock)((nsync_mu *)mutex)) {
      return 0;
    } else {
      return EBUSY;
    }
  }

  if (mutex->_type == PTHREAD_MUTEX_NORMAL) {
    c = 0;
    if (atomic_compare_exchange_strong_explicit(
            &mutex->_lock, &c, 1, memory_order_acquire, memory_order_relaxed)) {
      return 0;
    } else {
      return EBUSY;
    }
  }

  c = 0;
  t = gettid();
  d = 0x10100000 | t;
  if (!atomic_compare_exchange_strong_explicit(
          &mutex->_lock, &c, d, memory_order_acquire, memory_order_relaxed)) {
    if ((c & 0x000fffff) != t || mutex->_type == PTHREAD_MUTEX_ERRORCHECK) {
      return EBUSY;
    }
    if ((c & 0x0ff00000) == 0x0ff00000) {
      return EAGAIN;
    }
    atomic_fetch_add_explicit(&mutex->_lock, 0x00100000, memory_order_relaxed);
  }

  return 0;
}
