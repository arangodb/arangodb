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

#include <vector>

#include "index/index_features.hpp"
#include "index/index_meta.hpp"
#include "index/index_reader.hpp"
#include "utils/memory.hpp"
#include "utils/noncopyable.hpp"
#include "utils/string.hpp"

namespace irs {

struct TrackingDirectory;
class Comparer;

class MergeWriter : public util::noncopyable {
 public:
  using FlushProgress = std::function<bool()>;

  struct ReaderCtx {
    ReaderCtx(const SubReader* reader, IResourceManager& rm) noexcept;
    ReaderCtx(const SubReader& reader, IResourceManager& rm) noexcept
      : ReaderCtx{&reader, rm} {}

    const SubReader* reader;                    // segment reader
    ManagedVector<doc_id_t> doc_id_map;         // FIXME use bitpacking vector
    std::function<doc_id_t(doc_id_t)> doc_map;  // mapping function
  };

  MergeWriter(IResourceManager& rm) noexcept;

  explicit MergeWriter(directory& dir,
                       const SegmentWriterOptions& options) noexcept
    : dir_{dir},
      readers_{{options.resource_manager}},
      column_info_{&options.column_info},
      feature_info_{&options.feature_info},
      scorers_{options.scorers},
      scorers_features_{&options.scorers_features},
      comparator_{options.comparator} {
    IRS_ASSERT(column_info_);
  }
  MergeWriter(MergeWriter&&) = default;
  MergeWriter& operator=(MergeWriter&&) = delete;

  operator bool() const noexcept;

  template<typename Iterator>
  void Reset(Iterator begin, Iterator end) {
    readers_.reserve(readers_.size() + std::distance(begin, end));
    while (begin != end) {
      readers_.emplace_back(*begin++,
                            readers_.get_allocator().ResourceManager());
    }
  }

  // Flush all of the added readers into a single segment.
  // `segment` the segment that was flushed.
  // `progress` report flush progress (abort if 'progress' returns false).
  // Return merge successful.
  bool Flush(SegmentMeta& segment, const FlushProgress& progress = {});

  const ReaderCtx& operator[](size_t i) const noexcept {
    IRS_ASSERT(i < readers_.size());
    return readers_[i];
  }

 private:
  bool FlushSorted(TrackingDirectory& dir, SegmentMeta& segment,
                   const FlushProgress& progress);

  bool FlushUnsorted(TrackingDirectory& dir, SegmentMeta& segment,
                     const FlushProgress& progress);

  directory& dir_;
  ManagedVector<ReaderCtx> readers_;
  const ColumnInfoProvider* column_info_{};
  const FeatureInfoProvider* feature_info_{};
  ScorersView scorers_;
  const feature_set_t* scorers_features_{};
  const Comparer* const comparator_{};
};

static_assert(std::is_nothrow_move_constructible_v<MergeWriter>);

}  // namespace irs
