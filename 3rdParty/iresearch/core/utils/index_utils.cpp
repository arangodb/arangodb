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

NS_LOCAL

// FIXME
// - FIX segment_consolidate_clear_commit

/// @returns percentage of live documents
inline double_t fill_factor(const irs::segment_meta& segment) NOEXCEPT {
  return double(segment.live_docs_count)/segment.docs_count;
}

/// @returns approximated size of a segment in the absence of removals
inline size_t size_without_removals(const irs::segment_meta& segment) NOEXCEPT{
  return size_t(segment.size * fill_factor(segment));
}

struct segment_stat {
  segment_stat(const irs::segment_meta& meta) NOEXCEPT
    : meta(&meta), 
      size(size_without_removals(meta)),
      fill_factor(::fill_factor(meta)) {
  }

  bool operator<(const segment_stat& rhs) const NOEXCEPT {
    auto& lhs = *this;

    if (lhs.size == rhs.size) {
      if (lhs.fill_factor == rhs.fill_factor) {
        return lhs.meta->name < rhs.meta->name;
      }

      return lhs.fill_factor > rhs.fill_factor;
    }

    return lhs.size < rhs.size;
  }

  const irs::segment_meta* meta;
  size_t size; // approximate size of segment without removals
  double_t fill_factor;
}; // segment_stat

struct consolidation_candidate {
  typedef std::set<segment_stat>::const_iterator iterator_t;
  typedef std::pair<iterator_t, iterator_t> range_t;

  consolidation_candidate() = default;

  explicit consolidation_candidate(iterator_t i) NOEXCEPT
    : segments(i, i) {
  }

  iterator_t begin() const NOEXCEPT { return segments.first; }
  iterator_t end() const NOEXCEPT { return segments.second; }

  const segment_stat& front() const NOEXCEPT {
    assert(segments.first != segments.second);
    return *segments.first;
  }

  const segment_stat& back() const NOEXCEPT {
    assert(segments.first != segments.second);
    auto end = segments.second;
    return *(--end);
  }

  void reset() NOEXCEPT {
    segments = range_t();
    count = 0;
    size = 0;
    score = -1.;
  }

  range_t segments;
  size_t count{ 0 };
  size_t size{ 0 }; // estimated size of the level
  double_t score{ -1. }; // how good this permutation is
};

struct consolidation {
  explicit consolidation(
      const consolidation_candidate& candidate
  ) : size(candidate.size),
      score(candidate.score) {
    segments.reserve(candidate.count);
    for (const auto& segment : candidate) {
      segments.emplace_back(segment);
    }
  }

  bool operator<(const consolidation& rhs) const NOEXCEPT {
    if (score < rhs.score) {
      return true;
    } else if (score > rhs.score) {
      return false;
    }

    return segments.size() > segments.size();
  }

  std::vector<segment_stat> segments;
  size_t size{ 0 }; // estimated size of the level
  double_t score{ -1. }; // how good this permutation is
};

//void print_segment(const segment_stat& stat) {
//#ifdef IRESEARCH_DEBUG
//  IR_FRMT_TRACE(
//    "Name='%s', docs_count='" IR_SIZE_T_SPECIFIER "', live_docs_count='" IR_SIZE_T_SPECIFIER "', size='" IR_SIZE_T_SPECIFIER "'",
//    stat.meta->name.c_str(), stat.meta->docs_count, stat.meta->live_docs_count, stat.size
//  );
//#else
//  UNUSED(stat);
//#endif
//}

/// @returns score of the consolidation bucket
double_t consolidation_score(
    const consolidation_candidate& consolidation,
    const size_t segments_per_tier,
    const size_t floor_segment_bytes
) NOEXCEPT {
  switch (consolidation.count) {
    case 0:
      return -1.;
    case 1: {
      auto& meta = *consolidation.segments.first->meta;
      if (meta.docs_count == meta.live_docs_count) {
        // singleton without removals makes no sense
        // note: that is important to return score
        // higher than default value to avoid infinite loop
        return 0.;
      }
    } break;
  }

  size_t size_before_consolidation = 0;
  size_t size_after_consolidation = 0;
  for (auto& segment_stat : consolidation) {
    size_before_consolidation += segment_stat.meta->size;
    size_after_consolidation += segment_stat.size;
  }

  // detect how skewed the consolidation is, we want
  // to consolidate segments of approximately the same size
  const auto first = std::max(consolidation.front().size, floor_segment_bytes);
  const auto last = std::max(consolidation.back().size, floor_segment_bytes);

  auto score = double_t(first) / last;

  // favor consolidations that contain approximately the requested number of segments
  score *= std::pow(consolidation.count/double_t(segments_per_tier), 1.5);

  // carefully prefer smaller consolidations over the bigger ones
  score /= std::pow(size_after_consolidation, 0.05);

  // favor consolidations which clean out removals
  score /= std::pow(double_t(size_after_consolidation)/size_before_consolidation, 2.);

  return score;
}

