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

#ifndef IRESEARCH_INDEX_META_H
#define IRESEARCH_INDEX_META_H

#include <algorithm>
#include <vector>
#include <atomic>

#include <absl/container/flat_hash_set.h>

#include "store/directory.hpp"

#include "error/error.hpp"

#include "utils/string.hpp"
#include "utils/type_limits.hpp"

namespace iresearch {

/* -------------------------------------------------------------------
 * segment_meta
 * ------------------------------------------------------------------*/

class format;
typedef std::shared_ptr<const format> format_ptr;

}

MSVC_ONLY(template class IRESEARCH_API std::shared_ptr<const irs::format>;) // format_ptr

namespace iresearch {

struct IRESEARCH_API segment_meta {
  using file_set = absl::flat_hash_set<std::string>;

  segment_meta() = default;
  segment_meta(const segment_meta&) = default;
  segment_meta(segment_meta&& rhs)
    noexcept(noexcept(std::is_nothrow_move_constructible_v<file_set>));
  segment_meta(const string_ref& name, format_ptr codec);
  segment_meta(
    std::string&& name,
    format_ptr codec,
    uint64_t docs_count,
    uint64_t live_docs_count,
    bool column_store,
    file_set&& files,
    size_t size = 0,
    field_id sort = field_limits::invalid()
  ) noexcept;

  segment_meta& operator=(segment_meta&& rhs)
    noexcept(noexcept(std::is_nothrow_move_assignable_v<file_set>));
  segment_meta& operator=(const segment_meta&) = default;

  bool operator==(const segment_meta& other) const noexcept;
  bool operator!=(const segment_meta& other) const noexcept;

  file_set files;
  std::string name;
  uint64_t docs_count{}; // total number of documents in a segment
  uint64_t live_docs_count{}; // total number of live documents in a segment
  format_ptr codec;
  size_t size{}; // size of a segment in bytes
  uint64_t version{};
  field_id sort{ field_limits::invalid() };
  bool column_store{};
};

static_assert(std::is_nothrow_move_constructible_v<segment_meta>);
static_assert(std::is_nothrow_move_assignable_v<segment_meta>);

/* -------------------------------------------------------------------
 * index_meta
 * ------------------------------------------------------------------*/

struct directory;
class index_writer;

class IRESEARCH_API index_meta {
 public:
  struct IRESEARCH_API index_segment_t {
    index_segment_t() = default;
    index_segment_t(segment_meta&& meta);
    index_segment_t(const index_segment_t& other) = default;
    index_segment_t& operator=(const index_segment_t& other) = default;
    index_segment_t(index_segment_t&&) = default;
    index_segment_t& operator=(index_segment_t&&) = default;

    bool operator==(const index_segment_t& other) const noexcept;
    bool operator!=(const index_segment_t& other) const noexcept;

    std::string filename;
    segment_meta meta;
  }; // index_segment_t

  static_assert(std::is_nothrow_move_constructible_v<index_segment_t>);
  static_assert(std::is_nothrow_move_assignable_v<index_segment_t>);

  using index_segments_t = std::vector<index_segment_t>;
  using ptr = std::unique_ptr<index_meta>;

  index_meta();
  index_meta(index_meta&& rhs) noexcept;
  index_meta(const index_meta& rhs);
  index_meta& operator=(index_meta&& rhs) noexcept;
  index_meta& operator=(const index_meta&) = delete;

  bool operator==(const index_meta& other) const noexcept;
  bool operator!=(const index_meta& other) const noexcept {
    return !(*this == other);
  }

  template<typename ForwardIterator>
  void add(ForwardIterator begin, ForwardIterator end) {
    segments_.reserve(segments_.size() + std::distance(begin, end));
    std::move(begin, end, std::back_inserter(segments_));
  }

  void add(index_segment_t&& segment) {
    segments_.emplace_back(std::move(segment));
  }

  template<typename Visitor>
  bool visit_files(const Visitor& visitor) const {
    return const_cast<index_meta&>(*this).visit_files(visitor);
  }

  template<typename Visitor>
  bool visit_files(const Visitor& visitor) {
    for (auto& segment : segments_) {
      if (!visitor(segment.filename)) {
        return false;
      }

      for (auto& file : segment.meta.files) {        
        if (!visitor(const_cast<std::string&>(file))) {
          return false;
        }
      }
    }
    return true;
  }

  template<typename Visitor>
  bool visit_segments(const Visitor& visitor) const {
    for (auto& segment : segments_) {
      if (!visitor(segment.filename, segment.meta)) {
        return false;
      }
    }
    return true;
  }

  uint64_t increment() noexcept { return ++seg_counter_; }
  uint64_t counter() const noexcept { return seg_counter_; }
  uint64_t generation() const noexcept { return gen_; }

  index_segments_t::iterator begin() noexcept { return segments_.begin(); }
  index_segments_t::iterator end() noexcept { return segments_.end(); }

  index_segments_t::const_iterator begin() const noexcept { return segments_.begin(); }
  index_segments_t::const_iterator end() const noexcept { return segments_.end(); }

  void update_generation(const index_meta& rhs) noexcept {
    gen_ = rhs.gen_;
    last_gen_ = rhs.last_gen_;
  }

  size_t size() const noexcept { return segments_.size(); }
  bool empty() const noexcept { return segments_.empty(); }

  void clear() {
    segments_.clear();
    // leave version and generation counters unchanged do to possible readers
  }

  void reset(const index_meta& rhs) {
    // leave version and generation counters unchanged
    segments_ = rhs.segments_;
  }

  const index_segment_t& segment(size_t i) const noexcept {
    assert(i < segments_.size());
    return segments_[i];
  }

  const index_segment_t& operator[](size_t i) const noexcept {
    assert(i < segments_.size());
    return segments_[i];
  }

  const index_segments_t& segments() const noexcept {
    return segments_;
  }

  const bytes_ref& payload() const noexcept {
    return payload_;
  }

 private:
  friend class index_writer;
  friend struct index_meta_reader;
  friend struct index_meta_writer;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  uint64_t gen_;
  uint64_t last_gen_;
  std::atomic<uint64_t> seg_counter_;
  index_segments_t segments_;
  bstring payload_buf_;
  bytes_ref payload_;
  IRESEARCH_API_PRIVATE_VARIABLES_END

  uint64_t next_generation() const noexcept;

  void payload(bstring&& payload) noexcept {
    payload_buf_ = std::move(payload);
    payload_ = payload_buf_;
  }

  void payload(const bytes_ref& payload) {
    if (payload.null()) {
      payload_buf_.clear();
      payload_ = bytes_ref::NIL;
    } else {
      payload_buf_ = payload;
      payload_ = payload_buf_;
    }
  }
}; // index_meta

}

#endif
