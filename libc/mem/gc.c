/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2021 Justine Alexandra Roberts Tunney                              │
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
#include "libc/intrin/likely.h"
#include "libc/mem/mem.h"
#include "libc/nexgen32e/gc.internal.h"
#include "libc/nexgen32e/stackframe.h"
#include "libc/runtime/runtime.h"
#include "libc/str/str.h"
#include "libc/thread/tls.h"

forceinline bool PointerNotOwnedByParentStackFrame(struct StackFrame *frame,
                                                   struct StackFrame *parent,
                                                   void *ptr) {
  return !(((intptr_t)ptr > (intptr_t)frame) &&
           ((intptr_t)ptr < (intptr_t)parent));
}

static void TeardownGc(void) {
  int i;
  struct Garbages *g;
  struct CosmoTib *t;
  if (__tls_enabled) {
    t = __get_tls();
    if ((g = t->tib_garbages)) {
      // exit() currently doesn't use _gclongjmp() like pthread_exit()
      // so we need to run the deferred functions manually.
      while (g->i) {
        --g->i;
        ((void (*)(intptr_t))g->p[g->i].fn)(g->p[g->i].arg);
      }
      free(g->p);
      free(g);
    }
  }
}

__attribute__((__constructor__)) static void InitializeGc(void) {
  atexit(TeardownGc);
}

// add item to garbage shadow stack.
// then rewrite caller's return address on stack.
static void DeferFunction(struct StackFrame *frame, void *fn, void *arg) {
  int n2;
  struct Garbage *p2;
  struct Garbages *g;
  struct CosmoTib *t;
  __require_tls();
  t = __get_tls();
  g = t->tib_garbages;
  if (UNLIKELY(!g)) {
    if (!(g = malloc(sizeof(struct Garbages)))) notpossible;
    g->i = 0;
    g->n = 4;
    if (!(g->p = malloc(g->n * sizeof(struct Garbage)))) notpossible;
    t->tib_garbages = g;
  } else if (UNLIKELY(g->i == g->n)) {
    p2 = g->p;
    n2 = g->n + (g->n >> 1);
    if ((p2 = realloc(p2, n2 * sizeof(*p2)))) {
      g->p = p2;
      g->n = n2;
    } else {
      notpossible;
    }
  }
  g->p[g->i].frame = frame;
  g->p[g->i].fn = (intptr_t)fn;
  g->p[g->i].arg = (intptr_t)arg;
  g->p[g->i].ret = frame->addr;
  g->i++;
  frame->addr = (intptr_t)__gc;
}

// the gnu extension macros for _gc / _defer point here
void __defer(void *rbp, void *fn, void *arg) {
  struct StackFrame *f, *frame = rbp;
  f = __builtin_frame_address(0);
  _unassert(f->next == frame);
  _unassert(PointerNotOwnedByParentStackFrame(f, frame, arg));
  DeferFunction(frame, fn, arg);
}

/**
 * Frees memory when function returns.
 *
 * This garbage collector overwrites the return address on the stack so
 * that the RET instruction calls a trampoline which calls free(). It's
 * loosely analogous to Go's defer keyword rather than a true cycle gc.
 *
 *     const char *s = _gc(strdup("hello"));
 *     puts(s);
 *
 * This macro is equivalent to:
 *
 *      _defer(free, ptr)
 *
 * @warning do not return a gc()'d pointer
 * @warning do not realloc() with gc()'d pointer
 * @warning be careful about static keyword due to impact of inlining
 * @note you should use -fno-omit-frame-pointer
 * @threadsafe
 */
void *(_gc)(void *thing) {
  struct StackFrame *frame;
  frame = __builtin_frame_address(0);
  DeferFunction(frame->next, _weakfree, thing);
  return thing;
}

/**
 * Calls fn(arg) when function returns.
 *
 * This garbage collector overwrites the return address on the stack so
 * that the RET instruction calls a trampoline which calls free(). It's
 * loosely analogous to Go's defer keyword rather than a true cycle gc.
 *
 * @warning do not return a gc()'d pointer
 * @warning do not realloc() with gc()'d pointer
 * @warning be careful about static keyword due to impact of inlining
 * @note you should use -fno-omit-frame-pointer
 * @threadsafe
 */
void *(_defer)(void *fn, void *arg) {
  struct StackFrame *frame;
  frame = __builtin_frame_address(0);
  DeferFunction(frame->next, fn, arg);
  return arg;
}
