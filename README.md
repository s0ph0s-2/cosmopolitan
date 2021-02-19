![Cosmopolitan Honeybadger](usr/share/img/honeybadger.png)

# Cosmopolitan

[Cosmopolitan Libc](https://justine.lol/cosmopolitan/index.html) makes C
a build-once run-anywhere language, like Java, except it doesn't need an
interpreter or virtual machine. Instead, it reconfigures stock GCC and
Clang to output a POSIX-approved polyglot format that runs natively on
Linux + Mac + Windows + FreeBSD + OpenBSD + NetBSD + BIOS with the best
possible performance and the tiniest footprint imaginable.

## Background

For an introduction to this project, please read the [αcτµαlly pδrταblε
εxεcµταblε](https://justine.lol/ape.html) blog post and [cosmopolitan
libc](https://justine.lol/cosmopolitan/index.html) website. We also have
[API documentation](https://justine.lol/cosmopolitan/documentation.html).

## Getting Started

If you're doing your development work on Linux or BSD then you need just
five files to get started:

```sh
wget https://justine.lol/cosmopolitan/cosmopolitan-amalgamation-0.2.zip
unzip cosmopolitan-amalgamation-0.2.zip
printf 'main() { printf("hello world\\n"); }\n' >hello.c
gcc -g -O -static -nostdlib -nostdinc -fno-pie -no-pie -mno-red-zone \
  -o hello.com.dbg hello.c -fuse-ld=bfd -Wl,-T,ape.lds \
  -include cosmopolitan.h crt.o ape.o cosmopolitan.a
objcopy -S -O binary hello.com.dbg hello.com
./hello.com
```

If you're developing on Windows or MacOS then you need to download an
x86_64-pc-linux-gnu toolchain beforehand. See the [Compiling on
Windows](https://justine.lol/cosmopolitan/windows-compiling.html)
tutorial. It's needed because the ELF object format is what makes
universal binaries possible.

Cosmopolitan can also be compiled from source on any Linux distro.

```sh
wget https://justine.lol/cosmopolitan/cosmopolitan-0.2.tar.gz
tar xf cosmopolitan-0.2.tar.gz  # see releases page
cd cosmopolitan-0.2
make -j16
o//examples/hello.com
find o -name \*.com | xargs ls -rShal | less
```
