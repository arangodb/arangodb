//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_UTF8_PATH_H
#define IRESEARCH_UTF8_PATH_H

#include "boost/filesystem/path.hpp"
#include "error/error.hpp"

NS_ROOT

class utf8_path: private ::boost::filesystem::path {
 public:
  utf8_path();
  utf8_path(const ::boost::filesystem::path& path);
  utf8_path& operator/(const std::string& utf8_name);
  utf8_path& operator/(const iresearch::string_ref& utf8_name);
  utf8_path& operator/(const wchar_t* ucs2_name);
  utf8_path& operator/(const iresearch::basic_string_ref<wchar_t>& ucs2_name);
  utf8_path& operator/(const std::wstring& ucs2_name);
  bool exists(bool& result) const NOEXCEPT;
  bool exists_file(bool& result) const NOEXCEPT;
  bool file_mtime(std::time_t& result) const NOEXCEPT;
  bool file_size(uint64_t& result) const NOEXCEPT;
  const ::boost::filesystem::path::string_type& native() const NOEXCEPT;
  const ::boost::filesystem::path::value_type* c_str() const NOEXCEPT;
  bool mkdir() const NOEXCEPT;
  bool remove() const NOEXCEPT;
  bool rename(const utf8_path& destination) const NOEXCEPT;
  bool rmdir() const;
  std::string utf8() const;
  void clear();

 private:
  ::boost::filesystem::path path_;
};

NS_END

#endif
