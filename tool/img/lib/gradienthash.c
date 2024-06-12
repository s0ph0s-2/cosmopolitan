/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>

#include "third_party/stb/stb_image_resize.h"

const int GRADIENT_HASH_SRGB_LUMA[3] = { 2126, 7152, 722 };
const int GRADIENT_HASH_SRGB_LUMA_DIV = 10000;
const int GRADIENT_HASH_THUMBNAIL_WIDTH = 9;
const int GRADIENT_HASH_THUMBNAIL_HEIGHT = 8;

inline unsigned char RGBToLuma(const unsigned char *pixel_data) {
    return ((pixel_data[0] * GRADIENT_HASH_SRGB_LUMA[0])
        + (pixel_data[1] * GRADIENT_HASH_SRGB_LUMA[1])
        + (pixel_data[2] * GRADIENT_HASH_SRGB_LUMA[2])) / GRADIENT_HASH_SRGB_LUMA_DIV;
}

int ConvertToGrayscale(const unsigned char *image_data,
                       int image_width, int image_height,
                       int image_channels,
                       unsigned char *grayscale_data) {
    unsigned int input_length = image_width * image_height * image_channels;
    unsigned int output_idx = 0;
    if (image_channels != 3 && image_channels != 4) return 0;
    for (int pixel_idx = 0; pixel_idx < input_length; pixel_idx += image_channels, output_idx += 1) {
        grayscale_data[output_idx] = RGBToLuma(image_data + pixel_idx);
    }
    return 1;
}

// The DCT implementation here is based on
// https://github.com/ejmahler/rust_dct/blob/master/src/algorithm/type2and3_naive.rs
double complex ComputeSingleTwiddle(size_t i, size_t fft_len) {
    double angle_constant = M_PI * -2.0 / ((double)fft_len);
    double theta = angle_constant * (double)i;
    double complex c = CMPLX(cos(theta), sin(theta));
    return c;
}

/**
 * Compute the twiddle factors for a DCT type 2 or 3 that will process signals of length `length`.
 * @param length The length of the signal to process
 * @param twiddles A pointer to a memory address where this function can return the array of twiddle factors (pointer to allocated memory). Caller must free this memory.
 * @return The length of the array in `twiddles`. Will be 0 if memory allocation error.
*/
size_t ComputeTwiddlesForLength(size_t length, double complex **twiddles) {
    size_t twiddle_count = length * 4;
    double complex *twiddles_scratch = calloc(twiddle_count, sizeof(double complex));
    if (!twiddles_scratch) {
        return 0;
    }
    for (size_t i = 0; i < twiddle_count; i += 1) {
        twiddles_scratch[i] = ComputeSingleTwiddle(i, twiddle_count);
    }
    *twiddles = twiddles_scratch;
    return twiddle_count;
}

int ComputeDCT2WithScratch(unsigned char *buffer, size_t buffer_size, unsigned char *scratch, size_t scratch_size, double complex *twiddles, size_t twiddles_size) {
    // TODO: validate buffer and scratch lengths
    assert(scratch_size >= buffer_size);
    assert(twiddles_size >= (4 * buffer_size));
    memcpy(scratch, buffer, buffer_size);

    for (size_t k = 0; k < buffer_size; k += 1) {
        buffer[k] = 0;
        size_t twiddle_stride = k * 2;
        size_t twiddle_idx = k;
        for (size_t i = 0; i < buffer_size; i += 1) {
            double complex twiddle = twiddles[twiddle_idx];
            buffer[k] = buffer[k] + scratch[i] * creal(twiddle);
            twiddle_idx += twiddle_stride;
            if (twiddle_idx >= twiddles_size) {
                twiddle_idx -= twiddles_size;
            }
        }
    }
}

int GradientHashResize(unsigned char *image_data, int image_width, int image_height, int image_channels, uint64_t *hash) {
    int rc;
    uint64_t val;
    unsigned char *grayscale = calloc(image_width * image_height, sizeof(unsigned char));
    if (!grayscale) return 0;
    // Convert image to grayscale
    rc = ConvertToGrayscale(image_data, image_width, image_height, image_channels, grayscale);
    if (!rc) {
        free(grayscale);
        return 0;
    }
    // Resize image to 9 x 8
    unsigned char *thumbnail = calloc(
        GRADIENT_HASH_THUMBNAIL_HEIGHT * GRADIENT_HASH_THUMBNAIL_WIDTH,
        sizeof(unsigned char)
    );
    if (!thumbnail) {
        free(grayscale);
        return 0;
    }
    rc = stbir_resize_uint8(
        grayscale,
        image_width,
        image_height,
        0,
        thumbnail,
        GRADIENT_HASH_THUMBNAIL_WIDTH,
        GRADIENT_HASH_THUMBNAIL_HEIGHT,
        0,
        1
    );
    free(grayscale);
    if (!rc) {
        free(thumbnail);
        return 0;
    }
    // Compute each bit of the hash
    for (unsigned int row = 0; row < GRADIENT_HASH_THUMBNAIL_HEIGHT; row += 1) {
        for (unsigned int col = 0; col < (GRADIENT_HASH_THUMBNAIL_WIDTH - 1); col += 1) {
            unsigned int pixel_idx = (row * GRADIENT_HASH_THUMBNAIL_WIDTH) + col;
            val = ((uint64_t)(thumbnail[pixel_idx] > thumbnail[pixel_idx + 1])) << ((row * 8) + col);
            printf("val(%u, %u): %llu\n", row, col, val);
            *hash += val;
        }
    }
    free(thumbnail);
    return 1;
}

int GradientHash(unsigned char *image_data, int image_width, int image_height, int image_channels, uint64_t *hash) {
    int rc;
    uint64_t val;
    unsigned char *grayscale = calloc(image_width * image_height, sizeof(unsigned char));
    if (!grayscale) return 0;
    // Convert image to grayscale
    rc = ConvertToGrayscale(image_data, image_width, image_height, image_channels, grayscale);
    if (!rc) {
        free(grayscale);
        return 0;
    }
    // Compute DCT type 2 of the image in both the row and column directions
    unsigned char *dct;
    unsigned char *dct_scratch;
    free(dct);
    free(dct_scratch);
    // Crop the DCT data to 9 x 8
    unsigned char *dct_crop;
    // Compute each bit of the hash
    for (unsigned int row = 0; row < GRADIENT_HASH_THUMBNAIL_HEIGHT; row += 1) {
        for (unsigned int col = 0; col < (GRADIENT_HASH_THUMBNAIL_WIDTH - 1); col += 1) {
            unsigned int pixel_idx = (row * GRADIENT_HASH_THUMBNAIL_WIDTH) + col;
            val = ((uint64_t)(dct_crop[pixel_idx] > dct_crop[pixel_idx + 1])) << ((row * 8) + col);
            printf("val(%u, %u): %llu\n", row, col, val);
            *hash += val;
        }
    }
    free(dct_crop);
    return 1;
}