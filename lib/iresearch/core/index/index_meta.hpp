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

#include <optional>
#include <span>
#include <vector>

#include "error/error.hpp"
#include "utils/string.hpp"
#include "utils/type_limits.hpp"

#include <absl/container/flat_hash_set.h>

namespace irs {

class format;
class IndexWriter;

struct SegmentInfo {
  SegmentInfo() = default;

  //  Added for testing purposes.
  SegmentInfo(
    const std::string& _name,
    uint64_t _byte_size
  ) : name(_name), byte_size(_byte_size)
  {}

  bool operator==(const SegmentInfo&) const = default;

  std::string name;            // FIXME(gnusi): move to SegmentMeta
  uint64_t docs_count{};       // Total number of documents in a segment
  uint64_t live_docs_count{};  // Total number of live documents in a segment
  uint64_t version{};
  uint64_t byte_size{};  // Size of a segment in bytes
};

static_assert(std::is_nothrow_move_constructible_v<SegmentInfo>);
static_assert(std::is_nothrow_move_assignable_v<SegmentInfo>);

struct SegmentMeta : SegmentInfo {
  bool operator==(const SegmentMeta&) const = default;

  std::vector<std::string> files;
  std::shared_ptr<const format> codec;
  field_id sort{field_limits::invalid()};
  bool column_store{};
};

inline bool HasRemovals(const SegmentInfo& meta) noexcept {
  return meta.live_docs_count != meta.docs_count;
}

static_assert(std::is_nothrow_move_constructible_v<SegmentMeta>);
static_assert(std::is_nothrow_move_assignable_v<SegmentMeta>);

struct IndexSegment {
  bool operator==(const IndexSegment&) const = default;

  std::string filename;
  SegmentMeta meta;
};

static_assert(std::is_nothrow_move_constructible_v<IndexSegment>);
static_assert(std::is_nothrow_move_assignable_v<IndexSegment>);

struct IndexMeta {
  bool operator==(const IndexMeta&) const = default;

  uint64_t gen{index_gen_limits::invalid()};
  uint64_t seg_counter{0};
  std::vector<IndexSegment> segments;
  std::optional<bstring> payload;
};

inline bytes_view GetPayload(const IndexMeta& meta) noexcept {
  return meta.payload ? *meta.payload : bytes_view{};
}

struct DirectoryMeta {
  bool operator==(const DirectoryMeta&) const = default;

  std::string filename;
  IndexMeta index_meta;
};

static_assert(std::is_nothrow_move_constructible_v<DirectoryMeta>);
static_assert(std::is_nothrow_move_assignable_v<DirectoryMeta>);

}  // namespace irs
