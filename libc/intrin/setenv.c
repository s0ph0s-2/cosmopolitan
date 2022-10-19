/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
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
#include "libc/intrin/kmalloc.h"
#include "libc/intrin/strace.internal.h"
#include "libc/mem/internal.h"
#include "libc/runtime/runtime.h"
#include "libc/str/str.h"
#include "libc/sysv/errfuns.h"

/**
 * Copies variable to environment.
 *
 * @return 0 on success, or -1 w/ errno and environment is unchanged
 * @raise EINVAL if `name` is empty or contains `'='`
 * @raise ENOMEM if we require more vespene gas
 * @see putenv(), getenv()
 */
int setenv(const char *name, const char *value, int overwrite) {
  int rc;
  char *s;
  size_t n, m;
  const char *t;
  if (!name || !*name || !value) return einval();
  for (t = name; *t; ++t) {
    if (*t == '=') return einval();
  }
  n = strlen(name);
  m = strlen(value);
  if (!(s = kmalloc(n + 1 + m + 1))) return -1;
  memcpy(mempcpy(mempcpy(s, name, n), "=", 1), value, m + 1);
  rc = PutEnvImpl(s, overwrite);
  STRACE("setenv(%#s, %#s, %d) → %d% m", name, value, overwrite, rc);
  return rc;
}
