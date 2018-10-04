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

#ifndef IRESEARCH_INDEX_UTILS_H
#define IRESEARCH_INDEX_UTILS_H

#include "index/index_writer.hpp"

NS_ROOT
NS_BEGIN(index_utils)

IRESEARCH_API index_writer::consolidation_policy_t consolidate_all();

// merge segment if: {threshold} > segment_bytes / (all_segment_bytes / #segments)
IRESEARCH_API index_writer::consolidation_policy_t consolidate_bytes(float byte_threshold = 0);

// merge segment if: {threshold} >= (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
IRESEARCH_API index_writer::consolidation_policy_t consolidate_bytes_accum(float byte_threshold = 0);

// merge segment if: {threshold} >= segment_docs{valid} / (all_segment_docs{valid} / #segments)
IRESEARCH_API index_writer::consolidation_policy_t consolidate_count(float docs_threshold = 0);

// merge segment if: {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})
IRESEARCH_API index_writer::consolidation_policy_t consolidate_fill(float fill_threshold = 0);

IRESEARCH_API index_writer::consolidation_policy_t consolidate_tier(
  size_t min_segments = 1,                              // minimum allowed number of segments to consolidate at once
  size_t max_segments = 10,                             // maximum allowed number of segments to consolidate at once
  size_t max_segments_bytes = size_t(5)*(1<<30),        // maxinum allowed size of all consolidated segments
  size_t floor_segment_bytes = size_t(2)*(1<<20),       // treat all smaller segments as equal for consolidation selection
  size_t lookahead = integer_traits<size_t>::const_max  // how many tiers have to be inspected
);

void read_document_mask(document_mask& docs_mask, const directory& dir, const segment_meta& meta);

////////////////////////////////////////////////////////////////////////////////
/// @brief writes segment_meta to the supplied directory
///        updates index_meta::index_segment_t::filename to the segment filename
///        updates segment_meta::size to the size of files written
////////////////////////////////////////////////////////////////////////////////
void write_index_segment(directory& dir, index_meta::index_segment_t& segment);

NS_END
NS_END

#endif
