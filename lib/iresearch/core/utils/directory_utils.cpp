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

#include "utils/directory_utils.hpp"

#include "formats/formats.hpp"
#include "index/index_meta.hpp"
#include "store/directory_attributes.hpp"
#include "utils/attributes.hpp"
#include "utils/log.hpp"

namespace irs {
namespace directory_utils {

// Return a reference to a file or empty() if not found
index_file_refs::ref_t Reference(const directory& dir, std::string_view name) {
  auto& refs = dir.attributes().refs();

  bool exists;

  // Do not add an attribute if the file definitly does not exist
  if (!dir.exists(exists, name) || !exists) {
    return nullptr;
  }

  auto ref = refs.add(name);

  return dir.exists(exists, name) && exists ? ref
                                            : index_file_refs::ref_t{nullptr};
}

bool RemoveAllUnreferenced(directory& dir) {
  auto& refs = dir.attributes().refs();

  dir.visit([&refs](std::string_view name) {
    refs.add(name);  // Ensure all files in dir are tracked
    return true;
  });

  directory_cleaner::clean(dir);
  return true;
}

}  // namespace directory_utils

TrackingDirectory::TrackingDirectory(directory& impl) noexcept
  : impl_{impl}, on_close_{[this](size_t size) noexcept {
      this->files_size_ += size;
    }} {}

index_output::ptr TrackingDirectory::create(std::string_view name)  //
  noexcept try {
  files_.emplace(name);
  if (auto result = impl_.create(name); IRS_LIKELY(result)) {
    result->SetCallback(&on_close_);
    return result;
  }
  files_.erase(name);  // Revert change
  return nullptr;
} catch (...) {
  return nullptr;
}

bool TrackingDirectory::rename(std::string_view src,
                               std::string_view dst) noexcept {
  if (!impl_.rename(src, dst)) {
    return false;
  }

  try {
    files_.emplace(dst);
    files_.erase(src);

    return true;
  } catch (...) {
    impl_.rename(dst, src);  // revert
  }

  return false;
}

void TrackingDirectory::ClearTracked() noexcept {
  files_size_ = 0;
  files_.clear();
}

std::vector<std::string> TrackingDirectory::FlushTracked(uint64_t& files_size) {
  std::vector<std::string> files(files_.size());
  auto files_begin = files.begin();
  for (auto begin = files_.begin(), end = files_.end(); begin != end;
       ++files_begin) {
    auto to_extract = begin++;
    *files_begin = std::move(files_.extract(to_extract).value());
  }
  files_size = std::exchange(files_size_, uint64_t{0});
  return files;
}

RefTrackingDirectory::RefTrackingDirectory(directory& impl,
                                           bool track_open /*= false*/)
  : attribute_(impl.attributes().refs()),
    impl_(impl),
    track_open_(track_open) {}

RefTrackingDirectory::RefTrackingDirectory(
  RefTrackingDirectory&& other) noexcept
  : attribute_(other.attribute_),  // references do not require std::move(...)
    impl_(other.impl_),            // references do not require std::move(...)
    refs_(std::move(other.refs_)),
    track_open_(std::move(other.track_open_)) {}

void RefTrackingDirectory::clear_refs() const {
  std::lock_guard lock{mutex_};
  refs_.clear();
}

index_output::ptr RefTrackingDirectory::create(std::string_view name) noexcept {
  try {
    // Do not change the order of calls!
    // The cleaner should "see" the file in directory
    // ONLY if there is a tracking reference present!
    auto ref = attribute_.add(name);
    auto result = impl_.create(name);

    // only track ref on successful call to impl_
    if (result) {
      std::lock_guard lock{mutex_};
      refs_.insert(std::move(ref));
    } else {
      attribute_.remove(name);
    }

    return result;
  } catch (...) {
  }

  return nullptr;
}

index_input::ptr RefTrackingDirectory::open(std::string_view name,
                                            IOAdvice advice) const noexcept {
  if (!track_open_) {
    return impl_.open(name, advice);
  }

  try {
    // Do not change the order of calls!
    // The cleaner should "see" the file in directory
    // ONLY if there is a tracking reference present!
    auto ref = attribute_.add(name);
    auto result = impl_.open(name, advice);

    // only track ref on successful call to impl_
    if (result) {
      std::lock_guard lock{mutex_};
      refs_.emplace(std::move(ref));
    } else {
      attribute_.remove(name);
    }

    return result;
  } catch (...) {
  }

  return nullptr;
}

bool RefTrackingDirectory::remove(std::string_view name) noexcept {
  if (!impl_.remove(name)) {
    return false;
  }

  try {
    attribute_.remove(name);

    std::lock_guard lock{mutex_};
    refs_.erase(name);
    return true;
  } catch (...) {
    // ignore failure since removal from impl_ was sucessful
  }

  return false;
}

bool RefTrackingDirectory::rename(std::string_view src,
                                  std::string_view dst) noexcept {
  if (!impl_.rename(src, dst)) {
    return false;
  }

  try {
    auto ref = attribute_.add(dst);

    {
      std::lock_guard lock{mutex_};
      refs_.emplace(std::move(ref));

      refs_.erase(src);
    }

    attribute_.remove(src);
    return true;
  } catch (...) {
  }

  return false;
}

std::vector<index_file_refs::ref_t> RefTrackingDirectory::GetRefs() const {
  std::lock_guard lock{mutex_};

  return {refs_.begin(), refs_.end()};
}

}  // namespace irs
