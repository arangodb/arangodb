////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstring>
#include <string>
#include <string_view>

#include "shared.hpp"
#include "utils/assert.hpp"

#include <absl/hash/hash.h>

namespace irs {

// MSVC++ > v14.0 (Visual Studio >2015) already implements this in <xstring>
// MacOS requires this definition to be before first usage (i.e. in bytes_view)
#if !defined(_MSC_VER) || (_MSC_VER <= 1900)
// We define this specialization because default implementation
// for unsigned char doesn't implement it effective
struct char_traits_byte {
  using char_type = byte_type;
  using int_type = int;
  using pos_type = std::streampos;
  using off_type = std::streamoff;
  using state_type = std::mbstate_t;

  static constexpr void assign(char_type& c1, const char_type& c2) noexcept {
    c1 = c2;
  }

  static constexpr bool eq(const char_type& c1, const char_type& c2) noexcept {
    return c1 == c2;
  }

  static constexpr bool lt(const char_type& c1, const char_type& c2) noexcept {
    return (static_cast<unsigned char>(c1) < static_cast<unsigned char>(c2));
  }

  static constexpr int compare(const char_type* s1, const char_type* s2,
                               size_t n) noexcept {
    if (n == 0) {
      return 0;
    }
    return std::memcmp(s1, s2, n);
  }

  static constexpr const char_type* find(const char_type* s, size_t n,
                                         const char_type& a) noexcept {
    if (n == 0) {
      return nullptr;
    }
    return static_cast<const char_type*>(std::memchr(s, a, n));
  }

  static constexpr char_type* move(char_type* s1, const char_type* s2,
                                   size_t n) noexcept {
    if (n == 0) {
      return s1;
    }
    return static_cast<char_type*>(std::memmove(s1, s2, n));
  }

  static constexpr char_type* copy(char_type* s1, const char_type* s2,
                                   size_t n) noexcept {
    if (n == 0) {
      return s1;
    }
    return static_cast<char_type*>(std::memcpy(s1, s2, n));
  }

  static constexpr char_type* assign(char_type* s, size_t n,
                                     char_type a) noexcept {
    if (n == 0) {
      return s;
    }
    return static_cast<char_type*>(std::memset(s, a, n));
  }

  static constexpr char_type to_char_type(const int_type& c) noexcept {
    return static_cast<char_type>(c);
  }

  // To keep both the byte 0xff and the eof symbol 0xffffffff
  // from ending up as 0xffffffff.
  static constexpr int_type to_int_type(const char_type& c) noexcept {
    return static_cast<int_type>(static_cast<unsigned char>(c));
  }

  static constexpr bool eq_int_type(const int_type& c1,
                                    const int_type& c2) noexcept {
    return c1 == c2;
  }

  static constexpr int_type eof() noexcept { return static_cast<int_type>(-1); }

  static constexpr int_type not_eof(const int_type& c) noexcept {
    return (c == eof()) ? 0 : c;
  }
};

template<typename Char>
using char_traits =
  std::conditional_t<std::is_same_v<Char, byte_type>, char_traits_byte,
                     std::char_traits<Char>>;

#else

using std::char_traits;

#endif

template<typename Char, typename Allocator = std::allocator<Char>>
using basic_string = std::basic_string<Char, char_traits<Char>, Allocator>;
template<typename Char>
using basic_string_view = std::basic_string_view<Char, char_traits<Char>>;

using bstring = basic_string<byte_type>;
using bytes_view = basic_string_view<byte_type>;

template<typename Char>
inline size_t CommonPrefixLength(const Char* lhs, size_t lhs_size,
                                 const Char* rhs, size_t rhs_size) noexcept {
  static_assert(1 == sizeof(Char), "1 != sizeof(Char)");

  const size_t* lhs_block = reinterpret_cast<const size_t*>(lhs);
  const size_t* rhs_block = reinterpret_cast<const size_t*>(rhs);

  size_t size = std::min(lhs_size, rhs_size);

  while (size >= sizeof(size_t) && *lhs_block == *rhs_block) {
    ++lhs_block;
    ++rhs_block;
    size -= sizeof(size_t);
  }

  const Char* lhs_block_start = reinterpret_cast<const Char*>(lhs_block);
  const Char* rhs_block_start = reinterpret_cast<const Char*>(rhs_block);

  while (size && *lhs_block_start == *rhs_block_start) {
    ++lhs_block_start;
    ++rhs_block_start;
    --size;
  }

  return lhs_block_start - lhs;
}

template<typename Char>
inline size_t CommonPrefixLength(basic_string_view<Char> lhs,
                                 basic_string_view<Char> rhs) noexcept {
  return CommonPrefixLength(lhs.data(), lhs.size(), rhs.data(), rhs.size());
}

template<typename Char>
inline constexpr Char kEmptyChar{};

template<typename Char>
constexpr basic_string_view<Char> kEmptyStringView{&kEmptyChar<Char>, 0};

template<typename Char>
constexpr bool IsNull(basic_string_view<Char> str) noexcept {
  return str.data() == nullptr;
}

template<typename ElemDst, typename ElemSrc>
constexpr inline basic_string_view<ElemDst> ViewCast(
  basic_string_view<ElemSrc> src) noexcept {
  static_assert(!std::is_same_v<ElemDst, ElemSrc>);
  static_assert(sizeof(ElemDst) == sizeof(ElemSrc));

  return {reinterpret_cast<const ElemDst*>(src.data()), src.size()};
}

namespace hash_utils {

inline size_t Hash(std::string_view value) noexcept {
  return absl::Hash<std::string_view>{}(value);
}
inline size_t Hash(bytes_view value) noexcept {
  static_assert(sizeof(byte_type) == sizeof(char));
  return Hash(ViewCast<char>(value));
}

template<typename T>
struct Hasher {
  size_t operator()(const T& value) const { return Hash(value); }
};

}  // namespace hash_utils
}  // namespace irs

template<>
struct std::hash<::irs::bstring> {
  size_t operator()(const ::irs::bstring& value) const noexcept {
    return ::irs::hash_utils::Hash(value);
  }
};

template<>
struct std::hash<::irs::bytes_view> {
  size_t operator()(::irs::bytes_view value) const noexcept {
    return ::irs::hash_utils::Hash(value);
  }
};
