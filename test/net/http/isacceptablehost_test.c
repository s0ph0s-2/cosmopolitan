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
#include "libc/testlib/ezbench.h"
#include "libc/testlib/testlib.h"
#include "net/http/http.h"

TEST(IsAcceptableHost, test) {
  EXPECT_TRUE(IsAcceptableHost("", -1));
  EXPECT_TRUE(IsAcceptableHost("0.0.0.0", -1));
  EXPECT_FALSE(IsAcceptableHost("1.2.3", -1));
  EXPECT_TRUE(IsAcceptableHost("1.2.3.4", -1));
  EXPECT_FALSE(IsAcceptableHost("1.2.3.4.5", -1));
  EXPECT_TRUE(IsAcceptableHost("1.2.3.4.5.arpa", -1));
  EXPECT_TRUE(IsAcceptableHost("255.255.255.255", -1));
  EXPECT_FALSE(IsAcceptableHost("255.255.255", -1));
  EXPECT_FALSE(IsAcceptableHost("256.255.255.255", -1));
  EXPECT_TRUE(IsAcceptableHost("hello.example", -1));
  EXPECT_FALSE(IsAcceptableHost("hello..example", -1));
  EXPECT_TRUE(IsAcceptableHost("hello", -1));
  EXPECT_FALSE(IsAcceptableHost("hello\177", -1));
  EXPECT_FALSE(IsAcceptableHost("hello.example\300\200", -1));
  EXPECT_FALSE(IsAcceptableHost(".", -1));
  EXPECT_FALSE(IsAcceptableHost(".e", -1));
  EXPECT_TRUE(IsAcceptableHost("e.", -1));
  EXPECT_FALSE(IsAcceptableHost(".hi.example", -1));
  EXPECT_FALSE(IsAcceptableHost("hi..example", -1));
  EXPECT_TRUE(IsAcceptableHost("hi-there.example", -1));
  EXPECT_TRUE(IsAcceptableHost("_there.example", -1));
  EXPECT_TRUE(IsAcceptableHost("-there.example", -1));
  EXPECT_TRUE(IsAcceptableHost("there-.example", -1));
  EXPECT_FALSE(IsAcceptableHost("ther#e.example", -1));
  EXPECT_TRUE(IsAcceptableHost("localhost", -1));
}

TEST(IsAcceptablePort, test) {
  EXPECT_TRUE(IsAcceptablePort("", -1));
  EXPECT_TRUE(IsAcceptablePort("0", -1));
  EXPECT_TRUE(IsAcceptablePort("65535", -1));
  EXPECT_FALSE(IsAcceptablePort("65536", -1));
  EXPECT_FALSE(IsAcceptablePort("-1", -1));
  EXPECT_FALSE(IsAcceptablePort("http", -1));
}

TEST(ParseIp, test) {
  EXPECT_EQ(-1, ParseIp("", -1));
  EXPECT_EQ(0x00000000, ParseIp("0.0.0.0", -1));
  EXPECT_EQ(0x01020304, ParseIp("1.2.3.4", -1));
  EXPECT_EQ(0x80020304, ParseIp("128.2.3.4", -1));
  EXPECT_EQ(0xFFFFFFFF, ParseIp("255.255.255.255", -1));
  EXPECT_EQ(0xcb007100, ParseIp("203.0.113.0", -1));
  EXPECT_EQ(0x00000000, ParseIp("...", -1)); /* meh */
  EXPECT_EQ(-1, ParseIp("128.2..3.4", -1));
  EXPECT_EQ(-1, ParseIp("1.2.3", -1));
  EXPECT_EQ(-1, ParseIp("256.255.255.255", -1));
  EXPECT_EQ(-1, ParseIp("1.2.3.4.5", -1));
  EXPECT_EQ(-1, ParseIp("1.2.3.4.5.arpa", -1));
  EXPECT_EQ(-1, ParseIp("255.255.255", -1));
  EXPECT_EQ(-1, ParseIp("hello", -1));
  EXPECT_EQ(-1, ParseIp("hello\177", -1));
  EXPECT_EQ(-1, ParseIp("hello.example\300\200", -1));
  EXPECT_EQ(-1, ParseIp(".", -1));
  EXPECT_EQ(-1, ParseIp(".e", -1));
  EXPECT_EQ(-1, ParseIp("e.", -1));
  EXPECT_EQ(-1, ParseIp(".hi.example", -1));
  EXPECT_EQ(-1, ParseIp("hi..example", -1));
  EXPECT_EQ(-1, ParseIp("hi-there.example", -1));
  EXPECT_EQ(-1, ParseIp("_there.example", -1));
  EXPECT_EQ(-1, ParseIp("-there.example", -1));
  EXPECT_EQ(-1, ParseIp("there-.example", -1));
  EXPECT_EQ(-1, ParseIp("ther#e.example", -1));
  EXPECT_EQ(-1, ParseIp("localhost", -1));
  EXPECT_EQ(-1, ParseIp("hello.example", -1));
  EXPECT_EQ(-1, ParseIp("hello..example", -1));
}

TEST(ParseForwarded, test) {
  uint32_t ip = 7;
  uint16_t port = 7;
  EXPECT_EQ(-1, ParseForwarded("", -1, &ip, &port));
  EXPECT_EQ(-1, ParseForwarded("0.0.0.0", -1, &ip, &port));
  EXPECT_EQ(-1, ParseForwarded("8.8.8.8", -1, &ip, &port));
  EXPECT_EQ(-1, ParseForwarded("[::1]:123", -1, &ip, &port));
  EXPECT_EQ(7, ip);
  EXPECT_EQ(7, port);
  EXPECT_EQ(0, ParseForwarded("0.0.0.1:123", -1, &ip, &port));
  EXPECT_EQ(0x00000001, ip);
  EXPECT_EQ(123, port);
  EXPECT_EQ(0, ParseForwarded("1.2.3.4:123", -1, &ip, &port));
  EXPECT_EQ(0x01020304, ip);
  EXPECT_EQ(123, port);
  EXPECT_EQ(0, ParseForwarded("128.2.3.4:123", -1, &ip, &port));
  EXPECT_EQ(0x80020304, ip);
  EXPECT_EQ(123, port);
  EXPECT_EQ(0, ParseForwarded("255.255.255.255:123", -1, &ip, &port));
  EXPECT_EQ(0xFFFFFFFF, ip);
  EXPECT_EQ(123, port);
  EXPECT_EQ(0, ParseForwarded("203.0.113.0:123", -1, &ip, &port));
  EXPECT_EQ(0xcb007100, ip);
  EXPECT_EQ(123, port);
  EXPECT_EQ(0, ParseForwarded("203.0.113.42:31337", -1, &ip, &port));
  EXPECT_EQ(-1, ParseForwarded("...:123", -1, &ip, &port));
  EXPECT_EQ(-1, ParseForwarded("203.0.113.0:123123123", -1, &ip, &port));
}

BENCH(IsAcceptableHost, bench) {
  uint32_t ip;
  uint16_t port;
  EZBENCH2("IsAcceptableHost 127.0.0.1", donothing,
           IsAcceptableHost("127.0.0.1", 9));
  EZBENCH2("IsAcceptablePort 80", donothing, IsAcceptablePort("80", 2));
  EZBENCH2("ParseForwarded 80", donothing,
           ParseForwarded("203.0.113.42:31337", 20, &ip, &port));
  EZBENCH2("IsAcceptableHost foo.example", donothing,
           IsAcceptableHost("foo.example:31337", 17));
}
