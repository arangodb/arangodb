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
#include "utf8_path.hpp"

#ifdef __APPLE__

#include "file_utils.hpp"
#include "locale_utils.hpp"
#include "log.hpp"
#include "error/error.hpp"

namespace {

#if defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-function"
#endif

// use inline to avoid GCC warning
inline bool append_path(std::string& buf, const irs::string_ref& value) {
  buf.append(value.c_str(), value.size());

  return true;
}

#if defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

} // namespace

namespace iresearch {

utf8_path::utf8_path(const char* utf8_path)
  : irs::utf8_path(irs::string_ref(utf8_path)) {
}

utf8_path::utf8_path(const std::string& utf8_path) {
  if (!append_path(path_, string_ref(utf8_path))) {
    // emulate boost::filesystem behaviour by throwing an exception
    throw io_error("path conversion failure");
  }
}

utf8_path::utf8_path(const irs::string_ref& utf8_path) {
  if (utf8_path.null()) {
    std::basic_string<native_char_t> buf;

    // emulate boost::filesystem behaviour by leaving path_ unset in case of error
    if (irs::file_utils::read_cwd(buf)) {
      *this += buf;
    }

    return;
  }

  if (!append_path(path_, utf8_path)) {
    // emulate boost::filesystem behaviour by throwing an exception
    throw io_error("path conversion failure");
  }
}

utf8_path& utf8_path::operator+=(const char* utf8_name) {
  return (*this) += irs::string_ref(utf8_name);
}

utf8_path& utf8_path::operator+=(const std::string &utf8_name) {
  if (!append_path(path_, string_ref(utf8_name))) {
    // emulate boost::filesystem behaviour by throwing an exception
    throw io_error("path conversion failure");
  }

  return *this;
}

utf8_path& utf8_path::operator+=(const string_ref& utf8_name) {
  if (!append_path(path_, utf8_name)) {
    // emulate boost::filesystem behaviour by throwing an exception
    throw io_error("path conversion failure");
  }

  return *this;
}

utf8_path& utf8_path::operator/=(const char* utf8_name) {
  return (*this) /= irs::string_ref(utf8_name);
}

utf8_path& utf8_path::operator/=(const std::string &utf8_name) {
  if (!path_.empty()) {
    path_.append(1, file_path_delimiter);
  }

  if (!append_path(path_, string_ref(utf8_name))) {
    // emulate boost::filesystem behaviour by throwing an exception
    throw io_error("path conversion failure");
  }

  return *this;
}

utf8_path& utf8_path::operator/=(const string_ref& utf8_name) {
  if (!path_.empty()) {
    path_.append(1, file_path_delimiter);
  }

  if (!append_path(path_, utf8_name)) {
    // emulate boost::filesystem behaviour by throwing an exception
    throw io_error("path conversion failure");
  }

  return *this;
}

bool utf8_path::is_absolute(bool& result) const noexcept {
  return irs::file_utils::absolute(result, c_str());
}

void utf8_path::clear() {
  path_.clear();
}

const utf8_path::native_char_t* utf8_path::c_str() const noexcept {
  return path_.c_str();
}

const utf8_path::native_str_t& utf8_path::native() const noexcept {
  return path_;
}

std::string utf8_path::u8string() const {
  #ifdef _WIN32
    std::string buf;

    if (!append_path(buf, path_)) {
      // emulate boost::filesystem behaviour by throwing an exception
      throw io_error("Path conversion failure");
    }

    return buf;
  #else
    return path_;
  #endif
}

utf8_path operator/(const utf8_path& lhs, const utf8_path& rhs) {
  auto left = lhs;
  left /= rhs.c_str();
  return left;
}

utf8_path current_path() {
  std::basic_string<utf8_path::native_char_t> buf;
  utf8_path cwd;
  // emulate boost::filesystem behaviour by leaving path_ unset in case of error
  if (irs::file_utils::read_cwd(buf)) {
    cwd += buf;
  }
  return cwd;
}

} // namespace iresearch
#else
namespace iresearch {
utf8_path current_path() {
  return std::filesystem::current_path();
}
} // namespace iresearch
#endif
