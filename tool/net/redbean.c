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
#include "libc/bits/popcnt.h"
#include "libc/bits/safemacros.internal.h"
#include "libc/calls/calls.h"
#include "libc/calls/struct/itimerval.h"
#include "libc/calls/struct/rusage.h"
#include "libc/calls/struct/stat.h"
#include "libc/errno.h"
#include "libc/fmt/conv.h"
#include "libc/fmt/itoa.h"
#include "libc/log/check.h"
#include "libc/log/log.h"
#include "libc/math.h"
#include "libc/mem/fmt.h"
#include "libc/nexgen32e/bsf.h"
#include "libc/nexgen32e/bsr.h"
#include "libc/nexgen32e/crc32.h"
#include "libc/runtime/clktck.h"
#include "libc/runtime/runtime.h"
#include "libc/sock/sock.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/af.h"
#include "libc/sysv/consts/auxv.h"
#include "libc/sysv/consts/ex.h"
#include "libc/sysv/consts/exit.h"
#include "libc/sysv/consts/inaddr.h"
#include "libc/sysv/consts/ipproto.h"
#include "libc/sysv/consts/itimer.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/msync.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/consts/rusage.h"
#include "libc/sysv/consts/shut.h"
#include "libc/sysv/consts/so.h"
#include "libc/sysv/consts/sock.h"
#include "libc/sysv/consts/sol.h"
#include "libc/sysv/consts/tcp.h"
#include "libc/sysv/consts/w.h"
#include "libc/time/time.h"
#include "libc/x/x.h"
#include "libc/zip.h"
#include "net/http/escape.h"
#include "net/http/http.h"
#include "net/http/ip.h"
#include "net/http/url.h"
#include "third_party/getopt/getopt.h"
#include "third_party/lua/lauxlib.h"
#include "third_party/lua/ltests.h"
#include "third_party/lua/lua.h"
#include "third_party/lua/lualib.h"
#include "third_party/regex/regex.h"
#include "third_party/zlib/zlib.h"

#define HASH_LOAD_FACTOR /* 1. / */ 4
#define DEFAULT_PORT     8080

#define read(F, P, N)   readv(F, &(struct iovec){P, N}, 1)
#define Hash(P, N)      max(1, crc32c(0, P, N));
#define LockInc(P)      asm volatile("lock inc%z0\t%0" : "=m"(*(P)))
#define AppendCrlf(P)   mempcpy(P, "\r\n", 2)
#define HasHeader(H)    (!!msg.headers[H].a)
#define HeaderData(H)   (inbuf.p + msg.headers[H].a)
#define HeaderLength(H) (msg.headers[H].b - msg.headers[H].a)
#define HeaderEqualCase(H, S) \
  SlicesEqualCase(S, strlen(S), HeaderData(H), HeaderLength(H))

static const struct itimerval kHeartbeat = {
    {0, 500000},
    {0, 500000},
};

static const uint8_t kGzipHeader[] = {
    0x1F,        // MAGNUM
    0x8B,        // MAGNUM
    0x08,        // CM: DEFLATE
    0x00,        // FLG: NONE
    0x00,        // MTIME: NONE
    0x00,        //
    0x00,        //
    0x00,        //
    0x00,        // XFL
    kZipOsUnix,  // OS
};

static const char *const kIndexPaths[] = {
#ifndef STATIC
    "index.lua",
#endif
    "index.html",
};

static const struct ContentTypeExtension {
  unsigned char ext[8];
  const char *mime;
} kContentTypeExtension[] = {
    {"7z", "application/x-7z-compressed"},     //
    {"aac", "audio/aac"},                      //
    {"apng", "image/apng"},                    //
    {"atom", "application/atom+xml"},          //
    {"avi", "video/x-msvideo"},                //
    {"avif", "image/avif"},                    //
    {"bmp", "image/bmp"},                      //
    {"c", "text/plain"},                       //
    {"cc", "text/plain"},                      //
    {"css", "text/css"},                       //
    {"csv", "text/csv"},                       //
    {"gif", "image/gif"},                      //
    {"gz", "application/gzip"},                //
    {"h", "text/plain"},                       //
    {"htm", "text/html"},                      //
    {"html", "text/html"},                     //
    {"i", "text/plain"},                       //
    {"ico", "image/vnd.microsoft.icon"},       //
    {"jar", "application/java-archive"},       //
    {"jpeg", "image/jpeg"},                    //
    {"jpg", "image/jpeg"},                     //
    {"js", "application/javascript"},          //
    {"json", "application/json"},              //
    {"m4a", "audio/mpeg"},                     //
    {"markdown", "text/plain"},                //
    {"md", "text/plain"},                      //
    {"mp2", "audio/mpeg"},                     //
    {"mp3", "audio/mpeg"},                     //
    {"mp4", "video/mp4"},                      //
    {"mpeg", "video/mpeg"},                    //
    {"mpg", "video/mpeg"},                     //
    {"oga", "audio/ogg"},                      //
    {"ogg", "application/ogg"},                //
    {"ogv", "video/ogg"},                      //
    {"ogx", "application/ogg"},                //
    {"otf", "font/otf"},                       //
    {"pdf", "application/pdf"},                //
    {"png", "image/png"},                      //
    {"rar", "application/vnd.rar"},            //
    {"rtf", "application/rtf"},                //
    {"s", "text/plain"},                       //
    {"sh", "application/x-sh"},                //
    {"sqlite", "application/vnd.sqlite3"},     //
    {"sqlite3", "application/vnd.sqlite3"},    //
    {"svg", "image/svg+xml"},                  //
    {"swf", "application/x-shockwave-flash"},  //
    {"t38", "image/t38"},                      //
    {"tar", "application/x-tar"},              //
    {"tiff", "image/tiff"},                    //
    {"ttf", "font/ttf"},                       //
    {"txt", "text/plain"},                     //
    {"ul", "audio/basic"},                     //
    {"ulaw", "audio/basic"},                   //
    {"wasm", "application/wasm"},              //
    {"wav", "audio/x-wav"},                    //
    {"weba", "audio/webm"},                    //
    {"webm", "video/webm"},                    //
    {"webp", "image/webp"},                    //
    {"woff", "font/woff"},                     //
    {"woff2", "font/woff2"},                   //
    {"wsdl", "application/wsdl+xml"},          //
    {"xhtml", "application/xhtml+xml"},        //
    {"xls", "application/vnd.ms-excel"},       //
    {"xml", "application/xml"},                //
    {"xsl", "application/xslt+xml"},           //
    {"xslt", "application/xslt+xml"},          //
    {"z", "application/zlib"},                 //
    {"zip", "application/zip"},                //
    {"zst", "application/zstd"},               //
};

static const char kRegCode[][8] = {
    "OK",     "NOMATCH", "BADPAT", "COLLATE", "ECTYPE", "EESCAPE", "ESUBREG",
    "EBRACK", "EPAREN",  "EBRACE", "BADBR",   "ERANGE", "ESPACE",  "BADRPT",
};

struct Buffer {
  size_t n, c;
  char *p;
};

struct Strings {
  size_t n;
  char **p;
};

static struct Freelist {
  size_t n, c;
  void **p;
} freelist;

static struct Unmaplist {
  size_t n, c;
  struct Unmap {
    int f;
    void *p;
    size_t n;
  } * p;
} unmaplist;

static struct Redirects {
  size_t n;
  struct Redirect {
    int code;
    const char *path;
    size_t pathlen;
    const char *location;
  } * p;
} redirects;

static struct Assets {
  uint32_t n;
  struct Asset {
    bool istext;
    uint32_t hash;
    uint64_t cf;
    uint64_t lf;
    int64_t lastmodified;
    char *lastmodifiedstr;
    struct File {
      char *path;
      struct stat st;
    } * file;
  } * p;
} assets;

static struct Shared {
  int workers;
  long double nowish;
  long double lastmeltdown;
  char currentdate[32];
  struct rusage server;
  struct rusage children;
  long accepterrors;
  long acceptinterrupts;
  long acceptresets;
  long badlengths;
  long badmessages;
  long badmethods;
  long badranges;
  long closeerrors;
  long compressedresponses;
  long connectionshandled;
  long connectsrefused;
  long continues;
  long decompressedresponses;
  long deflates;
  long dropped;
  long dynamicrequests;
  long emfiles;
  long enetdowns;
  long enfiles;
  long enobufs;
  long enomems;
  long enonets;
  long errors;
  long expectsrefused;
  long failedchildren;
  long forbiddens;
  long forkerrors;
  long frags;
  long fumbles;
  long http09;
  long http10;
  long http11;
  long http12;
  long hugepayloads;
  long identityresponses;
  long inflates;
  long listingrequests;
  long loops;
  long mapfails;
  long maps;
  long meltdowns;
  long messageshandled;
  long missinglengths;
  long netafrinic;
  long netanonymous;
  long netapnic;
  long netapple;
  long netarin;
  long netatt;
  long netcogent;
  long netcomcast;
  long netdod;
  long netford;
  long netlacnic;
  long netloopback;
  long netother;
  long netprivate;
  long netprudential;
  long netripe;
  long nettestnet;
  long netusps;
  long notfounds;
  long notmodifieds;
  long openfails;
  long partialresponses;
  long payloaddisconnects;
  long pipelinedrequests;
  long precompressedresponses;
  long readerrors;
  long readinterrupts;
  long readresets;
  long readtimeouts;
  long redirects;
  long reloads;
  long rewrites;
  long serveroptions;
  long shutdowns;
  long slowloris;
  long slurps;
  long statfails;
  long staticrequests;
  long stats;
  long statuszrequests;
  long synchronizationfailures;
  long terminatedchildren;
  long thiscorruption;
  long transfersrefused;
  long urisrefused;
  long verifies;
  long writeerrors;
  long writeinterruputs;
  long writeresets;
  long writetimeouts;
} * shared;

static bool killed;
static bool istext;
static bool zombied;
static bool gzipped;
static bool branded;
static bool funtrace;
static bool meltdown;
static bool heartless;
static bool printport;
static bool heartbeat;
static bool daemonize;
static bool logrusage;
static bool logbodies;
static bool loglatency;
static bool terminated;
static bool uniprocess;
static bool invalidated;
static bool logmessages;
static bool checkedmethod;
static bool connectionclose;
static bool keyboardinterrupt;
static bool encouragekeepalive;
static bool loggednetworkorigin;
static bool hasluaglobalhandler;

static int frags;
static int gmtoff;
static int server;
static int client;
static int daemonuid;
static int daemongid;
static int statuscode;
static int maxpayloadsize;
static int messageshandled;
static uint32_t clientaddrsize;

static lua_State *L;
static size_t zsize;
static char *content;
static uint8_t *cdir;
static uint8_t *zmap;
static size_t hdrsize;
static size_t msgsize;
static size_t amtread;
static char *extrahdrs;
static char *luaheaderp;
static const char *brand;
static const char *pidpath;
static const char *logpath;
static struct Strings loops;
static size_t contentlength;
static int64_t cacheseconds;
static uint8_t gzip_footer[8];
static const char *serverheader;
static struct Strings stagedirs;
static struct Strings hidepaths;

static struct Buffer inbuf;
static struct Buffer hdrbuf;
static struct Buffer outbuf;
static struct linger linger;
static struct timeval timeout;
static struct Buffer effectivepath;

static struct Url url;
static struct HttpRequest msg;
static char slashpath[PATH_MAX];

static long double startread;
static long double lastrefresh;
static long double startserver;
static long double startrequest;
static long double startconnection;
static struct sockaddr_in serveraddr;
static struct sockaddr_in clientaddr;

static wontreturn void PrintUsage(FILE *f, int rc) {
  /* clang-format off */
  fprintf(f, "\
SYNOPSIS\n\
\n\
  %s [-hvduzmbagf] [-p PORT] [-- SCRIPTARGS...]\n\
\n\
DESCRIPTION\n\
\n\
  redbean - single-file distributable web server\n\
\n\
FLAGS\n\
\n\
  -h        help\n\
  -s        increase silence [repeat]\n\
  -v        increase verbosity [repeat]\n\
  -d        daemonize\n\
  -u        uniprocess\n\
  -z        print port\n\
  -m        log messages\n\
  -b        log message body\n\
  -a        log resource usage\n\
  -g        log handler latency\n"
#ifndef TINY
"  -f        log worker function calls\n"
#endif
"  -H K:V    sets http header globally [repeat]\n\
  -D DIR    serve assets from local directory [repeat]\n\
  -t MS     tunes read and write timeouts [default 30000]\n\
  -M INT    tunes max message payload size [default 65536]\n\
  -c SEC    configures static asset cache-control headers\n\
  -r /X=/Y  redirect X to Y [repeat]\n\
  -R /X=/Y  rewrites X to Y [repeat]\n\
  -l ADDR   listen ip [default 0.0.0.0]\n\
  -p PORT   listen port [default 8080]\n\
  -L PATH   log file location\n\
  -P PATH   pid file location\n\
  -U INT    daemon set user id\n\
  -G INT    daemon set group id\n\
\n\
FEATURES\n\
\n"
#ifndef STATIC
"  - Lua v5.4\n"
#endif
"  - HTTP v0.9\n\
  - HTTP v1.0\n\
  - HTTP v1.1\n\
  - Pipelining\n\
  - Accounting\n\
  - Content-Encoding\n\
  - Range / Content-Range\n\
  - Last-Modified / If-Modified-Since\n\
\n\
USAGE\n\
\n\
  This executable is also a ZIP file that contains static assets.\n\
  You can run redbean interactively in your terminal as follows:\n\
\n\
    ./redbean.com -vvvmbag        # starts server verbosely\n\
    open http://127.0.0.1:8080/   # shows zip listing page\n\
    CTRL-C                        # 1x: graceful shutdown\n\
    CTRL-C                        # 2x: forceful shutdown\n\
\n\
  You can override the default listing page by adding:\n\
\n"
#ifndef STATIC
"    zip redbean.com index.lua     # lua server pages take priority\n"
#endif
"    zip redbean.com index.html    # default page for directory\n\
\n\
  The listing page only applies to the root directory. However the\n\
  default index page applies to subdirectories too. In order for it\n\
  to work, there needs to be an empty directory entry in the zip.\n\
  That should already be the default practice of your zip editor.\n\
\n\
    wget                     \\\n\
      --mirror               \\\n\
      --convert-links        \\\n\
      --adjust-extension     \\\n\
      --page-requisites      \\\n\
      --no-parent            \\\n\
      --no-if-modified-since \\\n\
      http://a.example/index.html\n\
    zip -r redbean.com a.example/  # default page for directory\n\
\n\
  redbean normalizes the trailing slash for you automatically:\n\
\n\
    $ printf 'GET /a.example HTTP/1.0\\n\\n' | nc 127.0.0.1 8080\n\
    HTTP/1.0 307 Temporary Redirect\n\
    Location: /a.example/\n\
\n\
  Virtual hosting is accomplished this way too. The Host is simply\n\
  prepended to the path, and if it doesn't exist, it gets removed.\n\
\n\
    $ printf 'GET / HTTP/1.1\\nHost:a.example\\n\\n' | nc 127.0.0.1 8080\n\
    HTTP/1.1 200 OK\n\
    Link: <http://127.0.0.1/a.example/index.html>; rel=\"canonical\"\n\
\n\
  If you mirror a lot of websites within your redbean then you can\n\
  actually tell your browser that redbean is your proxy server, in\n\
  which redbean will act as your private version of the Internet.\n\
\n\
    $ printf 'GET http://a.example HTTP/1.0\\n\\n' | nc 127.0.0.1 8080\n\
    HTTP/1.0 200 OK\n\
    Link: <http://127.0.0.1/a.example/index.html>; rel=\"canonical\"\n\
\n\
  If you use a reverse proxy, then redbean recognizes the following\n\
  provided that the proxy forwards requests over the local network:\n\
\n\
    X-Forwarded-For: 203.0.113.42:31337\n\
    X-Forwarded-Host: foo.example:80\n\
\n\
  There's a text/plain statistics page called /statusz that makes\n\
  it easy to track and monitor the health of your redbean:\n\
\n\
    printf 'GET /statusz\\n\\n' | nc 127.0.0.1 8080\n\
\n\
  redbean will display an error page using the /redbean.png logo\n\
  by default, embedded as a bas64 data uri. You can override the\n\
  custom page for various errors by adding files to the zip root.\n\
\n\
    zip redbean.com 404.html      # custom not found page\n\
\n\
  Audio video content should not be compressed in your ZIP files.\n\
  Uncompressed assets enable browsers to send Range HTTP request.\n\
  On the other hand compressed assets are best for gzip encoding.\n\
\n\
    zip redbean.com index.html    # adds file\n\
    zip -0 redbean.com video.mp4  # adds without compression\n\
\n\
  You can have redbean run as a daemon by doing the following:\n\
\n\
    redbean.com -vv -d -L redbean.log -P redbean.pid\n\
    kill -TERM $(cat redbean.pid) # 1x: graceful shutdown\n\
    kill -TERM $(cat redbean.pid) # 2x: forceful shutdown\n\
\n\
  redbean currently has a 32kb limit on request messages and 64kb\n\
  including the payload. redbean will grow to whatever the system\n\
  limits allow. Should fork() or accept() fail redbean will react\n\
  by going into \"meltdown mode\" which closes lingering workers.\n\
  You can trigger this at any time using:\n\
\n\
    kill -USR2 $(cat redbean.pid)\n\
\n\
  Another failure condition is running out of disk space in which\n\
  case redbean reacts by truncating the log file. Lastly, redbean\n\
  does the best job possible reporting on resource usage when the\n\
  logger is in debug mode noting that NetBSD is the best at this.\n\
\n\
  Your redbean is an actually portable executable, that's able to\n\
  run on six different operating systems. To do that, it needs to\n\
  overwrite its own MZ header at startup, with ELF or Mach-O, and\n\
  then puts the original back once the program loads. If you want\n\
  your redbean to follow the platform-local executable convention\n\
  then delete the /.ape file from zip.\n\
\n\
  redbean contains software licensed ISC, MIT, BSD-2, BSD-3, zlib\n\
  which makes it a permissively licensed gift to anyone who might\n\
  find it useful. The transitive closure of legalese can be found\n\
  inside the binary. redbean also respects your privacy and won't\n\
  phone home because your computer is its home.\n\
\n\
SEE ALSO\n\
\n\
  https://justine.lol/redbean/index.html\n\
  https://news.ycombinator.com/item?id=26271117\n\
\n", program_invocation_name);
  /* clang-format on */
  exit(rc);
}

static void OnChld(void) {
  zombied = true;
}

static void OnAlrm(void) {
  heartbeat = true;
}

static void OnUsr1(void) {
  invalidated = true;
}

static void OnUsr2(void) {
  meltdown = true;
}

static void OnTerm(void) {
  if (!terminated) {
    terminated = true;
  } else {
    killed = true;
  }
}

static void OnInt(void) {
  keyboardinterrupt = true;
  OnTerm();
}

static void OnHup(void) {
  if (daemonize) {
    OnUsr1();
  } else {
    OnTerm();
  }
}

static bool SlicesEqual(const char *a, size_t n, const char *b, size_t m) {
  return n == m && !memcmp(a, b, n);
}

static bool SlicesEqualCase(const char *a, size_t n, const char *b, size_t m) {
  return n == m && !memcasecmp(a, b, n);
}

static int CompareSlices(const char *a, size_t n, const char *b, size_t m) {
  int c;
  if ((c = memcmp(a, b, MIN(n, m)))) return c;
  if (n < m) return -1;
  if (n > m) return +1;
  return 0;
}

static int CompareSlicesCase(const char *a, size_t n, const char *b, size_t m) {
  int c;
  if ((c = memcasecmp(a, b, MIN(n, m)))) return c;
  if (n < m) return -1;
  if (n > m) return +1;
  return 0;
}