NS_END

NS_ROOT
NS_BEGIN(index_utils)

index_writer::consolidation_policy_t consolidate_tier(
    size_t min_segments_per_tier /* = 1*/,                     // minumum allowed number of segments to consolidate at once
    size_t max_segments_per_tier /*= 10*/,                     // maximum allowed number of segments to consolidate at once
    size_t max_segments_bytes /*= 5*(1<<30)*/,                 // maxinum allowed size of all consolidated segments
    size_t floor_segment_bytes /*= 2*(1<<20)*/,                // treat all smaller segments as equal for consolidation selection
    size_t lookahead /*= integer_traits<size_t>::const_max */  // how many tiers have to be inspected
) {
  // validate input
  min_segments_per_tier = (std::max)(size_t(1), min_segments_per_tier); // can't merge less than 1 segment
  max_segments_per_tier = (std::max)(size_t(1), max_segments_per_tier); // can't merge less than 1 segment
  min_segments_per_tier = (std::min)(min_segments_per_tier, max_segments_per_tier); // ensure min_segments_per_tier <= max_segments_per_tier
  max_segments_bytes = (std::max)(size_t(1), max_segments_bytes);
  floor_segment_bytes = (std::max)(size_t(1), floor_segment_bytes);
  lookahead = std::max(size_t(1), lookahead);

  return [max_segments_per_tier, min_segments_per_tier, floor_segment_bytes, max_segments_bytes, lookahead](
      std::set<const segment_meta*>& candidates,
      const directory& /*dir*/,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& consolidating_segments
  ) ->void {
    size_t consolidating_size = 0; // size of segments in bytes that are currently under consolidation
    size_t min_segment_size = integer_traits<size_t>::const_max; // the smallest segment
    size_t total_index_size = 0; // total size in bytes of all segments in index
    size_t total_docs_count = 0; // total number of documents in index
    size_t total_live_docs_count = 0; // total number of live documents in index

    ///////////////////////////////////////////////////////////////////////////
    /// Stage 0
    /// get sorted list of segments
    ///////////////////////////////////////////////////////////////////////////

    std::set<segment_stat> sorted_segments;

    // get sorted segments from index meta
    auto push_segments = [&sorted_segments](
        const std::string& /*filename*/,
        const irs::segment_meta& segment
    ) {
      sorted_segments.insert(segment);
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

    std::vector<consolidation> consolidation_candidates;

    for (consolidation_candidate best; sorted_segments.size() >= min_segments_per_tier; best.reset()) {
      for (auto i = sorted_segments.begin(), end = sorted_segments.end(); i != end; ++i) {
        consolidation_candidate candidate(i);

        while (
            candidate.segments.second != end
            && candidate.count < max_segments_per_tier
            && candidate.size < max_segments_bytes
        ) {
          candidate.size += candidate.segments.second->size;
          ++candidate.count;
          ++candidate.segments.second;

          if (candidate.count < min_segments_per_tier) {
            continue;
          }

          candidate.score = ::consolidation_score(
            candidate, max_segments_per_tier, floor_segment_bytes
          );

          if (best.score < candidate.score) {
            best = candidate;
          }
        }
      }

      assert(best.count);

      if (best.count) {
        // remember the best candidate
        consolidation_candidates.emplace_back(best);
        std::push_heap(consolidation_candidates.begin(), consolidation_candidates.end());

        // remove picked segments from the list
        sorted_segments.erase(best.segments.first, best.segments.second);

        if (consolidating_segments.size() >= lookahead) {
          break;
        }
      }
    }

    if (consolidation_candidates.empty()) {
      // nothing ot merge
      return;
    }

    ///////////////////////////////////////////////////////////////////////////
    /// Stage 4
    /// pick the best candidate
    ///////////////////////////////////////////////////////////////////////////

    for (auto& segment : consolidation_candidates.front().segments) {
      candidates.insert(segment.meta);
    }
  };
}

index_writer::consolidation_policy_t consolidate_all() {
  return [](
      std::set<const segment_meta*>& candidates,
      const directory& /*dir*/,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& /*consolidating_segments*/
   ) ->void {
    // merge every segment
    for (auto& segment : meta) {
      candidates.insert(&segment.meta);
    }
  };
}

index_writer::consolidation_policy_t consolidate_bytes(float byte_threshold /*= 0*/) {
  return [byte_threshold](
      std::set<const segment_meta*>& candidates,
      const directory& dir,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& /*consolidating_segments*/
  ) ->void {
    size_t all_segment_bytes_size = 0;
    size_t segment_count = meta.size();
    uint64_t length;

    for (auto& segment : meta) {
      for (auto& file : segment.meta.files) {
        if (dir.length(length, file)) {
          all_segment_bytes_size += length;
        }
      }
    }

    auto threshold = std::max<float>(0, std::min<float>(1, byte_threshold));
    auto threshold_bytes_avg = (all_segment_bytes_size / (float)segment_count) * threshold;

    // merge segment if: {threshold} > segment_bytes / (all_segment_bytes / #segments)
    for (auto& segment: meta) {
      size_t segment_bytes_size = 0;
      uint64_t length;

      for (auto& file: segment.meta.files) {
        if (dir.length(length, file)) {
          segment_bytes_size += length;
        }
      }

      if (threshold_bytes_avg >= segment_bytes_size) {
        candidates.insert(&segment.meta);
      }
    }
  };
}

index_writer::consolidation_policy_t consolidate_bytes_accum(float byte_threshold /*= 0*/) {
  return [byte_threshold](
      std::set<const segment_meta*>& candidates,
      const directory& dir,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& /*consolidating_segments*/
  ) ->void {
    size_t all_segment_bytes_size = 0;
    uint64_t length;

    for (auto& segment: meta) {
      for (auto& file: segment.meta.files) {
        if (dir.length(length, file)) {
          all_segment_bytes_size += length;
        }
      }
    }

    size_t cumulative_size = 0;
    auto threshold_size = all_segment_bytes_size * std::max<float>(0, std::min<float>(1, byte_threshold));

    // merge segment if: {threshold} >= (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
    for (auto& segment: meta) {
      size_t segment_bytes_size = 0;
      uint64_t length;

      for (auto& file: segment.meta.files) {
        if (dir.length(length, file)) {
          segment_bytes_size += length;
        }
      }

      if (cumulative_size + segment_bytes_size <= threshold_size) {
        cumulative_size += segment_bytes_size;
        candidates.insert(&segment.meta);
      }
    }
  };
}

index_writer::consolidation_policy_t consolidate_count(float docs_threshold /*= 0*/) {
  return [docs_threshold](
      std::set<const segment_meta*>& candidates,
      const directory& /*dir*/,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& /*consolidating_segments*/
  )->void {
    size_t all_segment_docs_count = 0;
    size_t segment_count = meta.size();

    //for (uint32_t i = 0; i < segment_count; ++i) {
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

index_writer::consolidation_policy_t consolidate_fill(float fill_threshold /*= 0*/) {
  return [fill_threshold](
      std::set<const segment_meta*>& candidates,
      const directory& /*dir*/,
      const index_meta& meta,
      const index_writer::consolidating_segments_t& /*consolidating_segments*/
  )->void {
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

void write_index_segment(directory& dir, index_meta::index_segment_t& segment) {
  assert(segment.meta.codec);
  assert(!segment.meta.size); // assume segment size will be calculated in a single place, here

  // estimate meta segment size
  for (auto& filename: segment.meta.files) {
    uint64_t size;

    if (dir.length(size, filename)) {
      segment.meta.size += size;
    }
  }

  auto writer = segment.meta.codec->get_segment_meta_writer();

  segment.filename = writer->filename(segment.meta);
  writer->write(dir, segment.meta);
}

NS_END // index_utils
NS_END // NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------