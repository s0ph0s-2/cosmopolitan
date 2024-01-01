/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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
#include "libc/calls/metalfile.internal.h"
#include "libc/calls/syscall-sysv.internal.h"
#include "libc/dce.h"
#include "libc/limits.h"
#include "libc/runtime/runtime.h"
#include "libc/serialize.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/ok.h"
#include "libc/testlib/ezbench.h"
#include "libc/testlib/subprocess.h"
#include "libc/testlib/testlib.h"

static char *self;
static bool skiptests;

void SetUpOnce(void) {
  self = GetProgramExecutableName();
  testlib_enable_tmp_setup_teardown();
  if (IsMetal()) {
    skiptests = true;
  } else if (IsOpenbsd() || (IsXnu() && !IsXnuSilicon())) {
    ASSERT_STRNE(self, "");
    ASSERT_SYS(0, 3, open(self, O_RDONLY));
    char buf[8];
    ASSERT_SYS(0, 8, pread(3, buf, 8, 0));
    ASSERT_SYS(0, 0, close(3));
    if (READ64LE(buf) != READ64LE("MZqFpD='") &&
        READ64LE(buf) != READ64LE("jartsr='") &&
        READ64LE(buf) != READ64LE("APEDBG='")) {
      fprintf(stderr,
              "we appear to be running as an assimilated binary on OpenBSD or "
              "x86_64 XNU;\nGetProgramExecutableName is unreliable here\n");
      skiptests = true;
    }
  }
}

__attribute__((__constructor__)) static void Child(int argc, char *argv[]) {
  if (argc >= 2 && !strcmp(argv[1], "Child")) {
    if (sys_chdir("/")) {
      exit(122);
    }
    if (strcmp(argv[2], GetProgramExecutableName())) {
      exit(123);
    }
    if (!IsXnuSilicon()) exit(0);
    /* TODO(mrdomino): argv[0] tests only pass on XnuSilicon right now because
                       __sys_execve fails there, so the ape loader is used.
                       the correct check is "we have been invoked either as an
                       assimilated binary or via the ape loader, and not via a
                       raw __sys_execve." */
    if (strcmp(argv[3], argv[0])) {
      exit(124);
    }
    exit(0);
  }
}

TEST(GetProgramExecutableName, ofThisFile) {
  if (IsMetal()) {
    EXPECT_STREQ(self, APE_COM_NAME);
  } else {
    EXPECT_EQ('/', *self);
    EXPECT_TRUE(!!strstr(self, "getprogramexecutablename_test"));
    EXPECT_SYS(0, 0, access(self, X_OK));
  }
}

TEST(GetProgramExecutableName, nullEnv) {
  if (skiptests) return;
  SPAWN(fork);
  execve(self, (char *[]){self, "Child", self, self, 0}, (char *[]){0});
  abort();
  EXITS(0);
}

TEST(GetProramExecutableName, weirdArgv0NullEnv) {
  if (skiptests) return;
  SPAWN(fork);
  execve(self, (char *[]){"hello", "Child", self, "hello", 0}, (char *[]){0});
  abort();
  EXITS(0);
}

TEST(GetProgramExecutableName, weirdArgv0CosmoVar) {
  if (skiptests) return;
  char buf[32 + PATH_MAX];
  stpcpy(stpcpy(buf, "COSMOPOLITAN_PROGRAM_EXECUTABLE="), self);
  SPAWN(fork);
  execve(self, (char *[]){"hello", "Child", self, "hello", 0},
         (char *[]){buf, 0});
  abort();
  EXITS(0);
}

TEST(GetProgramExecutableName, weirdArgv0WrongCosmoVar) {
  if (skiptests) return;
  char *bad = "COSMOPOLITAN_PROGRAM_EXECUTABLE=hi";
  SPAWN(fork);
  execve(self, (char *[]){"hello", "Child", self, "hello", 0},
         (char *[]){bad, 0});
  abort();
  EXITS(0);
}

TEST(GetProgramExecutableName, movedSelf) {
  if (skiptests) return;
  char buf[BUFSIZ];
  ASSERT_SYS(0, 3, open(GetProgramExecutableName(), O_RDONLY));
  ASSERT_SYS(0, 4, creat("test", 0755));
  ssize_t rc;
  while ((rc = read(3, buf, BUFSIZ)) > 0) {
    ASSERT_SYS(0, rc, write(4, buf, rc));
  }
  ASSERT_EQ(0, rc);
  ASSERT_SYS(0, 0, close(4));
  ASSERT_SYS(0, 0, close(3));
  ASSERT_NE(NULL, getcwd(buf, BUFSIZ - 5));
  stpcpy(buf + strlen(buf), "/test");
  SPAWN(fork);
  execve(buf, (char *[]){"hello", "Child", buf, "hello", 0}, (char *[]){0});
  abort();
  EXITS(0);
  SPAWN(fork);
  execve("./test", (char *[]){"hello", "Child", buf, "hello", 0},
         (char *[]){0});
  abort();
  EXITS(0);
}
