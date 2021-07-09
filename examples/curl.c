#if 0
/*─────────────────────────────────────────────────────────────────╗
│ To the extent possible under law, Justine Tunney has waived      │
│ all copyright and related or neighboring rights to this file,    │
│ as it is written in the following disclaimers:                   │
│   • http://unlicense.org/                                        │
│   • http://creativecommons.org/publicdomain/zero/1.0/            │
╚─────────────────────────────────────────────────────────────────*/
#endif
#include "libc/bits/safemacros.internal.h"
#include "libc/calls/calls.h"
#include "libc/calls/struct/iovec.h"
#include "libc/dce.h"
#include "libc/dns/dns.h"
#include "libc/errno.h"
#include "libc/fmt/conv.h"
#include "libc/fmt/fmt.h"
#include "libc/log/check.h"
#include "libc/log/log.h"
#include "libc/macros.internal.h"
#include "libc/rand/rand.h"
#include "libc/runtime/gc.h"
#include "libc/runtime/runtime.h"
#include "libc/sock/sock.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/af.h"
#include "libc/sysv/consts/dt.h"
#include "libc/sysv/consts/ex.h"
#include "libc/sysv/consts/exit.h"
#include "libc/sysv/consts/ipproto.h"
#include "libc/sysv/consts/shut.h"
#include "libc/sysv/consts/so.h"
#include "libc/sysv/consts/sock.h"
#include "libc/sysv/consts/sol.h"
#include "libc/sysv/consts/tcp.h"
#include "libc/time/struct/tm.h"
#include "libc/x/x.h"
#include "net/http/http.h"
#include "net/http/url.h"
#include "net/https/https.h"
#include "third_party/getopt/getopt.h"
#include "third_party/mbedtls/ctr_drbg.h"
#include "third_party/mbedtls/debug.h"
#include "third_party/mbedtls/error.h"
#include "third_party/mbedtls/pk.h"
#include "third_party/mbedtls/ssl.h"

/**
 * @fileoverview Downloads HTTP URL to stdout.
 *
 *     make -j8 o//examples/curl.com
 *     o//examples/curl.com http://justine.lol/ape.html
 */

#define HasHeader(H)    (!!msg.headers[H].a)
#define HeaderData(H)   (p + msg.headers[H].a)
#define HeaderLength(H) (msg.headers[H].b - msg.headers[H].a)
#define HeaderEqualCase(H, S) \
  SlicesEqualCase(S, strlen(S), HeaderData(H), HeaderLength(H))

struct Buffer {
  size_t i, n;
  char *p;
};

static inline bool SlicesEqualCase(const char *a, size_t n, const char *b,
                                   size_t m) {
  return n == m && !memcasecmp(a, b, n);
}

static bool TuneSocket(int fd, int a, int b, int x) {
  if (!b) return false;
  return setsockopt(fd, a, b, &x, sizeof(x)) != -1;
}

static int Socket(int family, int type, int protocol) {
  int fd;
  if ((fd = socket(family, type, protocol)) != -1) {
    TuneSocket(fd, SOL_SOCKET, SO_KEEPALIVE, 1);
    if (protocol == SOL_TCP) {
      TuneSocket(fd, SOL_TCP, TCP_KEEPIDLE, 60);
      TuneSocket(fd, SOL_TCP, TCP_KEEPINTVL, 60);
      TuneSocket(fd, SOL_TCP, TCP_FASTOPEN_CONNECT, 1);
      if (!TuneSocket(fd, SOL_TCP, TCP_QUICKACK, 1)) {
        TuneSocket(fd, SOL_TCP, TCP_NODELAY, 1);
      }
    }
  }
  return fd;
}

static int TlsSend(void *c, const unsigned char *p, size_t n) {
  int rc;
  VERBOSEF("begin send %zu", n);
  CHECK_NE(-1, (rc = write(*(int *)c, p, n)));
  VERBOSEF("end   send %zu", n);
  return rc;
}

