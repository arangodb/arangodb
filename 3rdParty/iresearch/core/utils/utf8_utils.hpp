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

#include <vector>

#include "shared.hpp"
#include "log.hpp"
#include "string.hpp"

NS_ROOT
NS_BEGIN(utf8_utils)

FORCE_INLINE const byte_type* next(const byte_type* it, const byte_type* end) noexcept {
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

FORCE_INLINE uint32_t next(const byte_type*& it) noexcept {
  IRS_ASSERT(it);

  uint32_t cp = *it;

  if ((cp >> 5) == 0x06) {
    cp = ((cp << 6) & 0x7FF) + (uint32_t(*++it) & 0x3F);
  } else if ((cp >> 4) == 0x0E) {
    cp = ((cp << 12) & 0xFFFF) + ((uint32_t(*++it) << 6) & 0xFFF);
    cp += (*++it) & 0x3F;
  } else if ((cp >> 3) == 0x1E) {
    cp = ((cp << 18) & 0x1FFFFF) + ((uint32_t(*++it) << 12) & 0x3FFFF);
    cp += (uint32_t(*++it) << 6) & 0xFFF;
    cp += uint32_t(*++it) & 0x3F;
  }

  ++it;

  return cp;
}

template<typename OutputIterator>
inline void to_utf8(const byte_type* begin, size_t size, OutputIterator out) {
  for (auto end = begin + size; begin < end; ++out) {
    *out = next(begin);
  }
}

template<typename OutputIterator>
inline void to_utf8(const bytes_ref& in, OutputIterator out) {
  to_utf8(in.begin(), in.size(), out);
}

NS_END
NS_END

#endif
