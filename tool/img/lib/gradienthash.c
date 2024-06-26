/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <assert.h>

#include "libc/runtime/runtime.h"
#include "third_party/stb/stb_image_resize.h"
#include "third_party/rxi_vec/vec.h"

const int GRADIENT_HASH_THUMBNAIL_WIDTH = 9;
const int GRADIENT_HASH_THUMBNAIL_HEIGHT = 8;

void PrintArrayUint8(char * prefix, unsigned char *array, size_t array_len) {
    printf("%s[", prefix);
    for (size_t i = 0; i < array_len; i += 1) {
        printf("%u%s", array[i], (i == array_len - 1) ? "" : ", ");
    }
    printf("]\n");
}

void PrintArrayFloat(char * prefix, float *array, size_t array_len) {
    printf("%s[", prefix);
    for (size_t i = 0; i < array_len; i += 1) {
        printf("%f%s", array[i], (i == array_len - 1) ? "" : ", ");
    }
    printf("]\n");
}

const float GRADIENT_HASH_SRGB_LUMA[3] = { 0.2126f, 0.7152f, 0.0722f };

inline unsigned char RGBToLuma(const unsigned char *pixel_data) {
    float r, g, b, sum;
    r = GRADIENT_HASH_SRGB_LUMA[0] * (float)pixel_data[0];
    g = GRADIENT_HASH_SRGB_LUMA[1] * (float)pixel_data[1];
    b = GRADIENT_HASH_SRGB_LUMA[2] * (float)pixel_data[2];
    sum = r + g + b;
    /*printf("%03d,%03d,%03d -> %f + %f + %f = %f (%d)\n",
        pixel_data[0],
        pixel_data[1],
        pixel_data[2],
        r,
        g,
        b,
        sum,
        (unsigned char)(uint64_t)(double)sum
    );*/
    return (unsigned char)sum;
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

void ConvertToFloat(const unsigned char *image_data, int image_width, int image_height, float *float_data) {
    size_t length = image_width * image_height;
    for (size_t i = 0; i < length; i += 1) {
        float_data[i] = (float)image_data[i];
    }
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
    // printf("twiddles for %d = [", length);
    for (size_t i = 0; i < twiddle_count; i += 1) {
        twiddles_scratch[i] = ComputeSingleTwiddle(i, twiddle_count);
        // printf("%f%+fi, ", creal(twiddles_scratch[i]), cimag(twiddles_scratch[i]));
    }
    // printf("]\n");
    *twiddles = twiddles_scratch;
    return twiddle_count;
}

void ComputeDCT2(float *input, size_t input_len, float *output, size_t output_len, double complex *twiddles, size_t twiddles_len) {
    assert(output_len >= input_len);
    assert(twiddles_len >= (4 * input_len));

    for (size_t k = 0; k < output_len; k += 1) {
        output[k] = 0;
        const size_t twiddle_stride = k * 2;
        size_t twiddle_idx = k;
        for (size_t i = 0; i < input_len; i += 1) {
            double complex twiddle = twiddles[twiddle_idx];
            output[k] = output[k] + input[i] * creal(twiddle);
            twiddle_idx += twiddle_stride;
            if (twiddle_idx >= twiddles_len) {
                twiddle_idx -= twiddles_len;
            }
        }
    }

    // Try rounding to 0 any values that are almost 0 (fuzzysearch's implementation seems to do this?)
    /*for (size_t k = 0; k < output_len; k += 1) {
        if (fabsf(output[k]) < 0.0001f) {
            output[k] = 0.0f;
        }
    }*/
}

int Compute1DDCT2WithScratch(float *image, int primary_dimension, int secondary_dimension, float *scratch, size_t scratch_len) {
    assert(primary_dimension * secondary_dimension == scratch_len);
    double complex *twiddles;
    size_t twiddle_count;

    twiddle_count = ComputeTwiddlesForLength(primary_dimension, &twiddles);
    if (!twiddle_count) {
        return 0;
    }
    for (size_t chunk_idx = 0; chunk_idx < secondary_dimension; chunk_idx += 1) {
        size_t chunk_start_idx = chunk_idx * primary_dimension;
        assert(chunk_start_idx < scratch_len);
        ComputeDCT2(
            image + chunk_start_idx, 
            primary_dimension,
            scratch + chunk_start_idx,
            primary_dimension,
            twiddles,
            twiddle_count
        );
    }
    free(twiddles);
    return 1;
}

/**
 * Transpose an X*Y array into an Y*X array. The transposed data is written to output. Input is only read, never written.
 * @param input X*Y array
 * @param width the M dimension of `input`
 * @param height the N dimension of `input`
 * @param output Y*X array
 */
void TransposeLinearizedArray(const float *input, int width, int height, float *output) {
    for (size_t x = 0; x < width; x += 1) {
        for (size_t y = 0; y < height; y += 1) {
            size_t input_idx = x + y * width;
            size_t output_idx = y + x * height;
            output[output_idx] = input[input_idx];
        }
    }
}

int Compute2DDCT2(float *image, int width, int height) {
    int rc;
    size_t scratch_size = width * height;
    float *scratch = calloc(scratch_size, sizeof(float));
    if (!scratch) {
        return 0;
    }
    rc = Compute1DDCT2WithScratch(image, width, height, scratch, scratch_size);
    if (!rc) {
        free(scratch);
        return 0;
    }

    // PrintArrayFloat("scratch = ", scratch, scratch_size);

    TransposeLinearizedArray(scratch, width, height, image);

    rc = Compute1DDCT2WithScratch(image, height, width, scratch, scratch_size);
    if (!rc) {
        free(scratch);
        return 0;
    }

    TransposeLinearizedArray(scratch, width, height, image);

    free(scratch);
    return 1;
}

/**
 * Crop the image in both dimensions to the specified dimensions. The resulting crop will be the requested dimensions, aligned in the upper-left corner of the input.
 * @param image_data The input image to crop.
 * @param width The width of the input image
 * @param height The height of the input image
 * @param cropped A pointer in which the address of the allocated cropped data array can be stored
 * @param crop_w The desired crop width
 * @param crop_h The desired crop height
 * @return 1 on success, 0 on failure (memory allocation unsuccessful)
 */
int Crop(float *image_data, int width, int height, float **cropped, int crop_w, int crop_h) {
    float *crop_data = calloc(crop_w * crop_h, sizeof(float));
    if (!crop_data) {
        return 0;
    }
    for (size_t row = 0; row < crop_h; row += 1) {
        for (size_t col = 0; col < crop_w; col += 1) {
            size_t input_idx = (row * width) + col;
            size_t output_idx = (row * crop_w) + col;
            crop_data[output_idx] = image_data[input_idx];
        }
    }
    *cropped = crop_data;
    return 1;
}

typedef struct GHImage {
    unsigned char *data;
    size_t width;
    size_t height;
    size_t channels;
} GHImage_t;

int GHImageInit(GHImage_t *image, size_t width, size_t height, size_t channels) {
    int data_size = width * height * channels;
    image->data = calloc(data_size, sizeof(unsigned char));
    if (!image->data) {
        return 0;
    }
    image->width = width;
    image->height = height;
    image->channels = channels;
    return 1;
}

void GHImageFree(GHImage_t *image) {
    free(image->data);
}

static size_t clamp_int64_to_size(int64_t x, int64_t min, int64_t max) {
    if (x > max) {
        return max;
    } else if (x < min) {
        return min;
    } else {
        return (size_t)x;
    }
}

static uint8_t clamp_float_to_uint8_rounded(float x, float min, float max) {
    if (x > max) {
        return (uint8_t)max;
    } else if (x < min) {
        return (uint8_t)min;
    } else {
        return (uint8_t)roundf(x);
    }
}

static float sincf(float t) {
  float a = t * M_PI;
  if (t == 0.0f) {
    return 1.0f;
  } else {
    return sinf(a) / a;
  }
}

static float lanczos3_kernel(float x) {
  if (fabsf(x) < 3.0f) {
    return sincf(x) * sincf(x / 3.0f);
  } else {
    return 0.0f;
  }
}

static size_t get_pixel_idx(GHImage_t image, size_t x, size_t y) {
    size_t idx = (y * image.width + x) * image.channels;
    // printf("idx = %d * %d + %d = %d\n", y, image.width, x, idx);
    return idx;
}

int ResampleHorizontal(GHImage_t image, int new_width, GHImage_t *output) {
    int rc;
    int width, height;
    vec_float_t ws;
    float max, ratio, sratio, src_support;

    assert(image.channels == 1);
    width = image.width;
    height = image.height;
    rc = GHImageInit(output, new_width, height, image.channels);
    if (!rc) {
        return 0;
    }
    vec_init(&ws);

    // Maximum value of an unsigned char.
    max = 255.0f;
    ratio = (float)width / (float)new_width;
    sratio = (ratio < 1.0) ? 1.0 : ratio;
    // Lanczos always uses 3
    src_support = 3.0f * sratio;

    for (size_t outx = 0; outx < new_width; outx += 1) {
        float inputx;
        int64_t left_l, right_l;
        size_t left, right;
        float sum;

        inputx = ((float)outx + 0.5f) * ratio;

        left_l = (int64_t)floorf(inputx - src_support);
        left = clamp_int64_to_size(left_l, 0, width);

        right_l = (int64_t)ceilf(inputx + src_support);
        right = clamp_int64_to_size(right_l, left + 1, width);

        inputx = inputx - 0.5f;

        vec_clear(&ws);
        sum = 0.0f;
        for (size_t i = left; i < right; i += 1) {
            float w = lanczos3_kernel(((float)i - inputx) / sratio);
            rc = vec_push(&ws, w);
            if (rc < 0) {
                GHImageFree(output);
                vec_deinit(&ws);
                return 0;
            }
            sum += w;
        }

        for (size_t y = 0; y < height; y += 1) {
            float t = 0.0f;

            int i;
            float w;
            vec_foreach(&ws, w, i) {
                size_t pixel_idx = get_pixel_idx(image, left + i, y);
                float k;
                k = (float)(image.data[pixel_idx]);
                t += k * w;
            }

            float tprime = t / sum;
            unsigned char tout = clamp_float_to_uint8_rounded(tprime, 0.0f, max);

            size_t out_idx = get_pixel_idx(*output, outx, y);
            output->data[out_idx] = tout;
        }
    }

    return 1;
}

int ResampleVertical(GHImage_t image, int new_height, GHImage_t *output) {
    int rc;
    int width, height;
    vec_float_t ws;
    float max, ratio, sratio, src_support;

    assert(image.channels == 1);
    width = image.width;
    height = image.height;
    // printf("Resampling %dx%d image to %dx%d\n", width, height, width, new_height);
    rc = GHImageInit(output, width, new_height, image.channels);
    if (!rc) {
        return 0;
    }
    vec_init(&ws);

    // Maximum value of an unsigned char.
    max = 255.0f;
    ratio = (float)height / (float)new_height;
    sratio = (ratio < 1.0) ? 1.0 : ratio;
    // Lanczos always uses 3
    src_support = 3.0f * sratio;

    for (size_t outy = 0; outy < new_height; outy += 1) {
        float inputy;
        int64_t left_l, right_l;
        size_t left, right;
        float sum;

        inputy = ((float)outy + 0.5f) * ratio;

        left_l = (int64_t)floorf(inputy - src_support);
        left = clamp_int64_to_size(left_l, 0, height);

        right_l = (int64_t)ceilf(inputy + src_support);
        right = clamp_int64_to_size(right_l, left + 1, height);

        // printf("left = %d; right = %d\n", left, right);
        inputy = inputy - 0.5f;

        vec_clear(&ws);
        sum = 0.0f;
        for (size_t i = left; i < right; i += 1) {
            float w = lanczos3_kernel(((float)i - inputy) / sratio);
            rc = vec_push(&ws, w);
            // printf("vec_push(&ws, %f)\n", w);
            if (rc < 0) {
                GHImageFree(output);
                vec_deinit(&ws);
                return 0;
            }
            sum += w;
        }
        // printf("w sum = %f\n", sum);
        for (size_t x = 0; x < width; x += 1) {
            float t = 0.0f;

            int i;
            float w;
            vec_foreach(&ws, w, i) {
                // printf("foreach w (%f) at index %d\n", w, i);
                size_t pixel_idx = get_pixel_idx(image, x, left + i);
                // printf("pixel_idx (read) = %d\n", pixel_idx);
                float k;
                k = (float)(image.data[pixel_idx]);
                // printf("pixel value = %f\n", k);
                t += k * w;
            }

            float tprime = t / sum;
            // printf("tprime = %f\n", tprime);
            unsigned char tout = clamp_float_to_uint8_rounded(tprime, 0.0f, max);
            // printf("tout = %d\n", tout);

            size_t output_idx = get_pixel_idx(*output, x, outy);
            output->data[output_idx] = tout;
            // printf("output_pixel_idx = %d\n", output_idx);
            // if (output_idx == 1) {
            //     exit(1);
            // }
        }
    }

    vec_deinit(&ws);

    return 1;
}

int Resize(GHImage_t image, int nheight, int nwidth, GHImage_t *result) {
    GHImage_t intermediate;
    int rc;
    rc = GHImageInit(result, nwidth, nheight, image.channels);
    if (!rc) {
        return 0;
    }
    rc = ResampleVertical(image, nheight, &intermediate);
    if (!rc) {
        GHImageFree(&intermediate);
        GHImageFree(result);
        return 0;
    }
    // PrintArrayUint8("vertical_resample = ", intermediate.data, intermediate.width * intermediate.height * intermediate.channels);
    rc = ResampleHorizontal(intermediate, nwidth, result);
    if (!rc) {
        GHImageFree(&intermediate);
        GHImageFree(result);
        return 0;
    }
    return 1;
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
            // printf("val(%u, %u): %llu\n", row, col, val);
            *hash += val;
        }
    }
    free(thumbnail);
    return 1;
}

