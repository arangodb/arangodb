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

#include "formats/format_utils.hpp"
#include "index_utils.hpp"

#include <cmath>

namespace {

/// @returns percentage of live documents
inline double_t fill_factor(const irs::segment_meta& segment) noexcept {
  return double(segment.live_docs_count)/segment.docs_count;
}

/// @returns approximated size of a segment in the absence of removals
inline size_t size_without_removals(const irs::segment_meta& segment) noexcept{
  return size_t(segment.size * fill_factor(segment));
}

namespace tier {

struct segment_stat {
  segment_stat(const irs::segment_meta& meta) noexcept
    : meta(&meta), 
      size(size_without_removals(meta)),
      fill_factor(::fill_factor(meta)) {
  }

  bool operator<(const segment_stat& rhs) const noexcept {
    auto& lhs = *this;

    if (lhs.size == rhs.size) {
      if (lhs.fill_factor > rhs.fill_factor) {
        return true;
      } else if (lhs.fill_factor < rhs.fill_factor) {
        return false;
      }

      return lhs.meta->name < rhs.meta->name;
    }

    return lhs.size < rhs.size;
  }

  operator const irs::segment_meta*() const noexcept {
    return meta;
  }

  const irs::segment_meta* meta;
  size_t size; // approximate size of segment without removals
  double_t fill_factor;
}; // segment_stat

struct consolidation_candidate {
  typedef std::set<segment_stat>::const_iterator iterator_t;
  typedef std::pair<iterator_t, iterator_t> range_t;

  explicit consolidation_candidate(iterator_t i) noexcept
    : segments(i, i) {
  }

  iterator_t begin() const noexcept { return segments.first; }
  iterator_t end() const noexcept { return segments.second; }

  const segment_stat& front() const noexcept {
    assert(segments.first != segments.second);
    return *segments.first;
  }

  const segment_stat& back() const noexcept {
    assert(segments.first != segments.second);
    auto end = segments.second;
    return *(--end);
  }

  void reset() noexcept {
    segments = range_t();
    count = 0;
    size = 0;
    score = -1.;
  }

  range_t segments;
  size_t count{ 0 };
  size_t size{ 0 }; // estimated size of the level
  double_t score{ DBL_MIN }; // how good this permutation is
};

/// @returns score of the consolidation bucket
double_t consolidation_score(
    const consolidation_candidate& consolidation,
    const size_t segments_per_tier,
    const size_t floor_segment_bytes
) noexcept {
  // to detect how skewed the consolidation we do the following:
  // 1. evaluate coefficient of variation, less is better
  // 2. good candidates are in range [0;1]
  // 3. favor condidates where number of segments is equal to 'segments_per_tier' approx
  // 4. prefer smaller consolidations
  // 5. prefer consolidations which clean removals

  switch (consolidation.count) {
    case 0:
      // empty consolidation makes not sense
      return DBL_MIN;
    case 1: {
      auto& meta = *consolidation.segments.first->meta;

      if (meta.docs_count == meta.live_docs_count) {
        // singletone without removals makes no sense
        return DBL_MIN;
      }

      // FIXME honor number of deletes???
      // signletone with removals makes sense if nothing better is found
       return DBL_MIN + DBL_EPSILON;
    }
  }

  size_t size_before_consolidation = 0;
  size_t size_after_consolidation = 0;
  size_t size_after_consolidation_floored = 0;
  for (auto& segment_stat : consolidation) {
    size_before_consolidation += segment_stat.meta->size;
    size_after_consolidation += segment_stat.size;
    size_after_consolidation_floored += std::max(segment_stat.size, floor_segment_bytes);
  }

  // evaluate coefficient of variation
  double_t sum_square_differences = 0;
  const auto segment_size_after_consolidaton_mean = double_t(size_after_consolidation_floored) / consolidation.count;
  for (auto& segment_stat : consolidation) {
    const double_t diff = std::max(segment_stat.size, floor_segment_bytes)-segment_size_after_consolidaton_mean;
    sum_square_differences += diff*diff;
  }

  const auto stdev = std::sqrt(sum_square_differences/consolidation.count);
  const auto cv = (stdev / segment_size_after_consolidaton_mean);

  // evaluate initial score
  auto score = 1. - cv;

  // favor consolidations that contain approximately the requested number of segments
  score *= std::pow(consolidation.count/double_t(segments_per_tier), 1.5);

  // FIXME use relative measure, e.g. cosolidation_size/total_size
  // carefully prefer smaller consolidations over the bigger ones
  score /= std::pow(size_after_consolidation, 0.5);

  // favor consolidations which clean out removals
  score /= std::pow(double_t(size_after_consolidation)/size_before_consolidation, 2);

  return score;
}

} // tier
}

