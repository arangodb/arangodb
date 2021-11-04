//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "table/get_context.h"

#include "db/blob//blob_fetcher.h"
#include "db/merge_helper.h"
#include "db/pinned_iterators_manager.h"
#include "db/read_callback.h"
#include "monitoring/file_read_sample.h"
#include "monitoring/perf_context_imp.h"
#include "monitoring/statistics.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/statistics.h"
#include "rocksdb/system_clock.h"

namespace ROCKSDB_NAMESPACE {

namespace {

void appendToReplayLog(std::string* replay_log, ValueType type, Slice value) {
#ifndef ROCKSDB_LITE
  if (replay_log) {
    if (replay_log->empty()) {
      // Optimization: in the common case of only one operation in the
      // log, we allocate the exact amount of space needed.
      replay_log->reserve(1 + VarintLength(value.size()) + value.size());
    }
    replay_log->push_back(type);
    PutLengthPrefixedSlice(replay_log, value);
  }
#else
  (void)replay_log;
  (void)type;
  (void)value;
#endif  // ROCKSDB_LITE
}

}  // namespace

GetContext::GetContext(const Comparator* ucmp,
                       const MergeOperator* merge_operator, Logger* logger,
                       Statistics* statistics, GetState init_state,
                       const Slice& user_key, PinnableSlice* pinnable_val,
                       std::string* timestamp, bool* value_found,
                       MergeContext* merge_context, bool do_merge,
                       SequenceNumber* _max_covering_tombstone_seq,
                       SystemClock* clock, SequenceNumber* seq,
                       PinnedIteratorsManager* _pinned_iters_mgr,
                       ReadCallback* callback, bool* is_blob_index,
                       uint64_t tracing_get_id, BlobFetcher* blob_fetcher)
    : ucmp_(ucmp),
      merge_operator_(merge_operator),
      logger_(logger),
      statistics_(statistics),
      state_(init_state),
      user_key_(user_key),
      pinnable_val_(pinnable_val),
      timestamp_(timestamp),
      value_found_(value_found),
      merge_context_(merge_context),
      max_covering_tombstone_seq_(_max_covering_tombstone_seq),
      clock_(clock),
      seq_(seq),
      replay_log_(nullptr),
      pinned_iters_mgr_(_pinned_iters_mgr),
      callback_(callback),
      do_merge_(do_merge),
      is_blob_index_(is_blob_index),
      tracing_get_id_(tracing_get_id),
      blob_fetcher_(blob_fetcher) {
  if (seq_) {
    *seq_ = kMaxSequenceNumber;
  }
  sample_ = should_sample_file_read();
}

GetContext::GetContext(
    const Comparator* ucmp, const MergeOperator* merge_operator, Logger* logger,
    Statistics* statistics, GetState init_state, const Slice& user_key,
    PinnableSlice* pinnable_val, bool* value_found, MergeContext* merge_context,
    bool do_merge, SequenceNumber* _max_covering_tombstone_seq,
    SystemClock* clock, SequenceNumber* seq,
    PinnedIteratorsManager* _pinned_iters_mgr, ReadCallback* callback,
    bool* is_blob_index, uint64_t tracing_get_id, BlobFetcher* blob_fetcher)
    : GetContext(ucmp, merge_operator, logger, statistics, init_state, user_key,
                 pinnable_val, nullptr, value_found, merge_context, do_merge,
                 _max_covering_tombstone_seq, clock, seq, _pinned_iters_mgr,
                 callback, is_blob_index, tracing_get_id, blob_fetcher) {}

// Called from TableCache::Get and Table::Get when file/block in which
// key may exist are not there in TableCache/BlockCache respectively. In this
// case we can't guarantee that key does not exist and are not permitted to do
// IO to be certain.Set the status=kFound and value_found=false to let the
// caller know that key may exist but is not there in memory
void GetContext::MarkKeyMayExist() {
  state_ = kFound;
  if (value_found_ != nullptr) {
    *value_found_ = false;
  }
}

void GetContext::SaveValue(const Slice& value, SequenceNumber /*seq*/) {
  assert(state_ == kNotFound);
  appendToReplayLog(replay_log_, kTypeValue, value);

  state_ = kFound;
  if (LIKELY(pinnable_val_ != nullptr)) {
    pinnable_val_->PinSelf(value);
  }
}

void GetContext::ReportCounters() {
  if (get_context_stats_.num_cache_hit > 0) {
    RecordTick(statistics_, BLOCK_CACHE_HIT, get_context_stats_.num_cache_hit);
  }
  if (get_context_stats_.num_cache_index_hit > 0) {
    RecordTick(statistics_, BLOCK_CACHE_INDEX_HIT,
               get_context_stats_.num_cache_index_hit);
  }
  if (get_context_stats_.num_cache_data_hit > 0) {
    RecordTick(statistics_, BLOCK_CACHE_DATA_HIT,
               get_context_stats_.num_cache_data_hit);
  }
  if (get_context_stats_.num_cache_filter_hit > 0) {
    RecordTick(statistics_, BLOCK_CACHE_FILTER_HIT,
               get_context_stats_.num_cache_filter_hit);
  }
  if (get_context_stats_.num_cache_compression_dict_hit > 0) {
    RecordTick(statistics_, BLOCK_CACHE_COMPRESSION_DICT_HIT,
               get_context_stats_.num_cache_compression_dict_hit);
  }
  if (get_context_stats_.num_cache_index_miss > 0) {
    RecordTick(statistics_, BLOCK_CACHE_INDEX_MISS,
               get_context_stats_.num_cache_index_miss);
  }
  if (get_context_stats_.num_cache_filter_miss > 0) {
    RecordTick(statistics_, BLOCK_CACHE_FILTER_MISS,
               get_context_stats_.num_cache_filter_miss);
  }
  if (get_context_stats_.num_cache_data_miss > 0) {
    RecordTick(statistics_, BLOCK_CACHE_DATA_MISS,
               get_context_stats_.num_cache_data_miss);
  }
  if (get_context_stats_.num_cache_compression_dict_miss > 0) {
    RecordTick(statistics_, BLOCK_CACHE_COMPRESSION_DICT_MISS,
               get_context_stats_.num_cache_compression_dict_miss);
  }
  if (get_context_stats_.num_cache_bytes_read > 0) {
    RecordTick(statistics_, BLOCK_CACHE_BYTES_READ,
               get_context_stats_.num_cache_bytes_read);
  }
  if (get_context_stats_.num_cache_miss > 0) {
    RecordTick(statistics_, BLOCK_CACHE_MISS,
               get_context_stats_.num_cache_miss);
  }
  if (get_context_stats_.num_cache_add > 0) {
    RecordTick(statistics_, BLOCK_CACHE_ADD, get_context_stats_.num_cache_add);
  }
  if (get_context_stats_.num_cache_add_redundant > 0) {
    RecordTick(statistics_, BLOCK_CACHE_ADD_REDUNDANT,
               get_context_stats_.num_cache_add_redundant);
  }
  if (get_context_stats_.num_cache_bytes_write > 0) {
    RecordTick(statistics_, BLOCK_CACHE_BYTES_WRITE,
               get_context_stats_.num_cache_bytes_write);
  }
  if (get_context_stats_.num_cache_index_add > 0) {
    RecordTick(statistics_, BLOCK_CACHE_INDEX_ADD,
               get_context_stats_.num_cache_index_add);
  }
  if (get_context_stats_.num_cache_index_add_redundant > 0) {
    RecordTick(statistics_, BLOCK_CACHE_INDEX_ADD_REDUNDANT,
               get_context_stats_.num_cache_index_add_redundant);
  }
  if (get_context_stats_.num_cache_index_bytes_insert > 0) {
    RecordTick(statistics_, BLOCK_CACHE_INDEX_BYTES_INSERT,
               get_context_stats_.num_cache_index_bytes_insert);
  }
  if (get_context_stats_.num_cache_data_add > 0) {
    RecordTick(statistics_, BLOCK_CACHE_DATA_ADD,
               get_context_stats_.num_cache_data_add);
  }
  if (get_context_stats_.num_cache_data_add_redundant > 0) {
    RecordTick(statistics_, BLOCK_CACHE_DATA_ADD_REDUNDANT,
               get_context_stats_.num_cache_data_add_redundant);
  }
  if (get_context_stats_.num_cache_data_bytes_insert > 0) {
    RecordTick(statistics_, BLOCK_CACHE_DATA_BYTES_INSERT,
               get_context_stats_.num_cache_data_bytes_insert);
  }
  if (get_context_stats_.num_cache_filter_add > 0) {
    RecordTick(statistics_, BLOCK_CACHE_FILTER_ADD,
               get_context_stats_.num_cache_filter_add);
  }
  if (get_context_stats_.num_cache_filter_add_redundant > 0) {
    RecordTick(statistics_, BLOCK_CACHE_FILTER_ADD_REDUNDANT,
               get_context_stats_.num_cache_filter_add_redundant);
  }
  if (get_context_stats_.num_cache_filter_bytes_insert > 0) {
    RecordTick(statistics_, BLOCK_CACHE_FILTER_BYTES_INSERT,
               get_context_stats_.num_cache_filter_bytes_insert);
  }
  if (get_context_stats_.num_cache_compression_dict_add > 0) {
    RecordTick(statistics_, BLOCK_CACHE_COMPRESSION_DICT_ADD,
               get_context_stats_.num_cache_compression_dict_add);
  }
  if (get_context_stats_.num_cache_compression_dict_add_redundant > 0) {
    RecordTick(statistics_, BLOCK_CACHE_COMPRESSION_DICT_ADD_REDUNDANT,
               get_context_stats_.num_cache_compression_dict_add_redundant);
  }
  if (get_context_stats_.num_cache_compression_dict_bytes_insert > 0) {
    RecordTick(statistics_, BLOCK_CACHE_COMPRESSION_DICT_BYTES_INSERT,
               get_context_stats_.num_cache_compression_dict_bytes_insert);
  }
}

bool GetContext::SaveValue(const ParsedInternalKey& parsed_key,
                           const Slice& value, bool* matched,
                           Cleanable* value_pinner) {
  assert(matched);
  assert((state_ != kMerge && parsed_key.type != kTypeMerge) ||
         merge_context_ != nullptr);
  if (ucmp_->EqualWithoutTimestamp(parsed_key.user_key, user_key_)) {
    *matched = true;
    // If the value is not in the snapshot, skip it
    if (!CheckCallback(parsed_key.sequence)) {
      return true;  // to continue to the next seq
    }

    appendToReplayLog(replay_log_, parsed_key.type, value);

    if (seq_ != nullptr) {
      // Set the sequence number if it is uninitialized
      if (*seq_ == kMaxSequenceNumber) {
        *seq_ = parsed_key.sequence;
      }
    }

    auto type = parsed_key.type;
    // Key matches. Process it
    if ((type == kTypeValue || type == kTypeMerge || type == kTypeBlobIndex) &&
        max_covering_tombstone_seq_ != nullptr &&
        *max_covering_tombstone_seq_ > parsed_key.sequence) {
      type = kTypeRangeDeletion;
    }
    switch (type) {
      case kTypeValue:
      case kTypeBlobIndex:
        assert(state_ == kNotFound || state_ == kMerge);
        if (type == kTypeBlobIndex && is_blob_index_ == nullptr) {
          // Blob value not supported. Stop.
          state_ = kUnexpectedBlobIndex;
          return false;
        }
        if (is_blob_index_ != nullptr) {
          *is_blob_index_ = (type == kTypeBlobIndex);
        }
        if (kNotFound == state_) {
          state_ = kFound;
          if (do_merge_) {
            if (LIKELY(pinnable_val_ != nullptr)) {
              if (LIKELY(value_pinner != nullptr)) {
                // If the backing resources for the value are provided, pin them
                pinnable_val_->PinSlice(value, value_pinner);
              } else {
                TEST_SYNC_POINT_CALLBACK("GetContext::SaveValue::PinSelf",
                                         this);
                // Otherwise copy the value
                pinnable_val_->PinSelf(value);
              }
            }
          } else {
            // It means this function is called as part of DB GetMergeOperands
            // API and the current value should be part of
            // merge_context_->operand_list
            if (is_blob_index_ != nullptr && *is_blob_index_) {
              PinnableSlice pin_val;
              if (GetBlobValue(value, &pin_val) == false) {
                return false;
              }
              Slice blob_value(pin_val);
              push_operand(blob_value, nullptr);
            } else {
              push_operand(value, value_pinner);
            }
          }
        } else if (kMerge == state_) {
          assert(merge_operator_ != nullptr);
          if (is_blob_index_ != nullptr && *is_blob_index_) {
            PinnableSlice pin_val;
            if (GetBlobValue(value, &pin_val) == false) {
              return false;
            }
            Slice blob_value(pin_val);
            state_ = kFound;
            if (do_merge_) {
              Merge(&blob_value);
            } else {
              // It means this function is called as part of DB GetMergeOperands
              // API and the current value should be part of
              // merge_context_->operand_list
              push_operand(blob_value, nullptr);
            }
          } else {
            state_ = kFound;
            if (do_merge_) {
              Merge(&value);
            } else {
              // It means this function is called as part of DB GetMergeOperands
              // API and the current value should be part of
              // merge_context_->operand_list
              push_operand(value, value_pinner);
            }
          }
        }
        if (state_ == kFound) {
          size_t ts_sz = ucmp_->timestamp_size();
          if (ts_sz > 0 && timestamp_ != nullptr) {
            Slice ts = ExtractTimestampFromUserKey(parsed_key.user_key, ts_sz);
            timestamp_->assign(ts.data(), ts.size());
          }
        }
        return false;

      case kTypeDeletion:
      case kTypeDeletionWithTimestamp:
      case kTypeSingleDeletion:
      case kTypeRangeDeletion:
        // TODO(noetzli): Verify correctness once merge of single-deletes
        // is supported
        assert(state_ == kNotFound || state_ == kMerge);
        if (kNotFound == state_) {
          state_ = kDeleted;
        } else if (kMerge == state_) {
          state_ = kFound;
          Merge(nullptr);
          // If do_merge_ = false then the current value shouldn't be part of
          // merge_context_->operand_list
        }
        return false;

      case kTypeMerge:
        assert(state_ == kNotFound || state_ == kMerge);
        state_ = kMerge;
        // value_pinner is not set from plain_table_reader.cc for example.
        push_operand(value, value_pinner);
        if (do_merge_ && merge_operator_ != nullptr &&
            merge_operator_->ShouldMerge(
                merge_context_->GetOperandsDirectionBackward())) {
          state_ = kFound;
          Merge(nullptr);
          return false;
        }
        return true;

      default:
        assert(false);
        break;
    }
  }

  // state_ could be Corrupt, merge or notfound
  return false;
}

void GetContext::Merge(const Slice* value) {
  if (LIKELY(pinnable_val_ != nullptr)) {
    if (do_merge_) {
      Status merge_status = MergeHelper::TimedFullMerge(
          merge_operator_, user_key_, value, merge_context_->GetOperands(),
          pinnable_val_->GetSelf(), logger_, statistics_, clock_);
      pinnable_val_->PinSelf();
      if (!merge_status.ok()) {
        state_ = kCorrupt;
      }
    }
  }
}

bool GetContext::GetBlobValue(const Slice& blob_index,
                              PinnableSlice* blob_value) {
  Status status = blob_fetcher_->FetchBlob(user_key_, blob_index, blob_value);
  if (!status.ok()) {
    if (status.IsIncomplete()) {
      MarkKeyMayExist();
      return false;
    }
    state_ = kCorrupt;
    return false;
  }
  *is_blob_index_ = false;
  return true;
}

void GetContext::push_operand(const Slice& value, Cleanable* value_pinner) {
  if (pinned_iters_mgr() && pinned_iters_mgr()->PinningEnabled() &&
      value_pinner != nullptr) {
    value_pinner->DelegateCleanupsTo(pinned_iters_mgr());
    merge_context_->PushOperand(value, true /*value_pinned*/);
  } else {
    merge_context_->PushOperand(value, false);
  }
}

void replayGetContextLog(const Slice& replay_log, const Slice& user_key,
                         GetContext* get_context, Cleanable* value_pinner) {
#ifndef ROCKSDB_LITE
  Slice s = replay_log;
  while (s.size()) {
    auto type = static_cast<ValueType>(*s.data());
    s.remove_prefix(1);
    Slice value;
    bool ret = GetLengthPrefixedSlice(&s, &value);
    assert(ret);
    (void)ret;

    bool dont_care __attribute__((__unused__));
    // Since SequenceNumber is not stored and unknown, we will use
    // kMaxSequenceNumber.
    get_context->SaveValue(
        ParsedInternalKey(user_key, kMaxSequenceNumber, type), value,
        &dont_care, value_pinner);
  }
#else   // ROCKSDB_LITE
  (void)replay_log;
  (void)user_key;
  (void)get_context;
  (void)value_pinner;
  assert(false);
#endif  // ROCKSDB_LITE
}

}  // namespace ROCKSDB_NAMESPACE
