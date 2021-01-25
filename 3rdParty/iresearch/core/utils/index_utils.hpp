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

namespace iresearch {
namespace index_utils {

////////////////////////////////////////////////////////////////////////////////
/// @param threshold merge segment if:
///   {threshold} > segment_bytes / (all_segment_bytes / #segments)
////////////////////////////////////////////////////////////////////////////////
struct consolidate_bytes {
  float threshold = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @param threshold merge segment if:
///   {threshold} >= (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
////////////////////////////////////////////////////////////////////////////////
struct consolidate_bytes_accum {
  float threshold = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @param threshold merge first {threshold} segments
////////////////////////////////////////////////////////////////////////////////
struct consolidate_count {
  size_t threshold = integer_traits<size_t>::const_max;
};

////////////////////////////////////////////////////////////////////////////////
/// @param threshold merge segment if:
///   {threshold} >= segment_docs{valid} / (all_segment_docs{valid} / #segments)
////////////////////////////////////////////////////////////////////////////////
struct consolidate_docs_live {
  float threshold = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @param threshold merge segment if:
///   {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})
////////////////////////////////////////////////////////////////////////////////
struct consolidate_docs_fill {
  float threshold = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @param min_segments minimum allowed number of segments to consolidate at once
/// @param max_segments maximum allowed number of segments to consolidate at once
/// @param max_segments_bytes maxinum allowed size of all consolidated segments
/// @param floor_segment_bytes treat all smaller segments as equal for consolidation selection
/// @param lookahead how many tiers have to be inspected
/// @param min_score filter out candidates with score less than min_score
////////////////////////////////////////////////////////////////////////////////
struct consolidate_tier {
  size_t min_segments = 1;
  size_t max_segments = 10;
  size_t max_segments_bytes = size_t(5)*(1<<30);
  size_t floor_segment_bytes = size_t(2)*(1<<20);
  double_t min_score = 0.;
};

////////////////////////////////////////////////////////////////////////////////
/// @return a consolidation policy with the specified options
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API index_writer::consolidation_policy_t consolidation_policy(
  const consolidate_bytes& options
);

////////////////////////////////////////////////////////////////////////////////
/// @return a consolidation policy with the specified options
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API index_writer::consolidation_policy_t consolidation_policy(
  const consolidate_bytes_accum& options
);

////////////////////////////////////////////////////////////////////////////////
/// @return a consolidation policy with the specified options
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API index_writer::consolidation_policy_t consolidation_policy(
  const consolidate_count& options
);

////////////////////////////////////////////////////////////////////////////////
/// @return a consolidation policy with the specified options
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API index_writer::consolidation_policy_t consolidation_policy(
  const consolidate_docs_fill& options
);

////////////////////////////////////////////////////////////////////////////////
/// @return a consolidation policy with the specified options
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API index_writer::consolidation_policy_t consolidation_policy(
  const consolidate_docs_live& options
);

////////////////////////////////////////////////////////////////////////////////
/// @return a consolidation policy with the specified options
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API index_writer::consolidation_policy_t consolidation_policy(
  const consolidate_tier& options
);

void read_document_mask(document_mask& docs_mask, const directory& dir, const segment_meta& meta);

////////////////////////////////////////////////////////////////////////////////
/// @brief writes segment_meta to the supplied directory
///        updates index_meta::index_segment_t::filename to the segment filename
///        updates segment_meta::size to the size of files written
////////////////////////////////////////////////////////////////////////////////
void flush_index_segment(directory& dir, index_meta::index_segment_t& segment);

}
}

#endif