static void *FreeLater(void *p) {
  if (p) {
    if (++freelist.n > freelist.c) {
      freelist.c = freelist.n + 2;
      freelist.c += freelist.c >> 1;
      freelist.p = xrealloc(freelist.p, freelist.c * sizeof(*freelist.p));
    }
    freelist.p[freelist.n - 1] = p;
  }
  return p;
}

static void UnmapLater(int f, void *p, size_t n) {
  if (++unmaplist.n > unmaplist.c) {
    unmaplist.c = unmaplist.n + 2;
    unmaplist.c += unmaplist.c >> 1;
    unmaplist.p = xrealloc(unmaplist.p, unmaplist.c * sizeof(*unmaplist.p));
  }
  unmaplist.p[unmaplist.n - 1].f = f;
  unmaplist.p[unmaplist.n - 1].p = p;
  unmaplist.p[unmaplist.n - 1].n = n;
}

static void CollectGarbage(void) {
  DestroyHttpRequest(&msg);
  while (freelist.n) {
    free(freelist.p[--freelist.n]);
  }
  while (unmaplist.n) {
    --unmaplist.n;
    LOGIFNEG1(munmap(unmaplist.p[unmaplist.n].p, unmaplist.p[unmaplist.n].n));
    LOGIFNEG1(close(unmaplist.p[unmaplist.n].f));
  }
}

static void UseOutput(void) {
  content = FreeLater(outbuf.p);
  contentlength = outbuf.n;
  outbuf.p = 0;
  outbuf.n = 0;
  outbuf.c = 0;
}

static void DropOutput(void) {
  free(outbuf.p);
  outbuf.p = 0;
  outbuf.n = 0;
  outbuf.c = 0;
}

static void ClearOutput(void) {
  outbuf.n = 0;
}

static void Grow(size_t n) {
  do {
    if (outbuf.c) {
      outbuf.c += outbuf.c >> 1;
    } else {
      outbuf.c = 16 * 1024;
    }
  } while (n > outbuf.c);
  outbuf.p = xrealloc(outbuf.p, outbuf.c);
}

static void AppendData(const char *data, size_t size) {
  size_t n;
  n = outbuf.n + size;
  if (n > outbuf.c) Grow(n);
  memcpy(outbuf.p + outbuf.n, data, size);
  outbuf.n = n;
}

static void Append(const char *fmt, ...) {
  int n;
  char *p;
  va_list va, vb;
  va_start(va, fmt);
  va_copy(vb, va);
  n = vsnprintf(outbuf.p + outbuf.n, outbuf.c - outbuf.n, fmt, va);
  if (n >= outbuf.c - outbuf.n) {
    Grow(outbuf.n + n + 1);
    vsnprintf(outbuf.p + outbuf.n, outbuf.c - outbuf.n, fmt, vb);
  }
  va_end(vb);
  va_end(va);
  outbuf.n += n;
}

static char *MergePaths(const char *p, size_t n, const char *q, size_t m,
                        size_t *z) {
  char *r;
  if (n && p[n - 1] == '/') --n;
  if (m && q[0] == '/') ++q, --m;
  r = xmalloc(n + 1 + m + 1);
  mempcpy(mempcpy(mempcpy(mempcpy(r, p, n), "/", 1), q, m), "", 1);
  if (z) *z = n + 1 + m;
  return r;
}

static long FindRedirect(const char *path, size_t n) {
  int c, m, l, r, z;
  l = 0;
  r = redirects.n - 1;
  while (l <= r) {
    m = (l + r) >> 1;
    c = CompareSlices(redirects.p[m].path, redirects.p[m].pathlen, path, n);
    if (c < 0) {
      l = m + 1;
    } else if (c > 0) {
      r = m - 1;
    } else {
      return m;
    }
  }
  return -1;
}

static void ProgramRedirect(int code, const char *src, const char *dst) {
  long i, j;
  struct Redirect r;
  if (code && code != 301 && code != 302 && code != 307 && code != 308) {
    fprintf(stderr, "error: unsupported redirect code %d\n", code);
    exit(1);
  }
  r.code = code;
  r.path = strdup(src);
  r.pathlen = strlen(src);
  r.location = strdup(dst);
  if ((i = FindRedirect(r.path, r.pathlen)) != -1) {
    redirects.p[i] = r;
  } else {
    i = redirects.n;
    redirects.p = xrealloc(redirects.p, (i + 1) * sizeof(*redirects.p));
    for (j = i; j; --j) {
      if (CompareSlices(r.path, r.pathlen, redirects.p[j - 1].path,
                        redirects.p[j - 1].pathlen) < 0) {
        redirects.p[j] = redirects.p[j - 1];
      } else {
        break;
      }
    }
    redirects.p[j] = r;
    ++redirects.n;
  }
}

static void ProgramRedirectArg(int code, const char *arg) {
  char *s;
  const char *p;
  if (!(p = strchr(arg, '='))) {
    fprintf(stderr, "error: redirect arg missing '='\n");
    exit(1);
  }
  ProgramRedirect(code, (s = strndup(arg, p - arg)), p + 1);
  free(s);
}

static int CompareInts(const uint64_t x, uint64_t y) {
  return x > y ? 1 : x < y ? -1 : 0;
}

static const char *BisectContentType(uint64_t ext) {
  int c, m, l, r;
  l = 0;
  r = ARRAYLEN(kContentTypeExtension) - 1;
  while (l <= r) {
    m = (l + r) >> 1;
    c = CompareInts(READ64BE(kContentTypeExtension[m].ext), ext);
    if (c < 0) {
      l = m + 1;
    } else if (c > 0) {
      r = m - 1;
    } else {
      return kContentTypeExtension[m].mime;
    }
  }
  return NULL;
}

static const char *FindContentType(const char *path, size_t n) {
  size_t i;
  uint64_t x;
  const char *p, *r;
  if ((p = memrchr(path, '.', n))) {
    for (x = 0, i = n; i-- > p + 1 - path;) {
      x <<= 8;
      x |= tolower(path[i] & 255);
    }
    if ((r = BisectContentType(bswap_64(x)))) {
      return r;
    }
  }
  return NULL;
}

static const char *GetContentType(struct Asset *a, const char *path, size_t n) {
  const char *r;
  if (a->file && (r = FindContentType(a->file->path, strlen(a->file->path)))) {
    return r;
  }
  return firstnonnull(
      FindContentType(path, n),
      firstnonnull(FindContentType(ZIP_LFILE_NAME(zmap + a->lf),
                                   ZIP_LFILE_NAMESIZE(zmap + a->lf)),
                   a->istext ? "text/plain" : "application/octet-stream"));
}

static void DescribeAddress(char buf[32], uint32_t addr, uint16_t port) {
  char *p;
  const char *s;
  p = buf;
  p += uint64toarray_radix10((addr & 0xFF000000) >> 030, p), *p++ = '.';
  p += uint64toarray_radix10((addr & 0x00FF0000) >> 020, p), *p++ = '.';
  p += uint64toarray_radix10((addr & 0x0000FF00) >> 010, p), *p++ = '.';
  p += uint64toarray_radix10((addr & 0x000000FF) >> 000, p), *p++ = ':';
  p += uint64toarray_radix10(port, p);
  if ((s = GetIpCategoryName(CategorizeIp(addr)))) {
    *p++ = ' ';
    p = stpcpy(p, s);
  }
  *p++ = '\0';
}

static void GetServerAddr(uint32_t *ip, uint16_t *port) {
  *ip = ntohl(serveraddr.sin_addr.s_addr);
  if (port) *port = ntohs(serveraddr.sin_port);
}

static void GetClientAddr(uint32_t *ip, uint16_t *port) {
  *ip = ntohl(clientaddr.sin_addr.s_addr);
  if (port) *port = ntohs(clientaddr.sin_port);
}

static void GetRemoteAddr(uint32_t *ip, uint16_t *port) {
  GetClientAddr(ip, port);
  if (HasHeader(kHttpXForwardedFor) &&
      (IsPrivateIp(*ip) || IsLoopbackIp(*ip))) {
    ParseForwarded(HeaderData(kHttpXForwardedFor),
                   HeaderLength(kHttpXForwardedFor), ip, port);
  }
}

static char *DescribeClient(void) {
  uint32_t ip;
  uint16_t port;
  static char clientaddrstr[32];
  GetRemoteAddr(&ip, &port);
  DescribeAddress(clientaddrstr, ip, port);
  return clientaddrstr;
}

static char *DescribeServer(void) {
  uint32_t ip;
  uint16_t port;
  static char serveraddrstr[32];
  GetServerAddr(&ip, &port);
  DescribeAddress(serveraddrstr, ip, port);
  return serveraddrstr;
}

static void ProgramBrand(const char *s) {
  free(brand);
  free(serverheader);
  brand = strdup(s);
  if (!(serverheader = EncodeHttpHeaderValue(brand, -1, 0))) {
    fprintf(stderr, "error: brand isn't latin1 encodable: %`'s", brand);
    exit(1);
  }
}

static void ProgramLinger(long sec) {
  linger.l_onoff = sec > 0;
  linger.l_linger = MAX(0, sec);
}

static void ProgramTimeout(long ms) {
  ldiv_t d;
  if (ms <= 30) {
    fprintf(stderr, "error: timeout needs to be 31ms or greater\n");
    exit(1);
  }
  d = ldiv(ms, 1000);
  timeout.tv_sec = d.quot;
  timeout.tv_usec = d.rem * 1000;
}

static void ProgramCache(long x) {
  cacheseconds = x;
}

static void ProgramPort(long x) {
  serveraddr.sin_port = htons(x);
}

static void SetDefaults(void) {
#ifdef STATIC
  ProgramBrand("redbean-static/0.4");
#else
  ProgramBrand("redbean/0.4");
#endif
  __log_level = kLogInfo;
  maxpayloadsize = 64 * 1024;
  ProgramCache(-1);
  ProgramTimeout(30 * 1000);
  ProgramPort(DEFAULT_PORT);
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = INADDR_ANY;
  if (IsWindows()) uniprocess = true;
}

static char *RemoveTrailingSlashes(char *s) {
  size_t n;
  n = strlen(s);
  while (n && (s[n - 1] == '/' || s[n - 1] == '\\')) s[--n] = '\0';
  return s;
}

static void AddString(struct Strings *l, char *s) {
  l->p = xrealloc(l->p, ++l->n * sizeof(*l->p));
  l->p[l->n - 1] = s;
}

static bool HasString(struct Strings *l, char *s) {
  size_t i;
  for (i = 0; i < l->n; ++i) {
    if (!strcmp(l->p[i], s)) {
      return true;
    }
  }
  return false;
}

static void AddStagingDirectory(const char *dirpath) {
  char *s;
  s = RemoveTrailingSlashes(strdup(dirpath));
  if (isempty(s) || !isdirectory(s)) {
    fprintf(stderr, "error: not a directory: %`'s\n", s);
    exit(1);
  }
  AddString(&stagedirs, s);
}

static void ProgramHeader(const char *s) {
  char *p, *v, *h;
  if ((p = strchr(s, ':')) && IsValidHttpToken(s, p - s) &&
      (v = EncodeLatin1(p + 1, -1, 0, kControlC0 | kControlC1 | kControlWs))) {
    switch (GetHttpHeader(s, p - s)) {
      case kHttpDate:
      case kHttpConnection:
      case kHttpContentLength:
      case kHttpContentEncoding:
      case kHttpContentRange:
        fprintf(stderr, "error: can't program header: %`'s\n", s);
        exit(1);
      case kHttpServer:
        ProgramBrand(p + 1);
        break;
      default:
        p = xasprintf("%s%.*s:%s\r\n", extrahdrs ? extrahdrs : "", p - s, s, v);
        free(extrahdrs);
        extrahdrs = p;
        break;
    }
    free(v);
  } else {
    fprintf(stderr, "error: illegal header: %`'s\n", s);
    exit(1);
  }
}

static void GetOpts(int argc, char *argv[]) {
  int opt;
  while ((opt = getopt(argc, argv,
                       "azhdugvsmbfl:p:r:R:H:c:L:P:U:G:B:D:t:M:")) != -1) {
    switch (opt) {
      case 'v':
        __log_level++;
        break;
      case 's':
        __log_level--;
        break;
      case 'd':
        daemonize = true;
        break;
      case 'a':
        logrusage = true;
        break;
      case 'u':
        uniprocess = true;
        break;
      case 'g':
        loglatency = true;
        break;
      case 'm':
        logmessages = true;
        break;
      case 'b':
        logbodies = true;
        break;
      case 'z':
        printport = true;
        break;
      case 'f':
        funtrace = true;
        break;
      case 'k':
        encouragekeepalive = true;
        break;
      case 't':
        ProgramTimeout(strtol(optarg, NULL, 0));
        break;
      case 'r':
        ProgramRedirectArg(307, optarg);
        break;
      case 'R':
        ProgramRedirectArg(0, optarg);
        break;
      case 'D':
        AddStagingDirectory(optarg);
        break;
      case 'c':
        ProgramCache(strtol(optarg, NULL, 0));
        break;
      case 'p':
        ProgramPort(strtol(optarg, NULL, 0));
        break;
      case 'M':
        maxpayloadsize = atoi(optarg);
        maxpayloadsize = MAX(1450, maxpayloadsize);
        break;
      case 'l':
        CHECK_EQ(1, inet_pton(AF_INET, optarg, &serveraddr.sin_addr));
        break;
      case 'B':
        ProgramBrand(optarg);
        break;
      case 'H':
        ProgramHeader(optarg);
        break;
      case 'L':
        logpath = optarg;
        break;
      case 'P':
        pidpath = optarg;
        break;
      case 'U':
        daemonuid = atoi(optarg);
        break;
      case 'G':
        daemongid = atoi(optarg);
        break;
      case 'h':
        PrintUsage(stdout, EXIT_SUCCESS);
      default:
        PrintUsage(stderr, EX_USAGE);
    }
  }
  if (logpath) {
    CHECK_NOTNULL(freopen(logpath, "a", stderr));
  }
}

static void Daemonize(void) {
  char ibuf[21];
  int i, fd, pid;
  for (i = 3; i < 128; ++i) {
    if (i != server) {
      close(i);
    }
  }
  if ((pid = fork()) > 0) exit(0);
  setsid();
  if ((pid = fork()) > 0) _exit(0);
  umask(0);
  if (pidpath) {
    fd = open(pidpath, O_CREAT | O_EXCL | O_WRONLY, 0644);
    write(fd, ibuf, uint64toarray_radix10(getpid(), ibuf));
    close(fd);
  }
  if (!logpath) logpath = "/dev/null";
  freopen("/dev/null", "r", stdin);
  freopen(logpath, "a", stdout);
  freopen(logpath, "a", stderr);
  LOGIFNEG1(setgid(daemongid));
  LOGIFNEG1(setuid(daemonuid));
}

static void ReportWorkerExit(int pid, int ws) {
  --shared->workers;
  if (WIFEXITED(ws)) {
    if (WEXITSTATUS(ws)) {
      LockInc(&shared->failedchildren);
      WARNF("%d exited with %d (%,d workers remain)", pid, WEXITSTATUS(ws),
            shared->workers);
    } else {
      DEBUGF("%d exited (%,d workers remain)", pid, shared->workers);
    }
  } else {
    LockInc(&shared->terminatedchildren);
    WARNF("%d terminated with %s (%,d workers remain)", pid,
          strsignal(WTERMSIG(ws)), shared->workers);
  }
}

static void AppendResourceReport(struct rusage *ru, const char *nl) {
  long utime, stime;
  long double ticks;
  if (ru->ru_maxrss) {
    Append("ballooned to %,ldkb in size%s", ru->ru_maxrss, nl);
  }
  if ((utime = ru->ru_utime.tv_sec * 1000000 + ru->ru_utime.tv_usec) |
      (stime = ru->ru_stime.tv_sec * 1000000 + ru->ru_stime.tv_usec)) {
    ticks = ceill((long double)(utime + stime) / (1000000.L / CLK_TCK));
    Append("needed %,ldµs cpu (%d%% kernel)%s", utime + stime,
           (int)((long double)stime / (utime + stime) * 100), nl);
    if (ru->ru_idrss) {
      Append("needed %,ldkb memory on average%s", lroundl(ru->ru_idrss / ticks),
             nl);
    }
    if (ru->ru_isrss) {
      Append("needed %,ldkb stack on average%s", lroundl(ru->ru_isrss / ticks),
             nl);
    }
    if (ru->ru_ixrss) {
      Append("mapped %,ldkb shared on average%s", lroundl(ru->ru_ixrss / ticks),
             nl);
    }
  }
  if (ru->ru_minflt || ru->ru_majflt) {
    Append("caused %,ld page faults (%d%% memcpy)%s",
           ru->ru_minflt + ru->ru_majflt,
           (int)((long double)ru->ru_minflt / (ru->ru_minflt + ru->ru_majflt) *
                 100),
           nl);
  }
  if (ru->ru_nvcsw + ru->ru_nivcsw > 1) {
    Append(
        "%,ld context switches (%d%% consensual)%s",
        ru->ru_nvcsw + ru->ru_nivcsw,
        (int)((long double)ru->ru_nvcsw / (ru->ru_nvcsw + ru->ru_nivcsw) * 100),
        nl);
  }
  if (ru->ru_msgrcv || ru->ru_msgsnd) {
    Append("received %,ld message%s and sent %,ld%s", ru->ru_msgrcv,
           ru->ru_msgrcv == 1 ? "" : "s", ru->ru_msgsnd, nl);
  }
  if (ru->ru_inblock || ru->ru_oublock) {
    Append("performed %,ld read%s and %,ld write i/o operations%s",
           ru->ru_inblock, ru->ru_inblock == 1 ? "" : "s", ru->ru_oublock, nl);
  }
  if (ru->ru_nsignals) {
    Append("received %,ld signals%s", ru->ru_nsignals, nl);
  }
  if (ru->ru_nswap) {
    Append("got swapped %,ld times%s", ru->ru_nswap, nl);
  }
}

static void AddTimeval(struct timeval *x, const struct timeval *y) {
  x->tv_sec += y->tv_sec;
  x->tv_usec += y->tv_usec;
  if (x->tv_usec >= 1000000) {
    x->tv_usec -= 1000000;
    x->tv_sec += 1;
  }
}

static void AddRusage(struct rusage *x, const struct rusage *y) {
  AddTimeval(&x->ru_utime, &y->ru_utime);
  AddTimeval(&x->ru_stime, &y->ru_stime);
  x->ru_maxrss = MAX(x->ru_maxrss, y->ru_maxrss);
  x->ru_ixrss += y->ru_ixrss;
  x->ru_idrss += y->ru_idrss;
  x->ru_isrss += y->ru_isrss;
  x->ru_minflt += y->ru_minflt;
  x->ru_majflt += y->ru_majflt;
  x->ru_nswap += y->ru_nswap;
  x->ru_inblock += y->ru_inblock;
  x->ru_oublock += y->ru_oublock;
  x->ru_msgsnd += y->ru_msgsnd;
  x->ru_msgrcv += y->ru_msgrcv;
  x->ru_nsignals += y->ru_nsignals;
  x->ru_nvcsw += y->ru_nvcsw;
  x->ru_nivcsw += y->ru_nivcsw;
}

static void ReportWorkerResources(int pid, struct rusage *ru) {
  const char *s;
  if (logrusage || LOGGABLE(kLogDebug)) {
    AppendResourceReport(ru, "\n");
    if (outbuf.n) {
      if ((s = IndentLines(outbuf.p, outbuf.n - 1, 0, 1))) {
        LOGF("resource report for pid %d\n%s", pid, s);
        free(s);
      }
      ClearOutput();
    }
  }
}

static void HandleWorkerExit(int pid, int ws, struct rusage *ru) {
  ++shared->connectionshandled;
  AddRusage(&shared->children, ru);
  ReportWorkerExit(pid, ws);
  ReportWorkerResources(pid, ru);
}

