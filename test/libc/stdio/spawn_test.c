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
#include "libc/calls/calls.h"
#include "libc/dce.h"
#include "libc/fmt/conv.h"
#include "libc/intrin/safemacros.internal.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/spawn.h"
#include "libc/stdio/stdio.h"
#include "libc/sysv/consts/auxv.h"
#include "libc/sysv/consts/o.h"
#include "libc/testlib/ezbench.h"
#include "libc/testlib/testlib.h"

char testlib_enable_tmp_setup_teardown;

__attribute__((__constructor__)) static void init(void) {
  switch (atoi(nulltoempty(getenv("THE_DOGE")))) {
    case 42:
      exit(42);
    default:
      break;
  }
}

TEST(spawn, test) {
  int rc, ws, pid;
  char *prog = GetProgramExecutableName();
  char *args[] = {program_invocation_name, NULL};
  char *envs[] = {"THE_DOGE=42", NULL};
  EXPECT_EQ(0, posix_spawn(&pid, prog, NULL, NULL, args, envs));
  EXPECT_NE(-1, waitpid(pid, &ws, 0));
  EXPECT_TRUE(WIFEXITED(ws));
  EXPECT_EQ(42, WEXITSTATUS(ws));
}

TEST(spawn, pipe) {
  char buf[10];
  int p[2], pid, status;
  const char *pn = "./echo.com";
  posix_spawn_file_actions_t fa;
  testlib_extract("/zip/echo.com", "echo.com", 0755);
  ASSERT_SYS(0, 0, pipe(p));
  ASSERT_EQ(0, posix_spawn_file_actions_init(&fa));
  ASSERT_EQ(0, posix_spawn_file_actions_addclose(&fa, p[0]));
  ASSERT_EQ(0, posix_spawn_file_actions_adddup2(&fa, p[1], 1));
  ASSERT_EQ(0, posix_spawn_file_actions_addclose(&fa, p[1]));
  ASSERT_EQ(0, posix_spawnp(&pid, pn, &fa, 0, (char *[]){pn, "hello", 0}, 0));
  ASSERT_SYS(0, 0, close(p[1]));
  ASSERT_SYS(0, pid, waitpid(pid, &status, 0));
  ASSERT_SYS(0, 6, read(p[0], buf, sizeof(buf)));
  ASSERT_SYS(0, 0, close(p[0]));
  ASSERT_EQ(0, posix_spawn_file_actions_destroy(&fa));
}

/*
 * BEST LINUX FORK+EXEC+EXIT+WAIT PERFORMANCE
 * The fastest it can go with  fork() is 40µs
 * The fastest it can go with vfork() is 26µs
 */

void BenchmarkProcessLifecycle(void) {
  int rc, ws, pid;
  char *prog = "/tmp/tiny64";
  char *args[] = {"tiny64", NULL};
  char *envs[] = {NULL};
  ASSERT_EQ(0, posix_spawn(&pid, prog, NULL, NULL, args, envs));
  ASSERT_NE(-1, waitpid(pid, &ws, 0));
  ASSERT_TRUE(WIFEXITED(ws));
  ASSERT_EQ(42, WEXITSTATUS(ws));
}

const char kTinyLinuxExit[128] = {
    0x7f, 0x45, 0x4c, 0x46, 0x02, 0x01, 0x01, 0x00,  // ⌂ELF☻☺☺ 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //         
    0x02, 0x00, 0x3e, 0x00, 0x01, 0x00, 0x00, 0x00,  // ☻ > ☺   
    0x78, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,  // x @     
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // @       
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //         
    0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x38, 0x00,  //     @ 8 
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // ☺       
    0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,  // ☺   ♣   
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //         
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,  //   @     
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,  //   @     
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Ç       
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Ç       
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //  ►      
    0x6a, 0x2a, 0x5f, 0x6a, 0x3c, 0x58, 0x0f, 0x05,  // j*_j<X☼♣
};

/* BENCH(spawn, bench) { */
/*   int fd; */
/*   if (IsLinux()) { */
/*     fd = open("/tmp/tiny64", O_CREAT | O_TRUNC | O_WRONLY, 0755); */
/*     write(fd, kTinyLinuxExit, 128); */
/*     close(fd); */
/*     EZBENCH2("spawn", donothing, BenchmarkProcessLifecycle()); */
/*     unlink("/tmp/tiny64"); */
/*   } */
/* } */
