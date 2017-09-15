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

#ifndef IRESEARCH_FILE_SYSTEM_DIRECTORY_H
#define IRESEARCH_FILE_SYSTEM_DIRECTORY_H

#include "directory.hpp"
#include "utils/string.hpp"
#include "utils/attributes.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class fs_directory
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API fs_directory : public directory {
 public:
  static bool create_directory(const string_ref& dir);

  static bool remove_directory(const string_ref& dir);

  explicit fs_directory(const std::string& dir);

  using directory::attributes;

  virtual attribute_store& attributes() NOEXCEPT override;

  virtual void close() NOEXCEPT override;

  virtual index_output::ptr create(const std::string& name) NOEXCEPT override;

  const std::string& directory() const NOEXCEPT;

  virtual bool exists(
    bool& result, const std::string& name
  ) const NOEXCEPT override;

  virtual bool length(
    uint64_t& result, const std::string& name
  ) const NOEXCEPT override;

  virtual index_lock::ptr make_lock(const std::string& name) NOEXCEPT override;

  virtual bool mtime(
    std::time_t& result, const std::string& name
  ) const NOEXCEPT override;

  virtual index_input::ptr open(
    const std::string& name
  ) const NOEXCEPT override;

  virtual bool remove(const std::string& name) NOEXCEPT override;

  virtual bool rename(
    const std::string& src, const std::string& dst
  ) NOEXCEPT override;

  virtual bool sync(const std::string& name) NOEXCEPT override;

  virtual bool visit(const visitor_f& visitor) const override;

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  attribute_store attributes_;
  std::string dir_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // fs_directory

NS_END

#endif