static void WaitAll(void) {
  int ws, pid;
  struct rusage ru;
  for (;;) {
    if ((pid = wait4(-1, &ws, 0, &ru)) != -1) {
      HandleWorkerExit(pid, ws, &ru);
    } else {
      if (errno == ECHILD) break;
      if (errno == EINTR) {
        if (killed) {
          killed = false;
          terminated = false;
          WARNF("redbean shall terminate harder");
          LOGIFNEG1(kill(0, SIGTERM));
        }
        continue;
      }
      FATALF("%s wait error %s", DescribeServer(), strerror(errno));
    }
  }
}

static void ReapZombies(void) {
  int ws, pid;
  struct rusage ru;
  do {
    zombied = false;
    if ((pid = wait4(-1, &ws, WNOHANG, &ru)) != -1) {
      if (pid) {
        HandleWorkerExit(pid, ws, &ru);
      } else {
        break;
      }
    } else {
      if (errno == ECHILD) break;
      if (errno == EINTR) continue;
      FATALF("%s wait error %s", DescribeServer(), strerror(errno));
    }
  } while (!terminated);
}

static inline ssize_t WritevAll(int fd, struct iovec *iov, int iovlen) {
  ssize_t rc;
  size_t wrote;
  do {
    if ((rc = writev(fd, iov, iovlen)) != -1) {
      wrote = rc;
      do {
        if (wrote >= iov->iov_len) {
          wrote -= iov->iov_len;
          ++iov;
          --iovlen;
        } else {
          iov->iov_base = (char *)iov->iov_base + wrote;
          iov->iov_len -= wrote;
          wrote = 0;
        }
      } while (wrote);
    } else if (errno == EINTR) {
      LockInc(&shared->writeinterruputs);
      if (killed || (meltdown && nowl() - startread > 2)) {
        return -1;
      }
    } else {
      return -1;
    }
  } while (iovlen);
  return 0;
}

static bool ClientAcceptsGzip(void) {
  return msg.version >= 10 && /* RFC1945 § 3.5 */
         HeaderHas(&msg, inbuf.p, kHttpAcceptEncoding, "gzip", 4);
}

static void UpdateCurrentDate(long double now) {
  int64_t t;
  struct tm tm;
  t = now;
  shared->nowish = now;
  gmtime_r(&t, &tm);
  FormatHttpDateTime(shared->currentdate, &tm);
}

static int64_t GetGmtOffset(int64_t t) {
  struct tm tm;
  localtime_r(&t, &tm);
  return tm.tm_gmtoff;
}

static int64_t LocoTimeToZulu(int64_t x) {
  return x - gmtoff;
}

static int64_t GetZipCfileLastModified(const uint8_t *zcf) {
  const uint8_t *p, *pe;
  for (p = ZIP_CFILE_EXTRA(zcf), pe = p + ZIP_CFILE_EXTRASIZE(zcf); p + 4 <= pe;
       p += ZIP_EXTRA_SIZE(p)) {
    if (ZIP_EXTRA_HEADERID(p) == kZipExtraNtfs &&
        ZIP_EXTRA_CONTENTSIZE(p) >= 4 + 4 + 8 * 3 &&
        READ16LE(ZIP_EXTRA_CONTENT(p) + 4) == 1 &&
        READ16LE(ZIP_EXTRA_CONTENT(p) + 6) == 24) {
      return READ64LE(ZIP_EXTRA_CONTENT(p) + 8) / HECTONANOSECONDS -
             MODERNITYSECONDS;
    }
  }
  for (p = ZIP_CFILE_EXTRA(zcf), pe = p + ZIP_CFILE_EXTRASIZE(zcf); p + 4 <= pe;
       p += ZIP_EXTRA_SIZE(p)) {
    if (ZIP_EXTRA_HEADERID(p) == kZipExtraExtendedTimestamp &&
        ZIP_EXTRA_CONTENTSIZE(p) >= 1 + 4 && (*ZIP_EXTRA_CONTENT(p) & 1)) {
      return (int32_t)READ32LE(ZIP_EXTRA_CONTENT(p) + 1);
    }
  }
  for (p = ZIP_CFILE_EXTRA(zcf), pe = p + ZIP_CFILE_EXTRASIZE(zcf); p + 4 <= pe;
       p += ZIP_EXTRA_SIZE(p)) {
    if (ZIP_EXTRA_HEADERID(p) == kZipExtraUnix &&
        ZIP_EXTRA_CONTENTSIZE(p) >= 4 + 4) {
      return (int32_t)READ32LE(ZIP_EXTRA_CONTENT(p) + 4);
    }
  }
  return LocoTimeToZulu(DosDateTimeToUnix(ZIP_CFILE_LASTMODIFIEDDATE(zcf),
                                          ZIP_CFILE_LASTMODIFIEDTIME(zcf)));
}

static bool IsCompressed(struct Asset *a) {
  return !a->file &&
         ZIP_LFILE_COMPRESSIONMETHOD(zmap + a->lf) == kZipCompressionDeflate;
}

static int GetMode(struct Asset *a) {
  return a->file ? a->file->st.st_mode : GetZipCfileMode(zmap + a->cf);
}

static bool IsNotModified(struct Asset *a) {
  if (msg.version < 10) return false;
  if (!HasHeader(kHttpIfModifiedSince)) return false;
  return a->lastmodified >=
         ParseHttpDateTime(HeaderData(kHttpIfModifiedSince),
                           HeaderLength(kHttpIfModifiedSince));
}

static char *FormatUnixHttpDateTime(char *s, int64_t t) {
  struct tm tm;
  gmtime_r(&t, &tm);
  FormatHttpDateTime(s, &tm);
  return s;
}

static bool IsCompressionMethodSupported(int method) {
  return method == kZipCompressionNone || method == kZipCompressionDeflate;
}

static void IndexAssets(void) {
  int64_t lm;
  uint64_t cf, lf;
  struct Asset *p;
  uint32_t i, n, m, step, hash;
  CHECK_GE(HASH_LOAD_FACTOR, 2);
  CHECK(READ32LE(cdir) == kZipCdir64HdrMagic ||
        READ32LE(cdir) == kZipCdirHdrMagic);
  n = GetZipCdirRecords(cdir);
  m = roundup2pow(MAX(1, n) * HASH_LOAD_FACTOR);
  p = xcalloc(m, sizeof(struct Asset));
  for (cf = GetZipCdirOffset(cdir); n--; cf += ZIP_CFILE_HDRSIZE(zmap + cf)) {
    CHECK_EQ(kZipCfileHdrMagic, ZIP_CFILE_MAGIC(zmap + cf));
    lf = GetZipCfileOffset(zmap + cf);
    if (!IsCompressionMethodSupported(ZIP_LFILE_COMPRESSIONMETHOD(zmap + lf))) {
      LOGF("don't understand zip compression method %d used by %`'.*s",
           ZIP_LFILE_COMPRESSIONMETHOD(zmap + lf),
           ZIP_CFILE_NAMESIZE(zmap + cf), ZIP_CFILE_NAME(zmap + cf));
      continue;
    }
    hash = Hash(ZIP_CFILE_NAME(zmap + cf), ZIP_CFILE_NAMESIZE(zmap + cf));
    step = 0;
    do {
      i = (hash + (step * (step + 1)) >> 1) & (m - 1);
      ++step;
    } while (p[i].hash);
    lm = GetZipCfileLastModified(zmap + cf);
    p[i].hash = hash;
    p[i].lf = lf;
    p[i].cf = cf;
    p[i].istext = !!(ZIP_CFILE_INTERNALATTRIBUTES(zmap + cf) & kZipIattrText);
    p[i].lastmodified = lm;
    p[i].lastmodifiedstr = FormatUnixHttpDateTime(xmalloc(30), lm);
  }
  assets.p = p;
  assets.n = m;
}

static void OpenZip(const char *path) {
  int fd;
  uint8_t *p;
  struct stat st;
  CHECK_NE(-1, (fd = open(path, O_RDONLY)));
  CHECK_NE(-1, fstat(fd, &st));
  CHECK((zsize = st.st_size));
  CHECK_NE(MAP_FAILED,
           (zmap = mmap(NULL, zsize, PROT_READ, MAP_SHARED, fd, 0)));
  CHECK_NOTNULL((cdir = GetZipCdir(zmap, zsize)));
  if (endswith(path, ".com.dbg") && (p = memmem(zmap, zsize, "MZqFpD", 6))) {
    zsize -= p - zmap;
    zmap = p;
  }
  close(fd);
}

static struct Asset *GetAssetZip(const char *path, size_t pathlen) {
  uint32_t i, step, hash;
  if (pathlen > 1 && path[0] == '/') ++path, --pathlen;
  hash = Hash(path, pathlen);
  for (step = 0;; ++step) {
    i = (hash + (step * (step + 1)) >> 1) & (assets.n - 1);
    if (!assets.p[i].hash) return NULL;
    if (hash == assets.p[i].hash &&
        pathlen == ZIP_LFILE_NAMESIZE(zmap + assets.p[i].lf) &&
        memcmp(path, ZIP_LFILE_NAME(zmap + assets.p[i].lf), pathlen) == 0) {
      return &assets.p[i];
    }
  }
}

static struct Asset *GetAssetFile(const char *path, size_t pathlen) {
  size_t i;
  struct Asset *a;
  if (stagedirs.n) {
    a = FreeLater(xcalloc(1, sizeof(struct Asset)));
    a->file = FreeLater(xmalloc(sizeof(struct File)));
    for (i = 0; i < stagedirs.n; ++i) {
      LockInc(&shared->stats);
      a->file->path = FreeLater(
          MergePaths(stagedirs.p[i], strlen(stagedirs.p[i]), path, pathlen, 0));
      if (stat(a->file->path, &a->file->st) != -1) {
        a->lastmodifiedstr = FormatUnixHttpDateTime(
            FreeLater(xmalloc(30)),
            (a->lastmodified = a->file->st.st_mtim.tv_sec));
        return a;
      } else {
        LockInc(&shared->statfails);
      }
    }
  }
  return NULL;
}

static struct Asset *GetAsset(const char *path, size_t pathlen) {
  char *path2;
  struct Asset *a;
  if (!(a = GetAssetFile(path, pathlen))) {
    if (!(a = GetAssetZip(path, pathlen))) {
      if (pathlen > 1 && path[pathlen - 1] != '/' &&
          pathlen + 1 <= sizeof(slashpath)) {
        memcpy(mempcpy(slashpath, path, pathlen), "/", 1);
        a = GetAssetZip(slashpath, pathlen + 1);
      }
    }
  }
  return a;
}

static bool MustNotIncludeMessageBody(void) { /* RFC2616 § 4.4 */
  return msg.method == kHttpHead || (100 <= statuscode && statuscode <= 199) ||
         statuscode == 204 || statuscode == 304;
}

static char *SetStatus(unsigned code, const char *reason) {
  statuscode = code;
  stpcpy(hdrbuf.p, "HTTP/1.0 000 ");
  hdrbuf.p[7] += msg.version & 1;
  hdrbuf.p[9] += code / 100;
  hdrbuf.p[10] += code / 10 % 10;
  hdrbuf.p[11] += code % 10;
  return AppendCrlf(stpcpy(hdrbuf.p + 13, reason));
}

static char *AppendHeader(char *p, const char *k, const char *v) {
  if (!v) return p;
  return AppendCrlf(stpcpy(stpcpy(stpcpy(p, k), ": "), v));
}

static char *AppendContentType(char *p, const char *ct) {
  p = stpcpy(p, "Content-Type: ");
  p = stpcpy(p, ct);
  if (startswith(ct, "text/")) {
    istext = true;
    if (!strchr(ct, ';')) {
      p = stpcpy(p, "; charset=utf-8");
    }
  }
  return AppendCrlf(p);
}

static char *AppendExpires(char *p, int64_t t) {
  struct tm tm;
  gmtime_r(&t, &tm);
  p = stpcpy(p, "Expires: ");
  p = FormatHttpDateTime(p, &tm);
  return AppendCrlf(p);
}

static char *AppendCache(char *p, int64_t seconds) {
  if (seconds < 0) return p;
  p = stpcpy(p, "Cache-Control: max-age=");
  p += uint64toarray_radix10(seconds, p);
  if (seconds) {
    p = stpcpy(p, ", public");
  } else {
    p = stpcpy(p, ", no-store");
  }
  p = AppendCrlf(p);
  return AppendExpires(p, (int64_t)shared->nowish + seconds);
}

static char *AppendServer(char *p, const char *s) {
  p = stpcpy(p, "Server: ");
  p = stpcpy(p, s);
  return AppendCrlf(p);
}

static char *AppendContentLength(char *p, size_t n) {
  p = stpcpy(p, "Content-Length: ");
  p += uint64toarray_radix10(n, p);
  return AppendCrlf(p);
}

static char *AppendContentRange(char *p, long a, long b, long c) {
  p = stpcpy(p, "Content-Range: bytes ");
  if (a >= 0 && b > 0) {
    p += uint64toarray_radix10(a, p);
    *p++ = '-';
    p += uint64toarray_radix10(a + b - 1, p);
  } else {
    *p++ = '*';
  }
  *p++ = '/';
  p += uint64toarray_radix10(c, p);
  return AppendCrlf(p);
}

static bool Inflate(void *dp, size_t dn, const void *sp, size_t sn) {
  z_stream zs;
  LockInc(&shared->inflates);
  zs.next_in = sp;
  zs.avail_in = sn;
  zs.total_in = sn;
  zs.next_out = dp;
  zs.avail_out = dn;
  zs.total_out = dn;
  zs.zfree = Z_NULL;
  zs.zalloc = Z_NULL;
  CHECK_EQ(Z_OK, inflateInit2(&zs, -MAX_WBITS));
  switch (inflate(&zs, Z_NO_FLUSH)) {
    case Z_STREAM_END:
      CHECK_EQ(Z_OK, inflateEnd(&zs));
      return true;
    case Z_DATA_ERROR:
      inflateEnd(&zs);
      WARNF("Z_DATA_ERROR");
      return false;
    case Z_NEED_DICT:
      inflateEnd(&zs);
      WARNF("Z_NEED_DICT");
      return false;
    case Z_MEM_ERROR:
      FATALF("Z_MEM_ERROR");
    default:
      abort();
  }
}

static bool Verify(void *data, size_t size, uint32_t crc) {
  uint32_t got;
  LockInc(&shared->verifies);
  if (crc == (got = crc32_z(0, data, size))) {
    return true;
  } else {
    LockInc(&shared->thiscorruption);
    WARNF("corrupt zip file at %`'.*s had crc 0x%08x but expected 0x%08x",
          msg.uri.b - msg.uri.a, inbuf.p + msg.uri.a, got, crc);
    return false;
  }
}

static void *Deflate(const void *data, size_t size, size_t *out_size) {
  void *res;
  z_stream zs;
  LockInc(&shared->deflates);
  CHECK_EQ(Z_OK, deflateInit2(memset(&zs, 0, sizeof(zs)), 4, Z_DEFLATED,
                              -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY));
  zs.next_in = data;
  zs.avail_in = size;
  zs.avail_out = compressBound(size);
  zs.next_out = res = xmalloc(zs.avail_out);
  CHECK_EQ(Z_STREAM_END, deflate(&zs, Z_FINISH));
  CHECK_EQ(Z_OK, deflateEnd(&zs));
  *out_size = zs.total_out;
  return xrealloc(res, zs.total_out);
}

static void *LoadAsset(struct Asset *a, size_t *out_size) {
  int mode;
  size_t size;
  uint8_t *data;
  if (S_ISDIR(GetMode(a))) {
    WARNF("can't load directory");
    return NULL;
  }
  if (!a->file) {
    size = GetZipLfileUncompressedSize(zmap + a->lf);
    if (size == SIZE_MAX || !(data = malloc(size + 1))) return NULL;
    if (IsCompressed(a)) {
      if (!Inflate(data, size, ZIP_LFILE_CONTENT(zmap + a->lf),
                   GetZipLfileCompressedSize(zmap + a->lf))) {
        free(data);
        return NULL;
      }
    } else {
      memcpy(data, ZIP_LFILE_CONTENT(zmap + a->lf), size);
    }
    if (!Verify(data, size, ZIP_LFILE_CRC32(zmap + a->lf))) {
      free(data);
      return NULL;
    }
    data[size] = '\0';
    if (out_size) *out_size = size;
    return data;
  } else {
    LockInc(&shared->slurps);
    return xslurp(a->file->path, out_size);
  }
}

static void AppendLogo(void) {
  size_t n;
  char *p, *q;
  struct Asset *a;
  if ((a = GetAsset("/redbean.png", 12)) && (p = LoadAsset(a, &n))) {
    q = EncodeBase64(p, n, &n);
    Append("<img src=\"data:image/png;base64,");
    AppendData(q, n);
    Append("\">\r\n");
    free(q);
    free(p);
  }
}

static inline ssize_t Send(struct iovec *iov, int iovlen) {
  ssize_t rc;
  if ((rc = WritevAll(client, iov, iovlen)) == -1) {
    if (errno == ECONNRESET) {
      LockInc(&shared->writeresets);
      DEBUGF("%s write reset", DescribeClient());
    } else if (errno == EAGAIN) {
      LockInc(&shared->writetimeouts);
      WARNF("%s write timeout", DescribeClient());
    } else {
      LockInc(&shared->writeerrors);
      WARNF("%s write error %s", DescribeClient(), strerror(errno));
    }
    connectionclose = true;
  }
  return rc;
}

static char *CommitOutput(char *p) {
  uint32_t crc;
  if (!contentlength) {
    if (istext && outbuf.n >= 100) {
      p = stpcpy(p, "Vary: Accept-Encoding\r\n");
      if (ClientAcceptsGzip()) {
        gzipped = true;
        crc = crc32_z(0, outbuf.p, outbuf.n);
        WRITE32LE(gzip_footer + 0, crc);
        WRITE32LE(gzip_footer + 4, outbuf.n);
        content = FreeLater(Deflate(outbuf.p, outbuf.n, &contentlength));
        DropOutput();
      } else {
        UseOutput();
      }
    } else {
      UseOutput();
    }
  } else {
    DropOutput();
  }
  return p;
}

static char *ServeDefaultErrorPage(char *p, unsigned code, const char *reason) {
  p = AppendContentType(p, "text/html; charset=ISO-8859-1");
  reason = FreeLater(EscapeHtml(reason, -1, 0));
  Append("\
<!doctype html>\r\n\
<title>");
  Append("%d %s", code, reason);
  Append("\
</title>\r\n\
<style>\r\n\
html { color: #111; font-family: sans-serif; }\r\n\
img { vertical-align: middle; }\r\n\
</style>\r\n\
<h1>\r\n");
  AppendLogo();
  Append("%d %s\r\n", code, reason);
  Append("</h1>\r\n");
  UseOutput();
  return p;
}

static char *ServeErrorImpl(unsigned code, const char *reason) {
  size_t n;
  char *p, *s;
  struct Asset *a;
  LockInc(&shared->errors);
  ClearOutput();
  p = SetStatus(code, reason);
  s = xasprintf("/%d.html", code);
  a = GetAsset(s, strlen(s));
  free(s);
  if (!a) {
    return ServeDefaultErrorPage(p, code, reason);
  } else if (a->file) {
    LockInc(&shared->slurps);
    content = FreeLater(xslurp(a->file->path, &contentlength));
    return AppendContentType(p, "text/html; charset=utf-8");
  } else {
    content = (char *)ZIP_LFILE_CONTENT(zmap + a->lf);
    contentlength = GetZipLfileCompressedSize(zmap + a->lf);
    if (IsCompressed(a)) {
      n = GetZipLfileUncompressedSize(zmap + a->lf);
      if ((s = FreeLater(malloc(n))) && Inflate(s, n, content, contentlength)) {
        content = s;
        contentlength = n;
      } else {
        return ServeDefaultErrorPage(p, code, reason);
      }
    }
    if (Verify(content, contentlength, ZIP_LFILE_CRC32(zmap + a->lf))) {
      return AppendContentType(p, "text/html; charset=utf-8");
    } else {
      return ServeDefaultErrorPage(p, code, reason);
    }
  }
}

