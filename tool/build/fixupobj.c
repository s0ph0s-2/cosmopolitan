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
#include "libc/assert.h"
#include "libc/calls/calls.h"
#include "libc/calls/struct/stat.h"
#include "libc/dce.h"
#include "libc/elf/elf.h"
#include "libc/elf/scalar.h"
#include "libc/elf/struct/rela.h"
#include "libc/elf/struct/shdr.h"
#include "libc/elf/struct/sym.h"
#include "libc/errno.h"
#include "libc/fmt/itoa.h"
#include "libc/fmt/magnumstrs.internal.h"
#include "libc/intrin/bits.h"
#include "libc/intrin/kprintf.h"
#include "libc/limits.h"
#include "libc/log/log.h"
#include "libc/macros.internal.h"
#include "libc/mem/gc.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/msync.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/prot.h"
#include "libc/zip.internal.h"
#include "third_party/getopt/getopt.h"

/**
 * @fileoverview GCC Codegen Fixer-Upper.
 */

#define COSMO_TLS_REG     28
#define MRS_TPIDR_EL0     0xd53bd040u
#define MOV_REG(DST, SRC) (0xaa0003e0u | (SRC) << 16 | (DST))

static const unsigned char kFatNops[8][8] = {
    {},                                          //
    {0x90},                                      // nop
    {0x66, 0x90},                                // xchg %ax,%ax
    {0x0f, 0x1f, 0x00},                          // nopl (%rax)
    {0x0f, 0x1f, 0x40, 0x00},                    // nopl 0x00(%rax)
    {0x0f, 0x1f, 0x44, 0x00, 0x00},              // nopl 0x00(%rax,%rax,1)
    {0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00},        // nopw 0x00(%rax,%rax,1)
    {0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00},  // nopl 0x00000000(%rax)
};

static int mode;
static int fildes;
static char *symstrs;
static char *secstrs;
static ssize_t esize;
static Elf64_Sym *syms;
static const char *epath;
static Elf64_Xword symcount;
static const Elf64_Ehdr *elf;

nullterminated() static void Print(int fd, const char *s, ...) {
  va_list va;
  char buf[2048];
  va_start(va, s);
  buf[0] = 0;
  do {
    strlcat(buf, s, sizeof(buf));
  } while ((s = va_arg(va, const char *)));
  write(fd, buf, strlen(buf));
  va_end(va);
}

static wontreturn void Die(const char *reason) {
  Print(2, epath, ": ", reason, "\n", NULL);
  exit(1);
}

static wontreturn void SysExit(const char *func) {
  const char *errstr;
  if (!(errstr = _strerdoc(errno))) errstr = "EUNKNOWN";
  Print(2, epath, ": ", func, " failed with ", errstr, "\n", NULL);
  exit(1);
}

static wontreturn void PrintUsage(int fd, int exitcode) {
  Print(fd, "\n\
NAME\n\
\n\
  Cosmopolitan Object Fixer\n\
\n\
SYNOPSIS\n\
\n\
  ",
        program_invocation_name, " [FLAGS] OBJECT...\n\
\n\
DESCRIPTION\n\
\n\
  This program applies fixups to ELF object files and executables that\n\
  at build time whenever they're created by the toolchain. It's needed\n\
  so that zip assets work correctly, plus this'll make code go faster.\n\
  This program is also able to spot some coding errors like privileged\n\
  functions calling unprivileged ones.\n\
\n\
  Multiple binary files may be specified, which are modified in-place.\n\
\n\
FLAGS\n\
\n\
  -h            show this help\n\
  -c            checks only mode\n\
\n\
",
        NULL);
  exit(exitcode);
}

static void GetOpts(int argc, char *argv[]) {
  int opt;
  mode = O_RDWR;
  while ((opt = getopt(argc, argv, "ch")) != -1) {
    switch (opt) {
      case 'c':
        mode = O_RDONLY;
        break;
      case 'h':
        PrintUsage(1, 0);
      default:
        PrintUsage(2, 1);
    }
  }
  if (optind == argc) {
    Print(2,
          "error: no elf object files specified\n"
          "run ",
          program_invocation_name, " -h for usage\n", NULL);
    exit(1);
  }
}

static Elf64_Shdr *FindElfSectionByName(const char *name) {
  long i;
  Elf64_Shdr *shdr;
  const char *secname;
  for (i = 0; i < elf->e_shnum; ++i) {
    if ((shdr = GetElfSectionHeaderAddress(elf, esize, i)) &&
        (secname = GetElfString(elf, esize, secstrs, shdr->sh_name)) &&
        !strcmp(name, secname)) {
      return shdr;
    }
  }
  return 0;
}

