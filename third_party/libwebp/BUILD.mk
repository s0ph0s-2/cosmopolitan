#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set noet ft=make ts=8 sw=8 fenc=utf-8 :vi ────────────────────┘

PKGS += THIRD_PARTY_LIBWEBP

THIRD_PARTY_LIBWEBP_HDRS = $(THIRD_PARTY_LIBWEBP_A_HDRS)

THIRD_PARTY_LIBWEBP_ARTIFACTS += THIRD_PARTY_LIBWEBP_A
THIRD_PARTY_LIBWEBP = $(THIRD_PARTY_LIBWEBP_A_DEPS) $(THIRD_PARTY_LIBWEBP_A)
THIRD_PARTY_LIBWEBP_A = o/$(MODE)/third_party/libwebp/libwebp.a

THIRD_PARTY_LIBWEBP_A_HDRS :=							\
		third_party/libwebp/tests/fuzzer/img_grid.h\
		third_party/libwebp/tests/fuzzer/img_alpha.h\
		third_party/libwebp/tests/fuzzer/fuzz_utils.h\
		third_party/libwebp/tests/fuzzer/img_peak.h\
		third_party/libwebp/imageio/tiffdec.h\
		third_party/libwebp/imageio/image_enc.h\
		third_party/libwebp/imageio/image_dec.h\
		third_party/libwebp/imageio/metadata.h\
		third_party/libwebp/imageio/jpegdec.h\
		third_party/libwebp/imageio/webpdec.h\
		third_party/libwebp/imageio/pngdec.h\
		third_party/libwebp/imageio/imageio_util.h\
		third_party/libwebp/imageio/wicdec.h\
		third_party/libwebp/imageio/pnmdec.h\
		third_party/libwebp/examples/example_util.h\
		third_party/libwebp/examples/unicode_gif.h\
		third_party/libwebp/examples/gifdec.h\
		third_party/libwebp/examples/unicode.h\
		third_party/libwebp/examples/stopwatch.h\
		third_party/libwebp/examples/anim_util.h\
		third_party/libwebp/sharpyuv/sharpyuv_csp.h\
		third_party/libwebp/sharpyuv/sharpyuv_dsp.h\
		third_party/libwebp/sharpyuv/sharpyuv_cpu.h\
		third_party/libwebp/sharpyuv/sharpyuv_gamma.h\
		third_party/libwebp/sharpyuv/sharpyuv.h\
		third_party/libwebp/src/mux/animi.h\
		third_party/libwebp/src/mux/muxi.h\
		third_party/libwebp/src/utils/endian_inl_utils.h\
		third_party/libwebp/src/utils/utils.h\
		third_party/libwebp/src/utils/bit_reader_utils.h\
		third_party/libwebp/src/utils/bit_reader_inl_utils.h\
		third_party/libwebp/src/utils/quant_levels_utils.h\
		third_party/libwebp/src/utils/quant_levels_dec_utils.h\
		third_party/libwebp/src/utils/color_cache_utils.h\
		third_party/libwebp/src/utils/filters_utils.h\
		third_party/libwebp/src/utils/random_utils.h\
		third_party/libwebp/src/utils/rescaler_utils.h\
		third_party/libwebp/src/utils/palette.h\
		third_party/libwebp/src/utils/thread_utils.h\
		third_party/libwebp/src/utils/bit_writer_utils.h\
		third_party/libwebp/src/utils/huffman_utils.h\
		third_party/libwebp/src/utils/huffman_encode_utils.h\
		third_party/libwebp/src/webp/format_constants.h\
		third_party/libwebp/src/webp/config.h\
		third_party/libwebp/src/webp/mux.h\
		third_party/libwebp/src/webp/types.h\
		third_party/libwebp/src/webp/demux.h\
		third_party/libwebp/src/webp/mux_types.h\
		third_party/libwebp/src/webp/encode.h\
		third_party/libwebp/src/webp/decode.h\
		third_party/libwebp/src/enc/backward_references_enc.h\
		third_party/libwebp/src/enc/histogram_enc.h\
		third_party/libwebp/src/enc/vp8i_enc.h\
		third_party/libwebp/src/enc/cost_enc.h\
		third_party/libwebp/src/enc/vp8li_enc.h\
		third_party/libwebp/src/dec/vp8i_dec.h\
		third_party/libwebp/src/dec/vp8_dec.h\
		third_party/libwebp/src/dec/common_dec.h\
		third_party/libwebp/src/dec/vp8li_dec.h\
		third_party/libwebp/src/dec/alphai_dec.h\
		third_party/libwebp/src/dec/webpi_dec.h\
		third_party/libwebp/src/dsp/lossless.h\
		third_party/libwebp/src/dsp/msa_macro.h\
		third_party/libwebp/src/dsp/neon.h\
		third_party/libwebp/src/dsp/yuv.h\
		third_party/libwebp/src/dsp/quant.h\
		third_party/libwebp/src/dsp/mips_macro.h\
		third_party/libwebp/src/dsp/cpu.h\
		third_party/libwebp/src/dsp/dsp.h\
		third_party/libwebp/src/dsp/lossless_common.h\
		third_party/libwebp/src/dsp/common_sse2.h\
		third_party/libwebp/src/dsp/common_sse41.h\
		third_party/libwebp/extras/webp_to_sdl.h\
		third_party/libwebp/extras/sharpyuv_risk_table.h\
		third_party/libwebp/extras/extras.h\

