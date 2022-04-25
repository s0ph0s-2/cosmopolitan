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
#include "libc/bits/bits.h"
#include "libc/bits/xchg.internal.h"
#include "libc/calls/calls.h"
#include "libc/dce.h"
#include "libc/fmt/fmt.h"
#include "libc/intrin/kprintf.h"
#include "libc/linux/mmap.h"
#include "libc/linux/munmap.h"
#include "libc/log/log.h"
#include "libc/mem/mem.h"
#include "libc/rand/rand.h"
#include "libc/runtime/gc.internal.h"
#include "libc/runtime/memtrack.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/msync.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/prot.h"
#include "libc/testlib/ezbench.h"
#include "libc/testlib/testlib.h"
#include "libc/x/x.h"

char testlib_enable_tmp_setup_teardown;

TEST(mmap, testMapFile) {
  int fd;
  char *p;
  char path[PATH_MAX];
  sprintf(path, "%s%s.%ld", kTmpPath, program_invocation_short_name, lemur64());
  ASSERT_NE(-1, (fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644)));
  EXPECT_EQ(5, write(fd, "hello", 5));
  EXPECT_NE(-1, fdatasync(fd));
  EXPECT_NE(MAP_FAILED, (p = mmap(NULL, 5, PROT_READ, MAP_PRIVATE, fd, 0)));
  EXPECT_STREQN("hello", p, 5);
  EXPECT_NE(-1, munmap(p, 5));
  EXPECT_NE(-1, close(fd));
  EXPECT_NE(-1, unlink(path));
}

TEST(mmap, testMapFile_fdGetsClosed_makesNoDifference) {
  int fd;
  char *p, buf[16], path[PATH_MAX];
  sprintf(path, "%s%s.%ld", kTmpPath, program_invocation_short_name, lemur64());
  ASSERT_NE(-1, (fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644)));
  EXPECT_EQ(5, write(fd, "hello", 5));
  EXPECT_NE(-1, fdatasync(fd));
  EXPECT_NE(MAP_FAILED,
            (p = mmap(NULL, 5, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)));
  EXPECT_NE(-1, close(fd));
  EXPECT_STREQN("hello", p, 5);
  p[1] = 'a';
  EXPECT_NE(-1, msync(p, PAGESIZE, MS_SYNC));
  ASSERT_NE(-1, (fd = open(path, O_RDONLY)));
  EXPECT_EQ(5, read(fd, buf, 5));
  EXPECT_STREQN("hallo", buf, 5);
  EXPECT_NE(-1, close(fd));
  EXPECT_NE(-1, munmap(p, 5));
  EXPECT_NE(-1, unlink(path));
}

