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

namespace iresearch {
namespace utf8_utils {

// max number of bytes to represent single UTF8 code point
constexpr size_t MAX_CODE_POINT_SIZE = 4;
constexpr uint32_t MIN_CODE_POINT = 0;
constexpr uint32_t MIN_2BYTES_CODE_POINT = 0x80;
constexpr uint32_t MIN_3BYTES_CODE_POINT = 0x800;
constexpr uint32_t MIN_4BYTES_CODE_POINT = 0x10000;
constexpr uint32_t MAX_CODE_POINT = 0x10FFFF;
constexpr uint32_t INVALID_CODE_POINT = integer_traits<uint32_t>::const_max;

FORCE_INLINE const byte_type* next(const byte_type* begin, const byte_type* end) noexcept {
  IRS_ASSERT(begin);
  IRS_ASSERT(end);

  if (begin < end) {
    const uint32_t cp_start = *begin++;
    if ((cp_start >> 5) == 0x06) {
      ++begin;
    } else if ((cp_start >> 4) == 0x0E) {
      begin += 2;
    } else if ((cp_start >> 3) == 0x1E) {
      begin += 3;
    } else if (cp_start >= 0x80) {
      begin = end;
    }
  }

  return begin > end ? end : begin;
}

FORCE_INLINE size_t cp_length(const uint32_t cp_start) noexcept {
  if (cp_start < 0x80) {
    return 1;
  } else if ((cp_start >> 5) == 0x06) {
    return 2;
  } else if ((cp_start >> 4) == 0x0E) {
    return 3;
  } else if ((cp_start >> 3) == 0x1E) {
    return 4;
  }

  return 0;
}

inline uint32_t next_checked(const byte_type*& begin, const byte_type* end) noexcept {
  IRS_ASSERT(begin);
  IRS_ASSERT(end);

  if (begin >= end) {
    return INVALID_CODE_POINT;
  }

  uint32_t cp = *begin;
  const size_t size = cp_length(cp);

  begin += size;

  if (begin <= end)  {
    switch (size) {
     case 1: return cp;

     case 2: return ((cp << 6) & 0x7FF) +
                    (uint32_t(begin[-1]) & 0x3F);

     case 3: return ((cp << 12) & 0xFFFF) +
                    ((uint32_t(begin[-2]) << 6) & 0xFFF) +
                    (uint32_t(begin[-1]) & 0x3F);

     case 4: return ((cp << 18) & 0x1FFFFF) +
                    ((uint32_t(begin[-3]) << 12) & 0x3FFFF) +
                    ((uint32_t(begin[-2]) << 6) & 0xFFF) +
                    (uint32_t(begin[-1]) & 0x3F);
    }
  }

  begin = end;
  return INVALID_CODE_POINT;
}

inline uint32_t next(const byte_type*& it) noexcept {
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

FORCE_INLINE constexpr uint32_t utf32_to_utf8(uint32_t cp, byte_type* begin) noexcept {
  if (cp < 0x80) {
    begin[0] = static_cast<byte_type>(cp);
    return 1;
  }

  if (cp < 0x800) {
    begin[0] = static_cast<byte_type>((cp >> 6) | 0xC0);
    begin[1] = static_cast<byte_type>((cp & 0x3F) | 0x80);
    return 2;
  }

  if (cp < 0x10000) {
    begin[0] = static_cast<byte_type>((cp >> 12) | 0xE0);
    begin[1] = static_cast<byte_type>(((cp >> 6) & 0x3F) | 0x80);
    begin[2] = static_cast<byte_type>((cp & 0x3F) | 0x80);
    return 3;
  }

  begin[0] = static_cast<byte_type>((cp >> 18) | 0xF0);
  begin[1] = static_cast<byte_type>(((cp >> 12) & 0x3F) | 0x80);
  begin[2] = static_cast<byte_type>(((cp >> 6) & 0x3F) | 0x80);
  begin[3] = static_cast<byte_type>((cp & 0x3F) | 0x80);
  return 4;
}

template<bool Checked = true>
inline const byte_type* find(const byte_type* begin, const byte_type* end, uint32_t ch) noexcept {
  for (const byte_type* char_begin = begin; begin < end; char_begin = begin) {
    const auto cp = Checked ? next_checked(begin, end) : next(begin);

    if (cp == ch) {
      return char_begin;
    }
  }

  return end;
}

template<bool Checked = true>
inline size_t find(const byte_type* begin, const size_t size, uint32_t ch) noexcept {
  size_t pos = 0;
  for (auto end = begin + size; begin < end; ++pos) {
    const auto cp = Checked ? next_checked(begin, end) : next(begin);

    if (cp == ch) {
      return pos;
    }
  }

  return bstring::npos;
}

template<bool Checked, typename OutputIterator>
inline bool utf8_to_utf32(const byte_type* begin, size_t size, OutputIterator out) {
  for (auto end = begin + size; begin < end; ) {
    const auto cp = Checked ? next_checked(begin, end) : next(begin);

    if constexpr (Checked) {
      if (cp == INVALID_CODE_POINT) {
        return false;
      }
    }

    *out = cp;
  }

  return true;
}

template<bool Checked = true, typename OutputIterator>
FORCE_INLINE bool utf8_to_utf32(const bytes_ref& in, OutputIterator out) {
  return utf8_to_utf32<Checked>(in.begin(), in.size(), out);
}

inline size_t utf8_length(const byte_type* begin, size_t size) noexcept {
  size_t length = 0;

  for (auto end = begin + size; begin < end; begin = next(begin, end)) {
    ++length;
  }

  return length;
}

FORCE_INLINE size_t utf8_length(const bytes_ref& in) noexcept {
  return utf8_length(in.c_str(), in.size());
}

} // utf8_utils
} // ROOT

#endif // IRESEARCH_UTF8_UTILS_H