THIRD_PARTY_LIBWEBP_A_SRCS := 							\
		third_party/libwebp/sharpyuv/sharpyuv_cpu.c\
		third_party/libwebp/sharpyuv/sharpyuv_csp.c\
		third_party/libwebp/sharpyuv/sharpyuv_dsp.c\
		third_party/libwebp/sharpyuv/sharpyuv_gamma.c\
		third_party/libwebp/sharpyuv/sharpyuv.c\
		third_party/libwebp/sharpyuv/sharpyuv_sse2.c\
		third_party/libwebp/sharpyuv/sharpyuv_neon.c\
		third_party/libwebp/src/dec/alpha_dec.c\
		third_party/libwebp/src/dec/buffer_dec.c\
		third_party/libwebp/src/dec/frame_dec.c\
		third_party/libwebp/src/dec/idec_dec.c\
		third_party/libwebp/src/dec/io_dec.c\
		third_party/libwebp/src/dec/quant_dec.c\
		third_party/libwebp/src/dec/tree_dec.c\
		third_party/libwebp/src/dec/vp8_dec.c\
		third_party/libwebp/src/dec/vp8l_dec.c\
		third_party/libwebp/src/dec/webp_dec.c\
		third_party/libwebp/src/enc/alpha_enc.c\
		third_party/libwebp/src/enc/analysis_enc.c\
		third_party/libwebp/src/enc/backward_references_cost_enc.c\
		third_party/libwebp/src/enc/backward_references_enc.c\
		third_party/libwebp/src/enc/config_enc.c\
		third_party/libwebp/src/enc/cost_enc.c\
		third_party/libwebp/src/enc/filter_enc.c\
		third_party/libwebp/src/enc/frame_enc.c\
		third_party/libwebp/src/enc/histogram_enc.c\
		third_party/libwebp/src/enc/iterator_enc.c\
		third_party/libwebp/src/enc/near_lossless_enc.c\
		third_party/libwebp/src/enc/picture_enc.c\
		third_party/libwebp/src/enc/picture_csp_enc.c\
		third_party/libwebp/src/enc/picture_psnr_enc.c\
		third_party/libwebp/src/enc/picture_rescale_enc.c\
		third_party/libwebp/src/enc/picture_tools_enc.c\
		third_party/libwebp/src/enc/predictor_enc.c\
		third_party/libwebp/src/enc/quant_enc.c\
		third_party/libwebp/src/enc/syntax_enc.c\
		third_party/libwebp/src/enc/token_enc.c\
		third_party/libwebp/src/enc/tree_enc.c\
		third_party/libwebp/src/enc/vp8l_enc.c\
		third_party/libwebp/src/enc/webp_enc.c\
		third_party/libwebp/src/dsp/alpha_processing.c\
		third_party/libwebp/src/dsp/cpu.c\
		third_party/libwebp/src/dsp/dec.c\
		third_party/libwebp/src/dsp/dec_clip_tables.c\
		third_party/libwebp/src/dsp/filters.c\
		third_party/libwebp/src/dsp/lossless.c\
		third_party/libwebp/src/dsp/rescaler.c\
		third_party/libwebp/src/dsp/upsampling.c\
		third_party/libwebp/src/dsp/yuv.c\
		third_party/libwebp/src/dsp/cost.c\
		third_party/libwebp/src/dsp/enc.c\
		third_party/libwebp/src/dsp/lossless_enc.c\
		third_party/libwebp/src/dsp/ssim.c\
		third_party/libwebp/src/dsp/cost_sse2.c\
		third_party/libwebp/src/dsp/enc_sse2.c\
		third_party/libwebp/src/dsp/lossless_enc_sse2.c\
		third_party/libwebp/src/dsp/ssim_sse2.c\
		third_party/libwebp/src/dsp/alpha_processing_sse2.c\
		third_party/libwebp/src/dsp/dec_sse2.c\
		third_party/libwebp/src/dsp/filters_sse2.c\
		third_party/libwebp/src/dsp/lossless_sse2.c\
		third_party/libwebp/src/dsp/rescaler_sse2.c\
		third_party/libwebp/src/dsp/upsampling_sse2.c\
		third_party/libwebp/src/dsp/yuv_sse2.c\
		third_party/libwebp/src/dsp/enc_sse41.c\
		third_party/libwebp/src/dsp/lossless_enc_sse41.c\
		third_party/libwebp/src/dsp/alpha_processing_sse41.c\
		third_party/libwebp/src/dsp/dec_sse41.c\
		third_party/libwebp/src/dsp/lossless_sse41.c\
		third_party/libwebp/src/dsp/upsampling_sse41.c\
		third_party/libwebp/src/dsp/yuv_sse41.c\
		third_party/libwebp/src/dsp/cost_neon.c\
		third_party/libwebp/src/dsp/enc_neon.c\
		third_party/libwebp/src/dsp/lossless_enc_neon.c\
		third_party/libwebp/src/dsp/alpha_processing_neon.c\
		third_party/libwebp/src/dsp/dec_neon.c\
		third_party/libwebp/src/dsp/filters_neon.c\
		third_party/libwebp/src/dsp/lossless_neon.c\
		third_party/libwebp/src/dsp/rescaler_neon.c\
		third_party/libwebp/src/dsp/upsampling_neon.c\
		third_party/libwebp/src/dsp/yuv_neon.c\
		third_party/libwebp/src/dsp/enc_msa.c\
		third_party/libwebp/src/dsp/lossless_enc_msa.c\
		third_party/libwebp/src/dsp/dec_msa.c\
		third_party/libwebp/src/dsp/filters_msa.c\
		third_party/libwebp/src/dsp/lossless_msa.c\
		third_party/libwebp/src/dsp/rescaler_msa.c\
		third_party/libwebp/src/dsp/upsampling_msa.c\
		third_party/libwebp/src/dsp/cost_mips32.c\
		third_party/libwebp/src/dsp/enc_mips32.c\
		third_party/libwebp/src/dsp/lossless_enc_mips32.c\
		third_party/libwebp/src/dsp/dec_mips32.c\
		third_party/libwebp/src/dsp/rescaler_mips32.c\
		third_party/libwebp/src/dsp/yuv_mips32.c\
		third_party/libwebp/src/dsp/cost_mips_dsp_r2.c\
		third_party/libwebp/src/dsp/enc_mips_dsp_r2.c\
		third_party/libwebp/src/dsp/lossless_enc_mips_dsp_r2.c\
		third_party/libwebp/src/dsp/alpha_processing_mips_dsp_r2.c\
		third_party/libwebp/src/dsp/dec_mips_dsp_r2.c\
		third_party/libwebp/src/dsp/filters_mips_dsp_r2.c\
		third_party/libwebp/src/dsp/lossless_mips_dsp_r2.c\
		third_party/libwebp/src/dsp/rescaler_mips_dsp_r2.c\
		third_party/libwebp/src/dsp/upsampling_mips_dsp_r2.c\
		third_party/libwebp/src/dsp/yuv_mips_dsp_r2.c\
		third_party/libwebp/src/utils/bit_reader_utils.c\
		third_party/libwebp/src/utils/color_cache_utils.c\
		third_party/libwebp/src/utils/filters_utils.c\
		third_party/libwebp/src/utils/huffman_utils.c\
		third_party/libwebp/src/utils/palette.c\
		third_party/libwebp/src/utils/quant_levels_dec_utils.c\
		third_party/libwebp/src/utils/rescaler_utils.c\
		third_party/libwebp/src/utils/random_utils.c\
		third_party/libwebp/src/utils/thread_utils.c\
		third_party/libwebp/src/utils/utils.c\
		third_party/libwebp/src/utils/bit_writer_utils.c\
		third_party/libwebp/src/utils/huffman_encode_utils.c\
		third_party/libwebp/src/utils/quant_levels_utils.c

