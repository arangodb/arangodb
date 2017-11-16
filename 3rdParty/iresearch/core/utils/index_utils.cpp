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

namespace {
}

NS_ROOT
NS_BEGIN(index_utils)

index_writer::consolidation_policy_t consolidate_bytes(float byte_threshold /*= 0*/) {
  return [byte_threshold](
    const directory& dir, const index_meta& meta
  )->index_writer::consolidation_acceptor_t {
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
    return [&dir, threshold_bytes_avg](const segment_meta& meta)->bool {
      size_t segment_bytes_size = 0;
      uint64_t length;

      for (auto& file: meta.files) {
        if (dir.length(length, file)) {
          segment_bytes_size += length;
        }
      }

      return threshold_bytes_avg > segment_bytes_size;
    };
  };
}

index_writer::consolidation_policy_t consolidate_bytes_accum(float byte_threshold /*= 0*/) {
  return [byte_threshold](
    const directory& dir, const index_meta& meta
  )->index_writer::consolidation_acceptor_t {
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

    // merge segment if: {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
    return std::bind(
      [&dir, threshold_size](const segment_meta& meta, size_t& cumulative_size)->bool {
        size_t segment_bytes_size = 0;
        uint64_t length;

        for (auto& file: meta.files) {
          if (dir.length(length, file)) {
            segment_bytes_size += length;
          }
        }

        if (cumulative_size + segment_bytes_size >= threshold_size) {
          return false;
        }

        cumulative_size += segment_bytes_size;

        return true;
      },
      std::placeholders::_1,
      cumulative_size
    );
  };
}

index_writer::consolidation_policy_t consolidate_count(float docs_threshold /*= 0*/) {
  return [docs_threshold](
    const directory& dir, const index_meta& meta
  )->index_writer::consolidation_acceptor_t {
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

    // merge segment if: {threshold} > segment_docs{valid} / (all_segment_docs{valid} / #segments)
    return [&dir, threshold_docs_avg](const segment_meta& meta)->bool {
      document_mask docs_mask;

      read_document_mask(docs_mask, dir, meta);

      return meta.docs_count <= docs_mask.size() // if no valid doc_ids left in segment
          || threshold_docs_avg > (meta.docs_count - docs_mask.size());
    };
  };
}

index_writer::consolidation_policy_t consolidate_fill(float fill_threshold /*= 0*/) {
  return [fill_threshold](
    const directory& dir, const index_meta& /*meta*/
  )->index_writer::consolidation_acceptor_t {
    auto threshold = std::max<float>(0, std::min<float>(1, fill_threshold));

    // merge segment if: {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})
    return [&dir, threshold](const segment_meta& meta)->bool {
      document_mask docs_mask;

      read_document_mask(docs_mask, dir, meta);

      return meta.docs_count <= docs_mask.size() // if no valid doc_ids left in segment
          || meta.docs_count * threshold > (meta.docs_count - docs_mask.size());
    };
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