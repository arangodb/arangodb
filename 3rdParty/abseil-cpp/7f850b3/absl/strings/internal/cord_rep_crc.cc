// Copyright 2021 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/strings/internal/cord_rep_crc.h"

#include <cassert>
#include <cstdint>

#include "absl/base/config.h"
#include "absl/strings/internal/cord_internal.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace cord_internal {

CordRepCrc* CordRepCrc::New(CordRep* child, uint32_t crc) {
  assert(child != nullptr);
  if (child->IsCrc()) {
    if (child->refcount.IsOne()) {
      child->crc()->crc = crc;
      return child->crc();
    }
    CordRep* old = child;
    child = old->crc()->child;
    CordRep::Ref(child);
    CordRep::Unref(old);
  }
  auto* new_cordrep = new CordRepCrc;
  new_cordrep->length = child->length;
  new_cordrep->tag = cord_internal::CRC;
  new_cordrep->child = child;
  new_cordrep->crc = crc;
  return new_cordrep;
}

void CordRepCrc::Destroy(CordRepCrc* node) {
  CordRep::Unref(node->child);
  delete node;
}

}  // namespace cord_internal
ABSL_NAMESPACE_END
}  // namespace absl
