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
#include "libc/assert.h"
#include "libc/calls/calls.h"
#include "libc/nt/createfile.h"
#include "libc/nt/enum/accessmask.h"
#include "libc/nt/enum/creationdisposition.h"
#include "libc/nt/enum/fileflagandattributes.h"
#include "libc/nt/enum/filesharemode.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/errfuns.h"

// code size optimization
// <sync libc/sysv/consts.sh>
#define _O_APPEND     0x00000400  // kNtFileAppendData
#define _O_CREAT      0x00000040  // kNtOpenAlways
#define _O_EXCL       0x00000080  // kNtCreateNew
#define _O_TRUNC      0x00000200  // kNtCreateAlways
#define _O_DIRECTORY  0x00010000  // kNtFileFlagBackupSemantics
#define _O_TMPFILE    0x00410000  // AttributeTemporary|FlagDeleteOnClose
#define _O_DIRECT     0x00004000  // kNtFileFlagNoBuffering
#define _O_NONBLOCK   0x00000800  // kNtFileFlagWriteThrough (not sent to win32)
#define _O_RANDOM     0x80000000  // kNtFileFlagRandomAccess
#define _O_SEQUENTIAL 0x40000000  // kNtFileFlagSequentialScan
#define _O_COMPRESSED 0x20000000  // kNtFileAttributeCompressed
#define _O_INDEXED    0x10000000  // !kNtFileAttributeNotContentIndexed
#define _O_CLOEXEC    0x00080000
// </sync libc/sysv/consts.sh>

textwindows int GetNtOpenFlags(int flags, int mode, uint32_t *out_perm,
                               uint32_t *out_share, uint32_t *out_disp,
                               uint32_t *out_attr) {
  bool is_creating_file;
  uint32_t perm, share, disp, attr;

  if (flags &
      ~(O_ACCMODE | _O_APPEND | _O_CREAT | _O_EXCL | _O_TRUNC | _O_DIRECTORY |
        _O_TMPFILE | _O_NONBLOCK | _O_RANDOM | _O_SEQUENTIAL | _O_COMPRESSED |
        _O_INDEXED | _O_CLOEXEC | _O_DIRECT)) {
    return einval();
  }

  switch (flags & O_ACCMODE) {
    case O_RDONLY:
      perm = kNtFileGenericRead;
      break;
    case O_WRONLY:
      perm = kNtFileGenericWrite;
      if (flags & _O_APPEND) {
        // kNtFileAppendData is already present in kNtFileGenericWrite.
        // WIN32 wont act on append access when write is already there.
        perm = kNtFileAppendData;
      }
      break;
    case O_RDWR:
      if (flags & _O_APPEND) {
        perm = kNtFileGenericRead | kNtFileAppendData;
      } else {
        perm = kNtFileGenericRead | kNtFileGenericWrite;
      }
      break;
    default:
      return einval();
  }

  attr = 0;
  is_creating_file = (flags & _O_CREAT) || (flags & _O_TMPFILE) == _O_TMPFILE;

  // POSIX O_EXEC doesn't mean the same thing as kNtGenericExecute. We
  // request execute access when we can determine it from mode's bits.
  // When opening existing files we greedily request execute access so
  // mmap() won't fail. If it causes CreateFile() to fail, our wrapper
  // will try calling CreateFile a second time without execute access.
  if (is_creating_file) {
    if (mode & 0111) {
      perm |= kNtGenericExecute;
    }
    if (~mode & 0200) {
      attr |= kNtFileAttributeReadonly;  // not sure why anyone would
    }
  } else {
    perm |= kNtGenericExecute;
  }

  // Be as generous as possible in sharing file access. Not doing this
  // you'll quickly find yourself no longer able to administer Windows
  if (flags & _O_EXCL) {
    share = kNtFileShareExclusive;
  } else {
    share = kNtFileShareRead | kNtFileShareWrite | kNtFileShareDelete;
  }

  // These POSIX to WIN32 mappings are relatively straightforward.
  if (flags & _O_EXCL) {
    if (is_creating_file) {
      disp = kNtCreateNew;
    } else {
      return einval();
    }
  } else if (is_creating_file) {
    if (flags & _O_TRUNC) {
      disp = kNtCreateAlways;
    } else {
      disp = kNtOpenAlways;
    }
  } else if (flags & _O_TRUNC) {
    disp = kNtTruncateExisting;
  } else {
    disp = kNtOpenExisting;
  }

  // Please use tmpfd() or tmpfile() instead of O_TMPFILE.
  if ((flags & _O_TMPFILE) == _O_TMPFILE) {
    attr |= kNtFileAttributeTemporary | kNtFileFlagDeleteOnClose;
  } else {
    attr |= kNtFileAttributeNormal;
    if (flags & _O_DIRECTORY) {
      attr |= kNtFileFlagBackupSemantics;
    }
  }

  // The Windows content indexing service ravages performance similar to
  // Windows Defender. Please pass O_INDEXED to openat() to enable this.
  if (~flags & _O_INDEXED) {
    attr |= kNtFileAttributeNotContentIndexed;
  }

  // Windows' transparent filesystem compression is really cool, as such
  // we've introduced a nonstandard O_COMPRESSED flag to help you use it
  if (flags & _O_COMPRESSED) {
    attr |= kNtFileAttributeCompressed;
  }

  // Not certain yet what benefit these flags offer.
  if (flags & _O_SEQUENTIAL) attr |= kNtFileFlagSequentialScan;
  if (flags & _O_RANDOM) attr |= kNtFileFlagRandomAccess;
  if (flags & _O_DIRECT) attr |= kNtFileFlagNoBuffering;

  if (out_perm) *out_perm = perm;
  if (out_share) *out_share = share;
  if (out_disp) *out_disp = disp;
  if (out_attr) *out_attr = attr;
  return 0;
}
