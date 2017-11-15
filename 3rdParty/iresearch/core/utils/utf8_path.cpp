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

#ifdef _WIN32
  #include <WinError.h>
#endif

#if defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  #pragma GCC diagnostic ignored "-Wunused-variable"
#endif

  #include <boost/filesystem/operations.hpp>

#if defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include <boost/locale/encoding.hpp>

#include "locale_utils.hpp"
#include "log.hpp"
#include "utf8_path.hpp"

NS_LOCAL

// in some situations codecvt returned from boost::filesystem::path seems to get corrupted
// always use UTF-8 locale for reading/writing filesystem paths
const boost::filesystem::path::codecvt_type& fs_codecvt() {
  static auto default_locale = iresearch::locale_utils::locale("", true);

  return std::use_facet<boost::filesystem::path::codecvt_type>(default_locale);
}

// check if the error code matches the system specific not-found errors
// error code list taken from Boost
bool fs_error_not_found(int err) {
  #ifdef _WIN32
    return err == ERROR_FILE_NOT_FOUND
      || err == ERROR_PATH_NOT_FOUND
      || err == ERROR_INVALID_NAME      // "tools/jam/src/:sys:stat.h", "//foo"
      || err == ERROR_INVALID_DRIVE     // USB card reader with no card inserted
      || err == ERROR_NOT_READY         // CD/DVD drive with no disc inserted
      || err == ERROR_INVALID_PARAMETER // ":sys:stat.h"
      || err == ERROR_BAD_PATHNAME      // "//nosuch" on Win64
      || err == ERROR_BAD_NETPATH       // "//nosuch" on Win32
      ;
  #else // POSIX
    return err == ENOENT || err == ENOTDIR;
  #endif
}

static auto& fs_cvt = fs_codecvt();

NS_END

NS_ROOT

utf8_path::utf8_path(bool current_working_path /*= false*/) {
  if (current_working_path) {
    path_ = ::boost::filesystem::current_path();
  }
}

utf8_path::utf8_path(const ::boost::filesystem::path& path): path_(path) {
}


utf8_path& utf8_path::operator+=(const string_ref& utf8_name) {
  #ifdef _WIN32
    auto native_name = boost::locale::conv::utf_to_utf<wchar_t>(
      utf8_name.c_str(), utf8_name.c_str() + utf8_name.size()
    );

    path_.concat(std::move(native_name), fs_cvt);
  #else
    path_.concat(utf8_name.c_str(), utf8_name.c_str() + utf8_name.size(), fs_cvt);
  #endif

  return *this;
}

utf8_path& utf8_path::operator+=(const std::string &utf8_name) {
  #ifdef _WIN32
    auto native_name = boost::locale::conv::utf_to_utf<wchar_t>(
      utf8_name.c_str(), utf8_name.c_str() + utf8_name.size()
    );

    path_.concat(std::move(native_name), fs_cvt);
  #else
    path_.concat(utf8_name.c_str(), utf8_name.c_str() + utf8_name.size(), fs_cvt);
  #endif

  return *this;
}

utf8_path& utf8_path::operator+=(const std::wstring &ucs2_name) {
  #ifdef _WIN32
    path_.append(ucs2_name, fs_cvt);
  #else
    auto native_name = boost::locale::conv::utf_to_utf<char>(
      ucs2_name.c_str(), ucs2_name.c_str() + ucs2_name.size()
    );

    path_.concat(std::move(native_name), fs_cvt);
  #endif

  return *this;
}

utf8_path& utf8_path::operator+=(const wchar_t* ucs2_name) {
  return (*this) += irs::basic_string_ref<wchar_t>(ucs2_name);
}

utf8_path& utf8_path::operator+=(const irs::basic_string_ref<wchar_t>& ucs2_name) {
  #ifdef _WIN32
    path_.concat(ucs2_name.c_str(),ucs2_name.c_str() + ucs2_name.size() , fs_cvt);
  #else
    auto native_name = boost::locale::conv::utf_to_utf<char>(
      ucs2_name.c_str(), ucs2_name.c_str() + ucs2_name.size()
    );

    path_.concat(std::move(native_name), fs_cvt);
  #endif

  return *this;
}

utf8_path& utf8_path::operator/=(const string_ref& utf8_name) {
  #ifdef _WIN32
    auto native_name = boost::locale::conv::utf_to_utf<wchar_t>(
      utf8_name.c_str(), utf8_name.c_str() + utf8_name.size()
    );

    path_.append(std::move(native_name), fs_cvt);
  #else
    path_.append(utf8_name.c_str(), utf8_name.c_str() + utf8_name.size(), fs_cvt);
  #endif

  return *this;
}

utf8_path& utf8_path::operator/=(const std::string &utf8_name) {
  #ifdef _WIN32
    auto native_name = boost::locale::conv::utf_to_utf<wchar_t>(
      utf8_name.c_str(), utf8_name.c_str() + utf8_name.size()
    );

    path_.append(std::move(native_name), fs_cvt);
  #else
    path_.append(utf8_name.c_str(), utf8_name.c_str() + utf8_name.size(), fs_cvt);
  #endif

  return *this;
}

