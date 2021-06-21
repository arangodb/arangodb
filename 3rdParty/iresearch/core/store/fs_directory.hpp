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

#ifndef IRESEARCH_FILE_SYSTEM_DIRECTORY_H
#define IRESEARCH_FILE_SYSTEM_DIRECTORY_H

#include "directory.hpp"
#include "utils/string.hpp"
#include "utils/attribute_store.hpp"

namespace iresearch {

//////////////////////////////////////////////////////////////////////////////
/// @class fs_directory
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API fs_directory : public directory {
 public:
  explicit fs_directory(const std::string& dir);

  using directory::attributes;

  virtual attribute_store& attributes() noexcept override;

  virtual index_output::ptr create(const std::string& name) noexcept override;

  const std::string& directory() const noexcept;

  virtual bool exists(
    bool& result, const std::string& name
  ) const noexcept override;

  virtual bool length(
    uint64_t& result, const std::string& name
  ) const noexcept override;

  virtual index_lock::ptr make_lock(const std::string& name) noexcept override;

  virtual bool mtime(
    std::time_t& result, const std::string& name
  ) const noexcept override;

  virtual index_input::ptr open(
    const std::string& name,
    IOAdvice advice
  ) const noexcept override;

  virtual bool remove(const std::string& name) noexcept override;

  virtual bool rename(
    const std::string& src, const std::string& dst
  ) noexcept override;

  virtual bool sync(const std::string& name) noexcept override;

  virtual bool visit(const visitor_f& visitor) const override;

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  attribute_store attributes_;
  std::string dir_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // fs_directory

}

#endif
