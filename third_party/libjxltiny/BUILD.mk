#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set noet ft=make ts=8 sw=8 fenc=utf-8 :vi ────────────────────┘

PKGS += THIRD_PARTY_LIBJXLTINY

THIRD_PARTY_LIBJXLTINY_HDRS = $(THIRD_PARTY_LIBJXLTINY_A_HDRS)

THIRD_PARTY_LIBJXLTINY_ARTIFACTS += THIRD_PARTY_LIBJXLTINY_A
THIRD_PARTY_LIBJXLTINY = $(THIRD_PARTY_LIBJXLTINY_A_DEPS) $(THIRD_PARTY_LIBJXLTINY_A)
THIRD_PARTY_LIBJXLTINY_A = o/$(MODE)/third_party/libjxltiny/jxl_tiny.a

THIRD_PARTY_LIBJXLTINY_A_HDRS :=					\
		third_party/libjxltiny/encoder/base/bits.h\
		third_party/libjxltiny/encoder/base/byte_order.h\
		third_party/libjxltiny/encoder/base/cache_aligned.h\
		third_party/libjxltiny/encoder/base/compiler_specific.h\
		third_party/libjxltiny/encoder/base/data_parallel.h\
		third_party/libjxltiny/encoder/base/padded_bytes.h\
		third_party/libjxltiny/encoder/base/printf_macros.h\
		third_party/libjxltiny/encoder/base/sanitizer_definitions.h\
		third_party/libjxltiny/encoder/base/span.h\
		third_party/libjxltiny/encoder/base/status.h\
		third_party/libjxltiny/encoder/base/cache_aligned.h\
		third_party/libjxltiny/encoder/ac_context.h\
		third_party/libjxltiny/encoder/ac_strategy.h\
		third_party/libjxltiny/encoder/chroma_from_luma.h\
		third_party/libjxltiny/encoder/common.h\
		third_party/libjxltiny/encoder/config.h\
		third_party/libjxltiny/encoder/dc_group_data.h\
		third_party/libjxltiny/encoder/dct_scales.h\
		third_party/libjxltiny/encoder/enc_ac_strategy.h\
		third_party/libjxltiny/encoder/enc_adaptive_quantization.h\
		third_party/libjxltiny/encoder/enc_bit_writer.h\
		third_party/libjxltiny/encoder/enc_chroma_from_luma.h\
		third_party/libjxltiny/encoder/enc_cluster.h\
		third_party/libjxltiny/encoder/enc_entropy_code.h\
		third_party/libjxltiny/encoder/enc_file.h\
		third_party/libjxltiny/encoder/enc_frame.h\
		third_party/libjxltiny/encoder/enc_group.h\
		third_party/libjxltiny/encoder/enc_huffman_tree.h\
		third_party/libjxltiny/encoder/enc_transforms-inl.h\
		third_party/libjxltiny/encoder/enc_xyb.h\
		third_party/libjxltiny/encoder/entropy_code.h\
		third_party/libjxltiny/encoder/fast_math-inl.h\
		third_party/libjxltiny/encoder/histogram.h\
		third_party/libjxltiny/encoder/image.h\
		third_party/libjxltiny/encoder/quant_weights.h\
		third_party/libjxltiny/encoder/read_pfm.h\
		third_party/libjxltiny/encoder/static_entropy_codes.h\
		third_party/libjxltiny/encoder/token.h\


THIRD_PARTY_LIBJXLTINY_A_SRCS := 							\
		third_party/libjxltiny/encoder/base/cache_aligned.cc\
		third_party/libjxltiny/encoder/base/data_parallel.cc\
		third_party/libjxltiny/encoder/base/padded_bytes.cc\
		third_party/libjxltiny/encoder/dct_scales.cc\
		third_party/libjxltiny/encoder/enc_ac_strategy.cc\
		third_party/libjxltiny/encoder/enc_adaptive_quantization.cc\
		third_party/libjxltiny/encoder/enc_bit_writer.cc\
		third_party/libjxltiny/encoder/enc_chroma_from_luma.cc\
		third_party/libjxltiny/encoder/enc_cluster.cc\
		third_party/libjxltiny/encoder/enc_entropy_code.cc\
		third_party/libjxltiny/encoder/enc_file.cc\
		third_party/libjxltiny/encoder/enc_frame.cc\
		third_party/libjxltiny/encoder/enc_group.cc\
		third_party/libjxltiny/encoder/enc_huffman_tree.cc\
		third_party/libjxltiny/encoder/enc_xyb.cc\
		third_party/libjxltiny/encoder/image.cc\
		third_party/libjxltiny/encoder/quant_weights.cc\
		third_party/libjxltiny/encoder/read_pfm.cc\

THIRD_PARTY_LIBJXLTINY_A_OBJS =					\
	$(THIRD_PARTY_LIBJXLTINY_A_SRCS:%.cc=o/$(MODE)/%.o)

THIRD_PARTY_LIBJXLTINY_A_DIRECTDEPS :=			\
	LIBC_INTRIN					\
	LIBC_SYSV					\
	LIBC_STDIO					\
	LIBC_MEM					\
	LIBC_THREAD					\
	LIBC_TINYMATH					\
	THIRD_PARTY_LIBCXX				\
	THIRD_PARTY_LIBCXXABI				\
	THIRD_PARTY_HIGHWAY				\
	THIRD_PARTY_LIBUNWIND

THIRD_PARTY_LIBJXLTINY_A_DEPS :=			\
	$(call uniq,$(foreach x,$(THIRD_PARTY_LIBJXLTINY_A_DIRECTDEPS),$($(x))))

$(THIRD_PARTY_LIBJXLTINY_A):				\
		third_party/libjxltiny/			\
		$(THIRD_PARTY_LIBJXLTINY_A).pkg		\
		$(THIRD_PARTY_LIBJXLTINY_A_OBJS)

$(THIRD_PARTY_LIBJXLTINY_A).pkg:				\
		$(THIRD_PARTY_LIBJXLTINY_A_OBJS)		\
		$(foreach x,$(THIRD_PARTY_LIBJXLTINY_A_DIRECTDEPS),$($(x)_A).pkg)

# -fmacro-prefix-map=/Users/s0ph0s/Developer/C/libjxltiny=.
$(THIRD_PARTY_LIBJXLTINY_A_OBJS): private			\
		CXXFLAGS +=					\
			-DHWY_STATIC_DEFINE			\
			-DNDEBUG				\
			-D__DATE__=\"redacted\"			\
			-D__TIMESTAMP__=\"redacted\"		\
			-D__TIME__=\"redacted\"			\
			-fno-rtti				\
			-funwind-tables				\
			-fno-omit-frame-pointer			\
			-std=c++11				\
			-fPIC					\
			-Wno-builtin-macro-redefined		\


THIRD_PARTY_LIBJXLTINY_LIBS = $(foreach x,$(THIRD_PARTY_LIBJXLTINY_ARTIFACTS),$($(x)))
THIRD_PARTY_LIBJXLTINY_SRCS = $(foreach x,$(THIRD_PARTY_LIBJXLTINY_ARTIFACTS),$($(x)_SRCS))
THIRD_PARTY_LIBJXLTINY_OBJS = $(foreach x,$(THIRD_PARTY_LIBJXLTINY_ARTIFACTS),$($(x)_OBJS))
$(THIRD_PARTY_LIBJXLTINY_OBJS): $(BUILD_FILES) third_party/libjxltiny/BUILD.mk

.PHONY: o/$(MODE)/third_party/libjxltiny