static int TlsRecv(void *c, unsigned char *p, size_t n, uint32_t o) {
  int r;
  struct iovec v[2];
  static unsigned a, b;
  static unsigned char t[4096];
  if (a < b) {
    r = MIN(n, b - a);
    memcpy(p, t + a, r);
    if ((a += r) == b) a = b = 0;
    return r;
  }
  v[0].iov_base = p;
  v[0].iov_len = n;
  v[1].iov_base = t;
  v[1].iov_len = sizeof(t);
  VERBOSEF("begin recv %zu", n + sizeof(t) - b);
  CHECK_NE(-1, (r = readv(*(int *)c, v, 2)));
  VERBOSEF("end   recv %zu", r);
  if (r > n) b = r - n;
  return MIN(n, r);
}

static void TlsDebug(void *c, int v, const char *f, int l, const char *s) {
  flogf(v, f, l, 0, "TLS %s", s);
}

static char *TlsError(int r) {
  static char b[128];
  mbedtls_strerror(r, b, sizeof(b));
  return b;
}

static wontreturn void PrintUsage(FILE *f, int rc) {
  fprintf(f, "usage: %s [-ksvV] URL\n", program_invocation_name);
  exit(rc);
}

static wontreturn void TlsDie(const char *s, int r) {
  if (IsTiny()) {
    fprintf(stderr, "error: %s (-0x%04x %s)\n", s, -r, TlsError(r));
  } else {
    fprintf(stderr, "error: %s (grep -0x%04x)\n", s, -r);
  }
  exit(1);
}

static int GetEntropy(void *c, unsigned char *p, size_t n) {
  CHECK_EQ(n, getrandom(p, n, 0));
  return 0;
}

static int AppendFmt(struct Buffer *b, const char *fmt, ...) {
  int n;
  char *p;
  va_list va, vb;
  va_start(va, fmt);
  va_copy(vb, va);
  n = vsnprintf(b->p + b->i, b->n - b->i, fmt, va);
  if (b->i + n + 1 > b->n) {
    do {
      if (b->n) {
        b->n += b->n >> 1;
      } else {
        b->n = 16;
      }
    } while (b->i + n + 1 > b->n);
    b->p = realloc(b->p, b->n);
    vsnprintf(b->p + b->i, b->n - b->i, fmt, vb);
  }
  va_end(vb);
  va_end(va);
  b->i += n;
  return n;
}