static char *ServeError(unsigned code, const char *reason) {
  LOGF("ERROR %d %s", code, reason);
  return ServeErrorImpl(code, reason);
}

static char *ServeFailure(unsigned code, const char *reason) {
  LOGF("FAILURE %d %s %s HTTP%02d %.*s %`'.*s %`'.*s %`'.*s %`'.*s", code,
       reason, DescribeClient(), msg.version, msg.xmethod.b - msg.xmethod.a,
       inbuf.p + msg.xmethod.a, HeaderLength(kHttpHost), HeaderData(kHttpHost),
       msg.uri.b - msg.uri.a, inbuf.p + msg.uri.a, HeaderLength(kHttpReferer),
       HeaderData(kHttpReferer), HeaderLength(kHttpUserAgent),
       HeaderData(kHttpUserAgent));
  return ServeErrorImpl(code, reason);
}

static char *ServeAssetCompressed(struct Asset *a) {
  uint32_t crc;
  LockInc(&shared->compressedresponses);
  DEBUGF("ServeAssetCompressed()");
  gzipped = true;
  crc = crc32_z(0, content, contentlength);
  WRITE32LE(gzip_footer + 0, crc);
  WRITE32LE(gzip_footer + 4, contentlength);
  content = FreeLater(Deflate(content, contentlength, &contentlength));
  return SetStatus(200, "OK");
}

static char *ServeAssetPrecompressed(struct Asset *a) {
  char *buf;
  size_t size;
  uint32_t crc;
  crc = ZIP_LFILE_CRC32(zmap + a->lf);
  size = GetZipLfileUncompressedSize(zmap + a->lf);
  if (ClientAcceptsGzip()) {
    LockInc(&shared->precompressedresponses);
    DEBUGF("ServeAssetPrecompressed()");
    gzipped = true;
    WRITE32LE(gzip_footer + 0, crc);
    WRITE32LE(gzip_footer + 4, size);
    return SetStatus(200, "OK");
  } else {
    LockInc(&shared->decompressedresponses);
    DEBUGF("ServeAssetDecompressed()");
    if ((buf = FreeLater(malloc(size))) &&
        Inflate(buf, size, content, contentlength) && Verify(buf, size, crc)) {
      content = buf;
      contentlength = size;
      return SetStatus(200, "OK");
    } else {
      return ServeError(500, "Internal Server Error");
    }
  }
}

static char *ServeAssetRange(struct Asset *a) {
  char *p;
  long rangestart, rangelength;
  DEBUGF("ServeAssetRange()");
  if (ParseHttpRange(HeaderData(kHttpRange), HeaderLength(kHttpRange),
                     contentlength, &rangestart, &rangelength) &&
      rangestart >= 0 && rangelength >= 0 && rangestart < contentlength &&
      rangestart + rangelength <= contentlength) {
    LockInc(&shared->partialresponses);
    p = SetStatus(206, "Partial Content");
    p = AppendContentRange(p, rangestart, rangelength, contentlength);
    content += rangestart;
    contentlength = rangelength;
    return p;
  } else {
    LockInc(&shared->badranges);
    LOGF("bad range %`'.*s", HeaderLength(kHttpRange), HeaderData(kHttpRange));
    p = SetStatus(416, "Range Not Satisfiable");
    p = AppendContentRange(p, -1, -1, contentlength);
    content = "";
    contentlength = 0;
    return p;
  }
}

static char *ServeAsset(struct Asset *a, const char *path, size_t pathlen) {
  int fd;
  char *p;
  void *data;
  size_t size;
  uint32_t crc;
  const char *ct;
  ct = GetContentType(a, path, pathlen);
  if (IsNotModified(a)) {
    LockInc(&shared->notmodifieds);
    p = SetStatus(304, "Not Modified");
  } else {
    if (!a->file) {
      content = (char *)ZIP_LFILE_CONTENT(zmap + a->lf);
      contentlength = GetZipLfileCompressedSize(zmap + a->lf);
    } else if (a->file->st.st_size) {
      size = a->file->st.st_size;
      if ((fd = open(a->file->path, O_RDONLY)) != -1) {
        data = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data != MAP_FAILED) {
          LockInc(&shared->maps);
          UnmapLater(fd, data, size);
          content = data;
          contentlength = size;
        } else {
          LockInc(&shared->mapfails);
          WARNF("mmap(%`'s) failed %s", a->file->path, strerror(errno));
          close(fd);
          return ServeError(500, "Internal Server Error");
        }
      } else {
        LockInc(&shared->openfails);
        WARNF("open(%`'s) failed %s", a->file->path, strerror(errno));
        if (errno == ENFILE) {
          LockInc(&shared->enfiles);
          return ServeError(503, "Service Unavailable");
        } else if (errno == EMFILE) {
          LockInc(&shared->emfiles);
          return ServeError(503, "Service Unavailable");
        } else {
          return ServeError(500, "Internal Server Error");
        }
      }
    } else {
      content = "";
      contentlength = 0;
    }
    if (IsCompressed(a)) {
      p = ServeAssetPrecompressed(a);
    } else if (msg.version >= 11 && HasHeader(kHttpRange)) {
      p = ServeAssetRange(a);
    } else if (!a->file) {
      LockInc(&shared->identityresponses);
      DEBUGF("ServeAssetZipIdentity(%`'s)", ct);
      if (Verify(content, contentlength, ZIP_LFILE_CRC32(zmap + a->lf))) {
        p = SetStatus(200, "OK");
      } else {
        return ServeError(500, "Internal Server Error");
      }
    } else if (ClientAcceptsGzip() &&
               (strlen(ct) >= 5 && !memcasecmp(ct, "text/", 5)) &&
               contentlength < 1024 * 1024 * 1024) {
      p = ServeAssetCompressed(a);
    } else {
      LockInc(&shared->identityresponses);
      DEBUGF("ServeAssetIdentity(%`'s)", ct);
      p = SetStatus(200, "OK");
    }
  }
  p = AppendContentType(p, ct);
  p = stpcpy(p, "Vary: Accept-Encoding\r\n");
  p = AppendHeader(p, "Last-Modified", a->lastmodifiedstr);
  if (msg.version >= 11) {
    p = AppendCache(p, cacheseconds);
    if (!IsCompressed(a)) {
      p = stpcpy(p, "Accept-Ranges: bytes\r\n");
    }
  }
  return p;
}

static char *GetAssetPath(uint64_t cf, size_t *out_size) {
  char *p1, *p2;
  size_t n1, n2;
  p1 = ZIP_CFILE_NAME(zmap + cf);
  n1 = ZIP_CFILE_NAMESIZE(zmap + cf);
  p2 = xmalloc(1 + n1 + 1);
  n2 = 1 + n1 + 1;
  p2[0] = '/';
  memcpy(p2 + 1, p1, n1);
  p2[1 + n1] = '\0';
  if (out_size) *out_size = 1 + n1;
  return p2;
}

static bool IsHiddenPath(const char *s) {
  size_t i;
  for (i = 0; i < hidepaths.n; ++i) {
    if (startswith(s, hidepaths.p[i])) {
      return true;
    }
  }
  return false;
}

static char *GetBasicAuthorization(size_t *z) {
  size_t n;
  const char *p, *q;
  p = inbuf.p + msg.headers[kHttpAuthorization].a;
  n = msg.headers[kHttpAuthorization].b - msg.headers[kHttpAuthorization].a;
  if ((q = memchr(p, ' ', n)) && SlicesEqualCase(p, q - p, "Basic", 5)) {
    return DecodeBase64(q + 1, n - (q + 1 - p), z);
  } else {
    return NULL;
  }
}

static void LaunchBrowser() {
  char openbrowsercommand[255];
  char *prog;
  if (IsWindows()) {
    prog = "explorer";
  } else if (IsXnu()) {
    prog = "open";
  } else {
    prog = "xdg-open";
  }
  struct in_addr addr = serveraddr.sin_addr;
  if (addr.s_addr == INADDR_ANY) addr.s_addr = htonl(INADDR_LOOPBACK);
  snprintf(openbrowsercommand, sizeof(openbrowsercommand), "%s http://%s:%d",
           prog, inet_ntoa(addr), ntohs(serveraddr.sin_port));
  DEBUGF("Opening browser with command %s\n", openbrowsercommand);
  system(openbrowsercommand);
}

static char *BadMethod(void) {
  LockInc(&shared->badmethods);
  return stpcpy(ServeError(405, "Method Not Allowed"), "Allow: GET, HEAD\r\n");
}

static int GetDecimalWidth(int x) {
  int w = x ? ceil(log10(x)) : 1;
  return w + (w - 1) / 3;
}

static int GetOctalWidth(int x) {
  return !x ? 1 : x < 8 ? 2 : 1 + bsr(x) / 3;
}

static const char *DescribeCompressionRatio(char rb[8], uint64_t lf) {
  long percent;
  if (ZIP_LFILE_COMPRESSIONMETHOD(zmap + lf) == kZipCompressionNone) {
    return "n/a";
  } else {
    percent = lround(100 - (double)GetZipLfileCompressedSize(zmap + lf) /
                               GetZipLfileUncompressedSize(zmap + lf) * 100);
    sprintf(rb, "%ld%%", MIN(999, MAX(-999, percent)));
    return rb;
  }
}

static char *ServeListing(void) {
  long x;
  ldiv_t y;
  int w[3];
  struct tm tm;
  const char *and;
  int64_t lastmod;
  uint64_t cf, lf;
  struct rusage ru;
  char *p, *q, *path;
  char rb[8], tb[64], *rp[6];
  size_t i, n, pathlen, rn[6];
  LockInc(&shared->listingrequests);
  if (msg.method != kHttpGet && msg.method != kHttpHead) return BadMethod();
  Append("\
<!doctype html>\r\n\
<meta charset=\"utf-8\">\r\n\
<title>redbean zip listing</title>\r\n\
<style>\r\n\
html { color: #111; font-family: sans-serif; }\r\n\
a { text-decoration: none; }\r\n\
pre a:hover { color: #00e; border-bottom: 1px solid #ccc; }\r\n\
h1 a { color: #111; }\r\n\
img { vertical-align: middle; }\r\n\
footer { color: #555; font-size: 10pt; }\r\n\
td { padding-right: 3em; }\r\n\
.eocdcomment { max-width: 800px; color: #333; font-size: 11pt; }\r\n\
</style>\r\n\
<header><h1>\r\n");
  AppendLogo();
  rp[0] = EscapeHtml(brand, -1, &rn[0]);
  AppendData(rp[0], rn[0]);
  free(rp[0]);
  Append("</h1>\r\n"
         "<div class=\"eocdcomment\">%.*s</div>\r\n"
         "<hr>\r\n"
         "</header>\r\n"
         "<pre>\r\n",
         strnlen(GetZipCdirComment(cdir), GetZipCdirCommentSize(cdir)),
         GetZipCdirComment(cdir));
  memset(w, 0, sizeof(w));
  n = GetZipCdirRecords(cdir);
  for (cf = GetZipCdirOffset(cdir); n--; cf += ZIP_CFILE_HDRSIZE(zmap + cf)) {
    CHECK_EQ(kZipCfileHdrMagic, ZIP_CFILE_MAGIC(zmap + cf));
    lf = GetZipCfileOffset(zmap + cf);
    path = GetAssetPath(cf, &pathlen);
    if (!IsHiddenPath(path)) {
      w[0] = min(80, max(w[0], strwidth(path, 0) + 2));
      w[1] = max(w[1], GetOctalWidth(GetZipCfileMode(zmap + cf)));
      w[2] = max(w[2], GetDecimalWidth(GetZipLfileUncompressedSize(zmap + lf)));
    }
    free(path);
  }
  n = GetZipCdirRecords(cdir);
  for (cf = GetZipCdirOffset(cdir); n--; cf += ZIP_CFILE_HDRSIZE(zmap + cf)) {
    CHECK_EQ(kZipCfileHdrMagic, ZIP_CFILE_MAGIC(zmap + cf));
    lf = GetZipCfileOffset(zmap + cf);
    path = GetAssetPath(cf, &pathlen);
    if (!IsHiddenPath(path)) {
      rp[0] = VisualizeControlCodes(path, pathlen, &rn[0]);
      rp[1] = EscapePath(path, pathlen, &rn[1]);
      rp[2] = EscapeHtml(rp[1], rn[1], &rn[2]);
      rp[3] = VisualizeControlCodes(ZIP_CFILE_COMMENT(zmap + cf),
                                    strnlen(ZIP_CFILE_COMMENT(zmap + cf),
                                            ZIP_CFILE_COMMENTSIZE(zmap + cf)),
                                    &rn[3]);
      rp[4] = EscapeHtml(rp[0], rn[0], &rn[4]);
      lastmod = GetZipCfileLastModified(zmap + cf);
      localtime_r(&lastmod, &tm);
      strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S %Z", &tm);
      if (IsCompressionMethodSupported(
              ZIP_LFILE_COMPRESSIONMETHOD(zmap + lf)) &&
          IsAcceptablePath(path, pathlen)) {
        Append("<a href=\"%.*s\">%-*.*s</a> %s  %0*o %4s  %,*ld  %'s\r\n",
               rn[2], rp[2], w[0], rn[4], rp[4], tb, w[1],
               GetZipCfileMode(zmap + cf), DescribeCompressionRatio(rb, lf),
               w[2], GetZipLfileUncompressedSize(zmap + lf), rp[3]);
      } else {
        Append("%-*.*s %s  %0*o %4s  %,*ld  %'s\r\n", w[0], rn[4], rp[4], tb,
               w[1], GetZipCfileMode(zmap + cf),
               DescribeCompressionRatio(rb, lf), w[2],
               GetZipLfileUncompressedSize(zmap + lf), rp[3]);
      }
      free(rp[4]);
      free(rp[3]);
      free(rp[2]);
      free(rp[1]);
      free(rp[0]);
    }
    free(path);
  }
  Append("</pre><footer><hr>\r\n");
  Append("<table border=\"0\"><tr>\r\n");
  Append("<td valign=\"top\">\r\n");
  Append("<a href=\"/statusz\">/statusz</a>\r\n");
  if (shared->connectionshandled) {
    Append("says your redbean<br>\r\n");
    AppendResourceReport(&shared->children, "<br>\r\n");
  }
  Append("<td valign=\"top\">\r\n");
  and = "";
  x = nowl() - startserver;
  y = ldiv(x, 24L * 60 * 60);
  if (y.quot) {
    Append("%,ld day%s ", y.quot, y.quot == 1 ? "" : "s");
    and = "and ";
  }
  y = ldiv(y.rem, 60 * 60);
  if (y.quot) {
    Append("%,ld hour%s ", y.quot, y.quot == 1 ? "" : "s");
    and = "and ";
  }
  y = ldiv(y.rem, 60);
  if (y.quot) {
    Append("%,ld minute%s ", y.quot, y.quot == 1 ? "" : "s");
    and = "and ";
  }
  Append("%s%,ld second%s of operation<br>\r\n", and, y.rem,
         y.rem == 1 ? "" : "s");
  x = shared->messageshandled;
  Append("%,ld message%s handled<br>\r\n", x, x == 1 ? "" : "s");
  x = shared->connectionshandled;
  Append("%,ld connection%s handled<br>\r\n", x, x == 1 ? "" : "s");
  x = shared->workers;
  Append("%,ld connection%s active<br>\r\n", x, x == 1 ? "" : "s");
  Append("</table>\r\n");
  Append("</footer>\r\n");
  p = SetStatus(200, "OK");
  p = AppendContentType(p, "text/html");
  p = AppendCache(p, 0);
  return CommitOutput(p);
}

static const char *MergeNames(const char *a, const char *b) {
  return FreeLater(xasprintf("%s.ru_utime", a));
}

static void AppendLong1(const char *a, long x) {
  if (x) Append("%s: %ld\r\n", a, x);
}

static void AppendLong2(const char *a, const char *b, long x) {
  if (x) Append("%s.%s: %ld\r\n", a, b, x);
}

static void AppendTimeval(const char *a, struct timeval *tv) {
  AppendLong2(a, "tv_sec", tv->tv_sec);
  AppendLong2(a, "tv_usec", tv->tv_usec);
}

static void AppendRusage(const char *a, struct rusage *ru) {
  AppendTimeval(MergeNames(a, "ru_utime"), &ru->ru_utime);
  AppendTimeval(MergeNames(a, "ru_stime"), &ru->ru_stime);
  AppendLong2(a, "ru_maxrss", ru->ru_maxrss);
  AppendLong2(a, "ru_ixrss", ru->ru_ixrss);
  AppendLong2(a, "ru_idrss", ru->ru_idrss);
  AppendLong2(a, "ru_isrss", ru->ru_isrss);
  AppendLong2(a, "ru_minflt", ru->ru_minflt);
  AppendLong2(a, "ru_majflt", ru->ru_majflt);
  AppendLong2(a, "ru_nswap", ru->ru_nswap);
  AppendLong2(a, "ru_inblock", ru->ru_inblock);
  AppendLong2(a, "ru_oublock", ru->ru_oublock);
  AppendLong2(a, "ru_msgsnd", ru->ru_msgsnd);
  AppendLong2(a, "ru_msgrcv", ru->ru_msgrcv);
  AppendLong2(a, "ru_nsignals", ru->ru_nsignals);
  AppendLong2(a, "ru_nvcsw", ru->ru_nvcsw);
  AppendLong2(a, "ru_nivcsw", ru->ru_nivcsw);
}

