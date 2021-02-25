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

#ifndef IRESEARCH_DIRECTORY_UTILS_H
#define IRESEARCH_DIRECTORY_UTILS_H

#include "shared.hpp"
#include "store/data_input.hpp"
#include "store/data_output.hpp"
#include "store/directory.hpp"
#include "store/directory_cleaner.hpp"

namespace iresearch {

class format;
class index_meta;
struct segment_meta;

namespace directory_utils {

// ----------------------------------------------------------------------------
// --SECTION--                                           memory_allocator utils
// ----------------------------------------------------------------------------

// inserts memory_allocator with a specified pool size into a provided
// directory and returns a refernece to it
// if size equals to 0, returns 'memory_allocator::global' allocator
IRESEARCH_API memory_allocator& ensure_allocator(
  directory& dir, size_t size
);

// returns a memory allocator assigned to a specified directory or
// `memory_allocator::global()` allocator if there is no allocator assigned
IRESEARCH_API memory_allocator& get_allocator(
  const directory& dir
);

// ----------------------------------------------------------------------------
// --SECTION--                                            index_file_refs utils
// ----------------------------------------------------------------------------

// return a reference to a file or empty() if not found
IRESEARCH_API index_file_refs::ref_t reference(
  directory& dir,
  const std::string& name,
  bool include_missing = false
);

// return success, visitor gets passed references to files retrieved from source
IRESEARCH_API bool reference(
  directory& dir,
  const std::function<const std::string*()>& source,
  const std::function<bool(index_file_refs::ref_t&& ref)>& visitor,
  bool include_missing = false
);

// return success, visitor gets passed references to files registered with index_meta
IRESEARCH_API bool reference(
  directory& dir,
  const index_meta& meta,
  const std::function<bool(index_file_refs::ref_t&& ref)>& visitor,
  bool include_missing = false
);

// return success, visitor gets passed references to files registered with segment_meta
IRESEARCH_API bool reference(
  directory& dir,
  const segment_meta& meta,
  const std::function<bool(index_file_refs::ref_t&& ref)>& visitor,
  bool include_missing = false
);

// remove all (tracked and non-tracked) files if they are unreferenced
IRESEARCH_API void remove_all_unreferenced(directory& dir);

// remove tracked files if they are unreferenced and not part of the latest segments
IRESEARCH_API directory_cleaner::removal_acceptor_t remove_except_current_segments(
  const directory& dir, const format& codec
);

}

//////////////////////////////////////////////////////////////////////////////
/// @class tracking_directory
/// @brief track files created/opened via file names
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API tracking_directory final : public directory {
  typedef std::unordered_set<std::string> file_set;

  // @param track_open - track file refs for calls to open(...)
  explicit tracking_directory(
    directory& impl,
    bool track_open = false
  ) noexcept;

  directory& operator*() noexcept {
    return impl_;
  }

  using directory::attributes;
  virtual attribute_store& attributes() noexcept override {
    return impl_.attributes();
  }

  virtual index_output::ptr create(const std::string& name) noexcept override;

  void clear_tracked() noexcept;

  virtual bool exists(
      bool& result, const std::string& name
  ) const noexcept override {
    return impl_.exists(result, name);
  }

  void flush_tracked(file_set& other) noexcept;

  virtual bool length(
      uint64_t& result, const std::string& name
  ) const noexcept override {
    return impl_.length(result, name);
  }

  virtual index_lock::ptr make_lock(
      const std::string& name
  ) noexcept override {
    return impl_.make_lock(name);
  }

  virtual bool mtime(
      std::time_t& result, const std::string& name
  ) const noexcept override {
    return impl_.mtime(result, name);
  }

  virtual index_input::ptr open(
    const std::string& name,
    IOAdvice advice
  ) const noexcept override;

  virtual bool remove(const std::string& name) noexcept override;

  virtual bool rename(
    const std::string& src, const std::string& dst
  ) noexcept override;

  virtual bool sync(const std::string& name) noexcept override {
    return impl_.sync(name);
  }

  virtual bool visit(const visitor_f& visitor) const override {
    return impl_.visit(visitor);
  }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  mutable file_set files_;
  directory& impl_;
  bool track_open_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // tracking_directory

//////////////////////////////////////////////////////////////////////////////
/// @class ref_tracking_directory
/// @brief track files created/opened via file refs instead of file names
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API ref_tracking_directory: public directory {
 public:
  using ptr = std::unique_ptr<ref_tracking_directory>;

  // @param track_open - track file refs for calls to open(...)
  explicit ref_tracking_directory(directory& impl, bool track_open = false);
  ref_tracking_directory(ref_tracking_directory&& other) noexcept;

  directory& operator*() noexcept {
    return impl_;
  }

  using directory::attributes;
  virtual attribute_store& attributes() noexcept override {
    return impl_.attributes();
  }

  void clear_refs() const noexcept;

  virtual index_output::ptr create(const std::string &name) noexcept override;

  virtual bool exists(
      bool& result, const std::string& name
  ) const noexcept override {
    return impl_.exists(result, name);
  }

  virtual bool length(
      uint64_t& result, const std::string& name
  ) const noexcept override {
    return impl_.length(result, name);
  }

  virtual index_lock::ptr make_lock(const std::string& name) noexcept override {
    return impl_.make_lock(name);
  }

  virtual bool mtime(
      std::time_t& result, const std::string& name
  ) const noexcept override {
    return impl_.mtime(result, name);
  }

  virtual index_input::ptr open(
    const std::string& name,
    IOAdvice advice
  ) const noexcept override;

  virtual bool remove(const std::string& name) noexcept override;

  virtual bool rename(const std::string& src, const std::string& dst) noexcept override;

  virtual bool sync(const std::string& name) noexcept override {
    return impl_.sync(name);
  }

  virtual bool visit(const visitor_f& visitor) const override {
    return impl_.visit(visitor);
  }

  bool visit_refs(const std::function<bool(const index_file_refs::ref_t& ref)>& visitor) const;

 private:
  typedef std::unordered_set<
    index_file_refs::ref_t,
    index_file_refs::counter_t::hash,
    index_file_refs::counter_t::equal_to
  > refs_t;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  index_file_refs::attribute_t& attribute_;
  directory& impl_;
  mutable std::mutex mutex_; // for use with refs_
  mutable refs_t refs_;
  bool track_open_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // ref_tracking_directory

}

#endif
