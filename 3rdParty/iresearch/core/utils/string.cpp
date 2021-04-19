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

#include <chrono>

#include <absl/hash/internal/city.h>

#include "string.hpp"

// -----------------------------------------------------------------------------
// --SECTION--                                                     hash function
// -----------------------------------------------------------------------------

namespace {
using namespace irs;
template<typename T>
size_t get_hash(const T* value, size_t size) noexcept {
  return absl::hash_internal::CityHash64(reinterpret_cast<const char*>(value), size);
}

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                   basic_string_ref implementation
// -----------------------------------------------------------------------------

#if defined(_MSC_VER) && defined(IRESEARCH_DLL)

template class IRESEARCH_API basic_string_ref<char>;
template class IRESEARCH_API basic_string_ref<byte_type>;

#endif

namespace hash_utils {

size_t hash(const std::string& value) noexcept {
  return get_hash(value.c_str(), value.size());
}

size_t hash(const bstring& value) noexcept {
  return get_hash(value.c_str(), value.size());
}

size_t hash(const char* value) noexcept {
  return get_hash(value, std::char_traits<char>::length(value) * sizeof(char));
}

size_t hash(const wchar_t* value) noexcept {
  return get_hash(value, std::char_traits<wchar_t>::length(value) * sizeof(wchar_t));
}

size_t hash(const bytes_ref& value) noexcept {
  return get_hash(value.c_str(), value.size());
}

size_t hash(const string_ref& value) noexcept {
  return get_hash(value.c_str(), value.size());
}

} // hash_utils
}
