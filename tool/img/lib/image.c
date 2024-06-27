#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "tool/img/lib/image.h"
#include "third_party/stb/stb_image.h"
#include "third_party/rxi_vec/vec.h"

int ILImageu8Init(ILImageu8_t *image, size_t width, size_t height, size_t channels) {
    int data_size = width * height * channels;
    image->data = calloc(data_size, sizeof(uint8_t));
    if (!image->data) {
        return 0;
    }
    image->width = width;
    image->height = height;
    image->channels = channels;
    return 1;
}

void ILImageu8Free(ILImageu8_t *image) {
    free(image->data);
    memset(image, 0, sizeof(*image));
}

int ILImagef32Init(ILImagef32_t *image, size_t width, size_t height, size_t channels) {
    int data_size = width * height * channels;
    image->data = calloc(data_size, sizeof(float));
    if (!image->data) {
        return 0;
    }
    image->width = width;
    image->height = height;
    image->channels = channels;
    return 1;
}

void ILImagef32Free(ILImagef32_t *image) {
    free(image->data);
    memset(image, 0, sizeof(*image));
}

int ILImageu8LoadFromFile(ILImageu8_t *image, char const *filename) {
    int w, h, channels;
    uint8_t *image_data = stbi_load(filename, &w, &h, &channels, 0);
    if (!image_data) {
        // printf("unable to load image: %s\n", stbi_failure_reason());
        return 0;
    }
    image->data = image_data;
    image->width = w;
    image->height = h;
    image->channels = channels;
    return 1;
}

int ILImageu8LoadFromMemory(ILImageu8_t *image, unsigned char const *buffer, size_t buffer_len) {
    int w, h, channels;
    uint8_t *image_data = stbi_load_from_memory(buffer, buffer_len, &w, &h, &channels, 0);
    if (!image_data) {
        // printf("unable to load image: %s\n", stbi_failure_reason());
        return 0;
    }
    image->data = image_data;
    image->width = w;
    image->height = h;
    image->channels = channels;
    return 1;
}

const float IMG_LIB_SRGB_LUMA[3] = { 0.2126f, 0.7152f, 0.0722f };

/**
 * Convert an RGB triplet to an sRGB luma value.
 * @param pixel_data A pointer to memory with at least 3 uint8_ts readable, representing R, G, and B channel values (in that order) in the range 0 to 255.
 * @return A luma channel value in the range 0 to 255.
 */
inline uint8_t RGBToLuma(const uint8_t *pixel_data) {
    float r, g, b, sum;
    r = IMG_LIB_SRGB_LUMA[0] * (float)pixel_data[0];
    g = IMG_LIB_SRGB_LUMA[1] * (float)pixel_data[1];
    b = IMG_LIB_SRGB_LUMA[2] * (float)pixel_data[2];
    sum = r + g + b;
    return (unsigned char)sum;
}

int ILImageu8ConvertToGrayscale(ILImageu8_t image, ILImageu8_t *output) {
    int rc;
    unsigned int input_length = image.width * image.height * image.channels;
    unsigned int output_idx = 0;
    if (image.channels != 3 && image.channels != 4) return 0;
    rc = ILImageu8Init(output, image.width, image.height, 1);
    if (!rc) {
        return 0;
    }
    for (int pixel_idx = 0; pixel_idx < input_length; pixel_idx += image.channels, output_idx += 1) {
        output->data[output_idx] = RGBToLuma(image.data + pixel_idx);
    }
    return 1;
}

int ILImageu8ConvertToFloat(ILImageu8_t image, ILImagef32_t *output) {
    int rc = ILImagef32Init(output, image.width, image.height, image.channels);
    if (!rc) return 0;
    size_t length = image.width * image.height * image.channels;
    for (size_t i = 0; i < length; i += 1) {
        output->data[i] = (float)image.data[i];
    }
    return 1;
}

int ILImagef32Crop(ILImagef32_t image, ILImagef32_t *output, size_t crop_w, size_t crop_h) {
    int rc;
    rc = ILImagef32Init(output, crop_w, crop_h, image.channels);
    if (!rc) return 0;
    for (size_t row = 0; row < crop_h; row += 1) {
        for (size_t col = 0; col < crop_w; col += 1) {
            for (size_t chan = 0; chan < image.channels; chan += 1) {
                size_t input_idx = (row * image.width + col) * image.channels + chan;
                size_t output_idx = (row * crop_w + col) * image.channels + chan;
                output->data[output_idx] = image.data[input_idx];
            }
        }
    }
    return 1;
}

