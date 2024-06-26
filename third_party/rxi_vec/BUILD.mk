#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set noet ft=make ts=8 sw=8 fenc=utf-8 :vi ────────────────────┘

PKGS += THIRD_PARTY_RXI_VEC

THIRD_PARTY_RXI_VEC_ARTIFACTS += THIRD_PARTY_RXI_VEC_A
THIRD_PARTY_RXI_VEC = $(THIRD_PARTY_RXI_VEC_A_DEPS) $(THIRD_PARTY_RXI_VEC_A)
THIRD_PARTY_RXI_VEC_A = o/$(MODE)/third_party/rxi_vec/vec.a
THIRD_PARTY_RXI_VEC_A_FILES := $(wildcard third_party/rxi_vec/*)
THIRD_PARTY_RXI_VEC_A_HDRS = $(filter %.h,$(THIRD_PARTY_RXI_VEC_A_FILES))
THIRD_PARTY_RXI_VEC_A_SRCS = $(filter %.c,$(THIRD_PARTY_RXI_VEC_A_FILES))

THIRD_PARTY_RXI_VEC_A_OBJS =			\
	$(THIRD_PARTY_RXI_VEC_A_SRCS:%.c=o/$(MODE)/%.o)

THIRD_PARTY_RXI_VEC_A_CHECKS =			\
	$(THIRD_PARTY_RXI_VEC_A).pkg		\
	$(THIRD_PARTY_RXI_VEC_A_HDRS:%=o/$(MODE)/%.ok)

THIRD_PARTY_RXI_VEC_A_DIRECTDEPS =			\
	LIBC_INTRIN					\
	LIBC_STR					\
	LIBC_MEM

THIRD_PARTY_RXI_VEC_A_DEPS :=			\
	$(call uniq,$(foreach x,$(THIRD_PARTY_RXI_VEC_A_DIRECTDEPS),$($(x))))

$(THIRD_PARTY_RXI_VEC_A):				\
		third_party/rxi_vec/		\
		$(THIRD_PARTY_RXI_VEC_A).pkg	\
		$(THIRD_PARTY_RXI_VEC_A_OBJS)

$(THIRD_PARTY_RXI_VEC_A).pkg:			\
		$(THIRD_PARTY_RXI_VEC_A_OBJS)	\
		$(foreach x,$(THIRD_PARTY_RXI_VEC_A_DIRECTDEPS),$($(x)_A).pkg)

THIRD_PARTY_RXI_VEC_LIBS = $(foreach x,$(THIRD_PARTY_RXI_VEC_ARTIFACTS),$($(x)))
THIRD_PARTY_RXI_VEC_SRCS = $(foreach x,$(THIRD_PARTY_RXI_VEC_ARTIFACTS),$($(x)_SRCS))
THIRD_PARTY_RXI_VEC_HDRS = $(foreach x,$(THIRD_PARTY_RXI_VEC_ARTIFACTS),$($(x)_HDRS))
THIRD_PARTY_RXI_VEC_CHECKS = $(foreach x,$(THIRD_PARTY_RXI_VEC_ARTIFACTS),$($(x)_CHECKS))
THIRD_PARTY_RXI_VEC_OBJS = $(foreach x,$(THIRD_PARTY_RXI_VEC_ARTIFACTS),$($(x)_OBJS))

.PHONY: o/$(MODE)/third_party/rxi_vec
o/$(MODE)/third_party/rxi_vec:			\
		$(THIRD_PARTY_RXI_VEC_CHECKS)
