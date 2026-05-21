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

#pragma once

#include "index_reader.hpp"
#include "shared.hpp"
#include "utils/object_pool.hpp"

namespace irs {

class DirectoryReaderImpl;
struct DirectoryMeta;

// Interface for an index reader over a directory of segments
class DirectoryReader final : public IndexReader {
 public:
  DirectoryReader() noexcept = default;
  DirectoryReader(DirectoryReader&&) noexcept = default;
  DirectoryReader& operator=(DirectoryReader&&) noexcept = default;

  // Create an index reader over the specified directory
  // if codec == nullptr then use the latest file for all known codecs
  explicit DirectoryReader(
    const directory& dir, format::ptr codec = nullptr,
    const IndexReaderOptions& opts = IndexReaderOptions{});
  explicit DirectoryReader(
    std::shared_ptr<const DirectoryReaderImpl>&& impl) noexcept;
  DirectoryReader(const DirectoryReader& other) noexcept;
  DirectoryReader& operator=(const DirectoryReader& other) noexcept;

  // Return the directory_meta this reader is based upon
  // Note that return value valid on an already open reader until call to
  // reopen()
  const DirectoryMeta& Meta() const;

  explicit operator bool() const noexcept { return nullptr != impl_; }

  bool operator==(std::nullptr_t) const noexcept { return !impl_; }

  bool operator==(const DirectoryReader& rhs) const noexcept {
    return impl_ == rhs.impl_;
  }

  DirectoryReader& operator*() noexcept { return *this; }
  const DirectoryReader& operator*() const noexcept { return *this; }
  DirectoryReader* operator->() noexcept { return this; }
  const DirectoryReader* operator->() const noexcept { return this; }

  const SubReader& operator[](size_t i) const final;

  uint64_t CountMappedMemory() const final;

  uint64_t docs_count() const final;

  uint64_t live_docs_count() const final;

  size_t size() const final;

  // Open a new instance based on the latest file for the specified codec
  // this call will attempt to reuse segments from the existing reader
  DirectoryReader Reopen() const;

  const std::shared_ptr<const DirectoryReaderImpl>& GetImpl() const noexcept {
    return impl_;
  }

 private:
  std::shared_ptr<const DirectoryReaderImpl> impl_;
};

}  // namespace irs