static void CheckPrivilegedCrossReferences(void) {
  long i;
  unsigned long x;
  Elf64_Shdr *shdr;
  const char *secname;
  Elf64_Rela *rela, *erela;
  if (!(shdr = FindElfSectionByName(".rela.privileged"))) return;
  if (!(rela = GetElfSectionAddress(elf, esize, shdr))) return;
  erela = rela + shdr->sh_size / sizeof(*rela);
  for (; rela < erela; ++rela) {
    if (!ELF64_R_TYPE(rela->r_info)) continue;
    if (!(x = ELF64_R_SYM(rela->r_info))) continue;
    if (x >= symcount) continue;
    if (syms[x].st_shndx == SHN_ABS) continue;
    if (!syms[x].st_shndx) continue;
    if ((shdr = GetElfSectionHeaderAddress(elf, esize, syms[x].st_shndx))) {
      if (~shdr->sh_flags & SHF_EXECINSTR) continue;  // data reference
      if ((secname = GetElfString(elf, esize, secstrs, shdr->sh_name)) &&
          strcmp(".privileged", secname)) {
        Print(2, epath,
              ": code in .privileged section "
              "references symbol '",
              GetElfString(elf, esize, symstrs, syms[x].st_name),
              "' in unprivileged code section '", secname, "'\n", NULL);
        exit(1);
      }
    }
  }
}

// Modify ARM64 code to use x28 for TLS rather than tpidr_el0.
static void RewriteTlsCode(void) {
  int i, dest;
  Elf64_Shdr *shdr;
  uint32_t *p, *pe;
  for (i = 0; i < elf->e_shnum; ++i) {
    if (!(shdr = GetElfSectionHeaderAddress(elf, esize, i))) {
      Die("elf header overflow");
    }
    if (shdr->sh_type == SHT_PROGBITS &&  //
        (shdr->sh_flags & SHF_ALLOC) &&   //
        (shdr->sh_flags & SHF_EXECINSTR)) {
      if (!(p = GetElfSectionAddress(elf, esize, shdr))) {
        Die("elf header overflow");
      }
      for (pe = p + shdr->sh_size / 4; p <= pe; ++p) {
        if ((*p & -32) == MRS_TPIDR_EL0) {
          *p = MOV_REG(*p & 31, COSMO_TLS_REG);
        }
      }
    }
  }
}

/**
 * Improve GCC11 `-fpatchable-function-entry` codegen.
 *
 * When using flags like `-fpatchable-function-entry=9,7` GCC v11 will
 * insert two `nop` instructions, rather than merging them into faster
 * "fat" nops.
 *
 * In order for this to work, the function symbol must be declared as
 * `STT_FUNC` and `st_size` must have the function's byte length.
 */
static void OptimizePatchableFunctionEntries(void) {
#ifdef __x86_64__
  long i, n;
  int nopcount;
  Elf64_Shdr *shdr;
  unsigned char *p, *pe;
  for (i = 0; i < symcount; ++i) {
    if (!syms[i].st_size) continue;
    if (ELF64_ST_TYPE(syms[i].st_info) != STT_FUNC) continue;
    if (!(shdr = GetElfSectionHeaderAddress(elf, esize, syms[i].st_shndx))) {
      Die("elf header overflow");
    }
    if (shdr->sh_type != SHT_PROGBITS) continue;
    if (!(p = GetElfSectionAddress(elf, esize, shdr))) {
      Die("elf header overflow");
    }
    if (syms[i].st_value < shdr->sh_addr) {
      Die("elf symbol beneath section");
    }
    if ((syms[i].st_value - shdr->sh_addr > esize ||
         (p += syms[i].st_value - shdr->sh_addr) >=
             (unsigned char *)elf + esize) ||
        (syms[i].st_size >= esize ||
         (pe = p + syms[i].st_size) >= (unsigned char *)elf + esize)) {
      Die("elf symbol overflow");
    };
    for (; p + 1 < pe; p += n) {
      if (p[0] != 0x90) break;
      if (p[1] != 0x90) break;
      for (n = 2; p + n < pe && n < ARRAYLEN(kFatNops); ++n) {
        if (p[n] != 0x90) break;
      }
      memcpy(p, kFatNops[n], n);
    }
  }
#endif /* __x86_64__ */
}

/**
 * Converts PKZIP recs from PC-relative to RVA-relative.
 */
