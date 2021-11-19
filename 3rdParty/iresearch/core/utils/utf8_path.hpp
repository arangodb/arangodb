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

#ifndef IRESEARCH_UTF8_PATH_H
#define IRESEARCH_UTF8_PATH_H

#ifndef __APPLE__
#include <filesystem>
namespace iresearch {
using utf8_path = std::filesystem::path;
utf8_path current_path();
}
#else

#include "string.hpp"

#include <functional>

namespace iresearch {

class IRESEARCH_API utf8_path {
 public:
  #ifdef _WIN32
    typedef wchar_t native_char_t;
  #else
    typedef char native_char_t;
  #endif

  typedef std::function<bool(const native_char_t* name)> directory_visitor;
  typedef std::basic_string<native_char_t> native_str_t;
  typedef std::basic_string<native_char_t> string_type; // to simplify move to std::filesystem::path
  typedef native_char_t value_type; // to simplify move to std::filesystem::path

  utf8_path() = default;
  utf8_path(const char* utf8_path); // cppcheck-suppress noExplicitConstructor
  utf8_path(const std::string& utf8_path); // cppcheck-suppress noExplicitConstructor
  utf8_path(const irs::string_ref& utf8_path); // cppcheck-suppress noExplicitConstructor
  utf8_path& operator+=(const char* utf8_name);
  utf8_path& operator+=(const std::string& utf8_name);
  utf8_path& operator+=(const irs::string_ref& utf8_name);
  utf8_path& operator/=(const char* utf8_name);
  utf8_path& operator/=(const std::string& utf8_name);
  utf8_path& operator/=(const irs::string_ref& utf8_name);

  bool is_absolute(bool& result) const noexcept;

  // std::filesystem::path compliant version
  bool is_absolute() const noexcept {
    bool result;
    return is_absolute(result) && result;
  }

  const native_char_t* c_str() const noexcept;
  const native_str_t& native() const noexcept;
  std::string u8string() const;
  std::string string() const { return u8string(); }

  template<typename T>
  utf8_path& assign(T&& source) {
    path_.clear();
    (*this) += source;
    return *this;
  }

  void clear();

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  native_str_t path_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

// need this operator to be closer to std::filesystem::path
utf8_path operator/( const utf8_path& lhs, const utf8_path& rhs );

utf8_path current_path();

}
#endif
#endif
