#ifndef COSMOPOLITAN_LIBC_TINYMATH_ATANF_DATA_H_
#define COSMOPOLITAN_LIBC_TINYMATH_ATANF_DATA_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

#define ATANF_POLY_NCOEFFS 8
extern const struct atanf_poly_data {
  float poly[ATANF_POLY_NCOEFFS];
} __atanf_poly_data _Hide;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_TINYMATH_ATANF_DATA_H_ */
