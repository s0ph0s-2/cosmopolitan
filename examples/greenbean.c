#if 0
/*─────────────────────────────────────────────────────────────────╗
│ To the extent possible under law, Justine Tunney has waived      │
│ all copyright and related or neighboring rights to this file,    │
│ as it is written in the following disclaimers:                   │
│   • http://unlicense.org/                                        │
│   • http://creativecommons.org/publicdomain/zero/1.0/            │
╚─────────────────────────────────────────────────────────────────*/
#endif
#ifdef __COSMOCC__
#define _COSMO_SOURCE
#include <assert.h>
#include <cosmo.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <sys/socket.h>
#include <time.h>
#else
#include "libc/assert.h"
#include "libc/atomic.h"
#include "libc/calls/calls.h"
#include "libc/calls/pledge.h"
#include "libc/calls/struct/sigaction.h"
#include "libc/calls/struct/timespec.h"
#include "libc/calls/struct/timeval.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/fmt/conv.h"
#include "libc/fmt/itoa.h"
#include "libc/intrin/kprintf.h"
#include "libc/log/log.h"
#include "libc/macros.internal.h"
#include "libc/mem/gc.h"
#include "libc/mem/mem.h"
#include "libc/runtime/runtime.h"
#include "libc/sock/sock.h"
#include "libc/sock/struct/sockaddr.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/af.h"
#include "libc/sysv/consts/auxv.h"
#include "libc/sysv/consts/clock.h"
#include "libc/sysv/consts/limits.h"
#include "libc/sysv/consts/sig.h"
#include "libc/sysv/consts/so.h"
#include "libc/sysv/consts/sock.h"
#include "libc/sysv/consts/sol.h"
#include "libc/sysv/consts/tcp.h"
#include "libc/sysv/consts/timer.h"
#include "libc/thread/thread.h"
#include "libc/thread/thread2.h"
#include "net/http/http.h"
#endif

/**
 * @fileoverview greenbean lightweight threaded web server no. 2
 *
 * This web server is the same as greenbean.c except it supports having
 * more than one thread on Windows. To do that we have to make the code
 * more complicated by not using SO_REUSEPORT. The approach we take, is
 * creating a single listener thread which adds accepted sockets into a
 * queue that worker threads consume. This way, if you like Windows you
 * can easily have a web server with 10,000+ connections.
 */

#define PORT      8080
#define KEEPALIVE 5000
#define LOGGING   1

#define STANDARD_RESPONSE_HEADERS \
  "Server: greenbean/1.o\r\n"     \
  "Referrer-Policy: origin\r\n"   \
  "Cache-Control: private; max-age=0\r\n"

int server;
atomic_int a_termsig;
atomic_int a_workers;
atomic_int a_messages;
atomic_int a_connections;
pthread_cond_t statuscond;
pthread_mutex_t statuslock;
const char *volatile status = "";

#if LOGGING
// prints persistent status line
//   \r moves cursor back to beginning of line
// \e[K clears text from cursor to end of line
#define LOG(FMT, ...) kprintf("\r\e[K" FMT "\n", ##__VA_ARGS__)
#else
#define LOG(FMT, ...) (void)0
#endif

// updates the status line if it's convenient to do so
void SomethingHappened(void) {
  unassert(!pthread_cond_signal(&statuscond));
}

// performs a guaranteed update of the main thread status line
void SomethingImportantHappened(void) {
  unassert(!pthread_mutex_lock(&statuslock));
  unassert(!pthread_cond_signal(&statuscond));
  unassert(!pthread_mutex_unlock(&statuslock));
}

