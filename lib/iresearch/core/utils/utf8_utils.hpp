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

#pragma once

#include "shared.hpp"
#include "utils/assert.hpp"
#include "utils/string.hpp"

namespace irs::utf8_utils {

// We don't account 4 special cases:
// E0 A0..BF 80..BF
// ED 80..9F 80..BF
// F0 90..BF 80..BF 80..BF
// F4 80..8F 80..BF 80..BF

inline constexpr uint8_t kMaxCharSize = 4;

// TODO(MBkkt) Find out is alignas specified correct and really align this data
alignas(16 * 4) inline constexpr uint8_t kNextTable[16 * 4] = {
  // 1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
  0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // C
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // D
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // E
  3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // F
};

inline constexpr uint8_t kNextTableBegin = 0xC0;

// icu::UnicodeString::kInvalidUChar is private
inline constexpr uint32_t kInvalidChar32 = 0xFFFF;

IRS_FORCE_INLINE constexpr const byte_type* Next(
  const byte_type* it, const byte_type* end) noexcept {
  IRS_ASSERT(it);
  IRS_ASSERT(end);
  IRS_ASSERT(it < end);
  const uint8_t cp_first = *it++;
  if (IRS_LIKELY(cp_first < kNextTableBegin)) {
    return it;  // ascii(0***'****) or invalid(10**'****)
  }
  // multi byte or invalid
  it += kNextTable[cp_first - kNextTableBegin];
  return it <= end ? it : end;
}

inline size_t Length(bytes_view str) {
  auto* it = str.data();
  auto* end = str.data() + str.size();
  size_t length = 0;
  for (; it != end; it = Next(it, end)) {
    ++length;
  }
  return length;
}

IRS_FORCE_INLINE constexpr uint32_t LengthFromChar32(uint32_t cp) {
  if (cp < 0x80) {
    return 1;
  }
  if (cp < 0x800) {
    return 2;
  }
  if (cp < 0x10000) {
    return 3;
  }
  return 4;
}

template<bool Checked>
inline uint32_t ToChar32Impl(const byte_type*& it,
                             const byte_type* end) noexcept {
  IRS_ASSERT(it);
  IRS_ASSERT(!Checked || end);
  IRS_ASSERT(!Checked || it < end);
  uint32_t cp = *it++;
  if (IRS_LIKELY(cp < kNextTableBegin)) {
    // ascii(0***'****) or invalid(10**'****)
    return !Checked || cp < 0x80 ? cp : kInvalidChar32;
  }
  auto length = kNextTable[cp - kNextTableBegin];
  if constexpr (Checked) {
    if (IRS_UNLIKELY(length == 0 || it + length > end)) {
      return kInvalidChar32;
    }
  }
  auto next = [&] { return static_cast<uint32_t>(*it++) & 0x3F; };
  switch (length) {
    case 1:
      cp = (cp & 0x1F) << 6;
      break;
    case 2:
      cp = (cp & 0xF) << 12;
      cp |= next() << 6;
      break;
    case 3:
      cp = (cp & 0x7) << 18;
      cp |= next() << 12;
      cp |= next() << 6;
      break;
    default:
      ABSL_UNREACHABLE();
  }
  cp |= next();
  return cp;
}

IRS_FORCE_INLINE uint32_t ToChar32(const byte_type*& it,
                                   const byte_type* end) noexcept {
  return ToChar32Impl<true>(it, end);
}

IRS_FORCE_INLINE uint32_t ToChar32(const byte_type*& it) noexcept {
  return ToChar32Impl<false>(it, nullptr);
}

inline constexpr uint32_t FromChar32(uint32_t cp, byte_type* begin) noexcept {
  if (cp < 0x80) {
    begin[0] = static_cast<byte_type>(cp);
    return 1;
  }
  auto convert = [](uint32_t cp, uint32_t mask = 0x3F, uint32_t header = 0x80) {
    return static_cast<byte_type>((cp & mask) | header);
  };

  if (cp < 0x800) {
    begin[0] = convert(cp >> 6, 0x1F, 0xC0);
    begin[1] = convert(cp);
    return 2;
  }

  if (cp < 0x10000) {
    begin[0] = convert(cp >> 12, 0xF, 0xE0);
    begin[1] = convert(cp >> 6);
    begin[2] = convert(cp);
    return 3;
  }

  begin[0] = convert(cp >> 18, 0x7, 0xF0);
  begin[1] = convert(cp >> 12);
  begin[2] = convert(cp >> 6);
  begin[3] = convert(cp);
  return 4;
}

template<bool Checked, typename OutputIterator>
inline bool ToUTF32(bytes_view data, OutputIterator&& out) {
  const auto* begin = data.data();
  for (const auto* end = begin + data.size(); begin != end;) {
    const auto cp = ToChar32Impl<Checked>(begin, end);
    if constexpr (Checked) {
      if (cp == kInvalidChar32) {
        return false;
      }
    }
    *out = cp;
  }
  return true;
}

}  // namespace irs::utf8_utils
