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

#ifndef IRESEARCH_HASH_UTILS_H
#define IRESEARCH_HASH_UTILS_H

#include "shared.hpp"
#include "string.hpp"

// -----------------------------------------------------------------------------
// --SECTION--                                                        hash utils
// -----------------------------------------------------------------------------

namespace iresearch {

FORCE_INLINE size_t hash_combine(size_t seed, size_t v) noexcept {
  return seed ^ (v + 0x9e3779b9 + (seed<<6) + (seed>>2));
}

template<typename T>
FORCE_INLINE size_t hash_combine(size_t seed, T const& v) noexcept(noexcept(std::hash<T>()(v))) {
  return hash_combine(seed, std::hash<T>()(v));
}

template<typename Elem>
class IRESEARCH_API_TEMPLATE hashed_basic_string_ref : public basic_string_ref<Elem> {
 public:
  typedef basic_string_ref<Elem> base_t;

  hashed_basic_string_ref(size_t hash, const base_t& ref) noexcept
    : base_t(ref), hash_(hash) {
  }

  hashed_basic_string_ref(size_t hash, const base_t& ref, size_t size) noexcept
    : base_t(ref, size), hash_(hash) {
  }

  hashed_basic_string_ref(
      size_t hash,
      const typename base_t::char_type* ptr) noexcept
    : base_t(ptr),  hash_(hash) {
  }

  hashed_basic_string_ref(
      size_t hash,
      const typename base_t::char_type* ptr,
      size_t size) noexcept
    : base_t(ptr, size),  hash_(hash) {
  }

  hashed_basic_string_ref(
      size_t hash,
      const std::basic_string<typename base_t::char_type>& str) noexcept
    : base_t(str), hash_(hash) {
  }

  hashed_basic_string_ref(
      size_t hash,
      const std::basic_string<typename base_t::char_type>& str,
      size_t size) noexcept
    : base_t(str, size), hash_(hash) {
  }

  size_t hash() const noexcept { return hash_; }

 private:
  size_t hash_;
}; // hashed_basic_string_ref 

template<typename Elem, typename Hasher = std::hash<basic_string_ref<Elem>>>
hashed_basic_string_ref<Elem> make_hashed_ref(const basic_string_ref<Elem>& ref, const Hasher& hasher = Hasher()) {
  return hashed_basic_string_ref<Elem>(hasher(ref), ref);
}

template<typename Elem, typename Hasher = std::hash<basic_string_ref<Elem>>>
hashed_basic_string_ref<Elem> make_hashed_ref(const basic_string_ref<Elem>& ref, size_t size, const Hasher& hasher = Hasher()) {
  return hashed_basic_string_ref<Elem>(hasher(ref), ref, size);
}

template<typename T>
inline size_t hash(const T* begin, size_t size) noexcept {
  assert(begin);

  size_t hash = 0;
  for (auto end = begin + size; begin != end; ) {
    hash = hash_combine(hash, *begin++);
  }

  return hash;
}

typedef hashed_basic_string_ref<byte_type> hashed_bytes_ref;
typedef hashed_basic_string_ref<char> hashed_string_ref;

}

// -----------------------------------------------------------------------------
// --SECTION--                                                    std extensions
// -----------------------------------------------------------------------------

namespace std {

template<>
struct hash<::iresearch::hashed_bytes_ref> {
  size_t operator()(const ::iresearch::hashed_bytes_ref& value) const {
    return value.hash();
  }
};

template<>
struct hash<::iresearch::hashed_string_ref> {
  size_t operator()(const ::iresearch::hashed_string_ref& value) const {
    return value.hash();
  }
};

template<>
struct hash<std::vector<::iresearch::bstring>> {
  size_t operator()(const std::vector<::iresearch::bstring>& value) const noexcept {
    return ::iresearch::hash(value.data(), value.size());
  }
};

template<typename Char>
struct hash<std::vector<::iresearch::hashed_basic_string_ref<Char>>> {
  size_t operator()(const std::vector<::iresearch::hashed_basic_string_ref<Char>>& value) const noexcept {
    return ::iresearch::hash(value.data(), value.size());
  }
};

template<typename Char>
struct hash<std::vector<::iresearch::basic_string_ref<Char>>> {
  size_t operator()(const std::vector<::iresearch::basic_string_ref<Char>>& value) const noexcept {
    return ::iresearch::hash(value.data(), value.size());
  }
};

} // std

#endif