TEST(mmap, testMapFixed_destroysEverythingInItsPath) {
  unsigned m1 = _mmi.i;
  EXPECT_NE(MAP_FAILED, mmap((void *)(kFixedmapStart + FRAMESIZE * 0),
                             FRAMESIZE, PROT_READ | PROT_WRITE,
                             MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  EXPECT_NE(MAP_FAILED, mmap((void *)(kFixedmapStart + FRAMESIZE * 1),
                             FRAMESIZE, PROT_READ | PROT_WRITE,
                             MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  EXPECT_NE(MAP_FAILED, mmap((void *)(kFixedmapStart + FRAMESIZE * 2),
                             FRAMESIZE, PROT_READ | PROT_WRITE,
                             MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  EXPECT_NE(MAP_FAILED, mmap((void *)(kFixedmapStart + FRAMESIZE * 0),
                             FRAMESIZE * 3, PROT_READ | PROT_WRITE,
                             MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  ASSERT_GT(_mmi.i, m1);
  EXPECT_NE(-1, munmap((void *)kFixedmapStart, FRAMESIZE * 3));
}

TEST(mmap, customStackMemory_isAuthorized) {
  char *stack;
  uintptr_t w, r;
  ASSERT_NE(MAP_FAILED,
            (stack = mmap(NULL, STACKSIZE, PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_PRIVATE | MAP_GROWSDOWN, -1, 0)));
  asm("mov\t%%rsp,%0\n\t"
      "mov\t%2,%%rsp\n\t"
      "push\t%3\n\t"
      "pop\t%1\n\t"
      "mov\t%0,%%rsp"
      : "=&r"(w), "=&r"(r)
      : "rm"(stack + STACKSIZE - 8), "i"(123));
  ASSERT_EQ(123, r);
}

TEST(mmap, fileOffset) {
  int fd;
  char *map;
  ASSERT_NE(-1, (fd = open("foo", O_CREAT | O_RDWR, 0644)));
  EXPECT_NE(-1, ftruncate(fd, FRAMESIZE * 2));
  EXPECT_NE(-1, pwrite(fd, "hello", 5, FRAMESIZE * 0));
  EXPECT_NE(-1, pwrite(fd, "there", 5, FRAMESIZE * 1));
  EXPECT_NE(-1, fdatasync(fd));
  ASSERT_NE(MAP_FAILED, (map = mmap(NULL, FRAMESIZE, PROT_READ, MAP_PRIVATE, fd,
                                    FRAMESIZE)));
  EXPECT_EQ(0, memcmp(map, "there", 5), "%#.*s", 5, map);
  EXPECT_NE(-1, munmap(map, FRAMESIZE));
  EXPECT_NE(-1, close(fd));
}

TEST(mmap, mapPrivate_writesDontChangeFile) {
  int fd;
  char *map, buf[6];
  ASSERT_NE(-1, (fd = open("bar", O_CREAT | O_RDWR, 0644)));
  EXPECT_NE(-1, ftruncate(fd, FRAMESIZE));
  EXPECT_NE(-1, pwrite(fd, "hello", 5, 0));
  ASSERT_NE(MAP_FAILED, (map = mmap(NULL, FRAMESIZE, PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE, fd, 0)));
  memcpy(map, "there", 5);
  EXPECT_NE(-1, msync(map, FRAMESIZE, MS_SYNC));
  EXPECT_NE(-1, munmap(map, FRAMESIZE));
  EXPECT_NE(-1, pread(fd, buf, 6, 0));
  EXPECT_EQ(0, memcmp(buf, "hello", 5), "%#.*s", 5, buf);
  EXPECT_NE(-1, close(fd));
}

TEST(isheap, nullPtr) {
  ASSERT_FALSE(_isheap(NULL));
}

TEST(isheap, malloc) {
  ASSERT_TRUE(_isheap(gc(malloc(1))));
}

TEST(isheap, emptyMalloc) {
  ASSERT_TRUE(_isheap(gc(malloc(0))));
}

TEST(isheap, mallocOffset) {
  char *p = gc(malloc(131072));
  ASSERT_TRUE(_isheap(p + 100000));
}

////////////////////////////////////////////////////////////////////////////////
// NON-SHARED READ-ONLY FILE MEMORY

TEST(mmap, cow) {
  int fd;
  char *p;
  char path[PATH_MAX];
  sprintf(path, "%s%s.%ld", kTmpPath, program_invocation_short_name, lemur64());
  ASSERT_NE(-1, (fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644)));
  EXPECT_EQ(5, write(fd, "hello", 5));
  EXPECT_NE(-1, fdatasync(fd));
  EXPECT_NE(MAP_FAILED,
            (p = mmap(NULL, 5, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)));
  EXPECT_STREQN("hello", p, 5);
  EXPECT_NE(-1, munmap(p, 5));
  EXPECT_NE(-1, close(fd));
  EXPECT_NE(-1, unlink(path));
}

////////////////////////////////////////////////////////////////////////////////
// NON-SHARED READ-ONLY FILE MEMORY BETWEEN PROCESSES

TEST(mmap, cowFileMapReadonlyFork) {
  char *p;
  int fd, pid, ws;
  char path[PATH_MAX], lol[6];
  sprintf(path, "%s%s.%ld", kTmpPath, program_invocation_short_name, lemur64());
  ASSERT_NE(-1, (fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644)));
  EXPECT_EQ(6, write(fd, "hello", 6));
  EXPECT_NE(-1, close(fd));
  ASSERT_NE(-1, (fd = open(path, O_RDONLY)));
  EXPECT_NE(MAP_FAILED, (p = mmap(NULL, 6, PROT_READ, MAP_PRIVATE, fd, 0)));
  EXPECT_STREQN("hello", p, 5);
  ASSERT_NE(-1, (ws = xspawn(0)));
  if (ws == -2) {
    ASSERT_STREQN("hello", p, 5);
    _exit(0);
  }
  EXPECT_STREQN("hello", p, 5);
  EXPECT_NE(-1, munmap(p, 6));
  EXPECT_NE(-1, close(fd));
  EXPECT_NE(-1, unlink(path));
}

////////////////////////////////////////////////////////////////////////////////
// NON-SHARED READ/WRITE FILE MEMORY BETWEEN PROCESSES

TEST(mmap, cowFileMapFork) {
  char *p;
  int fd, pid, ws;
  char path[PATH_MAX], lol[6];
  sprintf(path, "%s%s.%ld", kTmpPath, program_invocation_short_name, lemur64());
  ASSERT_NE(-1, (fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644)));
  EXPECT_EQ(6, write(fd, "parnt", 6));
  EXPECT_NE(-1, fdatasync(fd));
  EXPECT_NE(MAP_FAILED,
            (p = mmap(NULL, 6, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)));
  EXPECT_STREQN("parnt", p, 5);
  ASSERT_NE(-1, (ws = xspawn(0)));
  if (ws == -2) {
    ASSERT_STREQN("parnt", p, 5);
    strcpy(p, "child");
    ASSERT_STREQN("child", p, 5);
    _exit(0);
  }
  EXPECT_STREQN("parnt", p, 5);  // child changing memory did not change parent
  EXPECT_EQ(6, pread(fd, lol, 6, 0));
  EXPECT_STREQN("parnt", lol, 5);  // changing memory did not change file
  EXPECT_NE(-1, munmap(p, 6));
  EXPECT_NE(-1, close(fd));
  EXPECT_NE(-1, unlink(path));
}

////////////////////////////////////////////////////////////////////////////////
// SHARED ANONYMOUS MEMORY BETWEEN PROCESSES

TEST(mmap, sharedAnonMapFork) {
  char *p;
  int pid, ws;
  EXPECT_NE(MAP_FAILED, (p = mmap(NULL, 6, PROT_READ | PROT_WRITE,
                                  MAP_SHARED | MAP_ANONYMOUS, -1, 0)));
  strcpy(p, "parnt");
  EXPECT_STREQN("parnt", p, 5);
  ASSERT_NE(-1, (ws = xspawn(0)));
  if (ws == -2) {
    ASSERT_STREQN("parnt", p, 5);
    strcpy(p, "child");
    ASSERT_STREQN("child", p, 5);
    _exit(0);
  }
  EXPECT_STREQN("child", p, 5);  // boom
  EXPECT_NE(-1, munmap(p, 5));
}

////////////////////////////////////////////////////////////////////////////////
// SHARED FILE MEMORY BETWEEN PROCESSES

TEST(mmap, sharedFileMapFork) {
  char *p;
  int fd, pid, ws;
  char path[PATH_MAX], lol[6];
  sprintf(path, "%s%s.%ld", kTmpPath, program_invocation_short_name, lemur64());
  ASSERT_NE(-1, (fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644)));
  EXPECT_EQ(6, write(fd, "parnt", 6));
  EXPECT_NE(-1, fdatasync(fd));
  EXPECT_NE(MAP_FAILED,
            (p = mmap(NULL, 6, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)));
  EXPECT_STREQN("parnt", p, 5);
  ASSERT_NE(-1, (ws = xspawn(0)));
  if (ws == -2) {
    ASSERT_STREQN("parnt", p, 5);
    strcpy(p, "child");
    ASSERT_STREQN("child", p, 5);
    ASSERT_NE(-1, msync(p, 6, MS_SYNC | MS_INVALIDATE));
    _exit(0);
  }
  EXPECT_STREQN("child", p, 5);  // child changing memory changed parent memory
  // XXX: RHEL5 has a weird issue where if we read the file into its own
  //      shared memory then corruption occurs!
  EXPECT_EQ(6, pread(fd, lol, 6, 0));
  EXPECT_STREQN("child", lol, 5);  // changing memory changed file
  EXPECT_NE(-1, munmap(p, 6));
  EXPECT_NE(-1, close(fd));
  EXPECT_NE(-1, unlink(path));
}

////////////////////////////////////////////////////////////////////////////////
// BENCHMARKS

#define N (EZBENCH_COUNT * EZBENCH_TRIES)

int count;
void *ptrs[N];

void BenchUnmap(void) {
  int i;
  for (i = 0; i < count; ++i) {
    if (ptrs[i]) {
      ASSERT_EQ(0, munmap(ptrs[i], FRAMESIZE));
    }
  }
}

void BenchMmapPrivate(void) {
  void *p;
  p = mmap(0, FRAMESIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE,
           -1, 0);
  if (p == MAP_FAILED) abort();
  ptrs[count++] = p;
}

BENCH(mmap, bench) {
  EZBENCH2("mmap", donothing, BenchMmapPrivate());
  BenchUnmap();
}

void BenchUnmapLinux(void) {
  int i;
  for (i = 0; i < count; ++i) {
    if (ptrs[i]) {
      ASSERT_EQ(0, LinuxMunmap(ptrs[i], FRAMESIZE));
    }
  }
}

void BenchMmapPrivateLinux(void) {
  void *p;
  p = (void *)LinuxMmap(0, FRAMESIZE, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (p == MAP_FAILED) abort();
  ptrs[count++] = p;
}

BENCH(mmap, benchLinux) {
  void *p;
  if (!IsLinux()) return;
  EZBENCH2("mmap (linux)", donothing, BenchMmapPrivateLinux());
  BenchUnmapLinux();
}
