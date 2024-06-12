#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set noet ft=make ts=8 sw=8 fenc=utf-8 :vi ────────────────────┘

PKGS += TOOL_IMG_LIB

TOOL_IMG_LIB_ARTIFACTS += TOOL_IMG_LIB_A
TOOL_IMG_LIB = $(TOOL_IMG_LIB_A_DEPS) $(TOOL_IMG_LIB_A)
TOOL_IMG_LIB_A = o/$(MODE)/tool/img/lib/imglib.a
TOOL_IMG_LIB_A_HDRS = $(filter %.h,$(TOOL_IMG_LIB_A_FILES))
TOOL_IMG_LIB_A_SRCS_S = $(filter %.S,$(TOOL_IMG_LIB_A_FILES))
TOOL_IMG_LIB_A_SRCS_C = $(filter %.c,$(TOOL_IMG_LIB_A_FILES))
TOOL_IMG_LIB_A_CHECKS = $(TOOL_IMG_LIB_A).pkg

TOOL_IMG_LIB_A_FILES :=					\
	$(wildcard tool/img/lib/*)

TOOL_IMG_LIB_A_SRCS =					\
	$(TOOL_IMG_LIB_A_SRCS_S)			\
	$(TOOL_IMG_LIB_A_SRCS_C)

TOOL_IMG_LIB_A_OBJS =					\
	$(TOOL_IMG_LIB_A_SRCS_S:%.S=o/$(MODE)/%.o)	\
	$(TOOL_IMG_LIB_A_SRCS_C:%.c=o/$(MODE)/%.o)

TOOL_IMG_LIB_A_DIRECTDEPS =				\
	LIBC_FMT					\
	LIBC_INTRIN					\
	LIBC_MEM					\
	LIBC_STDIO					\
	LIBC_STR					\
	LIBC_TINYMATH				\
	THIRD_PARTY_STB

TOOL_IMG_LIB_A_DEPS :=					\
	$(call uniq,$(foreach x,$(TOOL_IMG_LIB_A_DIRECTDEPS),$($(x))))

$(TOOL_IMG_LIB_A):					\
		tool/img/lib/				\
		$(TOOL_IMG_LIB_A).pkg			\
		$(TOOL_IMG_LIB_A_OBJS)

$(TOOL_IMG_LIB_A).pkg:					\
		$(TOOL_IMG_LIB_A_OBJS)			\
		$(foreach x,$(TOOL_IMG_LIB_A_DIRECTDEPS),$($(x)_A).pkg)

$(TOOL_IMG_LIB_A_OBJS): tool/viz/lib/BUILD.mk

TOOL_IMG_LIB_LIBS = $(foreach x,$(TOOL_IMG_LIB_ARTIFACTS),$($(x)))
TOOL_IMG_LIB_SRCS = $(foreach x,$(TOOL_IMG_LIB_ARTIFACTS),$($(x)_SRCS))
TOOL_IMG_LIB_HDRS = $(foreach x,$(TOOL_IMG_LIB_ARTIFACTS),$($(x)_HDRS))
TOOL_IMG_LIB_BINS = $(foreach x,$(TOOL_IMG_LIB_ARTIFACTS),$($(x)_BINS))
TOOL_IMG_LIB_CHECKS = $(foreach x,$(TOOL_IMG_LIB_ARTIFACTS),$($(x)_CHECKS))
TOOL_IMG_LIB_OBJS = $(foreach x,$(TOOL_IMG_LIB_ARTIFACTS),$($(x)_OBJS))
TOOL_IMG_LIB_TESTS = $(foreach x,$(TOOL_IMG_LIB_ARTIFACTS),$($(x)_TESTS))

.PHONY: o/$(MODE)/tool/img/lib
o/$(MODE)/tool/img/lib: $(TOOL_IMG_LIB_CHECKS)
