//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_INDEX_UTILS_H
#define IRESEARCH_INDEX_UTILS_H

#include "index/index_writer.hpp"

NS_ROOT
NS_BEGIN(index_utils)

// merge segment if: {threshold} > segment_bytes / (all_segment_bytes / #segments)
IRESEARCH_API index_writer::consolidation_policy_t consolidate_bytes(float byte_threshold = 0);

// merge segment if: {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
IRESEARCH_API index_writer::consolidation_policy_t consolidate_bytes_accum(float byte_threshold = 0);

// merge segment if: {threshold} > segment_docs{valid} / (all_segment_docs{valid} / #segments)
IRESEARCH_API index_writer::consolidation_policy_t consolidate_count(float docs_threshold = 0);

// merge segment if: {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})
IRESEARCH_API index_writer::consolidation_policy_t consolidate_fill(float fill_threshold = 0);

void read_document_mask(
  iresearch::document_mask& docs_mask,
  const iresearch::directory& dir,
  const iresearch::segment_meta& meta
);

NS_END
NS_END

#endif
