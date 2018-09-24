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

#include "bitvector.hpp"
#include "formats/format_utils.hpp"
#include "index_utils.hpp"

NS_LOCAL

// FIXME
// - FIX segment_consolidate_clear_commit
// - store segment size in segment_meta

//uint64_t segment_size(
//    const irs::directory& dir,
//    const irs::segment_meta& segment
//) {
//  uint64_t total_size = 0;
//  uint64_t file_size = 0;
//  for (const auto& file : segment.files) {
//    if (dir.length(file_size, file)) {
//      total_size += file_size;
//    }
//  }
//  return total_size;
//}
//
//double fill_factor(const irs::segment_meta& segment) NOEXCEPT {
//  return double(segment.live_docs_count)/segment.docs_count;
//}
//
//std::vector<const irs::segment_meta*> get_sorted_segments(
//    const irs::index_meta& index,
//    const irs::directory& dir
//) {
//  std::vector<const irs::segment_meta*> segments;
//  segments.reserve(index.size());
//
//  // get segments from index meta
//  auto push_segments = [&segments](
//      const std::string& /*filename*/,
//      const irs::segment_meta& segment
//  ) NOEXCEPT { // NOEXCEPT - because we reserved enough space
//    segments.push_back(&segment);
//    return true;
//  };
//
//  index.visit_segments(push_segments);
//
//  // sort segments by size
//  auto less = [&dir](
//      const irs::segment_meta* lhs,
//      const irs::segment_meta* rhs) {
//    const auto lhs_size = segment_size(dir, *lhs);
//    const auto rhs_size = segment_size(dir, *rhs);
//
//    if (lhs_size == rhs_size) {
//      const auto lhs_fill_factor = fill_factor(*lhs);
//      const auto rhs_fill_factor = fill_factor(*rhs);
//
//      if (lhs_fill_factor == rhs_fill_factor) {
//        return lhs->name < rhs->name;
//      }
//
//      return lhs_fill_factor < rhs_fill_factor;
//    }
//
//    return lhs_size < rhs_size;
//  };
//
//  std::sort(segments.begin(), segments.end(), less);
//
//  return segments;
//}

NS_END

NS_ROOT
NS_BEGIN(index_utils)

//index_writer::consolidation_policy_t consolidate_tier(
//    size_t max_segments /*=10*/,                // maximum allowed number of segments to consolidate at once
//    size_t max_segments_bytes /*= 5*(1<<30)*/,  // maxinum allowed size of all consolidated segments
//    size_t floor_segment_bytes /*= 2*(1<<20)*/, // treat all smaller segments as equal for consolidation selection
//    double_t fill_factor /* = 0.7 */            // percentage of live documents in a segment
//) {
//  max_segments = (std::max)(size_t(1), max_segments); // can't merge less than 1 segment
//  fill_factor = (std::max)(0., (std::min)(1., fill_factor));
//
//  return [max_segments, fill_factor, floor_segment_bytes, max_segments_bytes](
//      std::set<const segment_meta*>& candidates,
//      const directory& dir,
//      const index_meta& meta,
//      const index_writer::consolidating_segments_t& consolidating_segments
//  ) ->void {
//    auto sorted_segments = get_sorted_segments(meta, dir);
//  };
//}

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

std::string write_segment_meta(
    directory& dir,
    const segment_meta& meta
) {
  assert(meta.codec);
  auto writer = meta.codec->get_segment_meta_writer();

  writer->write(dir, meta);
  return writer->filename(meta);
}

NS_END // index_utils
NS_END // NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
