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

#include "index_utils.hpp"

#include <cmath>
#include <set>

#include "formats/format_utils.hpp"

namespace tier {

    //  interface to fetch the required attributes from
    //  SegmentStats struct.
    //  We use this function in struct ConsolidationCandidate
    //  to fetch the segment dimensions from the SegmentStats
    //  struct.
    //
    void getSegmentDimensions(
        const tier::SegmentStats& segment,
        tier::SegmentAttributes& attrs) {

      auto* meta = segment.meta;
      attrs.byteSize = meta->byte_size;
      attrs.docsCount = meta->docs_count;
      attrs.liveDocsCount = meta->live_docs_count;
    }
}

namespace irs::index_utils {

ConsolidationPolicy MakePolicy(const ConsolidateBytes& options) {
  return [options](Consolidation& candidates, const IndexReader& reader,
                   const ConsolidatingSegments& consolidating_segments,
                  bool /*favorCleanupOverMerge*/) {
    const auto byte_threshold = options.threshold;
    size_t all_segment_bytes_size = 0;
    const auto segment_count = reader.size();

    for (auto& segment : reader) {
      all_segment_bytes_size += segment.Meta().byte_size;
    }

    const auto threshold = std::clamp(byte_threshold, 0.f, 1.f);
    const auto threshold_bytes_avg =
      (static_cast<float>(all_segment_bytes_size) /
       static_cast<float>(segment_count)) *
      threshold;

    // merge segment if: {threshold} > segment_bytes / (all_segment_bytes /
    // #segments)
    for (auto& segment : reader) {
      if (consolidating_segments.contains(segment.Meta().name)) {
        continue;
      }
      const auto segment_bytes_size = segment.Meta().byte_size;
      if (threshold_bytes_avg >= static_cast<float>(segment_bytes_size)) {
        candidates.emplace_back(&segment);
      }
    }
  };
}

ConsolidationPolicy MakePolicy(const ConsolidateBytesAccum& options) {
  return [options](Consolidation& candidates, const IndexReader& reader,
                   const ConsolidatingSegments& consolidating_segments,
                  bool /*favorCleanupOverMerge*/) {
    auto byte_threshold = options.threshold;
    size_t all_segment_bytes_size = 0;
    std::vector<std::pair<size_t, const SubReader*>> segments;
    segments.reserve(reader.size());

    for (auto& segment : reader) {
      if (consolidating_segments.contains(segment.Meta().name)) {
        continue;  // segment is already under consolidation
      }
      segments.emplace_back(SizeWithoutRemovals(segment.Meta()), &segment);
      all_segment_bytes_size += segments.back().first;
    }

    size_t cumulative_size = 0;
    const auto threshold_size = static_cast<float>(all_segment_bytes_size) *
                                std::clamp(byte_threshold, 0.f, 1.f);

    // prefer to consolidate smaller segments
    std::sort(
      segments.begin(), segments.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

    // merge segment if: {threshold} >= (segment_bytes +
    // sum_of_merge_candidate_segment_bytes) / all_segment_bytes
    for (auto& entry : segments) {
      const auto segment_bytes_size = entry.first;

      if (static_cast<float>(cumulative_size + segment_bytes_size) <=
          threshold_size) {
        cumulative_size += segment_bytes_size;
        candidates.emplace_back(entry.second);
      }
    }
  };
}

ConsolidationPolicy MakePolicy(const ConsolidateCount& options) {
  return [options](Consolidation& candidates, const IndexReader& reader,
                   const ConsolidatingSegments&,
                  bool /*favorCleanupOverMerge*/) {
    // merge first 'threshold' segments
    for (size_t i = 0, count = std::min(options.threshold, reader.size());
         i < count; ++i) {
      candidates.emplace_back(&reader[i]);
    }
  };
}

ConsolidationPolicy MakePolicy(const ConsolidateDocsFill& options) {
  return [options](Consolidation& candidates, const IndexReader& reader,
                   const ConsolidatingSegments& consolidating_segments,
                  bool /*favorCleanupOverMerge*/) {
    auto fill_threshold = options.threshold;
    auto threshold = std::clamp(fill_threshold, 0.f, 1.f);

    // merge segment if: {threshold} >= #segment_docs{valid} /
    // (#segment_docs{valid} + #segment_docs{removed})
    for (auto& segment : reader) {
      auto& meta = segment.Meta();
      if (consolidating_segments.contains(meta.name)) {
        continue;
      }
      if (!meta.live_docs_count  // if no valid doc_ids left in segment
          || static_cast<float>(meta.docs_count) * threshold >=
               static_cast<float>(meta.live_docs_count)) {
        candidates.emplace_back(&segment);
      }
    }
  };
}

ConsolidationPolicy MakePolicy(const ConsolidateDocsLive& options) {
  return [options](Consolidation& candidates, const IndexReader& meta,
                   const ConsolidatingSegments& consolidating_segments,
                  bool /*favorCleanupOverMerge*/) {
    const auto docs_threshold = options.threshold;
    const auto all_segment_docs_count = meta.live_docs_count();
    const auto segment_count = meta.size();

    const auto threshold = std::clamp(docs_threshold, 0.f, 1.f);
    const auto threshold_docs_avg =
      (static_cast<float>(all_segment_docs_count) /
       static_cast<float>(segment_count)) *
      threshold;

    // merge segment if: {threshold} >= segment_docs{valid} /
    // (all_segment_docs{valid} / #segments)
    for (auto& segment : meta) {
      auto& info = segment.Meta();
      if (consolidating_segments.contains(info.name)) {
        continue;
      }
      if (!info.live_docs_count  // if no valid doc_ids left in segment
          || threshold_docs_avg >= static_cast<float>(info.live_docs_count)) {
        candidates.emplace_back(&segment);
      }
    }
  };
}

ConsolidationPolicy MakePolicy(const ConsolidateTier& options) {
  const auto max_segments_bytes =
    (std::max)(size_t{1}, options.max_segments_bytes);

  const auto max_skew_threshold = options.max_skew_threshold;
  const auto min_deletion_ratio = options.min_deletion_ratio;

  return [max_segments_bytes, max_skew_threshold, min_deletion_ratio](
              Consolidation& candidates, const IndexReader& reader,
              const ConsolidatingSegments& consolidating_segments,
              bool favorCleanupOverMerge) {
    // size of segments in bytes that are currently under consolidation
    [[maybe_unused]] size_t consolidating_size = 0;
    // the smallest segment
    size_t min_segment_size = std::numeric_limits<size_t>::max();
    // total size in bytes of all segments in index
    [[maybe_unused]] size_t total_index_size = 0;
    size_t total_docs_count = 0;  // total number of documents in index
    // total number of live documents in index
    size_t total_live_docs_count = 0;

    ///////////////////////////////////////////////////////////////////////////
    /// Stage 0
    /// get sorted list of segments
    ///////////////////////////////////////////////////////////////////////////

    std::vector<tier::SegmentStats> sorted_segments;
    sorted_segments.reserve(reader.size());

    // get segments from index meta
    for (auto& segment : reader) {
      if (segment.live_docs_count()) {
        // skip empty segments, they'll be
        // removed from index by index_writer
        // during 'commit'
        sorted_segments.emplace_back(segment);
      }
    }

    ///////////////////////////////////////////////////////////////////////////
    /// Stage 1
    /// calculate overall stats
    ///////////////////////////////////////////////////////////////////////////

    auto segments_end = sorted_segments.data() + sorted_segments.size();
    for (auto begin = sorted_segments.data(); begin < segments_end;) {
      auto& segment = *begin;

      min_segment_size = std::min(min_segment_size, segment.size);
      total_index_size += segment.size;
      total_live_docs_count += segment.meta->live_docs_count;

      if (consolidating_segments.contains(segment.reader->Meta().name)) {
        consolidating_size += segment.size;
        // exclude removals from stats for consolidating segments
        total_docs_count += segment.meta->live_docs_count;

        // segment is already marked for consolidation, filter it out
        irstd::swap_remove(sorted_segments, begin);
        --segments_end;
      } else {
        total_docs_count += segment.meta->docs_count;
        ++begin;
      }
    }

    if (!total_docs_count) {
      // nothing to consolidate
      return;
    }

    ///////////////////////////////////////////////////////////////////////////
    /// Stage 2
    /// filter out "too large segments", segment is meant to be treated as large
    /// if
    /// - segment size is greater than 'max_segments_bytes / 2'
    /// - segment has many documents but only few deletions
    ///
    /// TODO - too_big_segments_threshold formula is unreasonable
    ///      - add unit tests as well
    ///////////////////////////////////////////////////////////////////////////

    const double_t total_fill_factor =
      static_cast<double_t>(total_live_docs_count) /
      static_cast<double_t>(total_docs_count);
    const size_t too_big_segments_threshold = max_segments_bytes / 2;
    segments_end = sorted_segments.data() + sorted_segments.size();
    for (auto begin = sorted_segments.data(); begin < segments_end;) {
      auto& segment = *begin;
      const double_t segment_fill_factor =
        static_cast<double_t>(segment.meta->live_docs_count) /
        static_cast<double_t>(segment.meta->docs_count);
      if (segment.size > too_big_segments_threshold &&
          (total_fill_factor <= segment_fill_factor)) {
        // filter out segments that are too big
        irstd::swap_remove(sorted_segments, begin);
        --segments_end;
      } else {
        ++begin;
      }
    }

    ///////////////////////////////////////////////////////////////////////////
    /// Stage 3
    /// Find cleanup and merge candidates
    ///////////////////////////////////////////////////////////////////////////

    auto cleanup = [&](tier::ConsolidationCandidate<tier::SegmentStats>& best) -> bool {
      return tier::findBestCleanupCandidate<tier::SegmentStats>(sorted_segments, min_deletion_ratio, tier::getSegmentDimensions, best);
    };

    auto merge = [&](tier::ConsolidationCandidate<tier::SegmentStats>& best) -> bool {
      return tier::findBestConsolidationCandidate<tier::SegmentStats>(
          sorted_segments,
          max_segments_bytes,
          max_skew_threshold,
          tier::getSegmentDimensions, best);
    };

    tier::ConsolidationCandidate<tier::SegmentStats> best;

    //  We use this arg to alternate between cleanup and
    //  merge giving both a fair chance during consolidation.
    //  Cleanup will reclaim disk space (possibly reducing no.
    //  of segments as well) whereas merging will reduce the no.
    //  of segments.
    //  However, if the favored operation yields no segments,
    //  we try the other operation so as not to lose this
    //  consolidation interval.
    if (favorCleanupOverMerge) {
      if (!cleanup(best))
        merge(best);
    }
    else {
      if (!merge(best))
        cleanup(best);
    }

    if (!best.initialized)
      return;

    ///////////////////////////////////////////////////////////////////////////
    /// Stage 4
    /// find consolidation candidates
    ///////////////////////////////////////////////////////////////////////////

    candidates.reserve(std::distance(best.first(), best.last()) + 1);
    std::copy(best.first(), best.last() + 1, std::back_inserter(candidates));
  };
}

void ReadDocumentMask(irs::DocumentMask& docs_mask, const irs::directory& dir,
                      const irs::SegmentMeta& meta) {
  if (!irs::HasRemovals(meta)) {
    return;  // nothing to read
  }

  auto reader = meta.codec->get_document_mask_reader();
  reader->read(dir, meta, docs_mask);
}

void FlushIndexSegment(directory& dir, IndexSegment& segment) {
  IRS_ASSERT(segment.meta.codec);
  IRS_ASSERT(segment.meta.byte_size);  // Ensure segment size is estimated

  auto writer = segment.meta.codec->get_segment_meta_writer();
  writer->write(dir, segment.filename, segment.meta);
}

}  // namespace irs::index_utils
