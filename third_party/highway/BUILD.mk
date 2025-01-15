#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set noet ft=make ts=8 sw=8 fenc=utf-8 :vi ────────────────────┘

PKGS += THIRD_PARTY_HIGHWAY

THIRD_PARTY_HIGHWAY_HDRS = $(THIRD_PARTY_HIGHWAY_A_HDRS)

THIRD_PARTY_HIGHWAY_ARTIFACTS += THIRD_PARTY_HIGHWAY_A
THIRD_PARTY_HIGHWAY = $(THIRD_PARTY_HIGHWAY_A_DEPS) $(THIRD_PARTY_HIGHWAY_A)
THIRD_PARTY_HIGHWAY_A = o/$(MODE)/third_party/highway/libhwy.a

THIRD_PARTY_HIGHWAY_A_HDRS :=					\
		third_party/highway/hwy/aligned_allocator.h\
		third_party/highway/hwy/base.h\
		third_party/highway/hwy/cache_control.h\
		third_party/highway/hwy/detect_compiler_arch.h\
		third_party/highway/hwy/detect_targets.h\
		third_party/highway/hwy/foreach_target.h\
		third_party/highway/hwy/highway.h\
		third_party/highway/hwy/highway_export.h\
		third_party/highway/hwy/nanobenchmark.h\
		third_party/highway/hwy/ops/arm_neon-inl.h\
		third_party/highway/hwy/ops/arm_sve-inl.h\
		third_party/highway/hwy/ops/emu128-inl.h\
		third_party/highway/hwy/ops/generic_ops-inl.h\
		third_party/highway/hwy/ops/rvv-inl.h\
		third_party/highway/hwy/ops/scalar-inl.h\
		third_party/highway/hwy/ops/set_macros-inl.h\
		third_party/highway/hwy/ops/shared-inl.h\
		third_party/highway/hwy/ops/wasm_128-inl.h\
		third_party/highway/hwy/ops/wasm_256-inl.h\
		third_party/highway/hwy/ops/x86_128-inl.h\
		third_party/highway/hwy/ops/x86_128-inl.h\
		third_party/highway/hwy/ops/x86_256-inl.h\
		third_party/highway/hwy/ops/x86_512-inl.h\
		third_party/highway/hwy/per_target.h\
		third_party/highway/hwy/print-inl.h\
		third_party/highway/hwy/print.h\
		third_party/highway/hwy/targets.h\

THIRD_PARTY_HIGHWAY_A_SRCS := 							\
		third_party/highway/hwy/print.cc\
		third_party/highway/hwy/aligned_allocator.cc\
		third_party/highway/hwy/targets.cc\
		third_party/highway/hwy/per_target.cc\
		third_party/highway/hwy/nanobenchmark.cc

THIRD_PARTY_HIGHWAY_A_OBJS =					\
	$(THIRD_PARTY_HIGHWAY_A_SRCS:%.cc=o/$(MODE)/%.o)

THIRD_PARTY_HIGHWAY_A_DIRECTDEPS :=			\
	LIBC_INTRIN					\
	LIBC_STDIO					\
	LIBC_MEM					\
	THIRD_PARTY_LIBCXX				\
	THIRD_PARTY_LIBCXXABI				\

THIRD_PARTY_HIGHWAY_A_DEPS :=				\
	$(call uniq,$(foreach x,$(THIRD_PARTY_HIGHWAY_A_DIRECTDEPS),$($(x))))

$(THIRD_PARTY_HIGHWAY_A):				\
		third_party/highway/			\
		$(THIRD_PARTY_HIGHWAY_A).pkg		\
		$(THIRD_PARTY_HIGHWAY_A_OBJS)

$(THIRD_PARTY_HIGHWAY_A).pkg:				\
		$(THIRD_PARTY_HIGHWAY_A_OBJS)		\
		$(foreach x,$(THIRD_PARTY_HIGHWAY_A_DIRECTDEPS),$($(x)_A).pkg)

$(THIRD_PARTY_HIGHWAY_A_OBJS): private				\
		CXXFLAGS +=					\
			-DHWY_STATIC_DEFINE			\
			-DNDEBUG				\
			-D__DATE__=\"redacted\"			\
			-D__TIMESTAMP__=\"redacted\"		\
			-D__TIME__=\"redacted\"			\
			-fvisibility=hidden			\
			-fPIC					\
			-fmerge-all-constants			\
			-fmath-errno				\
			-fno-exceptions				\
			-Wnon-virtual-dtor			\
			-Wno-builtin-macro-redefined		\
			-Wno-deprecated-enum-enum-conversion	\


THIRD_PARTY_HIGHWAY_LIBS = $(foreach x,$(THIRD_PARTY_HIGHWAY_ARTIFACTS),$($(x)))
THIRD_PARTY_HIGHWAY_SRCS = $(foreach x,$(THIRD_PARTY_HIGHWAY_ARTIFACTS),$($(x)_SRCS))
THIRD_PARTY_HIGHWAY_OBJS = $(foreach x,$(THIRD_PARTY_HIGHWAY_ARTIFACTS),$($(x)_OBJS))
$(THIRD_PARTY_HIGHWAY_OBJS): $(BUILD_FILES) third_party/highway/BUILD.mk

.PHONY: o/$(MODE)/third_party/highway
