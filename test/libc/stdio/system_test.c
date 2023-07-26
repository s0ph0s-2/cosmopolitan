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
#include "libc/calls/calls.h"
#include "libc/dce.h"
#include "libc/mem/gc.h"
#include "libc/mem/gc.internal.h"
#include "libc/paths.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/stdio.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/sig.h"
#include "libc/testlib/ezbench.h"
#include "libc/testlib/testlib.h"
#include "libc/x/x.h"
#ifdef __x86_64__

__static_yoink("_tr");
__static_yoink("glob");

char testlib_enable_tmp_setup_teardown;

void SetUp(void) {
  if (IsWindows()) {
    fprintf(stderr,
            "TODO(jart): Why does system_test have issues on Windows when "
            "running as a subprocess of something like runitd.com?\n");
    exit(0);
  }
}

TEST(system, haveShell) {
  ASSERT_TRUE(system(0));
}

TEST(system, echo) {
  ASSERT_EQ(0, system("echo hello >\"hello there.txt\""));
  EXPECT_STREQ("hello\n", _gc(xslurp("hello there.txt", 0)));
}

TEST(system, exit) {
  ASSERT_EQ(123, WEXITSTATUS(system("exit 123")));
}

TEST(system, testStdoutRedirect) {
  testlib_extract("/zip/echo.com", "echo.com", 0755);
  ASSERT_EQ(0, system("./echo.com hello >hello.txt"));
  EXPECT_STREQ("hello\n", _gc(xslurp("hello.txt", 0)));
}

TEST(system, testStdoutRedirect_withSpacesInFilename) {
  testlib_extract("/zip/echo.com", "echo.com", 0755);
  ASSERT_EQ(0, system("./echo.com hello >\"hello there.txt\""));
  EXPECT_STREQ("hello\n", _gc(xslurp("hello there.txt", 0)));
}

TEST(system, testStderrRedirect_toStdout) {
  if (IsAsan()) return;  // TODO(jart): fix me
  int pipefd[2];
  int stdoutBack = dup(1);
  ASSERT_NE(-1, stdoutBack);
  ASSERT_EQ(0, pipe(pipefd));
  ASSERT_NE(-1, dup2(pipefd[1], 1));
  int stderrBack = dup(2);
  ASSERT_NE(-1, stderrBack);
  char buf[5] = {0};

  ASSERT_NE(-1, dup2(1, 2));
  bool success = false;
  if (WEXITSTATUS(system("echo aaa 2>&1")) == 0) {
    success = read(pipefd[0], buf, sizeof(buf) - 1) == (sizeof(buf) - 1);
  }
  ASSERT_NE(-1, dup2(stderrBack, 2));
  ASSERT_EQ(true, success);
  ASSERT_STREQ("aaa\n", buf);
  buf[0] = 0;
  buf[1] = 0;
  buf[2] = 0;
  buf[3] = 0;
  testlib_extract("/zip/echo.com", "echo.com", 0755);
  ASSERT_NE(-1, dup2(1, 2));
  success = false;
  if (WEXITSTATUS(system("./echo.com aaa 2>&1")) == 0) {
    success = read(pipefd[0], buf, sizeof(buf) - 1) == (sizeof(buf) - 1);
  }
  ASSERT_NE(-1, dup2(stderrBack, 2));
  ASSERT_EQ(true, success);
  ASSERT_STREQ("aaa\n", buf);

  ASSERT_NE(-1, dup2(stdoutBack, 1));
  ASSERT_EQ(0, close(pipefd[1]));
  ASSERT_EQ(0, close(pipefd[0]));
}

BENCH(system, bench) {
  testlib_extract("/zip/echo.com", "echo.com", 0755);
  EZBENCH2("system cmd", donothing, system("./echo.com hi >/dev/null"));
  EZBENCH2("cocmd echo", donothing, system("echo hi >/dev/null"));
  EZBENCH2("cocmd exit", donothing, system("exit"));
}

TEST(system, and) {
  ASSERT_EQ(1, WEXITSTATUS(system("false && false")));
  ASSERT_EQ(1, WEXITSTATUS(system("true&& false")));
  ASSERT_EQ(1, WEXITSTATUS(system("false &&true")));
  ASSERT_EQ(0, WEXITSTATUS(system("true&&true")));
}

TEST(system, or) {
  ASSERT_EQ(1, WEXITSTATUS(system("false || false")));
  ASSERT_EQ(0, WEXITSTATUS(system("true|| false")));
  ASSERT_EQ(0, WEXITSTATUS(system("false ||true")));
  ASSERT_EQ(0, WEXITSTATUS(system("true||true")));
}

TEST(system, async1) {
  ASSERT_EQ(123, WEXITSTATUS(system("exit 123 & "
                                    "wait $!")));
}

TEST(system, async4) {
  ASSERT_EQ(0, WEXITSTATUS(system("echo a >a & "
                                  "echo b >b & "
                                  "echo c >c & "
                                  "echo d >d & "
                                  "wait")));
  ASSERT_TRUE(fileexists("a"));
  ASSERT_TRUE(fileexists("b"));
  ASSERT_TRUE(fileexists("c"));
  ASSERT_TRUE(fileexists("d"));
}

