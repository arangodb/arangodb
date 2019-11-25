////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_UTF8_UTILS_H
#define IRESEARCH_UTF8_UTILS_H

#include "shared.hpp"
#include "log.hpp"

NS_ROOT
NS_BEGIN(utf8_utils)

FORCE_INLINE  const byte_type* next(const byte_type* it, const byte_type* end) noexcept {
  IRS_ASSERT(it);
  IRS_ASSERT(end);
  if (it < end) {
    const uint8_t symbol_start = *it;
    if (symbol_start < 0x80) {
      ++it;
    } else if ((symbol_start >> 5) == 0x06) {
      it += 2;
    } else if ((symbol_start >> 4) == 0x0E) {
      it += 3;
    } else if ((symbol_start >> 3) == 0x1E) {
      it += 4;
    } else {
      IR_FRMT_ERROR("Invalid UTF-8 symbol increment");
      it = end;
    }
  }
  return it > end ? end : it;
}

NS_END
NS_END

#endif