void *Worker(void *id) {
  pthread_setname_np(pthread_self(), "Worker");

  // connection loop
  while (!a_termsig) {
    int client;
    uint32_t clientsize;
    int inmsglen, outmsglen;
    struct sockaddr_in clientaddr;
    char inbuf[1500], outbuf[1500], *p, *q;

    // musl libc and cosmopolitan libc support a posix thread extension
    // that makes thread cancelation work much better. your io routines
    // will just raise ECANCELED, so you can check for cancelation with
    // normal logic rather than needing to push and pop cleanup handler
    // functions onto the stack, or worse dealing with async interrupts
    unassert(!pthread_setcancelstate(PTHREAD_CANCEL_MASKED, 0));

    // wait for client connection
    clientsize = sizeof(clientaddr);
    client = accept(server, (struct sockaddr *)&clientaddr, &clientsize);

    // turn cancel off, so we don't need to check write() for ecanceled
    unassert(!pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0));

    if (client == -1) {
      // accept() errors are generally ephemeral or recoverable
      // it'd potentially be a good idea to exponential backoff here
      if (errno == ECANCELED) continue;  // pthread_cancel() was called
      LOG("accept() returned %m");
      SomethingHappened();
      continue;
    }

    // this causes read() and write() to raise eagain after some time
    struct timeval timeo = {KEEPALIVE / 1000, KEEPALIVE % 1000};
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));

    // log the incoming http message
    unsigned clientip = ntohl(clientaddr.sin_addr.s_addr);
    ++a_connections;
    LOG("%6H accepted connection from %hhu.%hhu.%hhu.%hhu:%hu", clientip >> 24,
        clientip >> 16, clientip >> 8, clientip, ntohs(clientaddr.sin_port));
    SomethingHappened();
    (void)clientip;

    // message loop
    ssize_t got, sent;
    struct HttpMessage msg;
    do {

      // wait for next http message (non-fragmented required)
      unassert(!pthread_setcancelstate(PTHREAD_CANCEL_MASKED, 0));
      got = read(client, inbuf, sizeof(inbuf));
      for (int i = 0; i < got; ++i) {
        if (!inbuf[i]) inbuf[i] = 1;
      }
      unassert(!pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0));
      if (got <= 0) {
        if (!got) {
          LOG("%6H client disconnected");
        } else if (errno == EAGAIN) {
          LOG("%6H client timed out");
        } else if (errno == ECANCELED) {
          LOG("%6H disconnecting client due to shutdown");
        } else {
          LOG("%6H read() returned %m");
        }
        SomethingHappened();
        break;
      }

      // check that client message wasn't fragmented into more reads
      InitHttpMessage(&msg, kHttpRequest);
      if ((inmsglen = ParseHttpMessage(&msg, inbuf, got)) <= 0) {
        if (!inmsglen) {
          LOG("%6H client sent fragmented message");
        } else {
          LOG("%6H client sent bad message");
        }
        SomethingHappened();
        break;
      }

      // update server status with details of new message
      ++a_messages;
      LOG("%6H received message from %hhu.%hhu.%hhu.%hhu:%hu for path %#.*s",
          clientip >> 24, clientip >> 16, clientip >> 8, clientip,
          ntohs(clientaddr.sin_port), msg.uri.b - msg.uri.a, inbuf + msg.uri.a);
      SomethingHappened();

      // display hello world html page for http://127.0.0.1:8080/
      struct tm tm;
      int64_t unixts;
      struct timespec ts;
      if (msg.method == kHttpGet &&
          (msg.uri.b - msg.uri.a == 1 && inbuf[msg.uri.a + 0] == '/')) {
        q = "<!doctype html>\r\n"
            "<title>hello world</title>\r\n"
            "<h1>hello world</h1>\r\n"
            "<p>this is a fun webpage\r\n"
            "<p>hosted by greenbean\r\n";
        p = stpcpy(outbuf, "HTTP/1.1 200 OK\r\n" STANDARD_RESPONSE_HEADERS
                           "Content-Type: text/html; charset=utf-8\r\n"
                           "Date: ");
        clock_gettime(0, &ts), unixts = ts.tv_sec;
        p = FormatHttpDateTime(p, gmtime_r(&unixts, &tm));
        p = stpcpy(p, "\r\nContent-Length: ");
        p = FormatInt32(p, strlen(q));
        p = stpcpy(p, "\r\n\r\n");
        p = stpcpy(p, q);
        outmsglen = p - outbuf;
        sent = write(client, outbuf, outmsglen);

      } else {
        // display 404 not found error page for every thing else
        q = "<!doctype html>\r\n"
            "<title>404 not found</title>\r\n"
            "<h1>404 not found</h1>\r\n";
        p = stpcpy(outbuf,
                   "HTTP/1.1 404 Not Found\r\n" STANDARD_RESPONSE_HEADERS
                   "Content-Type: text/html; charset=utf-8\r\n"
                   "Date: ");
        clock_gettime(0, &ts), unixts = ts.tv_sec;
        p = FormatHttpDateTime(p, gmtime_r(&unixts, &tm));
        p = stpcpy(p, "\r\nContent-Length: ");
        p = FormatInt32(p, strlen(q));
        p = stpcpy(p, "\r\n\r\n");
        p = stpcpy(p, q);
        outmsglen = p - outbuf;
        sent = write(client, outbuf, p - outbuf);
      }

      // if the client isn't pipelining and write() wrote the full
      // amount, then since we sent the content length and checked
      // that the client didn't attach a payload, we are so synced
      // thus we can safely process more messages
    } while (got == inmsglen &&    //
             sent == outmsglen &&  //
             !msg.headers[kHttpContentLength].a &&
             !msg.headers[kHttpTransferEncoding].a &&
             (msg.method == kHttpGet || msg.method == kHttpHead));

    DestroyHttpMessage(&msg);
    --a_connections;
    SomethingHappened();
    close(client);
  }

  --a_workers;
  SomethingImportantHappened();
  return 0;
}

