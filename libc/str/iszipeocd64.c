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
#include "libc/zip.h"

/**
 * Returns kZipOk if zip64 end of central directory header seems legit.
 */
int IsZipEocd64(const uint8_t *p, size_t n, size_t i) {
  if (i + kZipCdir64HdrMinSize > n) {
    return kZipErrorEocdOffsetOverflow;
  }
  if (READ32LE(p + i) != kZipCdir64HdrMagic) {
    return kZipErrorEocdMagicNotFound;
  }
  if (i + ZIP_CDIR64_HDRSIZE(p + i) + kZipCdir64LocatorSize > n) {
    return kZipErrorEocdSizeOverflow;
  }
  if (ZIP_LOCATE64_MAGIC(p + i + ZIP_CDIR64_HDRSIZE(p + i)) !=
      kZipCdir64LocatorMagic) {
    return kZipErrorCdirLocatorMagic;
  }
  if (ZIP_LOCATE64_OFFSET(p + i + ZIP_CDIR64_HDRSIZE(p + i)) != i) {
    return kZipErrorCdirLocatorOffset;
  }
  if (ZIP_CDIR64_RECORDS(p + i) * kZipCfileHdrMinSize >
      ZIP_CDIR64_SIZE(p + i)) {
    return kZipErrorCdirRecordsOverflow;
  }
  if (ZIP_CDIR64_OFFSET(p + i) + ZIP_CDIR64_SIZE(p + i) > i) {
    return kZipErrorCdirOffsetPastEocd;
  }
  return kZipOk;
}
