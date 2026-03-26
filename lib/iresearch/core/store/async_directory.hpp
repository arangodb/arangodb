////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

#include "store/mmap_directory.hpp"
#include "utils/object_pool.hpp"

namespace irs {

class AsyncFile;

struct AsyncFileDeleter {
  void operator()(AsyncFile* file) noexcept;
};

struct AsyncFileBuilder {
  using ptr = std::unique_ptr<AsyncFile, AsyncFileDeleter>;

  static ptr make(size_t queue_size, unsigned flags);
};

using AsyncFilePool = unbounded_object_pool<AsyncFileBuilder>;
using AsyncFilePtr = AsyncFilePool::ptr;

class AsyncDirectory final : public MMapDirectory {
 public:
  explicit AsyncDirectory(
    std::filesystem::path dir,
    directory_attributes attrs = directory_attributes{},
    const ResourceManagementOptions& rm = ResourceManagementOptions::kDefault,
    size_t pool_size = 16, size_t queue_size = 1024, unsigned flags = 0);

  index_output::ptr create(std::string_view name) noexcept final;
  bool sync(std::span<const std::string_view> names) noexcept final;

 private:
  AsyncFilePool async_pool_;
  size_t queue_size_;
  unsigned flags_;
};

}  // namespace irs
