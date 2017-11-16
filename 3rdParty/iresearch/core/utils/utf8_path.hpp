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

#if !defined(_MSC_VER)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  #pragma GCC diagnostic ignored "-Wunused-variable"
#endif

  #include <boost/filesystem/path.hpp>

#if !defined(_MSC_VER)
  #pragma GCC diagnostic pop
#endif

#include "string.hpp"

NS_ROOT

class utf8_path {
 public:
  utf8_path(bool current_working_path = false);
  utf8_path(const ::boost::filesystem::path& path);
  utf8_path& operator+=(const std::string& utf8_name);
  utf8_path& operator+=(const irs::string_ref& utf8_name);
  utf8_path& operator+=(const wchar_t* ucs2_name);
  utf8_path& operator+=(const irs::basic_string_ref<wchar_t>& ucs2_name);
  utf8_path& operator+=(const std::wstring& ucs2_name);
  utf8_path& operator/=(const std::string& utf8_name);
  utf8_path& operator/=(const irs::string_ref& utf8_name);
  utf8_path& operator/=(const wchar_t* ucs2_name);
  utf8_path& operator/=(const irs::basic_string_ref<wchar_t>& ucs2_name);
  utf8_path& operator/=(const std::wstring& ucs2_name);

  bool chdir() const NOEXCEPT;
  bool exists(bool& result) const NOEXCEPT;
  bool exists_file(bool& result) const NOEXCEPT;
  bool file_size(uint64_t& result) const NOEXCEPT;
  bool mkdir() const NOEXCEPT;
  bool mtime(std::time_t& result) const NOEXCEPT;
  bool remove() const NOEXCEPT;
  bool rename(const utf8_path& destination) const NOEXCEPT;
  bool rmdir() const;

  const ::boost::filesystem::path::string_type& native() const NOEXCEPT;
  const ::boost::filesystem::path::value_type* c_str() const NOEXCEPT;
  std::string utf8() const;

  void clear();

 private:
  ::boost::filesystem::path path_;
};

NS_END

#endif