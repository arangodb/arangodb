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

#include "shared.hpp"
#include "store/data_input.hpp"
#include "store/data_output.hpp"
#include "store/directory.hpp"
#include "store/directory_cleaner.hpp"

namespace irs {

struct IndexMeta;
struct SegmentMeta;

namespace directory_utils {

// return a reference to a file or empty() if not found
index_file_refs::ref_t Reference(const directory& dir, std::string_view name);

// Remove all (tracked and non-tracked) files if they are unreferenced
// return success
bool RemoveAllUnreferenced(directory& dir);

}  // namespace directory_utils

// Track files created/opened via file names
struct TrackingDirectory final : public directory {
  explicit TrackingDirectory(directory& impl) noexcept;

  directory& GetImpl() noexcept { return impl_; }

  directory_attributes& attributes() noexcept final {
    return impl_.attributes();
  }

  index_output::ptr create(std::string_view name) noexcept final;

  bool exists(bool& result, std::string_view name) const noexcept final {
    return impl_.exists(result, name);
  }

  bool length(uint64_t& result, std::string_view name) const noexcept final {
    return impl_.length(result, name);
  }

  index_lock::ptr make_lock(std::string_view name) noexcept final {
    return impl_.make_lock(name);
  }

  bool mtime(std::time_t& result, std::string_view name) const noexcept final {
    return impl_.mtime(result, name);
  }

  index_input::ptr open(std::string_view name,
                        IOAdvice advice) const noexcept final {
    return impl_.open(name, advice);
  }

  bool remove(std::string_view) noexcept final {
    // not supported
    return false;
  }

  bool rename(std::string_view src, std::string_view dst) noexcept final;

  bool sync(std::span<const std::string_view> files) noexcept final {
    return impl_.sync(files);
  }

  bool visit(const visitor_f& visitor) const final {
    return impl_.visit(visitor);
  }

  std::vector<std::string> FlushTracked(uint64_t& files_size);

  void ClearTracked() noexcept;

 private:
  using FileSet = absl::flat_hash_set<std::string>;

  directory& impl_;
  uint64_t files_size_{};
  FileSet files_;
  index_output::OnClose on_close_;
};

// Track files created/opened via file refs instead of file names
struct RefTrackingDirectory : public directory {
 public:
  using ptr = std::unique_ptr<RefTrackingDirectory>;

  // @param track_open - track file refs for calls to open(...)
  explicit RefTrackingDirectory(directory& impl, bool track_open = false);
  RefTrackingDirectory(RefTrackingDirectory&& other) noexcept;

  directory& operator*() noexcept { return impl_; }

  directory_attributes& attributes() noexcept final {
    return impl_.attributes();
  }

  void clear_refs() const;

  index_output::ptr create(std::string_view name) noexcept final;

  bool exists(bool& result, std::string_view name) const noexcept final {
    return impl_.exists(result, name);
  }

  bool length(uint64_t& result, std::string_view name) const noexcept final {
    return impl_.length(result, name);
  }

  index_lock::ptr make_lock(std::string_view name) noexcept final {
    return impl_.make_lock(name);
  }

  bool mtime(std::time_t& result, std::string_view name) const noexcept final {
    return impl_.mtime(result, name);
  }

  index_input::ptr open(std::string_view name,
                        IOAdvice advice) const noexcept final;

  bool remove(std::string_view name) noexcept final;

  bool rename(std::string_view src, std::string_view dst) noexcept final;

  bool sync(std::span<const std::string_view> names) noexcept final {
    return impl_.sync(names);
  }

  bool visit(const visitor_f& visitor) const final {
    return impl_.visit(visitor);
  }

  std::vector<index_file_refs::ref_t> GetRefs() const;

 private:
  using refs_t = absl::flat_hash_set<index_file_refs::ref_t,
                                     index_file_refs::counter_t::hash,
                                     index_file_refs::counter_t::equal_to>;

  index_file_refs& attribute_;
  directory& impl_;
  mutable std::mutex mutex_;  // for use with refs_
  mutable refs_t refs_;
  bool track_open_;
};

}  // namespace irs
