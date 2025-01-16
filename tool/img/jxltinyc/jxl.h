#ifndef COSMOPOLITAN_TOOL_IMG_JXLTINYC_JXL_H
#define COSMOPOLITAN_TOOL_IMG_JXLTINYC_JXL_H_
COSMOPOLITAN_C_START_

#ifdef __cplusplus
extern "C" {
#endif

int jxltinyc_write_jxl(const char *, int, int, int, const unsigned char *, float);
unsigned char *jxltinyc_write_jxl_to_mem(const unsigned char *, int, int, int, float, int *);

#ifdef __cplusplus
}
#endif

COSMOPOLITAN_C_END_
#endif