namespace iresearch {
namespace index_utils {

index_writer::consolidation_policy_t consolidation_policy(
    const consolidate_bytes& options) {
  return [options](
      std::set<const segment_meta*>& candidates,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& /*consolidating_segments*/
  )->void {
    auto byte_threshold = options.threshold;
    size_t all_segment_bytes_size = 0;
    size_t segment_count = meta.size();

    for (auto& segment : meta) {
      all_segment_bytes_size += segment.meta.size;
    }

    auto threshold = std::max<float>(0, std::min<float>(1, byte_threshold));
    auto threshold_bytes_avg = (all_segment_bytes_size / (float)segment_count) * threshold;

    // merge segment if: {threshold} > segment_bytes / (all_segment_bytes / #segments)
    for (auto& segment: meta) {
      const size_t segment_bytes_size = segment.meta.size;

      if (threshold_bytes_avg >= segment_bytes_size) {
        candidates.insert(&segment.meta);
      }
    }
  };
}

index_writer::consolidation_policy_t consolidation_policy(
    const consolidate_bytes_accum& options) {
  return [options](
      std::set<const segment_meta*>& candidates,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& consolidating_segments
  )->void {
    auto byte_threshold = options.threshold;
    size_t all_segment_bytes_size = 0;
    typedef std::pair<size_t, const segment_meta*> entry_t;
    std::vector<entry_t> segments;

    segments.reserve(meta.size());

    for (auto& segment: meta) {
      if (consolidating_segments.find(&segment.meta) != consolidating_segments.end()) {
        // segment is already under consolidation
        continue;
      }

      segments.emplace_back(size_without_removals(segment.meta), &(segment.meta));
      all_segment_bytes_size += segments.back().first;
    }

    size_t cumulative_size = 0;
    auto threshold_size = all_segment_bytes_size * std::max<float>(0, std::min<float>(1, byte_threshold));
    struct {
      bool operator()(const entry_t& lhs, const entry_t& rhs) const {
        return lhs.first < rhs.first;
      }
    } segments_less;

    std::sort(segments.begin(), segments.end(), segments_less); // prefer to consolidate smaller segments

    // merge segment if: {threshold} >= (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
    for (auto& entry: segments) {
      const auto segment_bytes_size = entry.first;

      if (cumulative_size + segment_bytes_size <= threshold_size) {
        cumulative_size += segment_bytes_size;
        candidates.insert(entry.second);
      }
    }
  };
}

index_writer::consolidation_policy_t consolidation_policy(
    const consolidate_count& options) {
  return [options](
      std::set<const segment_meta*>& candidates,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& /*consolidating_segments*/
   )->void {
    // merge first 'threshold' segments
    for (size_t i = 0, count = std::min(options.threshold, meta.size());
         i < count;
         ++i) {
      candidates.insert(&(meta[i].meta));
    }
  };
}

index_writer::consolidation_policy_t consolidation_policy(
    const consolidate_docs_fill& options) {
  return [options](
      std::set<const segment_meta*>& candidates,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& /*consolidating_segments*/
  )->void {
    auto fill_threshold = options.threshold;
    auto threshold = std::max<float>(0, std::min<float>(1, fill_threshold));

    // merge segment if: {threshold} >= #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})
    for (auto& segment: meta) {
      if (!segment.meta.live_docs_count // if no valid doc_ids left in segment
          || segment.meta.docs_count * threshold >= segment.meta.live_docs_count) {
        candidates.insert(&segment.meta);
      }
    }
  };
}

index_writer::consolidation_policy_t consolidation_policy(
    const consolidate_docs_live& options) {
  return [options](
      std::set<const segment_meta*>& candidates,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& /*consolidating_segments*/
  )->void {
    auto docs_threshold = options.threshold;
    size_t all_segment_docs_count = 0;
    size_t segment_count = meta.size();

    for (auto& segment : meta) {
      all_segment_docs_count += segment.meta.live_docs_count;
    }

    auto threshold = std::max<float>(0, std::min<float>(1, docs_threshold));
    auto threshold_docs_avg = (all_segment_docs_count / (float)segment_count) * threshold;

    // merge segment if: {threshold} >= segment_docs{valid} / (all_segment_docs{valid} / #segments)
    for (auto& segment: meta) {
      if (!segment.meta.live_docs_count // if no valid doc_ids left in segment
          || threshold_docs_avg >= segment.meta.live_docs_count) {
        candidates.insert(&segment.meta);
      }
    }
  };
}

index_writer::consolidation_policy_t consolidation_policy(
    const consolidate_tier& options) {

  // validate input
  const auto max_segments_per_tier = (std::max)(size_t(1), options.max_segments); // can't merge less than 1 segment
  auto min_segments_per_tier = (std::max)(size_t(1), options.min_segments); // can't merge less than 1 segment
  min_segments_per_tier = (std::min)(min_segments_per_tier, max_segments_per_tier); // ensure min_segments_per_tier <= max_segments_per_tier
  const auto max_segments_bytes = (std::max)(size_t(1), options.max_segments_bytes);
  const auto floor_segment_bytes = (std::max)(size_t(1), options.floor_segment_bytes);
  const auto min_score = options.min_score; // skip consolidation that have score less than min_score

  return [max_segments_per_tier, min_segments_per_tier, floor_segment_bytes, max_segments_bytes, min_score](
      std::set<const segment_meta*>& candidates,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& consolidating_segments) -> void {
    size_t consolidating_size = 0; // size of segments in bytes that are currently under consolidation
    size_t min_segment_size = integer_traits<size_t>::const_max; // the smallest segment
    size_t total_index_size = 0; // total size in bytes of all segments in index
    size_t total_docs_count = 0; // total number of documents in index
    size_t total_live_docs_count = 0; // total number of live documents in index

    ///////////////////////////////////////////////////////////////////////////
    /// Stage 0
    /// get sorted list of segments
    ///////////////////////////////////////////////////////////////////////////

    std::set<tier::segment_stat> sorted_segments;

    // get sorted segments from index meta
    auto push_segments = [&sorted_segments](
        const std::string& /*filename*/,
        const irs::segment_meta& segment) {
      if (segment.live_docs_count) {
        // skip empty segments, they'll be
        // removed from index by index_writer
        // during 'commit'
        sorted_segments.insert(segment);
      }

      return true;
    };

    meta.visit_segments(push_segments);

    ///////////////////////////////////////////////////////////////////////////
    /// Stage 1
    /// calculate overall stats
    ///////////////////////////////////////////////////////////////////////////

    for (auto begin = sorted_segments.begin(); begin != sorted_segments.end();)  {
      auto& segment = *begin;

      min_segment_size = std::min(min_segment_size, segment.size);
      total_index_size += segment.size;
      total_live_docs_count += segment.meta->live_docs_count;

      if (consolidating_segments.end() != consolidating_segments.find(segment.meta)) {
        consolidating_size += segment.size;
        total_docs_count += segment.meta->live_docs_count; // exclude removals from stats for consolidating segments
        begin = sorted_segments.erase(begin); // segment is already marked for consolidation, filter it out
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
    /// filter out "too large segments", segment is meant to be treated as large if
    /// - segment size is greater than 'max_segments_bytes / 2'
    /// - segment has many documents but only few deletions
    ///////////////////////////////////////////////////////////////////////////

    const double_t total_fill_factor = double_t(total_live_docs_count) / total_docs_count;
    const size_t too_big_segments_threshold = max_segments_bytes / 2;

    for (auto begin = sorted_segments.begin(); begin != sorted_segments.end();)  {
      auto& segment = *begin;
      const double_t segment_fill_factor = double_t(segment.meta->live_docs_count) / segment.meta->docs_count;
      if (segment.size > too_big_segments_threshold && (total_fill_factor <= segment_fill_factor)) {
        // filter out segments that are too big
        begin = sorted_segments.erase(begin);
      } else {
        ++begin;
      }
    }

    ///////////////////////////////////////////////////////////////////////////
    /// Stage 3
    /// find candidates
    ///////////////////////////////////////////////////////////////////////////

    tier::consolidation_candidate best(sorted_segments.begin());

    if (sorted_segments.size() >= min_segments_per_tier) {
      for (auto i = sorted_segments.begin(), end = sorted_segments.end(); i != end; ++i) {
        tier::consolidation_candidate candidate(i);

        while (candidate.segments.second != end
               && candidate.count < max_segments_per_tier) {
          candidate.size += candidate.segments.second->size;

          if (candidate.size > max_segments_bytes) {
            // overcome the limit
            break;
          }

          ++candidate.count;
          ++candidate.segments.second;

          if (candidate.count < min_segments_per_tier) {
            // not enough segments yet
            continue;
          }

          candidate.score = tier::consolidation_score(
            candidate, max_segments_per_tier, floor_segment_bytes
          );

          if (candidate.score < min_score) {
            // score is too small
            continue;
          }

          if (best.score < candidate.score) {
            best = candidate;
          }
        }
      }
    }

    ///////////////////////////////////////////////////////////////////////////
    /// Stage 4
    /// pick the best candidate
    ///////////////////////////////////////////////////////////////////////////

    candidates.insert(best.begin(), best.end());
  };
}

void read_document_mask(
  iresearch::document_mask& docs_mask,
  const iresearch::directory& dir,
  const iresearch::segment_meta& meta
) {
  if (!segment_reader::has<document_mask_reader>(meta)) {
    return; // nothing to read
  }

  auto reader = meta.codec->get_document_mask_reader();
  reader->read(dir, meta, docs_mask);
}

void flush_index_segment(directory& dir, index_meta::index_segment_t& segment) {
  assert(segment.meta.codec);
  assert(!segment.meta.size); // assume segment size will be calculated in a single place, here

  // estimate meta segment size
  for (auto& filename: segment.meta.files) {
    uint64_t size;

    if (!dir.length(size, filename)) {
      IR_FRMT_WARN("Failed to get length of the file '%s'", filename.c_str());
      continue;
    }

    segment.meta.size += size;
  }

  auto writer = segment.meta.codec->get_segment_meta_writer();

  writer->write(dir, segment.filename, segment.meta);
}

} // index_utils
} // namespace iresearch {
