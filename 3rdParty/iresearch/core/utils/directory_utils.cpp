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

#include "directory_utils.hpp"
#include "index/index_meta.hpp"
#include "formats/formats.hpp"
#include "utils/attributes.hpp"
#include "utils/log.hpp"

namespace iresearch {
namespace directory_utils {

// ----------------------------------------------------------------------------
// --SECTION--                                           memory_allocator utils
// ----------------------------------------------------------------------------

memory_allocator& ensure_allocator(
    directory& dir, size_t size
) {
  return size
    ? *dir.attributes().emplace<memory_allocator>(size)
    : memory_allocator::global();
}

memory_allocator& get_allocator(const directory& dir) {
  auto& allocator = dir.attributes().get<memory_allocator>();

  if (allocator) {
    return *allocator;
  }

  return memory_allocator::global();
}

// ----------------------------------------------------------------------------
// --SECTION--                                            index_file_refs utils
// ----------------------------------------------------------------------------

// return a reference to a file or empty() if not found
index_file_refs::ref_t reference(
    directory& dir, 
    const std::string& name,
    bool include_missing /*= false*/) {
  if (include_missing) {
    return dir.attributes().emplace<index_file_refs>()->add(name);
  }

  bool exists;

  // do not add an attribute if the file definitly does not exist
  if (!dir.exists(exists, name) || !exists) {
    return nullptr;
  }

  auto ref = dir.attributes().emplace<index_file_refs>()->add(name);

  return dir.exists(exists, name) && exists
    ? ref : index_file_refs::ref_t(nullptr);
}

#if defined(_MSC_VER)
  #pragma warning(disable : 4706)
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wparentheses"
#endif

// return success, visitor gets passed references to files retrieved from source
bool reference(
    directory& dir,
    const std::function<const std::string*()>& source,
    const std::function<bool(index_file_refs::ref_t&& ref)>& visitor,
    bool include_missing /*= false*/
) {
  auto& attribute = dir.attributes().emplace<index_file_refs>();

  for (const std::string* file; file = source();) {
    if (include_missing) {
      if (!visitor(attribute->add(*file))) {
        return false;
      }

      continue;
    }

    bool exists;

    // do not add an attribute if the file definitly does not exist
    if (!dir.exists(exists, *file) || !exists) {
      continue;
    }

    auto ref = attribute->add(*file);

    if (dir.exists(exists, *file) && exists && !visitor(std::move(ref))) {
      return false;
    }
  }

  return true;
}

#if defined(_MSC_VER)
  #pragma warning(default : 4706)
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

// return success, visitor gets passed references to files registered with index_meta
bool reference(
    directory& dir,
    const index_meta& meta,
    const std::function<bool(index_file_refs::ref_t&& ref)>& visitor,
    bool include_missing /*= false*/
) {
  if (meta.empty()) {
    return true;
  }

  auto& attribute = dir.attributes().emplace<index_file_refs>();

  return meta.visit_files([include_missing, &attribute, &dir, &visitor](const std::string& file) {
    if (include_missing) {
      return visitor(attribute->add(file));
    }

    bool exists;

    // do not add an attribute if the file definitly does not exist
    if (!dir.exists(exists, file) || !exists) {
      return true;
    }

    auto ref = attribute->add(file);

    if (dir.exists(exists, file) && exists) {
      return visitor(std::move(ref));
    }

    return true;
  });
}

// return success, visitor gets passed references to files registered with segment_meta
bool reference(
  directory& dir,
  const segment_meta& meta,
  const std::function<bool(index_file_refs::ref_t&& ref)>& visitor,
  bool include_missing /*= false*/
) {
  auto files = meta.files;

  if (files.empty()) {
    return true;
  }

  auto& attribute = dir.attributes().emplace<index_file_refs>();

  for (auto& file: files) {
    if (include_missing) {
      if (!visitor(attribute->add(file))) {
        return false;
      }

      continue;
    }

    bool exists;

    // do not add an attribute if the file definitly does not exist
    if (!dir.exists(exists, file) || !exists) {
      continue;
    }

    auto ref = attribute->add(file);

    if (dir.exists(exists, file) && exists && !visitor(std::move(ref))) {
      return false;
    }
  }

  return true;
}

void remove_all_unreferenced(directory& dir) {
  auto& attribute = dir.attributes().emplace<index_file_refs>();

  dir.visit([&attribute] (std::string& name) {
    attribute->add(std::move(name)); // ensure all files in dir are tracked
    return true;
  });

  directory_cleaner::clean(dir);
}

directory_cleaner::removal_acceptor_t remove_except_current_segments(
  const directory& dir, const format& codec
) {
  static const auto acceptor = [](
      const std::string& filename, 
      const std::unordered_set<std::string>& retain) {
    return retain.find(filename) == retain.end();
  };

  index_meta meta;
  auto reader = codec.get_index_meta_reader();

  std::string segment_file;
  const bool index_exists = reader->last_segments_file(dir, segment_file);

  if (!index_exists) {
    // can't find segments file
    return [](const std::string&)->bool { return true; };
  }

  reader->read(dir, meta, segment_file);

  std::unordered_set<std::string> retain;
  retain.reserve(meta.size());

  meta.visit_files([&retain] (std::string& file) {
    retain.emplace(std::move(file));
    return true;
  });

  retain.emplace(std::move(segment_file));

  return std::bind(acceptor, std::placeholders::_1, std::move(retain));
}

}

// -----------------------------------------------------------------------------
// --SECTION--                                                tracking_directory
// -----------------------------------------------------------------------------

tracking_directory::tracking_directory(
    directory& impl,
    bool track_open /*= false*/
) noexcept
  : impl_(impl),
    track_open_(track_open) {
}

index_output::ptr tracking_directory::create(
  const std::string& name
) noexcept {
  try {
    files_.emplace(name);
  } catch (...) {
  }

  auto result = impl_.create(name);

  if (result) {
    return result;
  }

  try {
    files_.erase(name); // revert change
  } catch (...) {
  }

  return nullptr;
}

index_input::ptr tracking_directory::open(
    const std::string& name,
    IOAdvice advice
) const noexcept {
  if (track_open_) {
    try {
      files_.emplace(name);
    } catch (...) {

      return nullptr;
    }
  }

  return impl_.open(name, advice);
}

bool tracking_directory::remove(const std::string& name) noexcept {
  if (!impl_.remove(name)) {
    return false;
  }

  try {
    files_.erase(name);
    return true;
  } catch (...) {
    // ignore failure since removal from impl_ was sucessful
  }

  return false;
}

bool tracking_directory::rename(
  const std::string& src, const std::string& dst
) noexcept {
  if (!impl_.rename(src, dst)) {
    return false;
  }

  try {
    files_.emplace(dst);
    files_.erase(src);

    return true;
  } catch (...) {
    impl_.rename(dst, src); // revert
  }

  return false;
}

void tracking_directory::clear_tracked() noexcept {
  files_.clear();
}

void tracking_directory::flush_tracked(file_set& other) noexcept {
  other = std::move(files_);
}

// -----------------------------------------------------------------------------
// --SECTION--                                            ref_tracking_directory
// -----------------------------------------------------------------------------

ref_tracking_directory::ref_tracking_directory(
    directory& impl,
    bool track_open /*= false*/
) : attribute_(impl.attributes().emplace<index_file_refs>()),
    impl_(impl),
    track_open_(track_open) {
}

ref_tracking_directory::ref_tracking_directory(
    ref_tracking_directory&& other
) noexcept
  : attribute_(other.attribute_), // references do not require std::move(...)
    impl_(other.impl_), // references do not require std::move(...)
    refs_(std::move(other.refs_)),
    track_open_(std::move(other.track_open_)) {
}

void ref_tracking_directory::clear_refs() const noexcept {
  auto lock = make_lock_guard(mutex_);
  refs_.clear();
}

index_output::ptr ref_tracking_directory::create(
  const std::string& name
) noexcept {
  try {
    auto result = impl_.create(name);

    // only track ref on successful call to impl_
    if (result) {
      auto ref = attribute_->add(name);

      auto lock = make_lock_guard(mutex_);
      refs_.emplace(ref);
    }

    return result;
  } catch (...) {
  }

  return nullptr;
}

index_input::ptr ref_tracking_directory::open(
  const std::string& name,
  IOAdvice advice
) const noexcept {
  if (!track_open_) {
    return impl_.open(name, advice);
  }

  auto result = impl_.open(name, advice);

  // only track ref on successful call to impl_
  if (result) {
    try {
      auto ref = attribute_->add(name);
      auto lock = make_lock_guard(mutex_);

      refs_.emplace(ref);
    } catch (...) {

      return nullptr;
    }
  }

  return result;
}

bool ref_tracking_directory::remove(const std::string& name) noexcept {
  if (!impl_.remove(name)) {
    return false;
  }

  try {
    attribute_->remove(name);

    // aliasing ctor
    const index_file_refs::ref_t ref(
      index_file_refs::ref_t(),
      &name);

    auto lock = make_lock_guard(mutex_);

    refs_.erase(ref);
    return true;
  } catch (...) {
    // ignore failure since removal from impl_ was sucessful
  }

  return false;
}

bool ref_tracking_directory::rename(
    const std::string& src, const std::string& dst) noexcept {
  if (!impl_.rename(src, dst)) {
    return false;
  }

  try {
    auto ref = attribute_->add(dst);

    {
      // aliasing ctor
      const index_file_refs::ref_t src_ref(
        index_file_refs::ref_t(),
        &src
      );

      auto lock = make_lock_guard(mutex_);

      refs_.emplace(ref);
      refs_.erase(src_ref);
    }

    attribute_->remove(src);
    return true;
  } catch (...) {
  }

  return false;
}

bool ref_tracking_directory::visit_refs(
    const std::function<bool(const index_file_refs::ref_t& ref)>& visitor) const {
  auto lock = make_lock_guard(mutex_);

  for (const auto& ref: refs_) {
    if (!visitor(ref)) {
      return false;
    }
  }

  return true;
}

}
