#ifndef COSMOPOLITAN_TOOL_IMG_LIB_GRADIENTHASH_H_
#define COSMOPOLITAN_TOOL_IMG_LIB_GRADIENTHASH_H_
COSMOPOLITAN_C_START_
#include "tool/img/lib/image.h"

/**
 * Compute the gradient hash (or dHash) of an image, after processing it with a Discrete Cosine Transform.
 * @param image The image to process.
 * @param hash Output parameter for the hash of the image.
 * @return 1 if successful, 0 if there was a memory allocation error.
 */
int GradientHash(ILImageu8_t image, uint64_t *hash);

COSMOPOLITAN_C_END_
#endif /* COSMOPOLITAN_TOOL_IMG_LIB_GRADIENTHASH_H_ */