void PrintEphemeralStatusLine(void) {
  kprintf("\r\e[K\e[32mgreenbean\e[0m "
          "workers=%d "
          "connections=%d "
          "messages=%d%s ",
          a_workers, a_connections, a_messages, status);
}

void OnTerm(int sig) {
  a_termsig = sig;
  status = " shutting down...";
  SomethingHappened();
}

int main(int argc, char *argv[]) {
  int i;

  // print cpu registers and backtrace on crash
  // note that pledge'll makes backtraces worse
  // you can press ctrl+\ to trigger backtraces
  // ShowCrashReports();

  // listen for ctrl-c, terminal close, and kill
  struct sigaction sa = {.sa_handler = OnTerm};
  unassert(!sigaction(SIGINT, &sa, 0));
  unassert(!sigaction(SIGHUP, &sa, 0));
  unassert(!sigaction(SIGTERM, &sa, 0));

  // you can pass the number of threads you want as the first command arg
  int threads = argc > 1 ? atoi(argv[1]) : __get_cpu_count();
  if (!(1 <= threads && threads <= 100000)) {
    tinyprint(2, "error: invalid number of threads\n", NULL);
    exit(1);
  }

  // create listening socket that'll be shared by threads
  int yes = 1;
  struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(PORT)};
  server = socket(AF_INET, SOCK_STREAM, 0);
  if (server == -1) {
    perror("socket");
    exit(1);
  }
  setsockopt(server, SOL_TCP, TCP_FASTOPEN, &yes, sizeof(yes));
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    exit(1);
  }
  if (listen(server, SOMAXCONN)) {
    perror("listen");
    exit(1);
  }

  // print all the ips that 0.0.0.0 would bind
  // Cosmo's GetHostIps() API is much easier than ioctl(SIOCGIFCONF)
  uint32_t *hostips;
  for (hostips = _gc(GetHostIps()), i = 0; hostips[i]; ++i) {
    kprintf("listening on http://%hhu.%hhu.%hhu.%hhu:%hu\n", hostips[i] >> 24,
            hostips[i] >> 16, hostips[i] >> 8, hostips[i], PORT);
  }

  // secure the server
  //
  // pledge() and unveil() let us whitelist which system calls and files
  // the server will be allowed to use. this way if it gets hacked, they
  // won't be able to do much damage, like compromising the whole server
  //
  // pledge violations on openbsd are logged nicely to the system logger
  // but on linux we need to use a cosmopolitan extension to get details
  // although doing that slightly weakens the security pledge() provides
  //
  // if your operating system doesn't support these security features or
  // is too old, then pledge() and unveil() don't consider this an error
  // so it works. if security is critical there's a special call to test
  // which is npassert(!pledge(0, 0)), and npassert(unveil("", 0) != -1)
  __pledge_mode = PLEDGE_PENALTY_RETURN_EPERM;  // c. greenbean --strace
  unveil("/dev/null", "rw");
  unveil(0, 0);
  pledge("stdio inet", 0);

  // initialize our synchronization data structures, which were written
  // by mike burrows in a library called *nsync we've tailored for libc
  unassert(!pthread_cond_init(&statuscond, 0));
  unassert(!pthread_mutex_init(&statuslock, 0));

  // spawn over 9000 worker threads
  //
  // you don't need weird i/o models, or event driven yoyo pattern code
  // to build a massively scalable server. the secret is to use threads
  // with tiny stacks. then you can write plain simple imperative code!
  //
  // we block signals in our worker threads so we won't need messy code
  // to spin on eintr. operating systems also deliver signals to random
  // threads, and we'd have ctrl-c, etc. be handled by the main thread.
  //
  // alternatively you can just use signal() instead of sigaction(); it
  // uses SA_RESTART because all the syscalls the worker currently uses
  // are documented as @restartable which means no EINTR toil is needed
  sigset_t block;
  sigemptyset(&block);
  sigaddset(&block, SIGINT);
  sigaddset(&block, SIGHUP);
  sigaddset(&block, SIGQUIT);
  pthread_attr_t attr;
  int pagesz = getauxval(AT_PAGESZ);
  unassert(!pthread_attr_init(&attr));
  unassert(!pthread_attr_setstacksize(&attr, 65536));
  unassert(!pthread_attr_setguardsize(&attr, pagesz));
  unassert(!pthread_attr_setsigmask_np(&attr, &block));
  unassert(!pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0));
  pthread_t *th = _gc(calloc(threads, sizeof(pthread_t)));
  for (i = 0; i < threads; ++i) {
    int rc;
    ++a_workers;
    if ((rc = pthread_create(th + i, &attr, Worker, (void *)(intptr_t)i))) {
      --a_workers;
      kprintf("pthread_create failed: %s\n", strerror(rc));
      if (rc == EAGAIN) {
        kprintf("sudo prlimit --pid=$$ --nofile=%d\n", threads * 2);
        kprintf("sudo prlimit --pid=$$ --nproc=%d\n", threads * 2);
      }
      if (!i) exit(1);
      threads = i;
      break;
    }
    if (!(i % 50)) {
      PrintEphemeralStatusLine();
    }
  }
  unassert(!pthread_attr_destroy(&attr));

  // show status line on terminal until terminated
  struct timespec tick = timespec_real();
  unassert(!pthread_mutex_lock(&statuslock));
  while (!a_termsig) {
    PrintEphemeralStatusLine();
    unassert(!pthread_cond_wait(&statuscond, &statuslock));
    // limit status line updates to sixty frames per second
    do tick = timespec_add(tick, (struct timespec){0, 1e9 / 60});
    while (timespec_cmp(tick, timespec_real()) < 0);
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &tick, 0);
  }
  unassert(!pthread_mutex_unlock(&statuslock));

  // cancel all the worker threads so they shut down asap
  // and it'll wait on active clients to gracefully close
  // you've never seen a production server close so fast!
  for (i = 0; i < threads; ++i) {
    pthread_cancel(th[i]);
  }

  // on windows this is the only way accept() can be canceled
  if (IsWindows()) close(server);

  // print status in terminal as the shutdown progresses
  unassert(!pthread_mutex_lock(&statuslock));
  while (a_workers) {
    unassert(!pthread_cond_wait(&statuscond, &statuslock));
    PrintEphemeralStatusLine();
  }
  unassert(!pthread_mutex_unlock(&statuslock));

  // wait for final termination and free thread memory
  for (i = 0; i < threads; ++i) {
    unassert(!pthread_join(th[i], 0));
  }

  // close the server socket
  if (!IsWindows()) close(server);

  // clean up terminal line
  LOG("thank you for choosing \e[32mgreenbean\e[0m");

  // clean up more resources
  unassert(!pthread_cond_destroy(&statuscond));
  unassert(!pthread_mutex_destroy(&statuslock));

  // quality assurance
  if (IsModeDbg()) {
    CheckForMemoryLeaks();
  }
}
