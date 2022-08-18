/*bin/echo   ' -*- mode:sh; indent-tabs-mode:nil; tab-width:8; coding:utf-8 -*-│
│vi: set net ft=sh ts=2 sts=2 sw=2 fenc=utf-8                               :vi│
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
╚────────────────────────────────────────────────────────────────'>/dev/null #*/
mkdir -p $dir
rm -f $dir/*.s $dir/*.S

IFS=","

scall() {
  {
    echo ".include \"o/libc/sysv/macros.internal.inc\""
    echo ".scall" "$*"
  } >"$dir/${1/$/-}.s"
}

syscon() {
  {
    echo ".include \"o/libc/sysv/consts/syscon.internal.inc\""
    echo ".syscon" "$*"
  } >"$dir/${2/$/-}.s"
}

sysstr() {
  {
    echo ".include \"o/libc/sysv/macros.internal.inc\""
    echo ".sysstr $1 \"$2\""
  } >"$dir/${1/$/-}.s"
}

errfun() {
  NAME="$1"
  ERRNO="$2"
  {
    printf '#include "libc/macros.internal.h"\n.text.unlikely\n\n'
    printf '%s:' "$NAME"
    if [ "${#NAME}" -gt 6 ]; then
      printf '\n'
    fi
    printf '	.leafprologue
	.profilable
	mov	%s(%%rip),%%ecx
	jmp	__errfun
	.endfn	%s,globl,hidden
' "$ERRNO" "$NAME"
  } >"$dir/${1/$/-}.S"
}
