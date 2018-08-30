//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/index_builder.h"
#include <assert.h>
#include <inttypes.h>

#include <list>
#include <string>

#include "rocksdb/comparator.h"
#include "rocksdb/flush_block_policy.h"
#include "table/format.h"
#include "table/partitioned_filter_block.h"

// Without anonymous namespace here, we fail the warning -Wmissing-prototypes
namespace rocksdb {
// using namespace rocksdb;
// Create a index builder based on its type.
IndexBuilder* IndexBuilder::CreateIndexBuilder(
    BlockBasedTableOptions::IndexType index_type,
    const InternalKeyComparator* comparator,
    const InternalKeySliceTransform* int_key_slice_transform,
    const BlockBasedTableOptions& table_opt) {
  IndexBuilder* result = nullptr;
  switch (index_type) {
    case BlockBasedTableOptions::kBinarySearch: {
      result = new ShortenedIndexBuilder(comparator,
                                         table_opt.index_block_restart_interval,
                                         table_opt.format_version);
    }
  break;
    case BlockBasedTableOptions::kHashSearch: {
      result = new HashIndexBuilder(comparator, int_key_slice_transform,
                                    table_opt.index_block_restart_interval,
                                    table_opt.format_version);
    }
  break;
    case BlockBasedTableOptions::kTwoLevelIndexSearch: {
      result = PartitionedIndexBuilder::CreateIndexBuilder(comparator, table_opt);
    }
    break;
    default: {
      assert(!"Do not recognize the index type ");
    }
  break;
  }
  return result;
}

PartitionedIndexBuilder* PartitionedIndexBuilder::CreateIndexBuilder(
    const InternalKeyComparator* comparator,
    const BlockBasedTableOptions& table_opt) {
  return new PartitionedIndexBuilder(comparator, table_opt);
}

PartitionedIndexBuilder::PartitionedIndexBuilder(
    const InternalKeyComparator* comparator,
    const BlockBasedTableOptions& table_opt)
    : IndexBuilder(comparator),
      index_block_builder_(table_opt.index_block_restart_interval,
                           table_opt.format_version),
      index_block_builder_without_seq_(table_opt.index_block_restart_interval,
                                       table_opt.format_version),
      sub_index_builder_(nullptr),
      table_opt_(table_opt),
      seperator_is_key_plus_seq_(false) {}

PartitionedIndexBuilder::~PartitionedIndexBuilder() {
  delete sub_index_builder_;
}

void PartitionedIndexBuilder::MakeNewSubIndexBuilder() {
  assert(sub_index_builder_ == nullptr);
  sub_index_builder_ = new ShortenedIndexBuilder(
      comparator_, table_opt_.index_block_restart_interval,
      table_opt_.format_version);
  flush_policy_.reset(FlushBlockBySizePolicyFactory::NewFlushBlockPolicy(
      table_opt_.metadata_block_size, table_opt_.block_size_deviation,
      sub_index_builder_->index_block_builder_));
  partition_cut_requested_ = false;
}

void PartitionedIndexBuilder::RequestPartitionCut() {
  partition_cut_requested_ = true;
}

void PartitionedIndexBuilder::AddIndexEntry(
    std::string* last_key_in_current_block,
    const Slice* first_key_in_next_block, const BlockHandle& block_handle) {
  // Note: to avoid two consecuitive flush in the same method call, we do not
  // check flush policy when adding the last key
  if (UNLIKELY(first_key_in_next_block == nullptr)) {  // no more keys
    if (sub_index_builder_ == nullptr) {
      MakeNewSubIndexBuilder();
    }
    sub_index_builder_->AddIndexEntry(last_key_in_current_block,
                                      first_key_in_next_block, block_handle);
    if (sub_index_builder_->seperator_is_key_plus_seq_) {
      // then we need to apply it to all sub-index builders
      seperator_is_key_plus_seq_ = true;
    }
    sub_index_last_key_ = std::string(*last_key_in_current_block);
    entries_.push_back(
        {sub_index_last_key_,
         std::unique_ptr<ShortenedIndexBuilder>(sub_index_builder_)});
    sub_index_builder_ = nullptr;
    cut_filter_block = true;
  } else {
    // apply flush policy only to non-empty sub_index_builder_
    if (sub_index_builder_ != nullptr) {
      std::string handle_encoding;
      block_handle.EncodeTo(&handle_encoding);
      bool do_flush =
          partition_cut_requested_ ||
          flush_policy_->Update(*last_key_in_current_block, handle_encoding);
      if (do_flush) {
        entries_.push_back(
            {sub_index_last_key_,
             std::unique_ptr<ShortenedIndexBuilder>(sub_index_builder_)});
        cut_filter_block = true;
        sub_index_builder_ = nullptr;
      }
    }
    if (sub_index_builder_ == nullptr) {
      MakeNewSubIndexBuilder();
    }
    sub_index_builder_->AddIndexEntry(last_key_in_current_block,
                                      first_key_in_next_block, block_handle);
    sub_index_last_key_ = std::string(*last_key_in_current_block);
    if (sub_index_builder_->seperator_is_key_plus_seq_) {
      // then we need to apply it to all sub-index builders
      seperator_is_key_plus_seq_ = true;
    }
  }
}

Status PartitionedIndexBuilder::Finish(
    IndexBlocks* index_blocks, const BlockHandle& last_partition_block_handle) {
  // It must be set to null after last key is added
  assert(sub_index_builder_ == nullptr);
  if (finishing_indexes == true) {
    Entry& last_entry = entries_.front();
    std::string handle_encoding;
    last_partition_block_handle.EncodeTo(&handle_encoding);
    index_block_builder_.Add(last_entry.key, handle_encoding);
    if (!seperator_is_key_plus_seq_) {
      index_block_builder_without_seq_.Add(ExtractUserKey(last_entry.key),
                                           handle_encoding);
    }
    entries_.pop_front();
  }
  // If there is no sub_index left, then return the 2nd level index.
  if (UNLIKELY(entries_.empty())) {
    if (seperator_is_key_plus_seq_) {
      index_blocks->index_block_contents = index_block_builder_.Finish();
    } else {
      index_blocks->index_block_contents =
          index_block_builder_without_seq_.Finish();
    }
    return Status::OK();
  } else {
    // Finish the next partition index in line and Incomplete() to indicate we
    // expect more calls to Finish
    Entry& entry = entries_.front();
    // Apply the policy to all sub-indexes
    entry.value->seperator_is_key_plus_seq_ = seperator_is_key_plus_seq_;
    auto s = entry.value->Finish(index_blocks);
    finishing_indexes = true;
    return s.ok() ? Status::Incomplete() : s;
  }
}

// Estimate size excluding the top-level index
// It is assumed that this method is called before writing index partition
// starts
size_t PartitionedIndexBuilder::EstimatedSize() const {
  size_t total = 0;
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    total += it->value->EstimatedSize();
  }
  total +=
      sub_index_builder_ == nullptr ? 0 : sub_index_builder_->EstimatedSize();
  return total;
}

// Since when this method is called we do not know the index block offsets yet,
// the top-level index does not exist. Hence we estimate the block offsets and
// create a temporary top-level index.
size_t PartitionedIndexBuilder::EstimateTopLevelIndexSize(
    uint64_t offset) const {
  BlockBuilder tmp_builder(
      table_opt_.index_block_restart_interval);  // tmp top-level index builder
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    std::string tmp_handle_encoding;
    uint64_t size = it->value->EstimatedSize();
    BlockHandle tmp_block_handle(offset, size);
    tmp_block_handle.EncodeTo(&tmp_handle_encoding);
    tmp_builder.Add(
        seperator_is_key_plus_seq_ ? it->key : ExtractUserKey(it->key),
        tmp_handle_encoding);
    offset += size;
  }
  return tmp_builder.CurrentSizeEstimate();
}

size_t PartitionedIndexBuilder::NumPartitions() const {
  return entries_.size();
}
}  // namespace rocksdb
