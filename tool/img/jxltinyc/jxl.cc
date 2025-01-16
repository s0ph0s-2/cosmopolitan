#include <math.h>
#include <vector>

#include "tool/img/jxltinyc/jxl.h"
#include "third_party/libjxltiny/encoder/enc_file.h"
#include "third_party/libjxltiny/encoder/image.h"

const float SRGB_U = 0.04045f;
const float SRGB_A = 12.92f;
const float SRGB_C = 0.055f;
const float SRGB_GAMMA = 2.4f;

/**
 * Convert sRGB uint8s to float, and undo the sRGB gamma function.
 * See: https://en.wikipedia.org/wiki/SRGB#Transfer_function_(%22gamma%22)
 * @param u_int The subpixel value as an unsigned integer in the range 0..255.
 * @return The same subpixel value, as a float in the range 0..1, and with the sRGB gamma correction undone.
 */
float srgb_to_float(uint8_t u_int) {
  float u = static_cast<float>(u_int) / 255.0f;
  if (u <= SRGB_U) {
    return u / SRGB_A;
  } else {
    return powf((u + SRGB_C) / (1 + SRGB_C), SRGB_GAMMA);
  }
}

bool check_dimensions(int x, int y, int comp) {
  if (x < 0) {
    return false;
  }
  if (y < 0) {
    return false;
  }
  if (comp < 3) {
    return false;
  }
  return true;
}

jxl::Image3F load_image(const unsigned char *data, int x_int, int y_int, int comp_int) {
  size_t x = static_cast<size_t>(x_int);
  size_t y = static_cast<size_t>(y_int);
  size_t comp = static_cast<size_t>(comp_int);
  
  jxl::Image3F img(x, y);
  const size_t stride = x * comp;
  for (size_t y_idx = 0; y_idx < y; ++y_idx) {
    const uint8_t* row_in = &data[y_idx * stride];
    for (size_t c = 0; c < 3; ++c) {
      float* row_out = img.PlaneRow(c, y_idx);
      for (size_t x_idx = 0; x_idx < x; ++x_idx) {
        uint8_t subpixel = row_in[x_idx * comp + c];
        row_out[x_idx] = srgb_to_float(subpixel);
      }
    }
  }
  return img;
}

ssize_t fwrite_with_retry(const void *buf, size_t stride, size_t count, FILE *f) {
  ssize_t ret;
  while (count > 0) {
    do {
      ret = fwrite(buf, stride, count, f);
    } while ((ret < 0) && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
    if (ret < 0)
      return ret;
    count -= ret;
    buf = reinterpret_cast<const uint8_t *>(buf) + (ret * stride);
  }
  return 0;
}

extern "C" {
/**
 * Write JPEG XL encoded image to a file.
 * 
 * This function currently requires that input images must:
 * - have 3 channels
 * - be sRGB
 * @param filename The name of the file to write into.
 * @param x Width of the image, in pixels.
 * @param y Height of the image, in pixels.
 * @param comp Number of components/color channels in the image.
 * @param data The pixel data for the image, as unsigned chars.
 * @param distance The target distance for the JPEG XL encoding. 0.0 is mathematically lossless, 1 is visually lossless. Recommended range is 0.5 to 3.0.
 * @return 1 if successful, 0 if an error occurred.
 */
int jxltinyc_write_jxl(const char * filename, int x, int y, int comp, const unsigned char *data, float distance) {
  if (!check_dimensions(x, y, comp)) {
    return 0;
  }
  jxl::Image3F img = load_image(data, x, y, comp);
  std::vector<uint8_t> output;
  if (!jxl::EncodeFile(img, distance, &output)) {
    return 0;
  }
  
  FILE *f = fopen(filename, "wb");
  if (!f) {
    return 0;
  }
  int rc = fwrite_with_retry(output.data(), sizeof(uint8_t), output.size(), f);
  fclose(f);
  if (rc != 0) {
    printf("failed to write\n");
    return 0;
  }
  return 1;
}

/**
 * Write JPEG XL encoded image to a buffer in memory.
 * 
 * This function currently requires that input images must:
 * - have 3 channels
 * - be sRGB
 * @param data The pixel data for the image, as unsigned chars.
 * @param x Width of the image, in pixels.
 * @param y Height of the image, in pixels.
 * @param comp Number of components/color channels in the image.
 * @param distance The target distance for the JPEG XL encoding. 0.0 is mathematically lossless, 1 is visually lossless. Recommended range is 0.5 to 3.0.
 * @param outsize The length of the output array.
 * @return Pointer to memory containing the JPEG XL-encoded data, which the caller must free. The size of the data is written to `outsize`.
 */
unsigned char *jxltinyc_write_jxl_to_mem(const unsigned char *data, int x, int y, int comp, float distance, int *outsize) {
  if (!check_dimensions(x, y, comp)) {
    return 0;
  }
  jxl::Image3F img = load_image(data, x, y, comp);
  std::vector<uint8_t> encoded;
  if (!jxl::EncodeFile(img, distance, &encoded)) {
    return 0;
  }

  uint8_t *output = reinterpret_cast<uint8_t *>(malloc(encoded.size() * sizeof(uint8_t)));
  if (output == NULL) {
    return 0;
  }
  memcpy(output, encoded.data(), encoded.size() * sizeof(uint8_t));
  *outsize = encoded.size();
  return output;
}

} // extern C