static void RelinkZipFiles(void) {
  int rela, recs;
  unsigned long cdsize, cdoffset;
  unsigned char foot[kZipCdirHdrMinSize];
  unsigned char *base, *xeof, *stop, *eocd, *cdir, *lfile, *cfile;
  base = (unsigned char *)elf;
  xeof = (unsigned char *)elf + esize;
  eocd = xeof - kZipCdirHdrMinSize;
  stop = base;
  // scan backwards for zip eocd todo record
  // that was created by libc/nexgen32e/zip.S
  for (;;) {
    if (eocd < stop) return;
    if (READ32LE(eocd) == kZipCdirHdrMagicTodo &&  //
        ZIP_CDIR_SIZE(eocd) &&                     //
        !ZIP_CDIR_OFFSET(eocd) &&                  //
        !ZIP_CDIR_RECORDS(eocd) &&                 //
        !ZIP_CDIR_RECORDSONDISK(eocd)) {
      break;
    }
    eocd = memrchr(stop, 'P', eocd - base);
  }
  // apply fixups to zip central directory recs
  recs = 0;
  cdir = (stop = eocd) - (cdsize = ZIP_CDIR_SIZE(eocd));
  for (cfile = cdir; cfile < stop; cfile += ZIP_CFILE_HDRSIZE(cfile)) {
    if (++recs >= 65536) {
      Die("too many zip central directory records");
    }
    if (cfile < base ||                        //
        cfile + kZipCfileHdrMinSize > xeof ||  //
        cfile + ZIP_CFILE_HDRSIZE(cfile) > xeof) {
      Die("zip central directory entry overflows image");
    }
    if (READ32LE(cfile) != kZipCfileHdrMagic) {
      Die("bad __zip_cdir_size or zip central directory corrupted");
    }
    if ((rela = ZIP_CFILE_OFFSET(cfile)) < 0) {
      lfile = cfile + kZipCfileOffsetOffset + rela;
    } else {
      lfile = base + rela;  // earlier fixup failed partway?
    }
    if (lfile < base ||                        //
        lfile + kZipLfileHdrMinSize > xeof ||  //
        lfile + ZIP_LFILE_SIZE(lfile) > xeof) {
      Die("zip local file overflows image");
    }
    if (READ32LE(lfile) != kZipLfileHdrMagic) {
      Die("zip central directory offset to local file corrupted");
    }
    if (rela < 0) {
      WRITE32LE(cfile + kZipCfileOffsetOffset, lfile - base);
    }
  }
  // append new eocd record to program image
  if (esize > INT_MAX - sizeof(foot) ||
      (cdoffset = esize) > INT_MAX - sizeof(foot)) {
    Die("the time has come to adopt zip64");
  }
  bzero(foot, sizeof(foot));
  WRITE32LE(foot, kZipCdirHdrMagic);
  WRITE32LE(foot + kZipCdirSizeOffset, cdsize);
  WRITE16LE(foot + kZipCdirRecordsOffset, recs);
  WRITE32LE(foot + kZipCdirOffsetOffset, cdoffset);
  WRITE16LE(foot + kZipCdirRecordsOnDiskOffset, recs);
  if (pwrite(fildes, cdir, cdsize, esize) != cdsize) {
    SysExit("cdir pwrite");
  }
  if (pwrite(fildes, foot, sizeof(foot), esize + cdsize) != sizeof(foot)) {
    SysExit("eocd pwrite");
  }
  eocd = foot;
}

static void FixupObject(void) {
  if ((fildes = open(epath, mode)) == -1) {
    SysExit("open");
  }
  if ((esize = lseek(fildes, 0, SEEK_END)) == -1) {
    SysExit("lseek");
  }
  if (esize) {
    if ((elf = mmap(0, esize, PROT_READ | PROT_WRITE, MAP_SHARED, fildes, 0)) ==
        MAP_FAILED) {
      SysExit("mmap");
    }
    if (!IsElf64Binary(elf, esize)) {
      Die("not an elf64 binary");
    }
    if (!(syms = GetElfSymbolTable(elf, esize, SHT_SYMTAB, &symcount))) {
      Die("missing elf symbol table");
    }
    if (!(secstrs = GetElfSectionNameStringTable(elf, esize))) {
      Die("missing elf section string table");
    }
    if (!(symstrs = GetElfStringTable(elf, esize, ".strtab"))) {
      Die("missing elf symbol string table");
    }
    CheckPrivilegedCrossReferences();
    if (mode == O_RDWR) {
      if (elf->e_machine == EM_NEXGEN32E) {
        OptimizePatchableFunctionEntries();
      } else if (elf->e_machine == EM_AARCH64) {
        RewriteTlsCode();
      }
      if (elf->e_type != ET_REL) {
        RelinkZipFiles();
      }
      if (msync(elf, esize, MS_ASYNC | MS_INVALIDATE)) {
        SysExit("msync");
      }
    }
    if (munmap(elf, esize)) {
      SysExit("munmap");
    }
  }
  if (close(fildes)) {
    SysExit("close");
  }
}

int main(int argc, char *argv[]) {
  int i, opt;
  if (!IsOptimized()) {
    ShowCrashReports();
  }
  GetOpts(argc, argv);
  for (i = optind; i < argc; ++i) {
    epath = argv[i];
    FixupObject();
  }
}
