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
#include "libc/calls/ntspawn.h"
#include "libc/errno.h"
#include "libc/mem/gc.internal.h"
#include "libc/mem/mem.h"
#include "libc/str/str.h"
#include "libc/testlib/testlib.h"

char16_t cmdline[ARG_MAX / 2];

TEST(mkntcmdline, emptyArgvList_cantBeEmptyOnWindows) {
  char *argv[] = {"foo", NULL};
  EXPECT_NE(-1, mkntcmdline(cmdline, argv));
  EXPECT_STREQ(u"foo", cmdline);
}

TEST(mkntcmdline, emptyArgvListWithProg_isEmpty) {
  char *argv[] = {NULL};
  EXPECT_NE(-1, mkntcmdline(cmdline, argv));
  EXPECT_STREQ(u"", cmdline);
}

TEST(mkntcmdline, emptyArg_getsQuoted) {
  char *argv[] = {"", NULL};
  EXPECT_NE(-1, mkntcmdline(cmdline, argv));
  EXPECT_STREQ(u"\"\"", cmdline);
}

TEST(mkntcmdline, ignoranceIsBliss) {
  char *argv[] = {"echo", "hello", "world", NULL};
  EXPECT_NE(-1, mkntcmdline(cmdline, argv));
  EXPECT_STREQ(u"echo hello world", cmdline);
}

TEST(mkntcmdline, spaceInArgument_getQuotesWrappedAround) {
  char *argv[] = {"echo", "hello there", "world", NULL};
  EXPECT_NE(-1, mkntcmdline(cmdline, argv));
  EXPECT_STREQ(u"echo \"hello there\" world", cmdline);
}

TEST(mkntcmdline, justSlash) {
  char *argv[] = {"\\", NULL};
  EXPECT_NE(-1, mkntcmdline(cmdline, argv));
  EXPECT_STREQ(u"\\", cmdline);
}

TEST(mkntcmdline, testUnicode) {
  char *argv1[] = {
      gc(strdup("(╯°□°)╯")),
      gc(strdup("要依法治国是赞美那些谁是公义的和惩罚恶人。 - 韩非")),
      NULL,
  };
  EXPECT_NE(-1, mkntcmdline(cmdline, argv1));
  EXPECT_STREQ(u"(╯°□°)╯ \"要依法治国是赞美那些谁是公义的和惩罚恶人。 - 韩非\"",
               cmdline);
}

TEST(mkntcmdline, fixAsBestAsWeCanForNow1) {
  char *argv1[] = {
      "/C/WINDOWS/system32/cmd.exe",
      "/C",
      "more <\"/C/Users/jart/AppData/Local/Temp/tmplquaa_d6\"",
      NULL,
  };
  EXPECT_NE(-1, mkntcmdline(cmdline, argv1));
  EXPECT_STREQ(u"C:\\WINDOWS\\system32\\cmd.exe /C \"more <"
               u"\"\"\"C:/Users/jart/AppData/Local/Temp/tmplquaa_d6\"\"\"\"",
               cmdline);
}

TEST(mkntcmdline, fixAsBestAsWeCanForNow2) {
  char *argv1[] = {
      "/C/WINDOWS/system32/cmd.exe",
      "/C",
      "less /C/Users/jart/AppData/Local/Temp/tmplquaa_d6",
      NULL,
  };
  EXPECT_NE(-1, mkntcmdline(cmdline, argv1));
  EXPECT_STREQ(u"C:\\WINDOWS\\system32\\cmd.exe /C \"less "
               u"C:/Users/jart/AppData/Local/Temp/tmplquaa_d6\"",
               cmdline);
}

TEST(mkntcmdline, testWut) {
  char *argv[] = {"C:\\Users\\jart\\𝑟𝑒𝑑𝑏𝑒𝑎𝑛.com", "--strace", NULL};
  EXPECT_NE(-1, mkntcmdline(cmdline, argv));
  EXPECT_STREQ(u"C:\\Users\\jart\\𝑟𝑒𝑑𝑏𝑒𝑎𝑛.com --strace", cmdline);
}