TEST(system, equals) {
  setenv("var", "wand", true);
  ASSERT_EQ(0, WEXITSTATUS(system("test a = a")));
  ASSERT_EQ(1, WEXITSTATUS(system("test a = b")));
  ASSERT_EQ(0, WEXITSTATUS(system("test x$var = xwand")));
  ASSERT_EQ(0, WEXITSTATUS(system("[ a = a ]")));
  ASSERT_EQ(1, WEXITSTATUS(system("[ a = b ]")));
}

TEST(system, notequals) {
  ASSERT_EQ(1, WEXITSTATUS(system("test a != a")));
  ASSERT_EQ(0, WEXITSTATUS(system("test a != b")));
}

TEST(system, usleep) {
  ASSERT_EQ(0, WEXITSTATUS(system("usleep & kill $!")));
}

TEST(system, kill) {
  int ws = system("kill -TERM $$; usleep");
  if (!IsWindows()) ASSERT_EQ(SIGTERM, WTERMSIG(ws));
}

TEST(system, exitStatusPreservedAfterSemiColon) {
  testlib_extract("/zip/false.com", "false.com", 0755);
  ASSERT_EQ(1, WEXITSTATUS(system("false;")));
  ASSERT_EQ(1, WEXITSTATUS(system("false; ")));
  ASSERT_EQ(1, WEXITSTATUS(system("./false.com;")));
  ASSERT_EQ(1, WEXITSTATUS(system("./false.com;")));
  int pipefd[2];
  int stdoutBack = dup(1);
  ASSERT_NE(-1, stdoutBack);
  ASSERT_EQ(0, pipe(pipefd));
  ASSERT_NE(-1, dup2(pipefd[1], 1));
  ASSERT_EQ(0, WEXITSTATUS(system("false; echo $?")));
  char buf[3] = {0};
  ASSERT_EQ(2, read(pipefd[0], buf, 2));
  ASSERT_STREQ("1\n", buf);
  ASSERT_EQ(0, WEXITSTATUS(system("./false.com; echo $?")));
  buf[0] = 0;
  buf[1] = 0;
  ASSERT_EQ(2, read(pipefd[0], buf, 2));
  ASSERT_STREQ("1\n", buf);
  ASSERT_NE(-1, dup2(stdoutBack, 1));
  ASSERT_EQ(0, close(pipefd[1]));
  ASSERT_EQ(0, close(pipefd[0]));
}

TEST(system, allowsLoneCloseCurlyBrace) {
  int pipefd[2];
  int stdoutBack = dup(1);
  ASSERT_NE(-1, stdoutBack);
  ASSERT_EQ(0, pipe(pipefd));
  ASSERT_NE(-1, dup2(pipefd[1], 1));
  char buf[6] = {0};

  ASSERT_EQ(0, WEXITSTATUS(system("echo \"aaa\"}")));
  ASSERT_EQ(sizeof(buf) - 1, read(pipefd[0], buf, sizeof(buf) - 1));
  ASSERT_STREQ("aaa}\n", buf);
  buf[0] = 0;
  buf[1] = 0;
  buf[2] = 0;
  buf[3] = 0;
  buf[4] = 0;
  testlib_extract("/zip/echo.com", "echo.com", 0755);
  ASSERT_EQ(0, WEXITSTATUS(system("./echo.com \"aaa\"}")));
  ASSERT_EQ(sizeof(buf) - 1, read(pipefd[0], buf, sizeof(buf) - 1));
  ASSERT_STREQ("aaa}\n", buf);

  ASSERT_NE(-1, dup2(stdoutBack, 1));
  ASSERT_EQ(0, close(pipefd[1]));
  ASSERT_EQ(0, close(pipefd[0]));
}

TEST(system, glob) {
  testlib_extract("/zip/echo.com", "echo.com", 0755);
  ASSERT_EQ(0, system("./ec*.com aaa"));
  ASSERT_EQ(0, system("./ec?o.com aaa"));
}

TEST(system, env) {
  ASSERT_EQ(0, system("env - a=b c=d >res"));
  ASSERT_STREQ("a=b\nc=d\n", gc(xslurp("res", 0)));
  ASSERT_EQ(0, system("env -i -0 a=b c=d >res"));
  ASSERT_STREQN("a=b\0c=d\0", gc(xslurp("res", 0)), 8);
  ASSERT_EQ(0, system("env -i0 a=b c=d >res"));
  ASSERT_STREQN("a=b\0c=d\0", gc(xslurp("res", 0)), 8);
  ASSERT_EQ(0, system("env - a=b c=d -u a z=g >res"));
  ASSERT_STREQ("c=d\nz=g\n", gc(xslurp("res", 0)));
  ASSERT_EQ(0, system("env - a=b c=d -ua z=g >res"));
  ASSERT_STREQ("c=d\nz=g\n", gc(xslurp("res", 0)));
  ASSERT_EQ(0, system("env - dope='show' >res"));
  ASSERT_STREQ("dope=show\n", gc(xslurp("res", 0)));
}

TEST(system, tr) {
  ASSERT_EQ(0, system("echo hello | tr a-z A-Z >res"));
  ASSERT_STREQ("HELLO\n", gc(xslurp("res", 0)));
}

#endif /* __x86_64__ */