void BoolsToBytes(const uint8_t bools[64], uint8_t bytes[8]) {
    for (size_t byte_idx = 0; byte_idx < 8; byte_idx += 1) {
        uint8_t working_byte = 0;
        for (size_t bit_idx = 0; bit_idx < 8; bit_idx += 1) {
            size_t bool_idx = byte_idx * 8 + bit_idx;
            working_byte |= bools[bool_idx] << bit_idx;
        }
        bytes[byte_idx] = working_byte;
    }
}

int GradientHash(unsigned char *image_data, int image_width, int image_height, int image_channels, uint64_t *hash) {
    int rc;
    // uint64_t val;
    size_t dct_width = 18;
    size_t dct_height = 16;
    unsigned char *grayscale = calloc(image_width * image_height, sizeof(unsigned char));
    unsigned char *dct_input_uint8;
    float *dct_input_float;
    float *dct_crop;

    if (!grayscale) return 0;
    // Convert image to grayscale
    rc = ConvertToGrayscale(image_data, image_width, image_height, image_channels, grayscale);
    if (!rc) {
        free(grayscale);
        return 0;
    }
    // PrintArrayUint8("grayscale = ", grayscale, image_width * image_height);
    // Resize the image to appropriate dimensions for the DCT
    /*dct_input_uint8 = calloc(
        dct_width * dct_height,
        sizeof(unsigned char)
    );
    if (!dct_input_uint8) {
        free(grayscale);
        return 0;
    }*/
    /*rc = stbir_resize_uint8(
        grayscale,
        image_width,
        image_height,
        0,
        dct_input_uint8,
        dct_width,
        dct_height,
        0,
        1
    );*/
    GHImage_t grayscale_img;
    grayscale_img.width = image_width;
    grayscale_img.height = image_height;
    grayscale_img.channels = 1;
    grayscale_img.data = grayscale;
    GHImage_t dct_input_uint8_img;
    rc = Resize(grayscale_img, dct_height, dct_width, &dct_input_uint8_img);
    if (!rc) {
        free(grayscale);
        return 0;
    }
    dct_input_uint8 = dct_input_uint8_img.data;
    PrintArrayUint8("img_vals = ", dct_input_uint8, dct_width * dct_height);
    // Compute DCT type 2 of the image in both the row and column directions
    dct_input_float = calloc(dct_width * dct_height, sizeof(float));
    if (!dct_input_float) {
        free(grayscale);
        return 0;
    }
    ConvertToFloat(dct_input_uint8, dct_width, dct_height, dct_input_float);
    rc = Compute2DDCT2(dct_input_float, dct_width, dct_height);
    if (!rc) {
        free(grayscale);
        return 0;
    }
    // Crop the DCT data to 9 x 8
    dct_crop = calloc((dct_width / 2) * (dct_height / 2), sizeof(float));
    if (!dct_crop) {
        free(dct_input_float);
        return 0;
    }
    rc = Crop(dct_input_float, dct_width, dct_height, &dct_crop, dct_width / 2, dct_height / 2);
    if (!rc) {
        free(grayscale);
        return 0;
    }
    free(grayscale);
    PrintArrayFloat("cropped = ", dct_crop, dct_width / 2 * dct_height / 2);
    *hash = 0;
    uint8_t bools[64];
    uint8_t bytes[8];
    size_t bool_idx = 0;
    // Compute each bit of the hash
    for (unsigned int row = 0; row < GRADIENT_HASH_THUMBNAIL_HEIGHT; row += 1) {
        for (unsigned int col = 0; col < (GRADIENT_HASH_THUMBNAIL_WIDTH - 1); col += 1) {
            unsigned int pixel_idx = (row * GRADIENT_HASH_THUMBNAIL_WIDTH) + col;
            bools[bool_idx] = dct_crop[pixel_idx] < dct_crop[pixel_idx + 1];
            bool_idx += 1;
        }
    }
    BoolsToBytes(bools, bytes);
    PrintArrayUint8("hash_bytes = ", bytes, 8);
    free(dct_crop);
    return 1;
}