static size_t ILImageu8PixelIdx(ILImageu8_t image, size_t x, size_t y) {
    size_t idx = (y * image.width + x) * image.channels;
    // printf("idx = %d * %d + %d = %d\n", y, image.width, x, idx);
    return idx;
}


/**
 * Clamp an int64 to a size_t in the specified boundaries. Input values outside of the boundaries will result in output values equal to the exceeded boundary.
 * @param x The input value to clamp.
 * @param min The minimum allowable value for `x`.
 * @param max The maximum allowable value for `x`.
 * @return Either `x`, `min`, or `max`, cast to size_t. 
 */
static size_t clamp_int64_to_size(int64_t x, int64_t min, int64_t max) {
    if (x > max) {
        return max;
    } else if (x < min) {
        return min;
    } else {
        return (size_t)x;
    }
}

/**
 * Clamp a float to a uint8_t in the specified boundaries, rounding to the closest integer.  Input values outside of the boundaries will result in output values equal to the exceeded boundary.
 * @param x The input value to clamp.
 * @param min The minimum allowable value for `x`.
 * @param max The maximum allowable value for `x`.
 * @return `x`, rounded to the nearest integer and cast to uint8_t, or `min`/`max`.
 */
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

/**
 * Compute the Lanczos kernel value with a window size of 3 for some point `x`.
 */
static float lanczos3_kernel(float x) {
  if (fabsf(x) < 3.0f) {
    return sincf(x) * sincf(x / 3.0f);
  } else {
    return 0.0f;
  }
}

/**
 * Use a Lanczos3 resampler to reduce (or increase) the width of an image.
 * @pre `image` has only one channel. The code could probably be updated to work with more channels fairly easily, I just couldn't be bothered to test it.
 * @param image The input image to resample.
 * @param new_width The desired new width of `image`.
 * @param output Uninitialized output for the resampled image. Caller must free this with ILImageu8Free().
 * @return 1 if successful, 0 if there was a memory allocation error.
 */
int ILImageu8ResampleHorizontal(ILImageu8_t image, int new_width, ILImageu8_t *output) {
    int rc;
    int width, height;
    vec_float_t ws;
    float max, ratio, sratio, src_support;

    assert(image.channels == 1);
    width = image.width;
    height = image.height;
    rc = ILImageu8Init(output, new_width, height, image.channels);
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
                ILImageu8Free(output);
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
                size_t pixel_idx = ILImageu8PixelIdx(image, left + i, y);
                float k;
                k = (float)(image.data[pixel_idx]);
                t += k * w;
            }

            float tprime = t / sum;
            unsigned char tout = clamp_float_to_uint8_rounded(tprime, 0.0f, max);

            size_t out_idx = ILImageu8PixelIdx(*output, outx, y);
            output->data[out_idx] = tout;
        }
    }

    return 1;
}

/**
 * Use a Lanczos3 resampler to reduce (or increase) the height of an image.
 * @pre `image` has only one channel. The code could probably be updated to work with more channels fairly easily, I just couldn't be bothered to test it.
 * @param image The input image to resample.
 * @param new_height The desired new height of `image`.
 * @param output Uninitialized output for the resampled image. Caller must free this with ILImageu8Free().
 * @return 1 if successful, 0 if there was a memory allocation error.
 */
int ILImageu8ResampleVertical(ILImageu8_t image, int new_height, ILImageu8_t *output) {
    int rc;
    int width, height;
    vec_float_t ws;
    float max, ratio, sratio, src_support;

    assert(image.channels == 1);
    width = image.width;
    height = image.height;
    // printf("Resampling %dx%d image to %dx%d\n", width, height, width, new_height);
    rc = ILImageu8Init(output, width, new_height, image.channels);
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
                ILImageu8Free(output);
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
                size_t pixel_idx = ILImageu8PixelIdx(image, x, left + i);
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

            size_t output_idx = ILImageu8PixelIdx(*output, x, outy);
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

int ILImageu8Resize(ILImageu8_t image, int nheight, int nwidth, ILImageu8_t *result) {
    ILImageu8_t intermediate;
    int rc;
    rc = ILImageu8Init(result, nwidth, nheight, image.channels);
    if (!rc) {
        return 0;
    }
    rc = ILImageu8ResampleVertical(image, nheight, &intermediate);
    if (!rc) {
        ILImageu8Free(&intermediate);
        ILImageu8Free(result);
        return 0;
    }
    // PrintArrayUint8("vertical_resample = ", intermediate.data, intermediate.width * intermediate.height * intermediate.channels);
    rc = ILImageu8ResampleHorizontal(intermediate, nwidth, result);
    if (!rc) {
        ILImageu8Free(&intermediate);
        ILImageu8Free(result);
        return 0;
    }
    return 1;
}