utf8_path& utf8_path::operator/=(const std::wstring &ucs2_name) {
  #ifdef _WIN32
    path_.append(ucs2_name, fs_cvt);
  #else
    auto native_name = boost::locale::conv::utf_to_utf<char>(
      ucs2_name.c_str(), ucs2_name.c_str() + ucs2_name.size()
    );

    path_.append(std::move(native_name), fs_cvt);
  #endif

  return *this;
}

utf8_path& utf8_path::operator/=(const wchar_t* ucs2_name) {
  return (*this) /= iresearch::basic_string_ref<wchar_t>(ucs2_name);
}

utf8_path& utf8_path::operator/=(const iresearch::basic_string_ref<wchar_t>& ucs2_name) {
  #ifdef _WIN32
    path_.append(ucs2_name.c_str(),ucs2_name.c_str() + ucs2_name.size() , fs_cvt);
  #else
    auto native_name = boost::locale::conv::utf_to_utf<char>(
      ucs2_name.c_str(), ucs2_name.c_str() + ucs2_name.size()
    );

    path_.append(std::move(native_name), fs_cvt);
  #endif

  return *this;
}

bool utf8_path::chdir() const NOEXCEPT {
  boost::system::error_code code;

  boost::filesystem::current_path(path_, code);

  return boost::system::errc::success == code.value();
}

bool utf8_path::exists(bool& result) const NOEXCEPT {
  boost::system::error_code code;

  result = boost::filesystem::exists(path_, code);

  return boost::system::errc::success == code.value()
    || fs_error_not_found(code.value());
}

bool utf8_path::exists_file(bool& result) const NOEXCEPT {
  boost::system::error_code code;

  result = boost::filesystem::is_regular_file(path_, code);

  return boost::system::errc::success == code.value()
    || fs_error_not_found(code.value());
}

bool utf8_path::file_size(uint64_t& result) const NOEXCEPT {
  boost::system::error_code code;

  try {
    result = boost::filesystem::file_size(path_, code);

    return boost::system::errc::success == code.value();
  } catch(...) { // boost::filesystem::file_size(...) is not noexcept
    IR_FRMT_ERROR("Caught exception at: %s code: %d for path: " IR_FILEPATH_SPECIFIER, __FUNCTION__, code.value(), path_.c_str());
  }

  return false;
}

bool utf8_path::mtime(std::time_t& result) const NOEXCEPT {
  boost::system::error_code code;

  try {
    result = boost::filesystem::last_write_time(path_, code);

    return boost::system::errc::success == code.value();
  } catch(...) { // boost::filesystem::last_write_time(...) is not noexcept
    IR_FRMT_ERROR("Caught exception at: %s code: %d for path: " IR_FILEPATH_SPECIFIER, __FUNCTION__, code.value(), path_.c_str());
  }

  return false;
}

bool utf8_path::mkdir() const NOEXCEPT {
  boost::system::error_code code;

  try {
    return !path_.empty() // at least Boost v1.62 cannot handle empty paths SIGSEGV
           && boost::filesystem::create_directories(path_, code)
           && boost::system::errc::success == code.value();
  } catch(...) { // boost::filesystem::create_directories(...) is not noexcept
    IR_FRMT_ERROR("Caught exception at: %s code: %d for path: " IR_FILEPATH_SPECIFIER, __FUNCTION__, code.value(), path_.c_str());
  }

  return false;
}

bool utf8_path::remove() const NOEXCEPT {
  boost::system::error_code code;

  try {
    return boost::filesystem::remove_all(path_, code) > 0
           && boost::system::errc::success == code.value();
  } catch(...) { // boost::filesystem::remove_all(...) is not noexcept
    IR_FRMT_ERROR("Caught exception at: %s code: %d for path: " IR_FILEPATH_SPECIFIER, __FUNCTION__, code.value(), path_.c_str());
  }

  return false;
}

bool utf8_path::rename(const utf8_path& destination) const NOEXCEPT {
  boost::system::error_code code;

  try {
    boost::filesystem::rename(path_, destination.path_, code);

    return true;
  } catch (...) {
    IR_FRMT_ERROR("Caught exception at: %s code: %d for path: " IR_FILEPATH_SPECIFIER, __FUNCTION__,code.value(), path_.c_str());
  }

  return false;
}

bool utf8_path::rmdir() const {
  boost::system::error_code code;

  boost::filesystem::remove_all(path_, code);

  return boost::system::errc::success == code.value();
}

void utf8_path::clear() {
  path_.clear();
}

const boost::filesystem::path::value_type* utf8_path::c_str() const NOEXCEPT {
  return path_.c_str();
}

const boost::filesystem::path::string_type& utf8_path::native() const NOEXCEPT {
  return path_.native();
}

std::string utf8_path::utf8() const {
  return path_.string(fs_cvt);
}

NS_END