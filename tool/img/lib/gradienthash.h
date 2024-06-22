#ifndef COSMOPOLITAN_TOOL_IMG_LIB_GRADIENTHASH_H_
#define COSMOPOLITAN_TOOL_IMG_LIB_GRADIENTHASH_H_
COSMOPOLITAN_C_START_

void TransposeLinearizedArray(const double *, int, int, double *);
int Crop(double *, int, int, double **, int, int);
int GradientHash(unsigned char *, int, int, int, uint64_t *);

COSMOPOLITAN_C_END_
#endif /* COSMOPOLITAN_TOOL_IMG_LIB_GRADIENTHASH_H_ */