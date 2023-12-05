/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
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
#include "libc/calls/calls.h"
#include "libc/cxxabi.h"
#include "libc/dce.h"
#include "libc/dns/hoststxt.h"
#include "libc/dns/servicestxt.h"
#include "libc/intrin/pushpop.internal.h"
#include "libc/intrin/safemacros.internal.h"
#include "libc/macros.internal.h"
#include "libc/mem/mem.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/thread/thread.h"

static struct HostsTxt *g_hoststxt;
static struct HostsTxtInitialStaticMemory {
  struct HostsTxt ht;
  pthread_mutex_t lock;
  struct HostsTxtEntry entries[8];
  char strings[64];
} g_hoststxt_init;

static const char *GetHostsTxtPath(char *path, size_t size) {
  if (!IsWindows()) {
    return "/etc/hosts";
  } else {
    return GetSystemDirectoryPath(path, size, "drivers\\etc\\hosts");
  }
}

/**
 * Returns hosts.txt map.
 *
 * @note yoinking realloc() ensures there's no size limits
 */
const struct HostsTxt *GetHostsTxt(void) {
  FILE *f;
  char pathbuf[256];
  struct HostsTxtInitialStaticMemory *init;
  init = &g_hoststxt_init;
  pthread_mutex_lock(&init->lock);
  if (!g_hoststxt) {
    g_hoststxt = &init->ht;
    init->ht.entries.n = pushpop(ARRAYLEN(init->entries));
    init->ht.entries.p = init->entries;
    init->ht.strings.n = pushpop(ARRAYLEN(init->strings));
    init->ht.strings.p = init->strings;
    __cxa_atexit((void *)FreeHostsTxt, &g_hoststxt, NULL);
    if ((f = fopen(GetHostsTxtPath(pathbuf, sizeof(pathbuf)), "r"))) {
      if (ParseHostsTxt(g_hoststxt, f) == -1) {
        /* TODO(jart): Elevate robustness. */
      }
      fclose(f);
    }
  }
  pthread_mutex_unlock(&init->lock);
  return g_hoststxt;
}

/**
 * Frees HOSTS.TXT data structure populated by ParseHostsTxt().
 */
void FreeHostsTxt(struct HostsTxt **ht) {
  if (*ht) {
    if ((*ht)->entries.p != g_hoststxt_init.entries) {
      free((*ht)->entries.p);
    }
    if ((*ht)->strings.p != g_hoststxt_init.strings) {
      free((*ht)->strings.p);
    }
    if (*ht != &g_hoststxt_init.ht) {
      free(*ht);
    }
    *ht = 0;
  }
}
