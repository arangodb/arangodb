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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <frozen/string.h>

#include "string.hpp"

#include <absl/hash/hash.h>

namespace irs {

IRS_FORCE_INLINE size_t hash_combine(size_t seed, size_t v) noexcept {
  return seed ^ (v + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

template<typename T>
IRS_FORCE_INLINE size_t
hash_combine(size_t seed, const T& v) noexcept(noexcept(std::hash<T>()(v))) {
  return hash_combine(seed, std::hash<T>()(v));
}

template<typename Elem>
class hashed_basic_string_view : public basic_string_view<Elem> {
 public:
  using base_t = basic_string_view<Elem>;

  template<typename Hasher = hash_utils::Hasher<base_t>>
  explicit hashed_basic_string_view(base_t ref, const Hasher& hasher = Hasher{})
    : hashed_basic_string_view{ref, hasher(ref)} {}

  hashed_basic_string_view(base_t ref, size_t hash) noexcept
    : base_t(ref), hash_(hash) {}

  size_t hash() const noexcept { return hash_; }

 private:
  size_t hash_;
};

template<typename T>
inline size_t hash(const T* begin, size_t size) noexcept {
  IRS_ASSERT(begin);

  size_t hash = 0;
  for (auto end = begin + size; begin != end;) {
    hash = hash_combine(hash, *begin++);
  }

  return hash;
}

using hashed_string_view = hashed_basic_string_view<char>;
using hashed_bytes_view = hashed_basic_string_view<byte_type>;

}  // namespace irs

namespace frozen {

template<>
struct elsa<std::string_view> {
  constexpr size_t operator()(std::string_view value) const noexcept {
    return elsa<frozen::string>{}({value.data(), value.size()});
  }
  constexpr std::size_t operator()(std::string_view value,
                                   std::size_t seed) const {
    return elsa<frozen::string>{}({value.data(), value.size()}, seed);
  }
};

}  // namespace frozen

namespace absl::hash_internal {

template<typename Char>
struct HashImpl<::irs::hashed_basic_string_view<Char>> {
  size_t operator()(const ::irs::hashed_basic_string_view<Char>& value) const {
    return value.hash();
  }
};

}  // namespace absl::hash_internal

template<typename Char>
struct std::hash<::irs::hashed_basic_string_view<Char>> {
  size_t operator()(
    const ::irs::hashed_basic_string_view<Char>& value) const noexcept {
    return value.hash();
  }
};
