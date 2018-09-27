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

NS_ROOT
NS_BEGIN(index_utils)

index_writer::consolidation_policy_t consolidate_bytes(float byte_threshold /*= 0*/) {
  return [byte_threshold](
    bitvector& candidates, const directory& dir, const index_meta& meta
  )->void {
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
    size_t i = 0;

    // merge segment if: {threshold} > segment_bytes / (all_segment_bytes / #segments)
    for (auto& segment: meta) {
      size_t segment_bytes_size = 0;
      uint64_t length;

      for (auto& file: segment.meta.files) {
        if (dir.length(length, file)) {
          segment_bytes_size += length;
        }
      }

      if (threshold_bytes_avg > segment_bytes_size) {
        candidates.set(i);
      }

      ++i;
    }
  };
}

index_writer::consolidation_policy_t consolidate_bytes_accum(float byte_threshold /*= 0*/) {
  return [byte_threshold](
    bitvector& candidates, const directory& dir, const index_meta& meta
  )->void {
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
    size_t i = 0;

    // merge segment if: {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
    for (auto& segment: meta) {
      size_t segment_bytes_size = 0;
      uint64_t length;

      for (auto& file: segment.meta.files) {
        if (dir.length(length, file)) {
          segment_bytes_size += length;
        }
      }

      if (cumulative_size + segment_bytes_size < threshold_size) {
        cumulative_size += segment_bytes_size;
        candidates.set(i);
      }

      ++i;
    }
  };
}

index_writer::consolidation_policy_t consolidate_count(float docs_threshold /*= 0*/) {
  return [docs_threshold](
    bitvector& candidates, const directory& dir, const index_meta& meta
  )->void {
    size_t all_segment_docs_count = 0;
    size_t segment_count = meta.size();

    //for (uint32_t i = 0; i < segment_count; ++i) {
    for (auto& segment : meta) {
      auto& segment_meta = segment.meta;
      document_mask docs_mask;

      read_document_mask(docs_mask, dir, segment_meta);
      all_segment_docs_count += segment_meta.docs_count - docs_mask.size();
    }

    auto threshold = std::max<float>(0, std::min<float>(1, docs_threshold));
    auto threshold_docs_avg = (all_segment_docs_count / (float)segment_count) * threshold;
    size_t i = 0;

    // merge segment if: {threshold} > segment_docs{valid} / (all_segment_docs{valid} / #segments)
    for (auto& segment: meta) {
      document_mask docs_mask;

      read_document_mask(docs_mask, dir, segment.meta);

      if (segment.meta.docs_count <= docs_mask.size() // if no valid doc_ids left in segment
          || threshold_docs_avg > (segment.meta.docs_count - docs_mask.size())) {
        candidates.set(i);
      }

      ++i;
    }
  };
}

index_writer::consolidation_policy_t consolidate_fill(float fill_threshold /*= 0*/) {
  return [fill_threshold](
    bitvector& candidates, const directory& dir, const index_meta& meta
  )->void {
    auto threshold = std::max<float>(0, std::min<float>(1, fill_threshold));
    size_t i = 0;

    // merge segment if: {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})
    for (auto& segment: meta) {
      document_mask docs_mask;

      read_document_mask(docs_mask, dir, segment.meta);

      if (segment.meta.docs_count <= docs_mask.size() // if no valid doc_ids left in segment
          || segment.meta.docs_count * threshold > (segment.meta.docs_count - docs_mask.size())) {
        candidates.set(i);
      }

      ++i;
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
  auto visitor = [&docs_mask](const doc_id_t& value)->bool {
    docs_mask.insert(value);
    return true;
  };

  // there will not be a document_mask list for new segments without deletes
  if (reader->prepare(dir, meta)) {
    read_all<iresearch::doc_id_t>(visitor, *reader, reader->begin());
    reader->end();
  }
}

NS_END
NS_END