int main(int argc, char *argv[]) {
  if (!NoDebug()) showcrashreports();

  /*
   * Read flags.
   */
  int opt;
  struct Headers {
    size_t n;
    char **p;
  } headers = {0};
  int method = kHttpGet;
  bool authmode = MBEDTLS_SSL_VERIFY_REQUIRED;
  const char *agent = "hurl/1.o (https://github.com/jart/cosmopolitan)";
  while ((opt = getopt(argc, argv, "qksvVIX:H:A:")) != -1) {
    switch (opt) {
      case 's':
      case 'q':
        break;
      case 'v':
        ++__log_level;
        break;
      case 'I':
        method = kHttpHead;
        break;
      case 'A':
        agent = optarg;
        break;
      case 'H':
        headers.p = realloc(headers.p, ++headers.n * sizeof(*headers.p));
        headers.p[headers.n - 1] = optarg;
        break;
      case 'X':
        CHECK((method = GetHttpMethod(optarg, strlen(optarg))));
        break;
      case 'V':
        ++mbedtls_debug_threshold;
        break;
      case 'k':
        authmode = MBEDTLS_SSL_VERIFY_NONE;
        break;
      case 'h':
        PrintUsage(stdout, EXIT_SUCCESS);
      default:
        PrintUsage(stderr, EX_USAGE);
    }
  }

  /*
   * Get argument.
   */
  const char *urlarg;
  if (optind == argc) PrintUsage(stderr, EX_USAGE);
  urlarg = argv[optind];

  /*
   * Parse URL.
   */
  struct Url url;
  char *host, *port;
  bool usessl = false;
  _gc(ParseUrl(urlarg, -1, &url));
  _gc(url.params.p);
  if (url.scheme.n) {
    if (url.scheme.n == 5 && !memcasecmp(url.scheme.p, "https", 5)) {
      usessl = true;
    } else if (!(url.scheme.n == 4 && !memcasecmp(url.scheme.p, "http", 4))) {
      fprintf(stderr, "error: not an http/https url: %s\n", urlarg);
      exit(1);
    }
  }
  if (url.host.n) {
    host = _gc(strndup(url.host.p, url.host.n));
    if (url.port.n) {
      port = _gc(strndup(url.port.p, url.port.n));
    } else {
      port = usessl ? "443" : "80";
    }
  } else {
    host = "127.0.0.1";
    port = usessl ? "443" : "80";
  }
  if (!IsAcceptableHost(host, -1)) {
    fprintf(stderr, "error: invalid host: %s\n", urlarg);
    exit(1);
  }
  url.fragment.p = 0, url.fragment.n = 0;
  url.scheme.p = 0, url.scheme.n = 0;
  url.user.p = 0, url.user.n = 0;
  url.pass.p = 0, url.pass.n = 0;
  url.host.p = 0, url.host.n = 0;
  url.port.p = 0, url.port.n = 0;
  if (!url.path.n || url.path.p[0] != '/') {
    char *p = _gc(xmalloc(1 + url.path.n));
    mempcpy(mempcpy(p, "/", 1), url.path.p, url.path.n);
    url.path.p = p;
    ++url.path.n;
  }

  /*
   * Create HTTP message.
   */
  struct Buffer request = {0};
  AppendFmt(&request,
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%s\r\n"
            "Connection: close\r\n"
            "User-Agent: %s\r\n",
            kHttpMethod[method], _gc(EncodeUrl(&url, 0)), host, port, agent);
  fprintf(stderr, "%`'.*s\n", request.i, request.p);
  for (int i = 0; i < headers.n; ++i) {
    AppendFmt(&request, "%s\r\n", headers.p[i]);
  }
  AppendFmt(&request, "\r\n");

  /*
   * Setup crypto.
   */
  mbedtls_ssl_config conf;
  mbedtls_ssl_context ssl;
  mbedtls_ctr_drbg_context drbg;
  if (usessl) {
    mbedtls_ssl_init(&ssl);
    mbedtls_ctr_drbg_init(&drbg);
    mbedtls_ssl_config_init(&conf);
    CHECK_EQ(0, mbedtls_ctr_drbg_seed(&drbg, GetEntropy, 0, "justine", 7));
    CHECK_EQ(0, mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                            MBEDTLS_SSL_TRANSPORT_STREAM,
                                            MBEDTLS_SSL_PRESET_DEFAULT));
    mbedtls_ssl_conf_ca_chain(&conf, GetSslRoots(), 0);
    mbedtls_ssl_conf_authmode(&conf, authmode);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
    if (!IsTiny()) mbedtls_ssl_conf_dbg(&conf, TlsDebug, 0);
    CHECK_EQ(0, mbedtls_ssl_setup(&ssl, &conf));
    CHECK_EQ(0, mbedtls_ssl_set_hostname(&ssl, host));
  }

  /*
   * Perform DNS lookup.
   */
  struct addrinfo *addr;
  struct addrinfo hints = {.ai_family = AF_INET,
                           .ai_socktype = SOCK_STREAM,
                           .ai_protocol = IPPROTO_TCP,
                           .ai_flags = AI_NUMERICSERV};
  CHECK_EQ(EAI_SUCCESS, getaddrinfo(host, port, &hints, &addr));

  /*
   * Connect to server.
   */
  int ret, sock;
  CHECK_NE(-1, (sock = Socket(addr->ai_family, addr->ai_socktype,
                              addr->ai_protocol)));
  CHECK_NE(-1, connect(sock, addr->ai_addr, addr->ai_addrlen));
  freeaddrinfo(addr);
  if (usessl) {
    mbedtls_ssl_set_bio(&ssl, &sock, TlsSend, 0, TlsRecv);
    if ((ret = mbedtls_ssl_handshake(&ssl))) {
      TlsDie("ssl handshake", ret);
    }
  }

  /*
   * Send HTTP Message.
   */
  if (usessl) {
    ret = mbedtls_ssl_write(&ssl, request.p, request.i);
    if (ret != request.i) TlsDie("ssl write", ret);
  } else {
    CHECK_EQ(request.i, write(sock, request.p, request.i));
  }

  /*
   * Handle response.
   */
  int t;
  char *p;
  ssize_t rc;
  struct HttpMessage msg;
  struct HttpUnchunker u;
  size_t g, i, n, hdrlen, paylen;
  InitHttpMessage(&msg, kHttpResponse);
  for (p = 0, hdrlen = paylen = t = i = n = 0;;) {
    if (i == n) {
      n += 1000;
      n += n >> 1;
      p = realloc(p, n);
    }
    if (usessl) {
      if ((rc = mbedtls_ssl_read(&ssl, p + i, n - i)) < 0) {
        if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
          rc = 0;
        } else {
          TlsDie("ssl read", rc);
        }
      }
    } else {
      CHECK_NE(-1, (rc = read(sock, p + i, n - i)));
    }
    g = rc;
    i += g;
    switch (t) {
      case kHttpClientStateHeaders:
        CHECK(g);
        CHECK_NE(-1, (rc = ParseHttpMessage(&msg, p, i)));
        if (rc) {
          hdrlen = rc;
          if (100 <= msg.status && msg.status <= 199) {
            CHECK(!HasHeader(kHttpContentLength) ||
                  HeaderEqualCase(kHttpContentLength, "0"));
            CHECK(!HasHeader(kHttpTransferEncoding) ||
                  HeaderEqualCase(kHttpTransferEncoding, "identity"));
            DestroyHttpMessage(&msg);
            InitHttpMessage(&msg, kHttpResponse);
            memmove(p, p + hdrlen, i - hdrlen);
            i -= hdrlen;
            break;
          }
          if (method == kHttpHead) {
            CHECK_EQ(hdrlen, write(1, p, hdrlen));
            goto Finished;
          } else if (msg.status == 204 || msg.status == 304) {
            goto Finished;
          }
          if (HasHeader(kHttpTransferEncoding) &&
              !HeaderEqualCase(kHttpTransferEncoding, "identity")) {
            CHECK(HeaderEqualCase(kHttpTransferEncoding, "chunked"));
            t = kHttpClientStateBodyChunked;
            memset(&u, 0, sizeof(u));
            goto Chunked;
          } else if (HasHeader(kHttpContentLength)) {
            CHECK_NE(-1, (rc = ParseContentLength(
                              HeaderData(kHttpContentLength),
                              HeaderLength(kHttpContentLength))));
            t = kHttpClientStateBodyLengthed;
            paylen = rc;
            if (paylen > i - hdrlen) {
              CHECK_EQ(i - hdrlen, write(1, p + hdrlen, i - hdrlen));
            } else {
              CHECK_EQ(paylen, write(1, p + hdrlen, paylen));
              goto Finished;
            }
          } else {
            t = kHttpClientStateBody;
            CHECK_EQ(i - hdrlen, write(1, p + hdrlen, i - hdrlen));
          }
        }
        break;
      case kHttpClientStateBody:
        if (!g) goto Finished;
        CHECK_EQ(g, write(1, p + i - g, g));
        break;
      case kHttpClientStateBodyLengthed:
        CHECK(g);
        if (i - hdrlen > paylen) g = hdrlen + paylen - (i - g);
        CHECK_EQ(g, write(1, p + i - g, g));
        if (i - hdrlen >= paylen) goto Finished;
        break;
      case kHttpClientStateBodyChunked:
      Chunked:
        CHECK_NE(-1, (rc = Unchunk(&u, p + hdrlen, i - hdrlen, &paylen)));
        if (rc) {
          CHECK_EQ(paylen, write(1, p + hdrlen, paylen));
          goto Finished;
        }
        break;
      default:
        abort();
    }
  }

/*
 * Close connection.
 */
Finished:
  CHECK_NE(-1, close(sock));

  /*
   * Free memory.
   */
  free(p);
  free(headers.p);
  if (usessl) {
    mbedtls_ssl_free(&ssl);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_ssl_config_free(&conf);
  }

  return 0;
}
