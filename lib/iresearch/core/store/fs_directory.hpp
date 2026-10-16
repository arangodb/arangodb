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

#pragma once

#include <filesystem>

#include "store/caching_directory.hpp"
#include "store/directory.hpp"
#include "store/directory_attributes.hpp"

namespace irs {

class FSDirectory : public directory {
 public:
  static constexpr size_t kDefaultPoolSize = 8;

  explicit FSDirectory(
    std::filesystem::path dir,
    directory_attributes attrs = directory_attributes{},
    const ResourceManagementOptions& rm = ResourceManagementOptions::kDefault,
    size_t fd_pool_size = kDefaultPoolSize);

  const std::filesystem::path& directory() const noexcept;

  using directory::attributes;
  directory_attributes& attributes() noexcept final { return attrs_; }

  index_output::ptr create(std::string_view name) noexcept override;

  bool exists(bool& result, std::string_view name) const noexcept override;

  bool length(uint64_t& result, std::string_view name) const noexcept override;

  index_lock::ptr make_lock(std::string_view name) noexcept final;

  bool mtime(std::time_t& result, std::string_view name) const noexcept final;

  index_input::ptr open(std::string_view name,
                        IOAdvice advice) const noexcept override;

  bool remove(std::string_view name) noexcept override;

  bool rename(std::string_view src, std::string_view dst) noexcept override;

  bool sync(std::span<const std::string_view> files) noexcept override;

  bool visit(const visitor_f& visitor) const final;

 protected:
  // TODO(MBkkt) store ResourceManagementOptions only in directory
  // Don't pass them to IndexWriter
  ResourceManagementOptions resource_manager_;

 private:
  bool sync(std::string_view name) noexcept;

  directory_attributes attrs_;
  std::filesystem::path dir_;
  size_t fd_pool_size_;
};

class CachingFSDirectory : public CachingDirectoryBase<FSDirectory, uint64_t> {
 public:
  using CachingDirectoryBase::CachingDirectoryBase;

  bool length(uint64_t& result, std::string_view name) const noexcept final;

  index_input::ptr open(std::string_view name,
                        IOAdvice advice) const noexcept final;
};

}  // namespace irs
