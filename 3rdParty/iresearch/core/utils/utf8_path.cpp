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

#include "file_utils.hpp"
#include "locale_utils.hpp"
#include "log.hpp"
#include "error/error.hpp"
#include "utf8_path.hpp"

namespace {

typedef irs::basic_string_ref<wchar_t> wstring_ref;

/**
 * @param encoding the output encoding of the converter
 * @param forceUnicodeSystem force the internal system encoding to be unicode
 * @return the converter capable of outputing in the specified target encoding
 **/
template<typename Internal>
const std::codecvt<Internal, char, std::mbstate_t>& utf8_codecvt() {
  // always use UTF-8 locale for reading/writing filesystem paths
  // forceUnicodeSystem since using UCS2
  static const auto utf8 =
    irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true);

  return irs::locale_utils::codecvt<Internal>(utf8);
}

#if defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-function"
#endif

// use inline to avoid GCC warning
inline bool append_path(std::string& buf, const irs::string_ref& value) {
  buf.append(value.c_str(), value.size());

  return true;
}

// use inline to avoid GCC warning
inline bool append_path(std::string& buf, const wstring_ref& value) {
  static auto& fs_cvt = utf8_codecvt<wchar_t>();
  auto size = value.size() * 4; // same ratio as boost::filesystem
  auto start = buf.size();

  buf.resize(start + size); // allocate enough space for the converted value

  std::mbstate_t state = std::mbstate_t(); // RHS required for MSVC
  const wchar_t* from_next;
  char* to_next;
  auto result = fs_cvt.out(
    state,
    value.c_str(),
    value.c_str() + value.size(),
    from_next,
    const_cast<char*>(buf.c_str() + start),
    const_cast<char*>(buf.c_str() + buf.size()),
    to_next
  );

  if (std::codecvt_base::ok != result) {
    buf.resize(start); // resize to the original buffer size

    return false;
  }

  buf.resize(to_next - &buf[0]); // resize to the appended value end

  return true;
}

// use inline to avoid GCC warning
inline bool append_path(std::wstring& buf, const irs::string_ref& value) {
  static auto& fs_cvt = utf8_codecvt<wchar_t>();
  auto size = value.size() * 3; // same ratio as boost::filesystem
  auto start = buf.size();

  buf.resize(start + size); // allocate enough space for the converted value

  std::mbstate_t state = std::mbstate_t(); // RHS required for MSVC
  const char* from_next;
  wchar_t* to_next;
  auto result = fs_cvt.in(
    state,
    value.c_str(),
    value.c_str() + value.size(),
    from_next,
    const_cast<wchar_t*>(buf.c_str() + start),
    const_cast<wchar_t*>(buf.c_str() + buf.size()),
    to_next
  );

  if (std::codecvt_base::ok != result) {
    buf.resize(start); // resize to the original buffer size

    return false;
  }

  buf.resize(to_next - &buf[0]); // resize to the appended value end

  return true;
}

// use inline to avoid GCC warning
inline bool append_path(std::wstring& buf, const wstring_ref& value) {
  buf.append(value.c_str(), value.size());

  return true;
}

#if defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

}

namespace iresearch {

utf8_path::utf8_path(bool current_working_path /*= false*/) {
  if (current_working_path) {
    std::basic_string<native_char_t> buf;

    // emulate boost::filesystem behaviour by leaving path_ unset in case of error
    if (irs::file_utils::read_cwd(buf)) {
      *this += buf;
    }
  }
}

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

utf8_path::utf8_path(const wchar_t* ucs2_path)
  : utf8_path(irs::basic_string_ref<wchar_t>(ucs2_path)) {
}

utf8_path::utf8_path(const irs::basic_string_ref<wchar_t>& ucs2_path) {
  if (ucs2_path.null()) {
    std::basic_string<native_char_t> buf;

    // emulate boost::filesystem behaviour by leaving path_ unset in case of error
    if (irs::file_utils::read_cwd(buf)) {
      *this += buf;
    }

    return;
  }

  if (!append_path(path_, ucs2_path)) {
    // emulate boost::filesystem behaviour by throwing an exception
    throw io_error("path conversion failure");
  }
}

utf8_path::utf8_path(const std::wstring& ucs2_path) {
  if (!append_path(path_, wstring_ref(ucs2_path))) {
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

utf8_path& utf8_path::operator+=(const wchar_t* ucs2_name) {
  return (*this) += irs::basic_string_ref<wchar_t>(ucs2_name);
}

utf8_path& utf8_path::operator+=(const irs::basic_string_ref<wchar_t>& ucs2_name) {
  if (!append_path(path_, ucs2_name)) {
    // emulate boost::filesystem behaviour by throwing an exception
    throw io_error("path conversion failure");
  }

  return *this;
}

utf8_path& utf8_path::operator+=(const std::wstring &ucs2_name) {
  if (!append_path(path_, wstring_ref(ucs2_name))) {
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

utf8_path& utf8_path::operator/=(const wchar_t* ucs2_name) {
  return (*this) /= basic_string_ref<wchar_t>(ucs2_name);
}

utf8_path& utf8_path::operator/=(const basic_string_ref<wchar_t>& ucs2_name) {
  if (!path_.empty()) {
    path_.append(1, file_path_delimiter);
  }

  if (!append_path(path_, ucs2_name)) {
    // emulate boost::filesystem behaviour by throwing an exception
    throw io_error("path conversion failure");
  }

  return *this;
}

utf8_path& utf8_path::operator/=(const std::wstring &ucs2_name) {
  if (!path_.empty()) {
    path_.append(1, file_path_delimiter);
  }

  if (!append_path(path_, wstring_ref(ucs2_name))) {
    // emulate boost::filesystem behaviour by throwing an exception
    throw io_error("path conversion failure");
  }

return *this;
}

bool utf8_path::absolute(bool& result) const noexcept {
  return irs::file_utils::absolute(result, c_str());
}

bool utf8_path::chdir() const noexcept {
  return irs::file_utils::set_cwd(c_str());
}

bool utf8_path::exists(bool& result) const noexcept {
  return irs::file_utils::exists(result, c_str());
}

bool utf8_path::exists_directory(bool& result) const noexcept {
  return irs::file_utils::exists_directory(result, c_str());
}

bool utf8_path::exists_file(bool& result) const noexcept {
  return irs::file_utils::exists_file(result, c_str());
}

bool utf8_path::file_size(uint64_t& result) const noexcept {
  return irs::file_utils::byte_size(result, c_str());
}

bool utf8_path::mtime(time_t& result) const noexcept {
  return irs::file_utils::mtime(result, c_str());
}


bool utf8_path::mkdir(bool createNew /* = true*/) const noexcept {
  return irs::file_utils::mkdir(c_str(), createNew); 
}

bool utf8_path::remove() const noexcept {
  return irs::file_utils::remove(c_str());
}

bool utf8_path::rename(const utf8_path& destination) const noexcept {
  return irs::file_utils::move(c_str(), destination.c_str());
}

bool utf8_path::visit_directory(
    const directory_visitor& visitor,
    bool include_dot_dir /*= true*/
) {
  if (!irs::file_utils::visit_directory(c_str(), visitor, include_dot_dir)) {
    bool result;

    return !(!exists_directory(result) || !result); // check for FS error, treat non-directory as error
  }

  return true;
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

std::string utf8_path::utf8() const {
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

std::string utf8_path::utf8_absolute() const {
  bool abs;

  if (absolute(abs) && abs) {
    return utf8(); // already absolute (on failure assume relative path)
  }

  utf8_path path(true);

  path /= path_;

  return path.utf8();
}

}
