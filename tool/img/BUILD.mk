#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set noet ft=make ts=8 sw=8 fenc=utf-8 :vi ────────────────────┘

PKGS += TOOL_IMG

TOOL_IMG_FILES := $(wildcard tool/img/*)
TOOL_IMG_HDRS = $(filter %.h,$(TOOL_IMG_FILES))
TOOL_IMG_SRCS_C = $(filter %.c,$(TOOL_IMG_FILES))
TOOL_IMG_SRCS_S = $(filter %.S,$(TOOL_IMG_FILES))
TOOL_IMG_SRCS = $(TOOL_IMG_SRCS_C) $(TOOL_IMG_SRCS_S)
TOOL_IMG_OBJS = $(TOOL_IMG_SRCS_C:%.c=o/$(MODE)/%.o) $(TOOL_IMG_SRCS_S:%.S=o/$(MODE)/%.o)
TOOL_IMG_BINS = $(TOOL_IMG_COMS) $(TOOL_IMG_COMS:%=%.dbg)

TOOL_IMG_COMS =							\
	$(TOOL_IMG_SRCS:%.c=o/$(MODE)/%)

TOOL_IMG_DIRECTDEPS =						\
	LIBC_CALLS						\
	LIBC_FMT						\
	LIBC_MEM						\
	LIBC_RUNTIME						\
	LIBC_STDIO						\
	LIBC_STR						\
	LIBC_SYSV						\
	THIRD_PARTY_STB						\
	TOOL_IMG_LIB

TOOL_IMG_DEPS :=					\
	$(call uniq,$(foreach x,$(TOOL_IMG_DIRECTDEPS),$($(x))))

o/$(MODE)/tool/img/img.pkg:						\
		$(TOOL_IMG_OBJS)					\
		$(foreach x,$(TOOL_IMG_DIRECTDEPS),$($(x)_A).pkg)

o/$(MODE)/tool/img/%.dbg:						\
		$(TOOL_IMG_DEPS)					\
		o/$(MODE)/tool/img/img.pkg				\
		o/$(MODE)/tool/img/%.o					\
		$(CRT)							\
		$(APE_NO_MODIFY_SELF)
	@$(APELINK)

.PHONY: o/$(MODE)/tool/img
o/$(MODE)/tool/img:							\
		o/$(MODE)/tool/img/lib					\
		$(TOOL_IMG_BINS)					\
		$(TOOL_IMG_CHECKS)