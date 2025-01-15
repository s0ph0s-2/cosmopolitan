// Copyright (c) the JPEG XL Project Authors.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef ENCODER_QUANT_WEIGHTS_H_
#define ENCODER_QUANT_WEIGHTS_H_

#include <stdint.h>
#include <string.h>

#include "third_party/highway/hwy/aligned_allocator.h"

#include "third_party/libjxltiny/encoder/ac_strategy.h"
#include "third_party/libjxltiny/encoder/base/cache_aligned.h"
#include "third_party/libjxltiny/encoder/base/compiler_specific.h"
#include "third_party/libjxltiny/encoder/base/status.h"

namespace jxl {
// Let's try to keep these 2**N for possible future simplicity.
const float kInvDCQuant[3] = {
    4096.0f,
    512.0f,
    256.0f,
};

const float kDCQuant[3] = {
    1.0f / kInvDCQuant[0],
    1.0f / kInvDCQuant[1],
    1.0f / kInvDCQuant[2],
};

class DequantMatrices {
 public:
  DequantMatrices();

  // Returns aligned memory.
  JXL_INLINE const float* Matrix(size_t quant_kind, size_t c) const {
    JXL_DASSERT(quant_kind < AcStrategy::kNumValidStrategies);
    return &table_[table_offsets_[quant_kind * 3 + c]];
  }

  JXL_INLINE const float* InvMatrix(size_t quant_kind, size_t c) const {
    JXL_DASSERT(quant_kind < AcStrategy::kNumValidStrategies);
    return &inv_table_[table_offsets_[quant_kind * 3 + c]];
  }

 private:
  hwy::AlignedFreeUniquePtr<float[]> table_storage_;
  const float* table_;
  const float* inv_table_;
  size_t table_offsets_[AcStrategy::kNumValidStrategies * 3];
};

}  // namespace jxl

#endif  // ENCODER_QUANT_WEIGHTS_H_