THIRD_PARTY_LIBWEBP_A_OBJS =					\
	$(THIRD_PARTY_LIBWEBP_A_SRCS:%.c=o/$(MODE)/%.o)

THIRD_PARTY_LIBWEBP_A_DIRECTDEPS :=			\
	LIBC_FMT					\
	LIBC_MEM					\
	LIBC_RUNTIME					\
	LIBC_STDIO					\
	LIBC_TINYMATH					\
	LIBC_INTRIN					\
	LIBC_STR					\
	THIRD_PARTY_MUSL				\

THIRD_PARTY_LIBWEBP_A_DEPS :=				\
	$(call uniq,$(foreach x,$(THIRD_PARTY_LIBWEBP_A_DIRECTDEPS),$($(x))))

$(THIRD_PARTY_LIBWEBP_A):				\
		third_party/libwebp/			\
		$(THIRD_PARTY_LIBWEBP_A).pkg		\
		$(THIRD_PARTY_LIBWEBP_A_OBJS)

$(THIRD_PARTY_LIBWEBP_A).pkg:				\
		$(THIRD_PARTY_LIBWEBP_A_OBJS)		\
		$(foreach x,$(THIRD_PARTY_LIBWEBP_A_DIRECTDEPS),$($(x)_A).pkg)

$(THIRD_PARTY_LIBWEBP_A_OBJS): private			\
		CFLAGS +=				\
			-DHAVE_CONFIG_H			\
			-DNDEBUG			\
			-fvisibility=hidden		\
			-Os

THIRD_PARTY_LIBWEBP_LIBS = $(foreach x,$(THIRD_PARTY_LIBWEBP_ARTIFACTS),$($(x)))
THIRD_PARTY_LIBWEBP_SRCS = $(foreach x,$(THIRD_PARTY_LIBWEBP_ARTIFACTS),$($(x)_SRCS))
THIRD_PARTY_LIBWEBP_OBJS = $(foreach x,$(THIRD_PARTY_LIBWEBP_ARTIFACTS),$($(x)_OBJS))
$(THIRD_PARTY_LIBWEBP_OBJS): $(BUILD_FILES) third_party/libwebp/BUILD.mk

.PHONY: o/$(MODE)/third_party/libwebp