static char *ServeStatusz(void) {
  char *p;
  LockInc(&shared->statuszrequests);
  if (msg.method != kHttpGet && msg.method != kHttpHead) return BadMethod();
  AppendLong1("pid", getpid());
  AppendLong1("ppid", getppid());
  AppendLong1("now", nowl());
  AppendLong1("nowish", shared->nowish);
  AppendLong1("heartless", heartless);
  AppendLong1("gmtoff", gmtoff);
  AppendLong1("CLK_TCK", CLK_TCK);
  AppendLong1("startserver", startserver);
  AppendLong1("lastmeltdown", shared->lastmeltdown);
  AppendLong1("workers", shared->workers);
  AppendLong1("assets.n", assets.n);
  AppendLong1("accepterrors", shared->accepterrors);
  AppendLong1("acceptinterrupts", shared->acceptinterrupts);
  AppendLong1("acceptresets", shared->acceptresets);
  AppendLong1("badlengths", shared->badlengths);
  AppendLong1("badmessages", shared->badmessages);
  AppendLong1("badmethods", shared->badmethods);
  AppendLong1("badranges", shared->badranges);
  AppendLong1("closeerrors", shared->closeerrors);
  AppendLong1("compressedresponses", shared->compressedresponses);
  AppendLong1("connectionshandled", shared->connectionshandled);
  AppendLong1("connectsrefused", shared->connectsrefused);
  AppendLong1("continues", shared->continues);
  AppendLong1("decompressedresponses", shared->decompressedresponses);
  AppendLong1("deflates", shared->deflates);
  AppendLong1("dropped", shared->dropped);
  AppendLong1("dynamicrequests", shared->dynamicrequests);
  AppendLong1("emfiles", shared->emfiles);
  AppendLong1("enetdowns", shared->enetdowns);
  AppendLong1("enfiles", shared->enfiles);
  AppendLong1("enobufs", shared->enobufs);
  AppendLong1("enomems", shared->enomems);
  AppendLong1("enonets", shared->enonets);
  AppendLong1("errors", shared->errors);
  AppendLong1("expectsrefused", shared->expectsrefused);
  AppendLong1("failedchildren", shared->failedchildren);
  AppendLong1("forbiddens", shared->forbiddens);
  AppendLong1("forkerrors", shared->forkerrors);
  AppendLong1("frags", shared->frags);
  AppendLong1("fumbles", shared->fumbles);
  AppendLong1("http09", shared->http09);
  AppendLong1("http10", shared->http10);
  AppendLong1("http11", shared->http11);
  AppendLong1("http12", shared->http12);
  AppendLong1("hugepayloads", shared->hugepayloads);
  AppendLong1("identityresponses", shared->identityresponses);
  AppendLong1("inflates", shared->inflates);
  AppendLong1("listingrequests", shared->listingrequests);
  AppendLong1("loops", shared->loops);
  AppendLong1("mapfails", shared->mapfails);
  AppendLong1("maps", shared->maps);
  AppendLong1("meltdowns", shared->meltdowns);
  AppendLong1("messageshandled", shared->messageshandled);
  AppendLong1("missinglengths", shared->missinglengths);
  AppendLong1("netafrinic", shared->netafrinic);
  AppendLong1("netanonymous", shared->netanonymous);
  AppendLong1("netapnic", shared->netapnic);
  AppendLong1("netapple", shared->netapple);
  AppendLong1("netarin", shared->netarin);
  AppendLong1("netatt", shared->netatt);
  AppendLong1("netcogent", shared->netcogent);
  AppendLong1("netcomcast", shared->netcomcast);
  AppendLong1("netdod", shared->netdod);
  AppendLong1("netford", shared->netford);
  AppendLong1("netlacnic", shared->netlacnic);
  AppendLong1("netloopback", shared->netloopback);
  AppendLong1("netother", shared->netother);
  AppendLong1("netprivate", shared->netprivate);
  AppendLong1("netprudential", shared->netprudential);
  AppendLong1("netripe", shared->netripe);
  AppendLong1("nettestnet", shared->nettestnet);
  AppendLong1("netusps", shared->netusps);
  AppendLong1("notfounds", shared->notfounds);
  AppendLong1("notmodifieds", shared->notmodifieds);
  AppendLong1("openfails", shared->openfails);
  AppendLong1("partialresponses", shared->partialresponses);
  AppendLong1("payloaddisconnects", shared->payloaddisconnects);
  AppendLong1("pipelinedrequests", shared->pipelinedrequests);
  AppendLong1("precompressedresponses", shared->precompressedresponses);
  AppendLong1("readerrors", shared->readerrors);
  AppendLong1("readinterrupts", shared->readinterrupts);
  AppendLong1("readresets", shared->readresets);
  AppendLong1("readtimeouts", shared->readtimeouts);
  AppendLong1("redirects", shared->redirects);
  AppendLong1("reloads", shared->reloads);
  AppendLong1("rewrites", shared->rewrites);
  AppendLong1("serveroptions", shared->serveroptions);
  AppendLong1("shutdowns", shared->shutdowns);
  AppendLong1("slowloris", shared->slowloris);
  AppendLong1("slurps", shared->slurps);
  AppendLong1("statfails", shared->statfails);
  AppendLong1("staticrequests", shared->staticrequests);
  AppendLong1("stats", shared->stats);
  AppendLong1("statuszrequests", shared->statuszrequests);
  AppendLong1("synchronizationfailures", shared->synchronizationfailures);
  AppendLong1("terminatedchildren", shared->terminatedchildren);
  AppendLong1("thiscorruption", shared->thiscorruption);
  AppendLong1("transfersrefused", shared->transfersrefused);
  AppendLong1("urisrefused", shared->urisrefused);
  AppendLong1("verifies", shared->verifies);
  AppendLong1("writeerrors", shared->writeerrors);
  AppendLong1("writeinterruputs", shared->writeinterruputs);
  AppendLong1("writeresets", shared->writeresets);
  AppendLong1("writetimeouts", shared->writetimeouts);
  AppendRusage("server", &shared->server);
  AppendRusage("children", &shared->children);
  p = SetStatus(200, "OK");
  p = AppendContentType(p, "text/plain");
  p = AppendCache(p, 0);
  return CommitOutput(p);
}

static char *RedirectSlash(void) {
  size_t n;
  char *p, *e;
  LockInc(&shared->redirects);
  p = SetStatus(307, "Temporary Redirect");
  p = stpcpy(p, "Location: ");
  e = EscapePath(url.path.p, url.path.n, &n);
  p = mempcpy(p, e, n);
  p = stpcpy(p, "/\r\n");
  free(e);
  return p;
}

static char *RoutePath(const char *, size_t);
static char *ServeIndex(const char *path, size_t pathlen) {
  size_t i, n;
  char *p, *q;
  p = NULL;
  for (i = 0; !p && i < ARRAYLEN(kIndexPaths); ++i) {
    q = MergePaths(path, pathlen, kIndexPaths[i], strlen(kIndexPaths[i]), &n);
    p = RoutePath(q, n);
    free(q);
  }
  return p;
}

static bool IsLua(struct Asset *a) {
  if (a->file) return endswith(a->file->path, ".lua");
  return ZIP_LFILE_NAMESIZE(zmap + a->lf) >= 4 &&
         !memcmp(ZIP_LFILE_NAME(zmap + a->lf) +
                     ZIP_LFILE_NAMESIZE(zmap + a->lf) - 4,
                 ".lua", 4);
}

static char *GetLuaResponse(void) {
  char *p;
  if (!(p = luaheaderp)) {
    p = SetStatus(200, "OK");
    p = AppendContentType(p, "text/html");
  }
  return p;
}

static char *ServeLua(struct Asset *a, const char *path, size_t pathlen) {
  char *code;
  effectivepath.p = path;
  effectivepath.n = pathlen;
  if ((code = FreeLater(LoadAsset(a, NULL)))) {
    if (luaL_dostring(L, code) == LUA_OK) {
      return CommitOutput(GetLuaResponse());
    } else {
      WARNF("%s", lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  }
  return ServeError(500, "Internal Server Error");
}

static char *HandleAsset(struct Asset *a, const char *path, size_t pathlen) {
#ifndef STATIC
  if (IsLua(a)) {
    LockInc(&shared->dynamicrequests);
    return ServeLua(a, path, pathlen);
  }
#endif
  if (msg.method == kHttpGet || msg.method == kHttpHead) {
    LockInc(&shared->staticrequests);
    return stpcpy(ServeAsset(a, path, pathlen),
                  "X-Content-Type-Options: nosniff\r\n");
  } else {
    return BadMethod();
  }
}

static char *HandleRedirect(struct Redirect *r) {
  int code;
  struct Asset *a;
  if (!r->code && (a = GetAsset(r->location, strlen(r->location)))) {
    LockInc(&shared->rewrites);
    DEBUGF("rewriting to %`'s", r->location);
    if (!HasString(&loops, r->location)) {
      AddString(&loops, r->location);
      return RoutePath(r->location, strlen(r->location));
    } else {
      LockInc(&shared->loops);
      return SetStatus(508, "Loop Detected");
    }
  } else if (msg.version < 10) {
    return ServeError(505, "HTTP Version Not Supported");
  } else {
    LockInc(&shared->redirects);
    code = r->code;
    if (!code) code = 307;
    DEBUGF("%d redirecting %`'s", code, r->location);
    return AppendHeader(SetStatus(code, GetHttpReason(code)), "Location",
                        FreeLater(EncodeHttpHeaderValue(r->location, -1, 0)));
  }
}

static char *HandleFolder(const char *path, size_t pathlen) {
  char *p;
  if (url.path.n && url.path.p[url.path.n - 1] != '/' &&
      SlicesEqual(path, pathlen, url.path.p, url.path.n)) {
    return RedirectSlash();
  }
  if ((p = ServeIndex(path, pathlen))) {
    return p;
  } else {
    LockInc(&shared->forbiddens);
    LOGF("directory %`'.*s lacks index page", pathlen, path);
    return ServeError(403, "Forbidden");
  }
}

static char *RoutePath(const char *path, size_t pathlen) {
  int m;
  long r;
  char *p;
  struct Asset *a;
  DEBUGF("RoutePath(%`'.*s)", pathlen, path);
  if ((a = GetAsset(path, pathlen))) {
    if ((m = GetMode(a)) & 0004) {
      if (!S_ISDIR(m)) {
        return HandleAsset(a, path, pathlen);
      } else {
        return HandleFolder(path, pathlen);
      }
    } else {
      LockInc(&shared->forbiddens);
      LOGF("asset %`'.*s %#o isn't readable", pathlen, path, m);
      return ServeError(403, "Forbidden");
    }
  } else if ((r = FindRedirect(path, pathlen)) != -1) {
    return HandleRedirect(redirects.p + r);
  } else {
    return NULL;
  }
}

static char *RouteHost(const char *host, size_t hostlen, const char *path,
                       size_t pathlen) {
  size_t hn;
  char *hp, *p;
  hn = 1 + hostlen + url.path.n;
  hp = FreeLater(xmalloc(3 + 1 + hn));
  hp[0] = '/';
  mempcpy(mempcpy(hp + 1, host, hostlen), path, pathlen);
  if ((p = RoutePath(hp, hn))) return p;
  if (ParseIp(host, hostlen) == -1) {
    if (hostlen > 4 && !memcmp(host, "www.", 4)) {
      mempcpy(mempcpy(hp + 1, host + 4, hostlen - 4), path, pathlen);
      if ((p = RoutePath(hp, hn - 4))) return p;
    } else {
      mempcpy(mempcpy(mempcpy(hp + 1, "www.", 4), host, hostlen), path,
              pathlen);
      if ((p = RoutePath(hp, hn + 4))) return p;
    }
  }
  return NULL;
}

static char *Route(const char *host, size_t hostlen, const char *path,
                   size_t pathlen) {
  char *p;
  if (hostlen && (p = RouteHost(host, hostlen, path, pathlen))) {
    return p;
  }
  if (SlicesEqual(path, pathlen, "/", 1)) {
    if ((p = ServeIndex("/", 1))) return p;
    return ServeListing();
  } else if ((p = RoutePath(path, pathlen))) {
    return p;
  } else if (SlicesEqual(path, pathlen, "/statusz", 8)) {
    return ServeStatusz();
  } else {
    LockInc(&shared->notfounds);
    return ServeError(404, "Not Found");
  }
}

static const char *LuaCheckPath(lua_State *L, int idx, size_t *pathlen) {
  const char *path;
  if (lua_isnoneornil(L, idx)) {
    path = url.path.p;
    *pathlen = url.path.n;
  } else {
    path = luaL_checklstring(L, idx, pathlen);
    if (!IsReasonablePath(path, *pathlen)) {
      WARNF("bad path %`'.*s", *pathlen, path);
      luaL_argerror(L, idx, "bad path");
      unreachable;
    }
  }
  return path;
}

static const char *LuaCheckHost(lua_State *L, int idx, size_t *hostlen) {
  const char *host;
  if (lua_isnoneornil(L, idx)) {
    host = url.host.p;
    *hostlen = url.host.n;
  } else {
    host = luaL_checklstring(L, idx, hostlen);
    if (!IsAcceptableHost(host, *hostlen)) {
      WARNF("bad host %`'.*s", *hostlen, host);
      luaL_argerror(L, idx, "bad host");
      unreachable;
    }
  }
  return host;
}

static int LuaServeListing(lua_State *L) {
  luaheaderp = ServeListing();
  return 0;
}

static int LuaServeStatusz(lua_State *L) {
  luaheaderp = ServeStatusz();
  return 0;
}

static int LuaServeAsset(lua_State *L) {
  size_t pathlen;
  struct Asset *a;
  const char *path;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen)) && !S_ISDIR(GetMode(a))) {
    luaheaderp = ServeAsset(a, path, pathlen);
    lua_pushboolean(L, true);
  } else {
    lua_pushboolean(L, false);
  }
  return 1;
}

static int LuaServeIndex(lua_State *L) {
  size_t pathlen;
  const char *path;
  path = LuaCheckPath(L, 1, &pathlen);
  lua_pushboolean(L, !!(luaheaderp = ServeIndex(path, pathlen)));
  return 1;
}

static int LuaRoutePath(lua_State *L) {
  size_t pathlen;
  const char *path;
  path = LuaCheckPath(L, 1, &pathlen);
  lua_pushboolean(L, !!(luaheaderp = RoutePath(path, pathlen)));
  return 1;
}

static int LuaRouteHost(lua_State *L) {
  size_t hostlen, pathlen;
  const char *host, *path;
  host = LuaCheckHost(L, 1, &hostlen);
  path = LuaCheckPath(L, 2, &pathlen);
  lua_pushboolean(L, !!(luaheaderp = RouteHost(host, hostlen, path, pathlen)));
  return 1;
}

static int LuaRoute(lua_State *L) {
  size_t hostlen, pathlen;
  const char *host, *path;
  host = LuaCheckHost(L, 1, &hostlen);
  path = LuaCheckPath(L, 2, &pathlen);
  lua_pushboolean(L, !!(luaheaderp = Route(host, hostlen, path, pathlen)));
  return 1;
}

static int LuaRespond(lua_State *L, char *respond(unsigned, const char *)) {
  char *p;
  int code;
  size_t reasonlen;
  const char *reason;
  code = luaL_checkinteger(L, 1);
  if (!(100 <= code && code <= 999)) {
    luaL_argerror(L, 1, "bad status code");
    unreachable;
  }
  if (lua_isnoneornil(L, 2)) {
    luaheaderp = respond(code, GetHttpReason(code));
  } else {
    reason = lua_tolstring(L, 2, &reasonlen);
    if (reasonlen < 128 && (p = EncodeHttpHeaderValue(reason, reasonlen, 0))) {
      luaheaderp = respond(code, p);
      free(p);
    } else {
      luaL_argerror(L, 2, "invalid");
      unreachable;
    }
  }
  return 0;
}

static int LuaSetStatus(lua_State *L) {
  return LuaRespond(L, SetStatus);
}

static int LuaServeError(lua_State *L) {
  return LuaRespond(L, ServeError);
}

static int LuaLoadAsset(lua_State *L) {
  void *data;
  struct Asset *a;
  const char *path;
  size_t size, pathlen;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen))) {
    if (!a->file && !IsCompressed(a)) {
      /* fast path: this avoids extra copy */
      data = ZIP_LFILE_CONTENT(zmap + a->lf);
      size = GetZipLfileUncompressedSize(zmap + a->lf);
      if (Verify(data, size, ZIP_LFILE_CRC32(zmap + a->lf))) {
        lua_pushlstring(L, data, size);
        return 1;
      }
    } else if ((data = LoadAsset(a, &size))) {
      lua_pushlstring(L, data, size);
      free(data);
      return 1;
    }
  }
  return 0;
}

static int LuaGetDate(lua_State *L) {
  lua_pushinteger(L, shared->nowish);
  return 1;
}

static int LuaGetVersion(lua_State *L) {
  lua_pushinteger(L, msg.version);
  return 1;
}

static int LuaGetMethod(lua_State *L) {
  if (msg.method) {
    lua_pushstring(L, kHttpMethod[msg.method]);
  } else {
    lua_pushlstring(L, inbuf.p + msg.xmethod.a, msg.xmethod.b - msg.xmethod.a);
  }
  return 1;
}

static int LuaGetAddr(lua_State *L, void GetAddr(uint32_t *, uint16_t *)) {
  uint32_t ip;
  uint16_t port;
  GetAddr(&ip, &port);
  lua_pushinteger(L, ip);
  lua_pushinteger(L, port);
  return 2;
}

static int LuaGetServerAddr(lua_State *L) {
  return LuaGetAddr(L, GetServerAddr);
}

static int LuaGetClientAddr(lua_State *L) {
  return LuaGetAddr(L, GetClientAddr);
}

static int LuaGetRemoteAddr(lua_State *L) {
  return LuaGetAddr(L, GetRemoteAddr);
}

static int LuaFormatIp(lua_State *L) {
  char b[16];
  uint32_t ip;
  ip = htonl(luaL_checkinteger(L, 1));
  inet_ntop(AF_INET, &ip, b, sizeof(b));
  lua_pushstring(L, b);
  return 1;
}

static int LuaParseIp(lua_State *L) {
  size_t n;
  const char *s;
  s = luaL_checklstring(L, 1, &n);
  lua_pushinteger(L, ParseIp(s, n));
  return 1;
}

static int LuaIsIp(lua_State *L, bool IsIp(uint32_t)) {
  lua_pushboolean(L, IsIp(luaL_checkinteger(L, 1)));
  return 1;
}

static int LuaIsPublicIp(lua_State *L) {
  return LuaIsIp(L, IsPublicIp);
}

static int LuaIsPrivateIp(lua_State *L) {
  return LuaIsIp(L, IsPrivateIp);
}

static int LuaIsLoopbackIp(lua_State *L) {
  return LuaIsIp(L, IsLoopbackIp);
}

static int LuaCategorizeIp(lua_State *L) {
  lua_pushstring(L, GetIpCategoryName(CategorizeIp(luaL_checkinteger(L, 1))));
  return 1;
}

static int LuaGetUrl(lua_State *L) {
  char *p;
  size_t n;
  p = EncodeUrl(&url, &n);
  lua_pushlstring(L, p, n);
  free(p);
  return 1;
}

static void LuaPushUrlView(lua_State *L, struct UrlView *v) {
  if (v->p) {
    lua_pushlstring(L, v->p, v->n);
  } else {
    lua_pushnil(L);
  }
}

static int LuaGetScheme(lua_State *L) {
  LuaPushUrlView(L, &url.scheme);
  return 1;
}

static int LuaGetPath(lua_State *L) {
  LuaPushUrlView(L, &url.path);
  return 1;
}

static int LuaGetEffectivePath(lua_State *L) {
  lua_pushlstring(L, effectivepath.p, effectivepath.n);
  return 1;
}

static int LuaGetFragment(lua_State *L) {
  LuaPushUrlView(L, &url.fragment);
  return 1;
}

