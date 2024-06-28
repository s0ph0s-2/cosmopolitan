/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi*/

#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <assert.h>

#include "libc/runtime/runtime.h"
#include "third_party/stb/stb_image_resize.h"
#include "tool/img/lib/image.h"

const int GRADIENT_HASH_THUMBNAIL_WIDTH = 9;
const int GRADIENT_HASH_THUMBNAIL_HEIGHT = 8;

#if 0
/**
 * Print an array of uint8_ts to stdout.
 * @param prefix Arbitrary text to include before the array contents (e.g. a variable name)
 * @param array The array to print
 * @param array_len The length of `array`
 */
void PrintArrayUint8(char *prefix, unsigned char *array, size_t array_len) {
    printf("%s[", prefix);
    for (size_t i = 0; i < array_len; i += 1) {
        printf("%u%s", array[i], (i == array_len - 1) ? "" : ", ");
    }
    printf("]\n");
}

/**
 * Print an array of floats to stdout.
 * @param prefix Arbitrary text to include before the array contents (e.g. a variable name)
 * @param array The array to print
 * @param array_len The length of `array`
 */
void PrintArrayFloat(char *prefix, float *array, size_t array_len) {
    printf("%s[", prefix);
    for (size_t i = 0; i < array_len; i += 1) {
        printf("%f%s", array[i], (i == array_len - 1) ? "" : ", ");
    }
    printf("]\n");
}
#endif

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

/**
 * Compute a Type 2 Discrete Cosine Transform on the given signal.
 * 
 * This implementation is naïve and takes O(n^2), but it fits on one page.
 * 
 * @param input The signal
 * @param input_len The number of samples in the input signal
 * @param output The array in which to put the results of the DCT.
 * @param output_len The capacity in number of elements of the `output` array. (i.e. counted as 'number of floats', not 'number of bytes').
 * @param twiddles The output of `ComputeTwiddlesForLength`.
 * @param twiddles_len The number of twiddles in `twiddles`.
 */
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
}

/**
 * Compute a 1-dimensional type-2 Discrete Cosine Transform on all the rows or columns of an image.
 * @param image The image to apply the transformation to.
 * @param primary_dimension The length of the primary dimension of the image. For example, if you're intending to transform the rows of the image, this parameter is the width of the image.
 * @param secondary_dimension The length of the secondary dimension of the image. For example, if you're intending to transform the rows of the image, this parameter is the height of the image.
 * @param output Where to put the transformed data.
 * @param output_len The length of the `output` array, which must be the same size as `image`.
 * @return 1 if the transformation was successful, 0 if there was a memory allocation error.
 */
int Compute1DDCT2WithScratch(float *image, int primary_dimension, int secondary_dimension, float *output, size_t output_len) {
    assert(primary_dimension * secondary_dimension == output_len);
    double complex *twiddles;
    size_t twiddle_count;

    twiddle_count = ComputeTwiddlesForLength(primary_dimension, &twiddles);
    if (!twiddle_count) {
        return 0;
    }
    for (size_t chunk_idx = 0; chunk_idx < secondary_dimension; chunk_idx += 1) {
        size_t chunk_start_idx = chunk_idx * primary_dimension;
        assert(chunk_start_idx < output_len);
        ComputeDCT2(
            image + chunk_start_idx, 
            primary_dimension,
            output + chunk_start_idx,
            primary_dimension,
            twiddles,
            twiddle_count
        );
    }
    free(twiddles);
    return 1;
}

/**
 * Transpose an X*Y array into an Y*X array. The transposed data is written to `output`. Input is only read, never written.
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

/**
 * Compute a Type 2 Discrete Cosine Transformation (in-place) for both dimensions of a 2D image.
 * @pre There is only one channel in `image`.
 * @param image The image to transform.  This image will be overwritten with the results of the DCT.
 * @return 1 if successful, 0 if there was a memory allocation error.
 */
int Compute2DDCT2(ILImagef32_t image) {
    int rc;
    assert(image.channels == 1);
    size_t scratch_size = image.width * image.height;
    float *scratch = calloc(scratch_size, sizeof(float));
    if (!scratch) {
        return 0;
    }
    rc = Compute1DDCT2WithScratch(image.data, image.width, image.height, scratch, scratch_size);
    if (!rc) {
        free(scratch);
        return 0;
    }
    TransposeLinearizedArray(scratch, image.width, image.height, image.data);

    rc = Compute1DDCT2WithScratch(image.data, image.height, image.width, scratch, scratch_size);
    if (!rc) {
        free(scratch);
        return 0;
    }
    TransposeLinearizedArray(scratch, image.width, image.height, image.data);

    free(scratch);
    return 1;
}

int GradientHash(ILImageu8_t image, uint64_t *hash) {
    int rc;
    uint64_t val;
    size_t dct_width = 18;
    size_t dct_height = 16;

    // Convert image to grayscale
    ILImageu8_t grayscale;
    rc = ILImageu8ConvertToGrayscale(image, &grayscale);
    if (!rc) {
        return 0;
    }
    // Resize the image to appropriate dimensions for the DCT
    ILImageu8_t dct_input_uint8;
    rc = ILImageu8Resize(grayscale, dct_height, dct_width, &dct_input_uint8);
    ILImageu8Free(&grayscale);
    if (!rc) {
        return 0;
    }
    // Compute DCT type 2 of the image in both the row and column directions
    ILImagef32_t dct_input_float;
    rc = ILImageu8ConvertToFloat(dct_input_uint8, &dct_input_float);
    ILImageu8Free(&dct_input_uint8);
    if (!rc) {
        return 0;
    }
    rc = Compute2DDCT2(dct_input_float);
    if (!rc) {
        ILImagef32Free(&dct_input_float);
        return 0;
    }
    // Crop the DCT data to 9 x 8
    ILImagef32_t dct_crop;
    rc = ILImagef32Crop(dct_input_float, &dct_crop, dct_width / 2, dct_height / 2);
    ILImagef32Free(&dct_input_float);
    if (!rc) {
        ILImagef32Free(&dct_crop);
        return 0;
    }
    *hash = 0;
    // Compute each bit of the hash
    for (unsigned int row = 0; row < GRADIENT_HASH_THUMBNAIL_HEIGHT; row += 1) {
        for (unsigned int col = 0; col < (GRADIENT_HASH_THUMBNAIL_WIDTH - 1); col += 1) {
            unsigned int pixel_idx = (row * GRADIENT_HASH_THUMBNAIL_WIDTH) + col;
            val = ((uint64_t)(dct_crop.data[pixel_idx] < dct_crop.data[pixel_idx + 1])) << (((7 - row) * 8) + col);
            *hash |= val;
        }
    }
    ILImagef32Free(&dct_crop);
    return 1;
}