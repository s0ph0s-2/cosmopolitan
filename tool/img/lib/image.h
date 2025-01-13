#ifndef COSMOPOLITAN_TOOL_IMG_LIB_IMAGE_H_
#define COSMOPOLITAN_TOOL_IMG_LIB_IMAGE_H_
COSMOPOLITAN_C_START_

typedef struct ILImageu8 {
    uint8_t *data;
    size_t width;
    size_t height;
    size_t channels;
} ILImageu8_t;

typedef struct ILImagef32 {
    float *data;
    size_t width;
    size_t height;
    size_t channels;
} ILImagef32_t;

/**
 * Initialize a new image. Each channel is stored in 8-bit unsigned integers.
 * @param image The uninitialized image to initialize.
 * @param width The width of the image.
 * @param height The height of the image.
 * @param channels The number of channels in the image.
 * @return 1 if successful, 0 if there was a memory allocation error.
 */
int ILImageu8Init(ILImageu8_t *, size_t, size_t, size_t);
/**
 * Initialize a new image by loading data from a file. Supports all formats allowed by STB.
 * @param image Uninitialized image to put data into.  Caller must free when finished.
 * @param filename The name of the file to load data from.
 * @return 1 if successful, 0 if there was an error. Use stbi_failure_reason() to get an error message.
 */
int ILImageu8LoadFromFile(ILImageu8_t *, char const *);
/**
 * Initialize a new image by loading data from a buffer. Supports all formats allowed by STB.
 * @param image Uninitialized image to put data into.  Caller must free when finished.
 * @param buffer The buffer to load image data from.
 * @param buffer_len The length of `buffer`.
 * @return 1 if successful, 0 if there was an error. Use stbi_failure_reason() to get an error message.
 */
int ILImageu8LoadFromMemory(ILImageu8_t *, unsigned char const *, size_t);
/**
 * Save the provided image to a WebP file with the specified filename.
 * @param image The image to save to a file.
 * @param filename The path and name of the file to create.
 * @param quality The WebP quality setting to use for encoding.
 * @return 1 if successful, 0 if there was an error.
 */
int ILImageu8SaveWebPFile(ILImageu8_t, char const *, float);
/**
 * Encode the provided image as a WebP file to a buffer in memory.
 * @param image The image to encode as WebP.
 * @param quality The WebP quality setting to use for encoding.
 * @param length The length of the data in the returned buffer.
 * @return NULL if there was an error, otherwise a pointer to a section of allocated memory that is `length` bytes long, containing a WebP encoding of `image`.
 */
uint8_t *ILImageu8SaveWebPBuffer(ILImageu8_t, float, int *);
/**
 * Save the provided image to a PNG file with the specified filename.
 * @param image The image to save to a file.
 * @param filename The path and name of the file to create.
 * @param stride_bytes Skip this many bytes between each row (in case you're doing a quick-and-dirty crop).
 * @return 1 if successful, 0 if there was an error.
 */
int ILImageu8SavePNGFile(ILImageu8_t, char const *, int);
/**
 * Encode the provided image as a PNG file to a buffer in memory.
 * @param image The image to encode as PNG.
 * @param stride_bytes Skip this many bytes between each row (in case you're doing a quick-and-dirty crop).
 * @param length The length of the data in the returned buffer.
 * @return NULL if there was an error, otherwise a pointer to a section of allocated memory that is `length` bytes long, containing a PNG encoding of `image`.
 */
uint8_t *ILImageu8SavePNGBuffer(ILImageu8_t, int, int *);
/**
 * Save the provided image to a JPEG file with the specified filename.
 * @param image The image to save to a file.
 * @param filename The path and name of the file to create.
 * @param quality The quality of the JPEG encode.  Only `100` works.
 * @return 1 if successful, 0 if there was an error.
 */
int ILImageu8SaveJPEGFile(ILImageu8_t, char const *, int);
/**
 * Encode the provided image as a JPEG file to a buffer in memory.
 * @param image The image to encode as JPEG.
 * @param quality The JPEG quality setting to use for encoding.
 * @param length The length of the data in the returned buffer.
 * @return NULL if there was an error, otherwise a pointer to a section of allocated memory that is `length` bytes long, containing a JPEG encoding of `image`.
 */
uint8_t *ILImageu8SaveJPEGBuffer(ILImageu8_t, int, int *);
/**
 * Free an image after you're done using it.
 * @param image The image to free.
 */
void ILImageu8Free(ILImageu8_t *);
/**
 * Convert an image to grayscale (sRGB luma from BT.709).
 * @param image The input image to convert to grayscale.
 * @param output An un-initialized output image (this function initializes it).
 * @return 1 if successful, 0 if an error occurred (wrong number of channels, out of memory). If an error is returned, no cleanup is required on the caller's part.
 */
int ILImageu8ConvertToGrayscale(ILImageu8_t, ILImageu8_t *);
/**
 * Convert an image of unsigned 8-bit integer values to an image of 32-bit floating point values.
 * @param image The input image (8 bit integers for each channel).
 * @param output The output image (uninitialized, 32 bit floating point values for each channel).
 * @return 1 if successful, 0 if no memory.
 */
int ILImageu8ConvertToFloat(ILImageu8_t, ILImagef32_t *);
/**
 * Compute the index into the image's internal data array for a particular pixel by x and y coordinates.
 * 
 * This function assumes that the origin is in the upper left corner of the image, with positive x across the horizontal axis (width) and positive y down the vertical axis (height).
 * 
 * @param image The image to calculate the index into.
 * @param x The X coordinate for the pixel in question.
 * @param y The Y coordinate for the pixel in question.
 * @return The index into the image's internal array for the pixel at (x, y).
 */
static size_t ILImageu8PixelIdx(ILImageu8_t, size_t, size_t);
/**
 * Resize an image using Lanczos3 resampling.
 * @param image The image to resize.
 * @param nheight The desired new height of the image.
 * @param nwidth The desired new width of the image.
 * @param result Uninitialized image for the output. Caller must free this after using it.
 * @return 1 if successful, 0 if there was a memory allocation error.
 */
int ILImageu8Resize(ILImageu8_t, int, int, ILImageu8_t *);
/**
 * Resize an image using Lanczos3 resampling to fit within a square with sides of the specified length.
 * @param image The image to resize.
 * @param side_size The size of the sides of the square, in pixels.
 * @param result The resized image.
 * @return 1 if successful, 0 if there was a memory allocation error.
 */
int ILImageu8ResizeToFitSquare(ILImageu8_t, size_t, ILImageu8_t *);

/**
 * Initialize a new image. Each channel is stored in 32-bit floating point numbers.
 * @param image The uninitialized image to initialize.
 * @param width The width of the image.
 * @param height The height of the image.
 * @param channels The number of channels in the image.
 * @return 1 if successful, 0 if there was a memory allocation error.
 */
int ILImagef32Init(ILImagef32_t *, size_t, size_t, size_t);
/**
 * Free an image after you're done using it.
 * @param image The image to free.
 */
void ILImagef32Free(ILImagef32_t *);
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
int ILImagef32Crop(ILImagef32_t, ILImagef32_t *, size_t, size_t);

COSMOPOLITAN_C_END_
#endif /* COSMOPOLITAN_TOOL_IMG_LIB_IMAGE_H_ */