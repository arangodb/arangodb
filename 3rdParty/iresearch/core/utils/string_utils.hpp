////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_STRING_UTILS_H
#define IRESEARCH_STRING_UTILS_H

#include "shared.hpp"

#if defined(_MSC_VER) && _MSC_VER < 1900 // before MSVC2015
  #define snprintf _snprintf
#endif

namespace iresearch {
namespace string_utils {

////////////////////////////////////////////////////////////////////////////////
/// @brief resize string to the full capacity of resize to the specified size
////////////////////////////////////////////////////////////////////////////////
template<typename T>
inline std::basic_string<T>& oversize(
  // 31 == 32 - 1: because std::basic_string reserves a \0 at the end
  // 32 is the original default value used in bytes_builder
  std::basic_string<T>& buf, size_t size = 31
) {
  buf.resize(size);
  buf.resize(buf.capacity()); // use up the entire buffer
  return buf;
}

////////////////////////////////////////////////////////////////////////////////
/// @param destination buffer
/// @return as per sprintf(...)
////////////////////////////////////////////////////////////////////////////////
template <typename... Args>
inline int to_string(std::string& buf, const char* format, Args&&... args) {
  auto result = snprintf(nullptr, 0, format, std::forward<Args>(args)...); // MSVC requires 'nullptr' buffer and '0' size to get expected size

  if (result <= 0) {
    return result;
  }

  auto start = buf.size();

  ++result; // +1 because snprintf(...) requires space for '\0'
  buf.resize(buf.size() + result);

  try {
    result = snprintf(&buf[start], result, format, std::forward<Args>(args)...);
    buf.resize(start + (std::max)(0, result));
  } catch (...) {
    buf.resize(start);

    throw;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @return formatted string
/// @note asserts on failure
////////////////////////////////////////////////////////////////////////////////
template <typename... Args>
inline std::string to_string(const char* format, Args&&... args) {
  std::string buf;
  const auto result = to_string(buf, format, std::forward<Args>(args)...);

  assert(result >= 0);
  assert(size_t(result) == buf.size());
  UNUSED(result);

  return buf;
}

} // string_utils
}

#endif