static int LuaGetUser(lua_State *L) {
  size_t n;
  const char *p, *q;
  if (url.user.p) {
    LuaPushUrlView(L, &url.user);
  } else if ((p = GetBasicAuthorization(&n))) {
    if (!(q = memchr(p, ':', n))) q = p + n;
    lua_pushlstring(L, p, q - p);
    free(p);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaGetPass(lua_State *L) {
  size_t n;
  const char *p, *q;
  if (url.user.p) {
    LuaPushUrlView(L, &url.pass);
  } else if ((p = GetBasicAuthorization(&n))) {
    if ((q = memchr(p, ':', n))) {
      lua_pushlstring(L, q + 1, p + n - (q + 1));
    } else {
      lua_pushnil(L);
    }
    free(p);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaGetHost(lua_State *L) {
  char b[16];
  if (url.host.n) {
    lua_pushlstring(L, url.host.p, url.host.n);
  } else {
    inet_ntop(AF_INET, &serveraddr.sin_addr.s_addr, b, sizeof(b));
    lua_pushstring(L, b);
  }
  return 1;
}

static int LuaGetPort(lua_State *L) {
  int i, x = 0;
  for (i = 0; i < url.port.n; ++i) x = url.port.p[i] - '0' + x * 10;
  if (!x) x = ntohs(serveraddr.sin_port);
  lua_pushinteger(L, x);
  return 1;
}

static int LuaFormatHttpDateTime(lua_State *L) {
  char buf[30];
  lua_pushstring(L, FormatUnixHttpDateTime(buf, luaL_checkinteger(L, 1)));
  return 1;
}

static int LuaParseHttpDateTime(lua_State *L) {
  size_t n;
  const char *s;
  s = luaL_checklstring(L, 1, &n);
  lua_pushinteger(L, ParseHttpDateTime(s, n));
  return 1;
}

static int LuaGetPayload(lua_State *L) {
  lua_pushlstring(L, inbuf.p + hdrsize, msgsize - hdrsize);
  return 1;
}

static void LuaPushLatin1(lua_State *L, const char *s, size_t n) {
  char *t;
  size_t m;
  t = DecodeLatin1(s, n, &m);
  lua_pushlstring(L, t, m);
  free(t);
}

static char *FoldHeader(int h, size_t *z) {
  char *p;
  size_t i, n, m;
  struct HttpRequestHeader *x;
  n = msg.headers[h].b - msg.headers[h].a;
  p = xmalloc(n);
  memcpy(p, inbuf.p + msg.headers[h].a, n);
  for (i = 0; i < msg.xheaders.n; ++i) {
    x = msg.xheaders.p + i;
    if (GetHttpHeader(inbuf.p + x->k.a, x->k.b - x->k.a) == h) {
      m = x->v.b - x->v.a;
      p = xrealloc(p, n + 2 + m);
      memcpy(mempcpy(p + n, ", ", 2), inbuf.p + x->v.a, m);
      n += 2 + m;
    }
  }
  *z = n;
  return p;
}

static int LuaGetHeader(lua_State *L) {
  int h;
  char *val;
  const char *key;
  size_t i, keylen, vallen;
  key = luaL_checklstring(L, 1, &keylen);
  if ((h = GetHttpHeader(key, keylen)) != -1) {
    if (msg.headers[h].a) {
      if (!kHttpRepeatable[h]) {
        LuaPushLatin1(L, inbuf.p + msg.headers[h].a,
                      msg.headers[h].b - msg.headers[h].a);
      } else {
        val = FoldHeader(h, &vallen);
        LuaPushLatin1(L, val, vallen);
        free(val);
      }
      return 1;
    }
  } else {
    for (i = 0; i < msg.xheaders.n; ++i) {
      if (SlicesEqualCase(key, keylen, inbuf.p + msg.xheaders.p[i].k.a,
                          msg.xheaders.p[i].k.b - msg.xheaders.p[i].k.a)) {
        LuaPushLatin1(L, inbuf.p + msg.xheaders.p[i].v.a,
                      msg.xheaders.p[i].v.b - msg.xheaders.p[i].v.a);
        return 1;
      }
    }
  }
  lua_pushnil(L);
  return 1;
}

static int LuaGetHeaders(lua_State *L) {
  size_t i;
  char *name;
  lua_newtable(L);
  for (i = 0; i < kHttpHeadersMax; ++i) {
    if (msg.headers[i].a) {
      LuaPushLatin1(L, inbuf.p + msg.headers[i].a,
                    msg.headers[i].b - msg.headers[i].a);
      lua_setfield(L, -2, GetHttpHeaderName(i));
    }
  }
  for (i = 0; i < msg.xheaders.n; ++i) {
    LuaPushLatin1(L, inbuf.p + msg.xheaders.p[i].v.a,
                  msg.xheaders.p[i].v.b - msg.xheaders.p[i].v.a);
    lua_setfield(L, -2,
                 (name = DecodeLatin1(
                      inbuf.p + msg.xheaders.p[i].k.a,
                      msg.xheaders.p[i].k.b - msg.xheaders.p[i].k.a, 0)));
    free(name);
  }
  return 1;
}

static int LuaSetHeader(lua_State *L) {
  int h;
  ssize_t rc;
  char *p, *q;
  const char *key, *val, *eval;
  size_t i, keylen, vallen, evallen;
  key = luaL_checklstring(L, 1, &keylen);
  val = luaL_checklstring(L, 2, &vallen);
  if ((h = GetHttpHeader(key, keylen)) == -1) {
    if (!IsValidHttpToken(key, keylen)) {
      luaL_argerror(L, 1, "invalid");
      unreachable;
    }
  }
  if (!(eval = EncodeHttpHeaderValue(val, vallen, &evallen))) {
    luaL_argerror(L, 2, "invalid");
    unreachable;
  }
  p = GetLuaResponse();
  while (p - hdrbuf.p + keylen + 2 + evallen + 2 + 512 > hdrbuf.n) {
    hdrbuf.n += hdrbuf.n >> 1;
    q = xrealloc(hdrbuf.p, hdrbuf.n);
    luaheaderp = p = q + (p - hdrbuf.p);
    hdrbuf.p = q;
  }
  switch (h) {
    case kHttpConnection:
      if (evallen != 5 || memcmp(eval, "close", 5)) {
        luaL_argerror(L, 2, "unsupported");
        unreachable;
      }
      connectionclose = true;
      break;
    case kHttpContentType:
      p = AppendContentType(p, eval);
      break;
    case kHttpServer:
      branded = true;
      p = AppendHeader(p, "Server", eval);
      break;
    default:
      p = AppendHeader(p, key, eval);
      break;
  }
  luaheaderp = p;
  free(eval);
  return 0;
}

static int LuaHasParam(lua_State *L) {
  size_t i, n;
  const char *s;
  s = luaL_checklstring(L, 1, &n);
  for (i = 0; i < url.params.n; ++i) {
    if (SlicesEqual(s, n, url.params.p[i].key.p, url.params.p[i].key.n)) {
      lua_pushboolean(L, true);
      return 1;
    }
  }
  lua_pushboolean(L, false);
  return 1;
}

static int LuaGetParam(lua_State *L) {
  size_t i, n;
  const char *s;
  s = luaL_checklstring(L, 1, &n);
  for (i = 0; i < url.params.n; ++i) {
    if (SlicesEqual(s, n, url.params.p[i].key.p, url.params.p[i].key.n)) {
      if (url.params.p[i].val.p) {
        lua_pushlstring(L, url.params.p[i].val.p, url.params.p[i].val.n);
        return 1;
      } else {
        break;
      }
    }
  }
  lua_pushnil(L);
  return 1;
}

static void LuaPushUrlParams(lua_State *L, struct UrlParams *h) {
  size_t i;
  if (h->p) {
    lua_newtable(L);
    for (i = 0; i < h->n; ++i) {
      lua_newtable(L);
      lua_pushlstring(L, h->p[i].key.p, h->p[i].key.n);
      lua_seti(L, -2, 1);
      if (h->p[i].val.p) {
        lua_pushlstring(L, h->p[i].val.p, h->p[i].val.n);
        lua_seti(L, -2, 2);
      }
      lua_seti(L, -2, i + 1);
    }
  } else {
    lua_pushnil(L);
  }
}

static int LuaGetParams(lua_State *L) {
  LuaPushUrlParams(L, &url.params);
  return 1;
}

static int LuaParseParams(lua_State *L) {
  void *m;
  size_t size;
  const char *data;
  struct UrlParams h;
  data = luaL_checklstring(L, 1, &size);
  memset(&h, 0, sizeof(h));
  m = ParseParams(data, size, &h);
  LuaPushUrlParams(L, &h);
  free(h.p);
  free(m);
  return 1;
}

static void LuaSetUrlView(lua_State *L, struct UrlView *v, const char *k) {
  LuaPushUrlView(L, v);
  lua_setfield(L, -2, k);
}

static int LuaParseUrl(lua_State *L) {
  void *m;
  size_t n;
  struct Url h;
  const char *p;
  p = luaL_checklstring(L, 1, &n);
  m = ParseUrl(p, n, &h);
  lua_newtable(L);
  LuaSetUrlView(L, &h.scheme, "scheme");
  LuaSetUrlView(L, &h.user, "user");
  LuaSetUrlView(L, &h.pass, "pass");
  LuaSetUrlView(L, &h.host, "host");
  LuaSetUrlView(L, &h.port, "port");
  LuaSetUrlView(L, &h.path, "path");
  LuaSetUrlView(L, &h.fragment, "fragment");
  LuaPushUrlParams(L, &h.params);
  lua_setfield(L, -2, "params");
  free(h.params.p);
  free(m);
  return 1;
}

static int LuaParseHost(lua_State *L) {
  void *m;
  size_t n;
  struct Url h;
  const char *p;
  memset(&h, 0, sizeof(h));
  p = luaL_checklstring(L, 1, &n);
  m = ParseHost(p, n, &h);
  lua_newtable(L);
  LuaPushUrlView(L, &h.host);
  LuaPushUrlView(L, &h.port);
  free(m);
  return 1;
}

static int LuaEncodeUrl(lua_State *L) {
  void *m;
  size_t size;
  struct Url h;
  int i, j, k, n;
  const char *data;
  if (!lua_isnil(L, 1)) {
    memset(&h, 0, sizeof(h));
    luaL_checktype(L, 1, LUA_TTABLE);
    if (lua_getfield(L, 1, "scheme"))
      h.scheme.p = lua_tolstring(L, -1, &h.scheme.n);
    if (lua_getfield(L, 1, "fragment"))
      h.fragment.p = lua_tolstring(L, -1, &h.fragment.n);
    if (lua_getfield(L, 1, "user")) h.user.p = lua_tolstring(L, -1, &h.user.n);
    if (lua_getfield(L, 1, "pass")) h.pass.p = lua_tolstring(L, -1, &h.pass.n);
    if (lua_getfield(L, 1, "host")) h.host.p = lua_tolstring(L, -1, &h.host.n);
    if (lua_getfield(L, 1, "port")) h.port.p = lua_tolstring(L, -1, &h.port.n);
    if (lua_getfield(L, 1, "path")) h.path.p = lua_tolstring(L, -1, &h.path.n);
    if (lua_getfield(L, 1, "params")) {
      luaL_checktype(L, -1, LUA_TTABLE);
      lua_len(L, -1);
      n = lua_tointeger(L, -1);
      for (i = -2, k = 0, j = 1; j <= n; ++j) {
        if (lua_geti(L, i--, j)) {
          luaL_checktype(L, -1, LUA_TTABLE);
          if (lua_geti(L, -1, 1)) {
            h.params.p =
                xrealloc(h.params.p, ++h.params.n * sizeof(*h.params.p));
            h.params.p[h.params.n - 1].key.p =
                lua_tolstring(L, -1, &h.params.p[h.params.n - 1].key.n);
            if (lua_geti(L, -2, 2)) {
              h.params.p[h.params.n - 1].val.p =
                  lua_tolstring(L, -1, &h.params.p[h.params.n - 1].val.n);
            } else {
              h.params.p[h.params.n - 1].val.p = 0;
              h.params.p[h.params.n - 1].val.n = 0;
            }
          }
          i--;
        }
        i--;
      }
    }
    data = EncodeUrl(&h, &size);
    lua_pushlstring(L, data, size);
    free(data);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaWrite(lua_State *L) {
  size_t size;
  const char *data;
  if (!lua_isnil(L, 1)) {
    data = luaL_checklstring(L, 1, &size);
    AppendData(data, size);
  }
  return 0;
}

static int LuaCheckControlFlags(lua_State *L, int idx) {
  int f = luaL_checkinteger(L, idx);
  if (f & ~(kControlWs | kControlC0 | kControlC1)) {
    luaL_argerror(L, idx, "invalid control flags");
    unreachable;
  }
  return f;
}

static int LuaHasControlCodes(lua_State *L) {
  int f;
  size_t n;
  const char *p;
  p = luaL_checklstring(L, 1, &n);
  f = LuaCheckControlFlags(L, 2);
  lua_pushboolean(L, HasControlCodes(p, n, f));
  return 1;
}

static int LuaIsValid(lua_State *L, bool IsValid(const char *, size_t)) {
  size_t size;
  const char *data;
  data = luaL_checklstring(L, 1, &size);
  lua_pushboolean(L, IsValid(data, size));
  return 1;
}

static int LuaIsValidHttpToken(lua_State *L) {
  return LuaIsValid(L, IsValidHttpToken);
}

static int LuaIsAcceptablePath(lua_State *L) {
  return LuaIsValid(L, IsAcceptablePath);
}

static int LuaIsReasonablePath(lua_State *L) {
  return LuaIsValid(L, IsReasonablePath);
}

static int LuaIsAcceptableHost(lua_State *L) {
  return LuaIsValid(L, IsAcceptableHost);
}

static int LuaIsAcceptablePort(lua_State *L) {
  return LuaIsValid(L, IsAcceptablePort);
}

static noinline int LuaCoderImpl(lua_State *L,
                                 char *Coder(const char *, size_t, size_t *)) {
  void *p;
  size_t n;
  p = luaL_checklstring(L, 1, &n);
  p = Coder(p, n, &n);
  lua_pushlstring(L, p, n);
  free(p);
  return 1;
}

static noinline int LuaCoder(lua_State *L,
                             char *Coder(const char *, size_t, size_t *)) {
  return LuaCoderImpl(L, Coder);
}

static int LuaUnderlong(lua_State *L) {
  return LuaCoder(L, Underlong);
}

static int LuaEncodeBase64(lua_State *L) {
  return LuaCoder(L, EncodeBase64);
}

static int LuaDecodeBase64(lua_State *L) {
  return LuaCoder(L, DecodeBase64);
}

static int LuaDecodeLatin1(lua_State *L) {
  return LuaCoder(L, DecodeLatin1);
}

static int LuaEscapeHtml(lua_State *L) {
  return LuaCoder(L, EscapeHtml);
}

static int LuaEscapeParam(lua_State *L) {
  return LuaCoder(L, EscapeParam);
}

static int LuaEscapePath(lua_State *L) {
  return LuaCoder(L, EscapePath);
}

static int LuaEscapeHost(lua_State *L) {
  return LuaCoder(L, EscapeHost);
}

static int LuaEscapeIp(lua_State *L) {
  return LuaCoder(L, EscapeIp);
}

static int LuaEscapeUser(lua_State *L) {
  return LuaCoder(L, EscapeUser);
}

static int LuaEscapePass(lua_State *L) {
  return LuaCoder(L, EscapePass);
}

static int LuaEscapeSegment(lua_State *L) {
  return LuaCoder(L, EscapeSegment);
}

static int LuaEscapeFragment(lua_State *L) {
  return LuaCoder(L, EscapeFragment);
}

static int LuaEscapeLiteral(lua_State *L) {
  return LuaCoder(L, EscapeJsStringLiteral);
}

static int LuaVisualizeControlCodes(lua_State *L) {
  return LuaCoder(L, VisualizeControlCodes);
}

static int LuaEncodeLatin1(lua_State *L) {
  int f;
  char *p;
  size_t n;
  p = luaL_checklstring(L, 1, &n);
  f = LuaCheckControlFlags(L, 2);
  p = EncodeLatin1(p, n, &n, f);
  lua_pushlstring(L, p, n);
  free(p);
  return 1;
}

static int LuaIndentLines(lua_State *L) {
  void *p;
  size_t n, j;
  p = luaL_checklstring(L, 1, &n);
  j = luaL_optinteger(L, 2, 1);
  if (!(0 <= j && j <= 65535)) {
    luaL_argerror(L, 2, "not in range 0..65535");
    unreachable;
  }
  p = IndentLines(p, n, &n, j);
  lua_pushlstring(L, p, n);
  free(p);
  return 1;
}

static int LuaGetMonospaceWidth(lua_State *L) {
  int w;
  if (lua_isinteger(L, 1)) {
    w = wcwidth(lua_tointeger(L, 1));
  } else if (lua_isstring(L, 1)) {
    w = strwidth(luaL_checkstring(L, 1), luaL_optinteger(L, 2, 0) & 7);
  } else {
    luaL_argerror(L, 1, "not integer or string");
    unreachable;
  }
  lua_pushinteger(L, w);
  return 1;
}

static int LuaPopcnt(lua_State *L) {
  lua_pushinteger(L, popcnt(luaL_checkinteger(L, 1)));
  return 1;
}

static int LuaBsr(lua_State *L) {
  long x;
  if ((x = luaL_checkinteger(L, 1))) {
    lua_pushinteger(L, bsr(x));
    return 1;
  } else {
    luaL_argerror(L, 1, "zero");
    unreachable;
  }
}

static int LuaBsf(lua_State *L) {
  long x;
  if ((x = luaL_checkinteger(L, 1))) {
    lua_pushinteger(L, bsf(x));
    return 1;
  } else {
    luaL_argerror(L, 1, "zero");
    unreachable;
  }
}

static int LuaHash(lua_State *L, uint32_t H(uint32_t, const void *, size_t)) {
  long i;
  size_t n;
  const char *p;
  i = luaL_checkinteger(L, 1);
  p = luaL_checklstring(L, 2, &n);
  lua_pushinteger(L, H(i, p, n));
  return 1;
}

static int LuaCrc32(lua_State *L) {
  return LuaHash(L, crc32_z);
}

static int LuaCrc32c(lua_State *L) {
  return LuaHash(L, crc32c);
}

static noinline int LuaProgramInt(lua_State *L, void Program(long)) {
  Program(luaL_checkinteger(L, 1));
  return 0;
}

static int LuaProgramPort(lua_State *L) {
  return LuaProgramInt(L, ProgramPort);
}

static int LuaProgramCache(lua_State *L) {
  return LuaProgramInt(L, ProgramCache);
}

static int LuaProgramLinger(lua_State *L) {
  return LuaProgramInt(L, ProgramLinger);
}

static int LuaProgramTimeout(lua_State *L) {
  return LuaProgramInt(L, ProgramTimeout);
}

static int LuaProgramBrand(lua_State *L) {
  ProgramBrand(luaL_checkstring(L, 1));
  return 0;
}

static int LuaProgramHeader(lua_State *L) {
  char *s;
  s = xasprintf("%s: %s", luaL_checkstring(L, 1), luaL_checkstring(L, 2));
  ProgramHeader(s);
  free(s);
  return 0;
}

static int LuaProgramRedirect(lua_State *L) {
  ProgramRedirect(luaL_checkinteger(L, 1), luaL_checkstring(L, 2),
                  luaL_checkstring(L, 3));
  return 0;
}

static int LuaGetLogLevel(lua_State *L) {
  lua_pushinteger(L, __log_level);
  return 1;
}

static int LuaSetLogLevel(lua_State *L) {
  __log_level = luaL_checkinteger(L, 1);
  return 0;
}

static int LuaHidePath(lua_State *L) {
  size_t pathlen;
  const char *path;
  path = luaL_checklstring(L, 1, &pathlen);
  AddString(&hidepaths, strndup(path, pathlen));
  return 0;
}

static int LuaLog(lua_State *L) {
  int level;
  char *module;
  lua_Debug ar;
  const char *msg;
  level = luaL_checkinteger(L, 1);
  if (LOGGABLE(level)) {
    msg = luaL_checkstring(L, 2);
    lua_getstack(L, 1, &ar);
    lua_getinfo(L, "nSl", &ar);
    if (!strcmp(ar.name, "main")) {
      module = strndup(effectivepath.p, effectivepath.n);
      flogf(level, module, ar.currentline, NULL, "%s", msg);
      free(module);
    } else {
      flogf(level, ar.name, ar.currentline, NULL, "%s", msg);
    }
  }
  return 0;
}

static int LuaIsHiddenPath(lua_State *L) {
  lua_pushboolean(L, IsHiddenPath(luaL_checkstring(L, 1)));
  return 1;
}

static int LuaGetZipPaths(lua_State *L) {
  char *path;
  uint64_t cf;
  size_t i, n, pathlen;
  lua_newtable(L);
  i = 0;
  n = GetZipCdirRecords(cdir);
  for (cf = GetZipCdirOffset(cdir); n--; cf += ZIP_CFILE_HDRSIZE(zmap + cf)) {
    CHECK_EQ(kZipCfileHdrMagic, ZIP_CFILE_MAGIC(zmap + cf));
    path = GetAssetPath(cf, &pathlen);
    lua_pushlstring(L, path, pathlen);
    lua_seti(L, -2, ++i);
    free(path);
  }
  return 1;
}

static int LuaGetAssetMode(lua_State *L) {
  size_t pathlen;
  struct Asset *a;
  const char *path;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen))) {
    lua_pushinteger(L, GetMode(a));
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaGetLastModifiedTime(lua_State *L) {
  size_t pathlen;
  struct Asset *a;
  const char *path;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen))) {
    if (a->file) {
      lua_pushinteger(L, a->file->st.st_mtim.tv_sec);
    } else {
      lua_pushinteger(L, GetZipCfileLastModified(zmap + a->cf));
    }
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaGetAssetSize(lua_State *L) {
  size_t pathlen;
  struct Asset *a;
  const char *path;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen))) {
    if (a->file) {
      lua_pushinteger(L, a->file->st.st_size);
    } else {
      lua_pushinteger(L, GetZipLfileUncompressedSize(zmap + a->lf));
    }
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaIsCompressed(lua_State *L) {
  size_t pathlen;
  struct Asset *a;
  const char *path;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAsset(path, pathlen))) {
    lua_pushboolean(L, IsCompressed(a));
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static int LuaGetComment(lua_State *L) {
  struct Asset *a;
  const char *path;
  size_t pathlen, m;
  path = LuaCheckPath(L, 1, &pathlen);
  if ((a = GetAssetZip(path, pathlen)) &&
      (m = strnlen(ZIP_CFILE_COMMENT(zmap + a->cf),
                   ZIP_CFILE_COMMENTSIZE(zmap + a->cf)))) {
    lua_pushlstring(L, ZIP_CFILE_COMMENT(zmap + a->cf), m);
  } else {
    lua_pushnil(L);
  }
  return 1;
}

static void LuaSetIntField(lua_State *L, const char *k, lua_Integer v) {
  lua_pushinteger(L, v);
  lua_setfield(L, -2, k);
}

static int LuaLaunchBrowser(lua_State *L) {
  LaunchBrowser();
  return 1;
}

static regex_t *LuaReCompileImpl(lua_State *L, const char *p, int f) {
  int e;
  regex_t *r;
  r = xmalloc(sizeof(regex_t));
  f &= REG_EXTENDED | REG_ICASE | REG_NEWLINE | REG_NOSUB;
  f ^= REG_EXTENDED;
  if ((e = regcomp(r, p, f)) != REG_OK) {
    free(r);
    luaL_error(L, "regcomp(%s) → REG_%s", p,
               kRegCode[MAX(0, MIN(ARRAYLEN(kRegCode) - 1, e))]);
    unreachable;
  }
  return r;
}

static int LuaReSearchImpl(lua_State *L, regex_t *r, const char *s, int f) {
  int i, n;
  regmatch_t *m;
  n = r->re_nsub + 1;
  m = xcalloc(n, sizeof(regmatch_t));
  if (regexec(r, s, n, m, f >> 8) == REG_OK) {
    for (i = 0; i < n; ++i) {
      lua_pushlstring(L, s + m[i].rm_so, m[i].rm_eo - m[i].rm_so);
    }
  } else {
    n = 0;
  }
  free(m);
  return n;
}

static int LuaReSearch(lua_State *L) {
  regex_t *r;
  int i, e, n, f;
  const char *p, *s;
  p = luaL_checkstring(L, 1);
  s = luaL_checkstring(L, 2);
  f = luaL_optinteger(L, 3, 0);
  if (f & ~(REG_EXTENDED | REG_ICASE | REG_NEWLINE | REG_NOSUB |
            REG_NOTBOL << 8 | REG_NOTEOL << 8)) {
    luaL_argerror(L, 3, "invalid flags");
    unreachable;
  }
  r = LuaReCompileImpl(L, p, f);
  n = LuaReSearchImpl(L, r, s, f);
  regfree(r);
  free(r);
  return n;
}

static int LuaReCompile(lua_State *L) {
  int f, e;
  const char *p;
  regex_t *r, **u;
  p = luaL_checkstring(L, 1);
  f = luaL_optinteger(L, 2, 0);
  if (f & ~(REG_EXTENDED | REG_ICASE | REG_NEWLINE | REG_NOSUB)) {
    luaL_argerror(L, 3, "invalid flags");
    unreachable;
  }
  r = LuaReCompileImpl(L, p, f);
  u = lua_newuserdata(L, sizeof(regex_t *));
  luaL_setmetatable(L, "regex_t*");
  *u = r;
  return 1;
}

static int LuaReRegexSearch(lua_State *L) {
  int f;
  regex_t **u;
  const char *s;
  u = luaL_checkudata(L, 1, "regex_t*");
  s = luaL_checkstring(L, 2);
  f = luaL_optinteger(L, 3, 0);
  if (!*u) {
    luaL_argerror(L, 1, "destroyed");
    unreachable;
  }
  if (f & ~(REG_NOTBOL << 8 | REG_NOTEOL << 8)) {
    luaL_argerror(L, 3, "invalid flags");
    unreachable;
  }
  return LuaReSearchImpl(L, *u, s, f);
}

static int LuaReRegexGc(lua_State *L) {
  regex_t **u;
  u = luaL_checkudata(L, 1, "regex_t*");
  if (*u) {
    regfree(*u);
    free(*u);
    *u = NULL;
  }
  return 0;
}

static const luaL_Reg kLuaRe[] = {
    {"compile", LuaReCompile},  //
    {"search", LuaReSearch},    //
    {NULL, NULL},               //
};

static const luaL_Reg kLuaReRegexMeth[] = {
    {"search", LuaReRegexSearch},  //
    {NULL, NULL},                  //
};

static const luaL_Reg kLuaReRegexMeta[] = {
    {"__gc", LuaReRegexGc},  //
    {NULL, NULL},            //
};

static void LuaReRegex(lua_State *L) {
  luaL_newmetatable(L, "regex_t*");
  luaL_setfuncs(L, kLuaReRegexMeta, 0);
  luaL_newlibtable(L, kLuaReRegexMeth);
  luaL_setfuncs(L, kLuaReRegexMeth, 0);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

static int LuaRe(lua_State *L) {
  luaL_newlib(L, kLuaRe);
  LuaSetIntField(L, "BASIC", REG_EXTENDED);
  LuaSetIntField(L, "ICASE", REG_ICASE);
  LuaSetIntField(L, "NEWLINE", REG_NEWLINE);
  LuaSetIntField(L, "NOSUB", REG_NOSUB);
  LuaSetIntField(L, "NOTBOL", REG_NOTBOL << 8);
  LuaSetIntField(L, "NOTEOL", REG_NOTEOL << 8);
  LuaReRegex(L);
  return 1;
}

static bool LuaRun(const char *path) {
  size_t pathlen;
  struct Asset *a;
  const char *code;
  pathlen = strlen(path);
  if ((a = GetAsset(path, pathlen))) {
    if ((code = LoadAsset(a, NULL))) {
      effectivepath.p = path;
      effectivepath.n = pathlen;
      DEBUGF("LuaRun(%`'s)", path);
      if (luaL_dostring(L, code) != LUA_OK) {
        WARNF("%s %s", path, lua_tostring(L, -1));
      }
      free(code);
    }
  }
  return !!a;
}

static const luaL_Reg kLuaFuncs[] = {
    {"CategorizeIp", LuaCategorizeIp},                    //
    {"DecodeBase64", LuaDecodeBase64},                    //
    {"DecodeLatin1", LuaDecodeLatin1},                    //
    {"EncodeBase64", LuaEncodeBase64},                    //
    {"EncodeLatin1", LuaEncodeLatin1},                    //
    {"EncodeUrl", LuaEncodeUrl},                          //
    {"EscapeFragment", LuaEscapeFragment},                //
    {"EscapeHost", LuaEscapeHost},                        //
    {"EscapeHtml", LuaEscapeHtml},                        //
    {"EscapeIp", LuaEscapeIp},                            //
    {"EscapeLiteral", LuaEscapeLiteral},                  //
    {"EscapeParam", LuaEscapeParam},                      //
    {"EscapePass", LuaEscapePass},                        //
    {"EscapePath", LuaEscapePath},                        //
    {"EscapeSegment", LuaEscapeSegment},                  //
    {"EscapeUser", LuaEscapeUser},                        //
    {"FormatHttpDateTime", LuaFormatHttpDateTime},        //
    {"FormatIp", LuaFormatIp},                            //
    {"GetAssetMode", LuaGetAssetMode},                    //
    {"GetAssetSize", LuaGetAssetSize},                    //
    {"GetClientAddr", LuaGetClientAddr},                  //
    {"GetComment", LuaGetComment},                        //
    {"GetDate", LuaGetDate},                              //
    {"GetEffectivePath", LuaGetEffectivePath},            //
    {"GetFragment", LuaGetFragment},                      //
    {"GetHeader", LuaGetHeader},                          //
    {"GetHeaders", LuaGetHeaders},                        //
    {"GetHost", LuaGetHost},                              //
    {"GetLastModifiedTime", LuaGetLastModifiedTime},      //
    {"GetLogLevel", LuaGetLogLevel},                      //
    {"GetMethod", LuaGetMethod},                          //
    {"GetMonospaceWidth", LuaGetMonospaceWidth},          //
    {"GetParam", LuaGetParam},                            //
    {"GetParams", LuaGetParams},                          //
    {"GetPass", LuaGetPass},                              //
    {"GetPath", LuaGetPath},                              //
    {"GetPayload", LuaGetPayload},                        //
    {"GetPort", LuaGetPort},                              //
    {"GetRemoteAddr", LuaGetRemoteAddr},                  //
    {"GetScheme", LuaGetScheme},                          //
    {"GetServerAddr", LuaGetServerAddr},                  //
    {"GetUrl", LuaGetUrl},                                //
    {"GetUser", LuaGetUser},                              //
    {"GetVersion", LuaGetVersion},                        //
    {"GetZipPaths", LuaGetZipPaths},                      //
    {"HasControlCodes", LuaHasControlCodes},              //
    {"HasParam", LuaHasParam},                            //
    {"HidePath", LuaHidePath},                            //
    {"IndentLines", LuaIndentLines},                      //
    {"IsAcceptableHost", LuaIsAcceptableHost},            //
    {"IsAcceptablePath", LuaIsAcceptablePath},            //
    {"IsAcceptablePort", LuaIsAcceptablePort},            //
    {"IsCompressed", LuaIsCompressed},                    //
    {"IsHiddenPath", LuaIsHiddenPath},                    //
    {"IsLoopbackIp", LuaIsLoopbackIp},                    //
    {"IsPrivateIp", LuaIsPrivateIp},                      //
    {"IsPublicIp", LuaIsPublicIp},                        //
    {"IsReasonablePath", LuaIsReasonablePath},            //
    {"IsValidHttpToken", LuaIsValidHttpToken},            //
    {"LaunchBrowser", LuaLaunchBrowser},                  //
    {"LoadAsset", LuaLoadAsset},                          //
    {"Log", LuaLog},                                      //
    {"ParseHost", LuaParseHost},                          //
    {"ParseHttpDateTime", LuaParseHttpDateTime},          //
    {"ParseIp", LuaParseIp},                              //
    {"ParseParams", LuaParseParams},                      //
    {"ParseUrl", LuaParseUrl},                            //
    {"ProgramBrand", LuaProgramBrand},                    //
    {"ProgramCache", LuaProgramCache},                    //
    {"ProgramHeader", LuaProgramHeader},                  //
    {"ProgramLinger", LuaProgramLinger},                  //
    {"ProgramPort", LuaProgramPort},                      //
    {"ProgramRedirect", LuaProgramRedirect},              //
    {"ProgramTimeout", LuaProgramTimeout},                //
    {"Route", LuaRoute},                                  //
    {"RouteHost", LuaRouteHost},                          //
    {"RoutePath", LuaRoutePath},                          //
    {"ServeAsset", LuaServeAsset},                        //
    {"ServeError", LuaServeError},                        //
    {"ServeIndex", LuaServeIndex},                        //
    {"ServeListing", LuaServeListing},                    //
    {"ServeStatusz", LuaServeStatusz},                    //
    {"SetHeader", LuaSetHeader},                          //
    {"SetLogLevel", LuaSetLogLevel},                      //
    {"SetStatus", LuaSetStatus},                          //
    {"Underlong", LuaUnderlong},                          //
    {"VisualizeControlCodes", LuaVisualizeControlCodes},  //
    {"Write", LuaWrite},                                  //
    {"bsf", LuaBsf},                                      //
    {"bsr", LuaBsr},                                      //
    {"crc32", LuaCrc32},                                  //
    {"crc32c", LuaCrc32c},                                //
    {"popcnt", LuaPopcnt},                                //
};

static const luaL_Reg kLuaLibs[] = {
    {"re", LuaRe},  //
};

static void LuaSetArgv(lua_State *L) {
  size_t i;
  lua_newtable(L);
  for (i = optind; i < __argc; ++i) {
    lua_pushstring(L, __argv[i]);
    lua_seti(L, -2, i - optind + 1);
  }
  lua_setglobal(L, "argv");
}

static void LuaSetConstant(lua_State *L, const char *s, long x) {
  lua_pushinteger(L, x);
  lua_setglobal(L, s);
}

static void LuaInit(void) {
#ifndef STATIC
  size_t i;
  L = luaL_newstate();
  luaL_openlibs(L);
  for (i = 0; i < ARRAYLEN(kLuaLibs); ++i) {
    luaL_requiref(L, kLuaLibs[i].name, kLuaLibs[i].func, 1);
    lua_pop(L, 1);
  }
  for (i = 0; i < ARRAYLEN(kLuaFuncs); ++i) {
    lua_pushcfunction(L, kLuaFuncs[i].func);
    lua_setglobal(L, kLuaFuncs[i].name);
  }
  LuaSetArgv(L);
  LuaSetConstant(L, "kLogDebug", kLogDebug);
  LuaSetConstant(L, "kLogVerbose", kLogVerbose);
  LuaSetConstant(L, "kLogInfo", kLogInfo);
  LuaSetConstant(L, "kLogWarn", kLogWarn);
  LuaSetConstant(L, "kLogError", kLogError);
  LuaSetConstant(L, "kLogFatal", kLogFatal);
  if (LuaRun("/.init.lua")) {
    hasluaglobalhandler = !!lua_getglobal(L, "OnHttpRequest");
    lua_pop(L, 1);
  } else {
    DEBUGF("no /.init.lua defined");
  }
#endif
}

static void LuaReload(void) {
#ifndef STATIC
  if (!LuaRun("/.reload.lua")) {
    DEBUGF("no /.reload.lua defined");
  }
#endif
}

static void HandleReload(void) {
  LOGF("reloading");
  LuaReload();
}

static void HandleHeartbeat(void) {
  if (nowl() - lastrefresh > 60 * 60) RefreshTime();
  UpdateCurrentDate(nowl());
  getrusage(RUSAGE_SELF, &shared->server);
#ifndef STATIC
  LuaRun("/.heartbeat.lua");
#endif
}

static void LogMessage(const char *d, const char *s, size_t n) {
  size_t n2, n3;
  char *s2, *s3;
  while (n && (s[n - 1] == '\r' || s[n - 1] == '\n')) --n;
  if ((s2 = DecodeLatin1(s, n, &n2))) {
    if ((s3 = IndentLines(s2, n2, &n3, 1))) {
      LOGF("%s %,ld byte message\n%.*s", d, n, n3, s3);
      free(s3);
    }
    free(s2);
  }
}

static void LogBody(const char *d, const char *s, size_t n) {
  char *s2, *s3;
  size_t n2, n3;
  if (!n) return;
  while (n && (s[n - 1] == '\r' || s[n - 1] == '\n')) --n;
  if ((s2 = VisualizeControlCodes(s, n, &n2))) {
    if ((s3 = IndentLines(s2, n2, &n3, 1))) {
      LOGF("%s %,ld byte payload\n%.*s", d, n, n3, s3);
      free(s3);
    }
    free(s2);
  }
}

static ssize_t SendString(const char *s) {
  size_t n;
  ssize_t rc;
  n = strlen(s);
  if (logmessages) LogMessage("sending", s, n);
  for (;;) {
    if ((rc = write(client, s, n)) != -1 || errno != EINTR) {
      return rc;
    }
  }
}

static ssize_t SendContinue(void) {
  return SendString("\
HTTP/1.1 100 Continue\r\n\
\r\n");
}

static ssize_t SendTimeout(void) {
  return SendString("\
HTTP/1.1 408 Request Timeout\r\n\
Connection: close\r\n\
Content-Length: 0\r\n\
\r\n");
}

static ssize_t SendServiceUnavailable(void) {
  return SendString("\
HTTP/1.1 503 Service Unavailable\r\n\
Connection: close\r\n\
Content-Length: 0\r\n\
\r\n");
}

static void LogClose(const char *reason) {
  if (amtread || meltdown || killed) {
    LockInc(&shared->fumbles);
    LOGF("%s %s with %,ld unprocessed and %,d handled (%,d workers)",
         DescribeClient(), reason, amtread, messageshandled, shared->workers);
  } else {
    DEBUGF("%s %s with %,d requests handled", DescribeClient(), reason,
           messageshandled);
  }
}

static const char *DescribeClose(void) {
  if (killed) return "killed";
  if (meltdown) return "meltdown";
  if (terminated) return "terminated";
  if (connectionclose) return "connectionclose";
  return "destroyed";
}

static void RecordNetworkOrigin(void) {
  uint32_t ip;
  GetRemoteAddr(&ip, 0);
  switch (CategorizeIp(ip)) {
    case kIpLoopback:
      LockInc(&shared->netloopback);
      break;
    case kIpPrivate:
      LockInc(&shared->netprivate);
      break;
    case kIpTestnet:
      LockInc(&shared->nettestnet);
      break;
    case kIpAfrinic:
      LockInc(&shared->netafrinic);
      break;
    case kIpLacnic:
      LockInc(&shared->netlacnic);
      break;
    case kIpApnic:
      LockInc(&shared->netapnic);
      break;
    case kIpArin:
      LockInc(&shared->netarin);
      break;
    case kIpRipe:
      LockInc(&shared->netripe);
      break;
    case kIpDod:
      LockInc(&shared->netdod);
      break;
    case kIpAtt:
      LockInc(&shared->netatt);
      break;
    case kIpApple:
      LockInc(&shared->netapple);
      break;
    case kIpFord:
      LockInc(&shared->netford);
      break;
    case kIpCogent:
      LockInc(&shared->netcogent);
      break;
    case kIpPrudential:
      LockInc(&shared->netprudential);
      break;
    case kIpUsps:
      LockInc(&shared->netusps);
      break;
    case kIpComcast:
      LockInc(&shared->netcomcast);
      break;
    case kIpAnonymous:
      LockInc(&shared->netanonymous);
      break;
    default:
      LockInc(&shared->netother);
      break;
  }
}

char *ServeServerOptions(void) {
  char *p;
  p = SetStatus(200, "OK");
#ifdef STATIC
  p = stpcpy(p, "Allow: GET, HEAD, OPTIONS\r\n");
#else
  p = stpcpy(p, "Accept: */*\r\n"
                "Accept-Charset: utf-8,ISO-8859-1;q=0.7,*;q=0.5\r\n"
                "Allow: GET, HEAD, POST, PUT, DELETE, OPTIONS\r\n");
#endif
  return p;
}
static bool HasAtMostThisElement(int h, const char *s) {
  size_t i, n;
  struct HttpRequestHeader *x;
  if (HasHeader(h)) {
    n = strlen(s);
    if (!SlicesEqualCase(s, n, inbuf.p + msg.headers[h].a,
                         msg.headers[h].b - msg.headers[h].a)) {
      return false;
    }
    for (i = 0; i < msg.xheaders.n; ++i) {
      x = msg.xheaders.p + i;
      if (GetHttpHeader(inbuf.p + x->k.a, x->k.b - x->k.a) == h &&
          !SlicesEqualCase(inbuf.p + x->v.a, x->v.b - x->v.a, s, n)) {
        return false;
      }
    }
  }
  return true;
}

static char *SynchronizeStream(void) {
  size_t got;
  ssize_t rc;
  int64_t cl;
  if ((cl = ParseContentLength(HeaderData(kHttpContentLength),
                               HeaderLength(kHttpContentLength))) == -1) {
    if (HasHeader(kHttpContentLength)) {
      LockInc(&shared->badlengths);
      LOGF("bad content length");
      return ServeFailure(400, "Bad Request");
    } else if (msg.method == kHttpPost || msg.method == kHttpPut) {
      LockInc(&shared->missinglengths);
      return ServeFailure(411, "Length Required");
    } else {
      cl = 0;
    }
  }
  if (hdrsize + cl > amtread) {
    if (hdrsize + cl > inbuf.n) {
      LockInc(&shared->hugepayloads);
      return ServeFailure(413, "Payload Too Large");
    }
    if (msg.version >= 11 && HeaderEqualCase(kHttpExpect, "100-continue")) {
      LockInc(&shared->continues);
      SendContinue();
    }
    while (amtread < hdrsize + cl) {
      LockInc(&shared->frags);
      if (++frags == 64) {
        LockInc(&shared->slowloris);
        LogClose("payload slowloris");
        return ServeFailure(408, "Request Timeout");
      }
      if ((rc = read(client, inbuf.p + amtread, inbuf.n - amtread)) != -1) {
        if (!(got = rc)) {
          LockInc(&shared->payloaddisconnects);
          LogClose("payload disconnect");
          return ServeFailure(400, "Bad Request"); /* XXX */
        }
        amtread += got;
      } else if (errno == ECONNRESET) {
        LockInc(&shared->readresets);
        LogClose("payload reset");
        return ServeFailure(400, "Bad Request"); /* XXX */
      } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
        LockInc(&shared->readtimeouts);
        LogClose("payload read timeout");
        return ServeFailure(408, "Request Timeout");
      } else if (errno == EINTR) {
        LockInc(&shared->readinterrupts);
        if (killed || ((meltdown || terminated) && nowl() - startread > 1)) {
          LockInc(&shared->dropped);
          LogClose(DescribeClose());
          return ServeFailure(503, "Service Unavailable");
        }
      } else {
        LockInc(&shared->readerrors);
        LOGF("%s payload read error %s", DescribeClient(), strerror(errno));
        return ServeFailure(500, "Internal Server Error");
      }
    }
  }
  msgsize = hdrsize + cl;
  return NULL;
}

static void ParseRequestParameters(void) {
  uint32_t ip;
  FreeLater(ParseRequestUri(inbuf.p + msg.uri.a, msg.uri.b - msg.uri.a, &url));
  if (!url.host.p) {
    GetRemoteAddr(&ip, 0);
    if (HasHeader(kHttpXForwardedHost) &&
        (IsPrivateIp(ip) || IsLoopbackIp(ip))) {
      FreeLater(ParseHost(HeaderData(kHttpXForwardedHost),
                          HeaderLength(kHttpXForwardedHost), &url));
    } else if (HasHeader(kHttpHost)) {
      FreeLater(
          ParseHost(HeaderData(kHttpHost), HeaderLength(kHttpHost), &url));
    }
  } else if (!url.path.n) {
    url.path.p = "/";
    url.path.n = 1;
  }
  if (HasHeader(kHttpContentType) &&
      IsMimeType(HeaderData(kHttpContentType), HeaderLength(kHttpContentType),
                 "application/x-www-form-urlencoded")) {
    FreeLater(ParseParams(inbuf.p + hdrsize, msgsize - hdrsize, &url.params));
  }
  FreeLater(url.params.p);
}

static char *OnHttpRequest(void) {
  effectivepath.p = url.path.p;
  effectivepath.n = url.path.n;
  lua_getglobal(L, "OnHttpRequest");
  if (lua_pcall(L, 0, 0, 0) == LUA_OK) {
    return CommitOutput(GetLuaResponse());
  } else {
    WARNF("%s", lua_tostring(L, -1));
    lua_pop(L, 1);
    return ServeError(500, "Internal Server Error");
  }
}

static char *HandleRequest(void) {
  char *p;
  if (msg.version < 10) {
    LockInc(&shared->http09);
  } else if (msg.version == 10) {
    LockInc(&shared->http10);
  } else if (msg.version == 11) {
    LockInc(&shared->http11);
  } else {
    LockInc(&shared->http12);
    return ServeFailure(505, "HTTP Version Not Supported");
  }
  if ((p = SynchronizeStream())) return p;
  if (logbodies) LogBody("received", inbuf.p + hdrsize, msgsize - hdrsize);
  if (msg.version < 11 || HeaderEqualCase(kHttpConnection, "close")) {
    connectionclose = true;
  }
  if (msg.method == kHttpOptions &&
      SlicesEqual(inbuf.p + msg.uri.a, msg.uri.b - msg.uri.a, "*", 1)) {
    LockInc(&shared->serveroptions);
    return ServeServerOptions();
  }
  if (msg.method == kHttpConnect) {
    LockInc(&shared->connectsrefused);
    return ServeFailure(501, "Not Implemented");
  }
  if (!HasAtMostThisElement(kHttpExpect, "100-continue")) {
    LockInc(&shared->expectsrefused);
    return ServeFailure(417, "Expectation Failed");
  }
  if (!HasAtMostThisElement(kHttpTransferEncoding, "identity")) {
    LockInc(&shared->transfersrefused);
    return ServeFailure(501, "Not Implemented");
  }
  ParseRequestParameters();
  if (!url.path.n || url.path.p[0] != '/' ||
      !IsAcceptablePath(url.path.p, url.path.n) ||
      !IsAcceptableHost(url.host.p, url.host.n) ||
      !IsAcceptablePort(url.port.p, url.port.n)) {
    LockInc(&shared->urisrefused);
    return ServeFailure(400, "Bad Request");
  }
  LOGF("RECEIVED %s HTTP%02d %.*s %s %`'.*s %`'.*s", DescribeClient(),
       msg.version, msg.xmethod.b - msg.xmethod.a, inbuf.p + msg.xmethod.a,
       FreeLater(EncodeUrl(&url, 0)), HeaderLength(kHttpReferer),
       HeaderData(kHttpReferer), HeaderLength(kHttpUserAgent),
       HeaderData(kHttpUserAgent));
  if (hasluaglobalhandler) return OnHttpRequest();
  return Route(url.host.p, url.host.n, url.path.p, url.path.n);
}

static bool HandleMessage(void) {
  int rc;
  int iovlen;
  char *p, *s;
  struct iovec iov[4];
  long actualcontentlength;
  g_syscount = 0;
  if ((rc = ParseHttpRequest(&msg, inbuf.p, amtread)) != -1) {
    if (!rc) return false;
    hdrsize = rc;
    if (logmessages) LogMessage("received", inbuf.p, hdrsize);
    RecordNetworkOrigin();
    p = HandleRequest();
  } else {
    LockInc(&shared->badmessages);
    connectionclose = true;
    LOGF("%s sent garbage %`'s", DescribeClient(),
         VisualizeControlCodes(inbuf.p, MIN(128, amtread), 0));
    p = ServeError(400, "Bad Message");
  }
  if (!msgsize) {
    amtread = 0;
    connectionclose = true;
    LockInc(&shared->synchronizationfailures);
    DEBUGF("could not synchronize message stream");
  }
  if (0 && connectionclose) {
    LockInc(&shared->shutdowns);
    shutdown(client, SHUT_RD);
  }
  if (msg.version >= 10) {
    p = AppendCrlf(stpcpy(stpcpy(p, "Date: "), shared->currentdate));
    if (!branded) p = AppendServer(p, serverheader);
    if (extrahdrs) p = stpcpy(p, extrahdrs);
    if (connectionclose) {
      p = stpcpy(p, "Connection: close\r\n");
    } else if (encouragekeepalive && msg.version >= 11) {
      p = stpcpy(p, "Connection: keep-alive\r\n");
    }
    actualcontentlength = contentlength;
    if (gzipped) {
      actualcontentlength += sizeof(kGzipHeader) + sizeof(gzip_footer);
      p = stpcpy(p, "Content-Encoding: gzip\r\n");
    }
    p = AppendContentLength(p, actualcontentlength);
    p = AppendCrlf(p);
    CHECK_LE(p - hdrbuf.p, hdrbuf.n);
    if (logmessages) LogMessage("sending", hdrbuf.p, p - hdrbuf.p);
    iov[0].iov_base = hdrbuf.p;
    iov[0].iov_len = p - hdrbuf.p;
    iovlen = 1;
    if (!MustNotIncludeMessageBody()) {
      if (gzipped) {
        iov[iovlen].iov_base = kGzipHeader;
        iov[iovlen].iov_len = sizeof(kGzipHeader);
        ++iovlen;
      }
      iov[iovlen].iov_base = content;
      iov[iovlen].iov_len = contentlength;
      ++iovlen;
      if (gzipped) {
        iov[iovlen].iov_base = gzip_footer;
        iov[iovlen].iov_len = sizeof(gzip_footer);
        ++iovlen;
      }
    }
  } else {
    iov[0].iov_base = content;
    iov[0].iov_len = contentlength;
    iovlen = 1;
  }
  if (loglatency || LOGGABLE(kLogDebug)) {
    flogf(kLogDebug, __FILE__, __LINE__, NULL, "%`'.*s latency %,ldµs",
          msg.uri.b - msg.uri.a, inbuf.p + msg.uri.a,
          (long)((nowl() - startrequest) * 1e6L));
  }
  Send(iov, iovlen);
  LockInc(&shared->messageshandled);
  ++messageshandled;
  return true;
}

static void InitRequest(void) {
  frags = 0;
  gzipped = 0;
  branded = 0;
  content = 0;
  msgsize = 0;
  loops.n = 0;
  outbuf.n = 0;
  luaheaderp = 0;
  contentlength = 0;
  InitHttpRequest(&msg);
}

static void HandleMessages(void) {
  ssize_t rc;
  size_t got;
  for (;;) {
    InitRequest();
    startread = nowl();
    for (;;) {
      if (!msg.i && amtread) {
        startrequest = nowl();
        if (HandleMessage()) break;
      }
      if ((rc = read(client, inbuf.p + amtread, inbuf.n - amtread)) != -1) {
        startrequest = nowl();
        got = rc;
        amtread += got;
        if (amtread) {
          DEBUGF("%s read %,zd bytes", DescribeClient(), got);
          if (HandleMessage()) {
            break;
          } else if (got) {
            LockInc(&shared->frags);
            if (++frags == 32) {
              SendTimeout();
              LogClose("slowloris");
              LockInc(&shared->slowloris);
              return;
            } else {
              DEBUGF("%s fragged msg added %,ld bytes to %,ld byte buffer",
                     DescribeClient(), amtread, got);
            }
          }
        }
        if (!got) {
          LogClose("disconnect");
          return;
        }
      } else if (errno == EINTR) {
        LockInc(&shared->readinterrupts);
      } else if (errno == EAGAIN) {
        LockInc(&shared->readtimeouts);
        if (amtread) SendTimeout();
        LogClose("timeout");
        return;
      } else if (errno == ECONNRESET) {
        LockInc(&shared->readresets);
        LogClose("reset");
        return;
      } else {
        LockInc(&shared->readerrors);
        WARNF("%s read failed %s", DescribeClient(), strerror(errno));
        return;
      }
      if (killed || (terminated && !amtread) ||
          (meltdown && (!amtread || nowl() - startread > 1))) {
        if (amtread) {
          LockInc(&shared->dropped);
          SendServiceUnavailable();
        }
        LogClose(DescribeClose());
        return;
      }
    }
    if (msgsize == amtread) {
      amtread = 0;
      if (connectionclose || killed || terminated || meltdown) {
        LogClose(DescribeClose());
        return;
      }
    } else {
      CHECK_LT(msgsize, amtread);
      LockInc(&shared->pipelinedrequests);
      DEBUGF("%,ld pipelined bytes", amtread - msgsize);
      memmove(inbuf.p, inbuf.p + msgsize, amtread - msgsize);
      amtread -= msgsize;
      if (connectionclose || killed) {
        LogClose(DescribeClose());
        return;
      }
    }
    CollectGarbage();
  }
}

static void EnterMeltdownMode(void) {
  if (shared->lastmeltdown && nowl() - shared->lastmeltdown < 1) return;
  WARNF("redbean is melting down (%,d workers)", shared->workers);
  LOGIFNEG1(kill(0, SIGUSR2));
  shared->lastmeltdown = nowl();
  ++shared->meltdowns;
}

static void EmergencyClose(int fd) {
  struct linger nolinger = {0};
  setsockopt(fd, SOL_SOCKET, SO_LINGER, &nolinger, sizeof(nolinger));
  close(fd);
}

static void HandleConnection(void) {
  int pid;
  clientaddrsize = sizeof(clientaddr);
  if ((client = accept4(server, &clientaddr, &clientaddrsize, SOCK_CLOEXEC)) !=
      -1) {
    startconnection = nowl();
    messageshandled = 0;
    if (uniprocess) {
      pid = -1;
      connectionclose = true;
    } else {
      switch ((pid = fork())) {
        case 0:
          meltdown = false;
          connectionclose = false;
          if (funtrace && !IsTiny()) {
            ftrace_install();
          }
          break;
        case -1:
          FATALF("%s too many processes %s", DescribeServer(), strerror(errno));
          ++shared->forkerrors;
          LockInc(&shared->dropped);
          EnterMeltdownMode();
          SendServiceUnavailable();
          EmergencyClose(client);
          return;
        default:
          ++shared->workers;
          close(client);
          return;
      }
    }
    if (!pid) close(server);
    DEBUGF("%s accepted", DescribeClient());
    HandleMessages();
    DEBUGF("%s closing after %,ldµs", DescribeClient(),
           (long)((nowl() - startconnection) * 1e6L));
    if (close(client) == -1) {
      LockInc(&shared->closeerrors);
      WARNF("%s close failed", DescribeClient());
    }
    if (!pid) {
      _exit(0);
    } else {
      CollectGarbage();
    }
  } else if (errno == EINTR || errno == EAGAIN) {
    ++shared->acceptinterrupts;
  } else if (errno == ENFILE) {
    LockInc(&shared->enfiles);
    WARNF("%s too many open files", DescribeServer());
    EnterMeltdownMode();
  } else if (errno == EMFILE) {
    LockInc(&shared->emfiles);
    WARNF("%s ran out of open file quota", DescribeServer());
    EnterMeltdownMode();
  } else if (errno == ENOMEM) {
    LockInc(&shared->enomems);
    WARNF("%s ran out of memory");
    EnterMeltdownMode();
  } else if (errno == ENOBUFS) {
    LockInc(&shared->enobufs);
    WARNF("%s ran out of buffer");
    EnterMeltdownMode();
  } else if (errno == ENONET) {
    ++shared->enonets;
    WARNF("%s network gone", DescribeServer());
    sleep(1);
  } else if (errno == ENETDOWN) {
    ++shared->enetdowns;
    WARNF("%s network down", DescribeServer());
    sleep(1);
  } else if (errno == ECONNABORTED) {
    ++shared->acceptresets;
    WARNF("%s connection reset before accept");
  } else if (errno == ENETUNREACH || errno == EHOSTUNREACH ||
             errno == EOPNOTSUPP || errno == ENOPROTOOPT || errno == EPROTO) {
    ++shared->accepterrors;
    WARNF("%s ephemeral accept error %s", DescribeServer(), strerror(errno));
  } else {
    FATALF("%s accept error %s", DescribeServer(), strerror(errno));
  }
}

static void Tune(int a, int b, int x, const char *as, const char *bs) {
#define Tune(A, B, X) Tune(A, B, X, #A, #B)
  if (!b) return;
  if (setsockopt(server, a, b, &x, sizeof(x)) == -1) {
    WARNF("setsockopt(server, %s, %s, %d) failed %s", as, bs, x,
          strerror(errno));
  }
}

static void TuneSockets(void) {
  Tune(SOL_SOCKET, SO_REUSEADDR, 1);
  Tune(IPPROTO_TCP, TCP_CORK, 0);
  Tune(IPPROTO_TCP, TCP_NODELAY, 1);
  Tune(IPPROTO_TCP, TCP_FASTOPEN, 1);
  Tune(IPPROTO_TCP, TCP_QUICKACK, 1);
  setsockopt(server, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
  setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(server, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

static void RestoreApe(const char *prog) {
  char *p;
  size_t n;
  struct Asset *a;
  extern char ape_rom_vaddr[] __attribute__((__weak__));
  if (IsWindows()) return; /* TODO */
  if (IsOpenbsd()) return; /* TODO */
  if (IsNetbsd()) return;  /* TODO */
  if (endswith(prog, ".com.dbg")) return;
  close(OpenExecutable());
  if ((a = GetAssetZip("/.ape", 5)) && (p = LoadAsset(a, &n))) {
    mprotect(ape_rom_vaddr, PAGESIZE, PROT_READ | PROT_WRITE);
    memcpy(ape_rom_vaddr, p, MIN(n, PAGESIZE));
    msync(ape_rom_vaddr, PAGESIZE, MS_ASYNC);
    mprotect(ape_rom_vaddr, PAGESIZE, PROT_NONE);
    free(p);
  } else {
    LOGF("/.ape not found");
  }
}

void RedBean(int argc, char *argv[], const char *prog) {
  uint32_t addrsize;
  gmtoff = GetGmtOffset((lastrefresh = startserver = nowl()));
  CHECK_GT(CLK_TCK, 0);
  CHECK_NE(MAP_FAILED,
           (shared = mmap(NULL, ROUNDUP(sizeof(struct Shared), FRAMESIZE),
                          PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
                          -1, 0)));
  OpenZip(prog);
  IndexAssets();
  RestoreApe(prog);
  SetDefaults();
  GetOpts(argc, argv);
  LuaInit();
  if (uniprocess) shared->workers = 1;
  xsigaction(SIGINT, OnInt, 0, 0, 0);
  xsigaction(SIGHUP, OnHup, 0, 0, 0);
  xsigaction(SIGTERM, OnTerm, 0, 0, 0);
  xsigaction(SIGCHLD, OnChld, 0, 0, 0);
  xsigaction(SIGUSR1, OnUsr1, 0, 0, 0);
  xsigaction(SIGUSR2, OnUsr2, 0, 0, 0);
  xsigaction(SIGALRM, OnAlrm, 0, 0, 0);
  xsigaction(SIGPIPE, SIG_IGN, 0, 0, 0);
  /* TODO(jart): SIGXCPU and SIGXFSZ */
  if (setitimer(ITIMER_REAL, &kHeartbeat, NULL) == -1) {
    heartless = true;
  }
  server = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
  CHECK_NE(-1, server);
  TuneSockets();
  if (bind(server, &serveraddr, sizeof(serveraddr)) == -1) {
    if (errno == EADDRINUSE) {
      fprintf(stderr, "error: address in use\n"
                      "try passing the -p PORT flag\n");
    } else {
      fprintf(stderr, "error: bind() failed: %s\n", strerror(errno));
    }
    exit(1);
  }
  CHECK_NE(-1, listen(server, 10));
  addrsize = sizeof(serveraddr);
  CHECK_NE(-1, getsockname(server, &serveraddr, &addrsize));
  struct in_addr addr = serveraddr.sin_addr;
  if (addr.s_addr == INADDR_ANY) addr.s_addr = htonl(INADDR_LOOPBACK);
  LOGF("LISTEN %s see http://%s:%d", DescribeServer(), inet_ntoa(addr),
       ntohs(serveraddr.sin_port));
  if (printport) {
    printf("%d\n", ntohs(serveraddr.sin_port));
    fflush(stdout);
  }
  if (daemonize) Daemonize();
  UpdateCurrentDate(nowl());
  freelist.c = 8;
  freelist.p = xcalloc(freelist.c, sizeof(*freelist.p));
  unmaplist.c = 1;
  unmaplist.p = xcalloc(unmaplist.c, sizeof(*unmaplist.p));
  hdrbuf.n = 4 * 1024;
  hdrbuf.p = xvalloc(hdrbuf.n);
  inbuf.n = maxpayloadsize;
  inbuf.p = xvalloc(inbuf.n);
  while (!terminated) {
    if (zombied) {
      ReapZombies();
    } else if (invalidated) {
      HandleReload();
      invalidated = false;
    } else if (heartbeat) {
      HandleHeartbeat();
      heartbeat = false;
    } else if (meltdown) {
      EnterMeltdownMode();
      meltdown = false;
    } else {
      if (heartless) HandleHeartbeat();
      HandleConnection();
    }
  }
  LOGIFNEG1(close(server));
  if (keyboardinterrupt) {
    LOGF("received keyboard interrupt");
  } else {
    LOGF("received term signal");
    if (!killed) {
      terminated = false;
    }
    DEBUGF("sending TERM to process group");
    LOGIFNEG1(kill(0, SIGTERM));
  }
  WaitAll();
  VERBOSEF("shutdown complete");
}

int main(int argc, char *argv[]) {
  if (!IsTiny()) {
    setenv("GDB", "", true);
    showcrashreports();
  }
  RedBean(argc, argv, (const char *)getauxval(AT_EXECFN));
  return 0;
}
