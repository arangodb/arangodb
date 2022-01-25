//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/memtable.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>

#include "db/dbformat.h"
#include "db/kv_checksum.h"
#include "db/merge_context.h"
#include "db/merge_helper.h"
#include "db/pinned_iterators_manager.h"
#include "db/range_tombstone_fragmenter.h"
#include "db/read_callback.h"
#include "logging/logging.h"
#include "memory/arena.h"
#include "memory/memory_usage.h"
#include "monitoring/perf_context_imp.h"
#include "monitoring/statistics.h"
#include "port/lang.h"
#include "port/port.h"
#include "rocksdb/comparator.h"
#include "rocksdb/env.h"
#include "rocksdb/iterator.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/write_buffer_manager.h"
#include "table/internal_iterator.h"
#include "table/iterator_wrapper.h"
#include "table/merging_iterator.h"
#include "util/autovector.h"
#include "util/coding.h"
#include "util/mutexlock.h"

namespace ROCKSDB_NAMESPACE {

ImmutableMemTableOptions::ImmutableMemTableOptions(
    const ImmutableOptions& ioptions,
    const MutableCFOptions& mutable_cf_options)
    : arena_block_size(mutable_cf_options.arena_block_size),
      memtable_prefix_bloom_bits(
          static_cast<uint32_t>(
              static_cast<double>(mutable_cf_options.write_buffer_size) *
              mutable_cf_options.memtable_prefix_bloom_size_ratio) *
          8u),
      memtable_huge_page_size(mutable_cf_options.memtable_huge_page_size),
      memtable_whole_key_filtering(
          mutable_cf_options.memtable_whole_key_filtering),
      inplace_update_support(ioptions.inplace_update_support),
      inplace_update_num_locks(mutable_cf_options.inplace_update_num_locks),
      inplace_callback(ioptions.inplace_callback),
      max_successive_merges(mutable_cf_options.max_successive_merges),
      statistics(ioptions.stats),
      merge_operator(ioptions.merge_operator.get()),
      info_log(ioptions.logger),
      allow_data_in_errors(ioptions.allow_data_in_errors) {}

MemTable::MemTable(const InternalKeyComparator& cmp,
                   const ImmutableOptions& ioptions,
                   const MutableCFOptions& mutable_cf_options,
                   WriteBufferManager* write_buffer_manager,
                   SequenceNumber latest_seq, uint32_t column_family_id)
    : comparator_(cmp),
      moptions_(ioptions, mutable_cf_options),
      refs_(0),
      kArenaBlockSize(OptimizeBlockSize(moptions_.arena_block_size)),
      mem_tracker_(write_buffer_manager),
      arena_(moptions_.arena_block_size,
             (write_buffer_manager != nullptr &&
              (write_buffer_manager->enabled() ||
               write_buffer_manager->cost_to_cache()))
                 ? &mem_tracker_
                 : nullptr,
             mutable_cf_options.memtable_huge_page_size),
      table_(ioptions.memtable_factory->CreateMemTableRep(
          comparator_, &arena_, mutable_cf_options.prefix_extractor.get(),
          ioptions.logger, column_family_id)),
      range_del_table_(SkipListFactory().CreateMemTableRep(
          comparator_, &arena_, nullptr /* transform */, ioptions.logger,
          column_family_id)),
      is_range_del_table_empty_(true),
      data_size_(0),
      num_entries_(0),
      num_deletes_(0),
      write_buffer_size_(mutable_cf_options.write_buffer_size),
      flush_in_progress_(false),
      flush_completed_(false),
      file_number_(0),
      first_seqno_(0),
      earliest_seqno_(latest_seq),
      creation_seq_(latest_seq),
      mem_next_logfile_number_(0),
      min_prep_log_referenced_(0),
      locks_(moptions_.inplace_update_support
                 ? moptions_.inplace_update_num_locks
                 : 0),
      prefix_extractor_(mutable_cf_options.prefix_extractor.get()),
      flush_state_(FLUSH_NOT_REQUESTED),
      clock_(ioptions.clock),
      insert_with_hint_prefix_extractor_(
          ioptions.memtable_insert_with_hint_prefix_extractor.get()),
      oldest_key_time_(std::numeric_limits<uint64_t>::max()),
      atomic_flush_seqno_(kMaxSequenceNumber),
      approximate_memory_usage_(0) {
  UpdateFlushState();
  // something went wrong if we need to flush before inserting anything
  assert(!ShouldScheduleFlush());

  // use bloom_filter_ for both whole key and prefix bloom filter
  if ((prefix_extractor_ || moptions_.memtable_whole_key_filtering) &&
      moptions_.memtable_prefix_bloom_bits > 0) {
    bloom_filter_.reset(
        new DynamicBloom(&arena_, moptions_.memtable_prefix_bloom_bits,
                         6 /* hard coded 6 probes */,
                         moptions_.memtable_huge_page_size, ioptions.logger));
  }
}

MemTable::~MemTable() {
  mem_tracker_.FreeMem();
  assert(refs_ == 0);
}

size_t MemTable::ApproximateMemoryUsage() {
  autovector<size_t> usages = {
      arena_.ApproximateMemoryUsage(), table_->ApproximateMemoryUsage(),
      range_del_table_->ApproximateMemoryUsage(),
      ROCKSDB_NAMESPACE::ApproximateMemoryUsage(insert_hints_)};
  size_t total_usage = 0;
  for (size_t usage : usages) {
    // If usage + total_usage >= kMaxSizet, return kMaxSizet.
    // the following variation is to avoid numeric overflow.
    if (usage >= port::kMaxSizet - total_usage) {
      return port::kMaxSizet;
    }
    total_usage += usage;
  }
  approximate_memory_usage_.store(total_usage, std::memory_order_relaxed);
  // otherwise, return the actual usage
  return total_usage;
}

bool MemTable::ShouldFlushNow() {
  size_t write_buffer_size = write_buffer_size_.load(std::memory_order_relaxed);
  // In a lot of times, we cannot allocate arena blocks that exactly matches the
  // buffer size. Thus we have to decide if we should over-allocate or
  // under-allocate.
  // This constant variable can be interpreted as: if we still have more than
  // "kAllowOverAllocationRatio * kArenaBlockSize" space left, we'd try to over
  // allocate one more block.
  const double kAllowOverAllocationRatio = 0.6;

  // If arena still have room for new block allocation, we can safely say it
  // shouldn't flush.
  auto allocated_memory = table_->ApproximateMemoryUsage() +
                          range_del_table_->ApproximateMemoryUsage() +
                          arena_.MemoryAllocatedBytes();

  approximate_memory_usage_.store(allocated_memory, std::memory_order_relaxed);

  // if we can still allocate one more block without exceeding the
  // over-allocation ratio, then we should not flush.
  if (allocated_memory + kArenaBlockSize <
      write_buffer_size + kArenaBlockSize * kAllowOverAllocationRatio) {
    return false;
  }

  // if user keeps adding entries that exceeds write_buffer_size, we need to
  // flush earlier even though we still have much available memory left.
  if (allocated_memory >
      write_buffer_size + kArenaBlockSize * kAllowOverAllocationRatio) {
    return true;
  }

  // In this code path, Arena has already allocated its "last block", which
  // means the total allocatedmemory size is either:
  //  (1) "moderately" over allocated the memory (no more than `0.6 * arena
  // block size`. Or,
  //  (2) the allocated memory is less than write buffer size, but we'll stop
  // here since if we allocate a new arena block, we'll over allocate too much
  // more (half of the arena block size) memory.
  //
  // In either case, to avoid over-allocate, the last block will stop allocation
  // when its usage reaches a certain ratio, which we carefully choose "0.75
  // full" as the stop condition because it addresses the following issue with
  // great simplicity: What if the next inserted entry's size is
  // bigger than AllocatedAndUnused()?
  //
  // The answer is: if the entry size is also bigger than 0.25 *
  // kArenaBlockSize, a dedicated block will be allocated for it; otherwise
  // arena will anyway skip the AllocatedAndUnused() and allocate a new, empty
  // and regular block. In either case, we *overly* over-allocated.
  //
  // Therefore, setting the last block to be at most "0.75 full" avoids both
  // cases.
  //
  // NOTE: the average percentage of waste space of this approach can be counted
  // as: "arena block size * 0.25 / write buffer size". User who specify a small
  // write buffer size and/or big arena block size may suffer.
  return arena_.AllocatedAndUnused() < kArenaBlockSize / 4;
}

void MemTable::UpdateFlushState() {
  auto state = flush_state_.load(std::memory_order_relaxed);
  if (state == FLUSH_NOT_REQUESTED && ShouldFlushNow()) {
    // ignore CAS failure, because that means somebody else requested
    // a flush
    flush_state_.compare_exchange_strong(state, FLUSH_REQUESTED,
                                         std::memory_order_relaxed,
                                         std::memory_order_relaxed);
  }
}

void MemTable::UpdateOldestKeyTime() {
  uint64_t oldest_key_time = oldest_key_time_.load(std::memory_order_relaxed);
  if (oldest_key_time == std::numeric_limits<uint64_t>::max()) {
    int64_t current_time = 0;
    auto s = clock_->GetCurrentTime(&current_time);
    if (s.ok()) {
      assert(current_time >= 0);
      // If fail, the timestamp is already set.
      oldest_key_time_.compare_exchange_strong(
          oldest_key_time, static_cast<uint64_t>(current_time),
          std::memory_order_relaxed, std::memory_order_relaxed);
    }
  }
}

int MemTable::KeyComparator::operator()(const char* prefix_len_key1,
                                        const char* prefix_len_key2) const {
  // Internal keys are encoded as length-prefixed strings.
  Slice k1 = GetLengthPrefixedSlice(prefix_len_key1);
  Slice k2 = GetLengthPrefixedSlice(prefix_len_key2);
  return comparator.CompareKeySeq(k1, k2);
}

int MemTable::KeyComparator::operator()(const char* prefix_len_key,
                                        const KeyComparator::DecodedType& key)
    const {
  // Internal keys are encoded as length-prefixed strings.
  Slice a = GetLengthPrefixedSlice(prefix_len_key);
  return comparator.CompareKeySeq(a, key);
}

void MemTableRep::InsertConcurrently(KeyHandle /*handle*/) {
#ifndef ROCKSDB_LITE
  throw std::runtime_error("concurrent insert not supported");
#else
  abort();
#endif
}

Slice MemTableRep::UserKey(const char* key) const {
  Slice slice = GetLengthPrefixedSlice(key);
  return Slice(slice.data(), slice.size() - 8);
}

KeyHandle MemTableRep::Allocate(const size_t len, char** buf) {
  *buf = allocator_->Allocate(len);
  return static_cast<KeyHandle>(*buf);
}

// Encode a suitable internal key target for "target" and return it.
// Uses *scratch as scratch space, and the returned pointer will point
// into this scratch space.
const char* EncodeKey(std::string* scratch, const Slice& target) {
  scratch->clear();
  PutVarint32(scratch, static_cast<uint32_t>(target.size()));
  scratch->append(target.data(), target.size());
  return scratch->data();
}

class MemTableIterator : public InternalIterator {
 public:
  MemTableIterator(const MemTable& mem, const ReadOptions& read_options,
                   Arena* arena, bool use_range_del_table = false)
      : bloom_(nullptr),
        prefix_extractor_(mem.prefix_extractor_),
        comparator_(mem.comparator_),
        valid_(false),
        arena_mode_(arena != nullptr),
        value_pinned_(
            !mem.GetImmutableMemTableOptions()->inplace_update_support) {
    if (use_range_del_table) {
      iter_ = mem.range_del_table_->GetIterator(arena);
    } else if (prefix_extractor_ != nullptr && !read_options.total_order_seek &&
               !read_options.auto_prefix_mode) {
      // Auto prefix mode is not implemented in memtable yet.
      bloom_ = mem.bloom_filter_.get();
      iter_ = mem.table_->GetDynamicPrefixIterator(arena);
    } else {
      iter_ = mem.table_->GetIterator(arena);
    }
  }
  // No copying allowed
  MemTableIterator(const MemTableIterator&) = delete;
  void operator=(const MemTableIterator&) = delete;

  ~MemTableIterator() override {
#ifndef NDEBUG
    // Assert that the MemTableIterator is never deleted while
    // Pinning is Enabled.
    assert(!pinned_iters_mgr_ || !pinned_iters_mgr_->PinningEnabled());
#endif
    if (arena_mode_) {
      iter_->~Iterator();
    } else {
      delete iter_;
    }
  }

#ifndef NDEBUG
  void SetPinnedItersMgr(PinnedIteratorsManager* pinned_iters_mgr) override {
    pinned_iters_mgr_ = pinned_iters_mgr;
  }
  PinnedIteratorsManager* pinned_iters_mgr_ = nullptr;
#endif

  bool Valid() const override { return valid_; }
  void Seek(const Slice& k) override {
    PERF_TIMER_GUARD(seek_on_memtable_time);
    PERF_COUNTER_ADD(seek_on_memtable_count, 1);
    if (bloom_) {
      // iterator should only use prefix bloom filter
      auto ts_sz = comparator_.comparator.user_comparator()->timestamp_size();
      Slice user_k_without_ts(ExtractUserKeyAndStripTimestamp(k, ts_sz));
      if (prefix_extractor_->InDomain(user_k_without_ts) &&
          !bloom_->MayContain(
              prefix_extractor_->Transform(user_k_without_ts))) {
        PERF_COUNTER_ADD(bloom_memtable_miss_count, 1);
        valid_ = false;
        return;
      } else {
        PERF_COUNTER_ADD(bloom_memtable_hit_count, 1);
      }
    }
    iter_->Seek(k, nullptr);
    valid_ = iter_->Valid();
  }
  void SeekForPrev(const Slice& k) override {
    PERF_TIMER_GUARD(seek_on_memtable_time);
    PERF_COUNTER_ADD(seek_on_memtable_count, 1);
    if (bloom_) {
      auto ts_sz = comparator_.comparator.user_comparator()->timestamp_size();
      Slice user_k_without_ts(ExtractUserKeyAndStripTimestamp(k, ts_sz));
      if (prefix_extractor_->InDomain(user_k_without_ts) &&
          !bloom_->MayContain(
              prefix_extractor_->Transform(user_k_without_ts))) {
        PERF_COUNTER_ADD(bloom_memtable_miss_count, 1);
        valid_ = false;
        return;
      } else {
        PERF_COUNTER_ADD(bloom_memtable_hit_count, 1);
      }
    }
    iter_->Seek(k, nullptr);
    valid_ = iter_->Valid();
    if (!Valid()) {
      SeekToLast();
    }
    while (Valid() && comparator_.comparator.Compare(k, key()) < 0) {
      Prev();
    }
  }
  void SeekToFirst() override {
    iter_->SeekToFirst();
    valid_ = iter_->Valid();
  }
  void SeekToLast() override {
    iter_->SeekToLast();
    valid_ = iter_->Valid();
  }
  void Next() override {
    PERF_COUNTER_ADD(next_on_memtable_count, 1);
    assert(Valid());
    iter_->Next();
    TEST_SYNC_POINT_CALLBACK("MemTableIterator::Next:0", iter_);
    valid_ = iter_->Valid();
  }
  bool NextAndGetResult(IterateResult* result) override {
    Next();
    bool is_valid = valid_;
    if (is_valid) {
      result->key = key();
      result->bound_check_result = IterBoundCheck::kUnknown;
      result->value_prepared = true;
    }
    return is_valid;
  }
  void Prev() override {
    PERF_COUNTER_ADD(prev_on_memtable_count, 1);
    assert(Valid());
    iter_->Prev();
    valid_ = iter_->Valid();
  }
  Slice key() const override {
    assert(Valid());
    return GetLengthPrefixedSlice(iter_->key());
  }
  Slice value() const override {
    assert(Valid());
    Slice key_slice = GetLengthPrefixedSlice(iter_->key());
    return GetLengthPrefixedSlice(key_slice.data() + key_slice.size());
  }

  Status status() const override { return Status::OK(); }

  bool IsKeyPinned() const override {
    // memtable data is always pinned
    return true;
  }

  bool IsValuePinned() const override {
    // memtable value is always pinned, except if we allow inplace update.
    return value_pinned_;
  }

 private:
  DynamicBloom* bloom_;
  const SliceTransform* const prefix_extractor_;
  const MemTable::KeyComparator comparator_;
  MemTableRep::Iterator* iter_;
  bool valid_;
  bool arena_mode_;
  bool value_pinned_;
};

InternalIterator* MemTable::NewIterator(const ReadOptions& read_options,
                                        Arena* arena) {
  assert(arena != nullptr);
  auto mem = arena->AllocateAligned(sizeof(MemTableIterator));
  return new (mem) MemTableIterator(*this, read_options, arena);
}

FragmentedRangeTombstoneIterator* MemTable::NewRangeTombstoneIterator(
    const ReadOptions& read_options, SequenceNumber read_seq) {
  if (read_options.ignore_range_deletions ||
      is_range_del_table_empty_.load(std::memory_order_relaxed)) {
    return nullptr;
  }
  auto* unfragmented_iter = new MemTableIterator(
      *this, read_options, nullptr /* arena */, true /* use_range_del_table */);
  if (unfragmented_iter == nullptr) {
    return nullptr;
  }
  auto fragmented_tombstone_list =
      std::make_shared<FragmentedRangeTombstoneList>(
          std::unique_ptr<InternalIterator>(unfragmented_iter),
          comparator_.comparator);

  auto* fragmented_iter = new FragmentedRangeTombstoneIterator(
      fragmented_tombstone_list, comparator_.comparator, read_seq);
  return fragmented_iter;
}

port::RWMutex* MemTable::GetLock(const Slice& key) {
  return &locks_[GetSliceRangedNPHash(key, locks_.size())];
}

MemTable::MemTableStats MemTable::ApproximateStats(const Slice& start_ikey,
                                                   const Slice& end_ikey) {
  uint64_t entry_count = table_->ApproximateNumEntries(start_ikey, end_ikey);
  entry_count += range_del_table_->ApproximateNumEntries(start_ikey, end_ikey);
  if (entry_count == 0) {
    return {0, 0};
  }
  uint64_t n = num_entries_.load(std::memory_order_relaxed);
  if (n == 0) {
    return {0, 0};
  }
  if (entry_count > n) {
    // (range_del_)table_->ApproximateNumEntries() is just an estimate so it can
    // be larger than actual entries we have. Cap it to entries we have to limit
    // the inaccuracy.
    entry_count = n;
  }
  uint64_t data_size = data_size_.load(std::memory_order_relaxed);
  return {entry_count * (data_size / n), entry_count};
}

Status MemTable::VerifyEncodedEntry(Slice encoded,
                                    const ProtectionInfoKVOS64& kv_prot_info) {
  uint32_t ikey_len = 0;
  if (!GetVarint32(&encoded, &ikey_len)) {
    return Status::Corruption("Unable to parse internal key length");
  }
  size_t ts_sz = GetInternalKeyComparator().user_comparator()->timestamp_size();
  if (ikey_len < 8 + ts_sz) {
    return Status::Corruption("Internal key length too short");
  }
  if (ikey_len > encoded.size()) {
    return Status::Corruption("Internal key length too long");
  }
  uint32_t value_len = 0;
  const size_t user_key_len = ikey_len - 8;
  Slice key(encoded.data(), user_key_len);
  encoded.remove_prefix(user_key_len);

  uint64_t packed = DecodeFixed64(encoded.data());
  ValueType value_type = kMaxValue;
  SequenceNumber sequence_number = kMaxSequenceNumber;
  UnPackSequenceAndType(packed, &sequence_number, &value_type);
  encoded.remove_prefix(8);

  if (!GetVarint32(&encoded, &value_len)) {
    return Status::Corruption("Unable to parse value length");
  }
  if (value_len < encoded.size()) {
    return Status::Corruption("Value length too short");
  }
  if (value_len > encoded.size()) {
    return Status::Corruption("Value length too long");
  }
  Slice value(encoded.data(), value_len);

  return kv_prot_info.StripS(sequence_number)
      .StripKVO(key, value, value_type)
      .GetStatus();
}

Status MemTable::Add(SequenceNumber s, ValueType type,
                     const Slice& key, /* user key */
                     const Slice& value,
                     const ProtectionInfoKVOS64* kv_prot_info,
                     bool allow_concurrent,
                     MemTablePostProcessInfo* post_process_info, void** hint) {
  // Format of an entry is concatenation of:
  //  key_size     : varint32 of internal_key.size()
  //  key bytes    : char[internal_key.size()]
  //  value_size   : varint32 of value.size()
  //  value bytes  : char[value.size()]
  uint32_t key_size = static_cast<uint32_t>(key.size());
  uint32_t val_size = static_cast<uint32_t>(value.size());
  uint32_t internal_key_size = key_size + 8;
  const uint32_t encoded_len = VarintLength(internal_key_size) +
                               internal_key_size + VarintLength(val_size) +
                               val_size;
  char* buf = nullptr;
  std::unique_ptr<MemTableRep>& table =
      type == kTypeRangeDeletion ? range_del_table_ : table_;
  KeyHandle handle = table->Allocate(encoded_len, &buf);

  char* p = EncodeVarint32(buf, internal_key_size);
  memcpy(p, key.data(), key_size);
  Slice key_slice(p, key_size);
  p += key_size;
  uint64_t packed = PackSequenceAndType(s, type);
  EncodeFixed64(p, packed);
  p += 8;
  p = EncodeVarint32(p, val_size);
  memcpy(p, value.data(), val_size);
  assert((unsigned)(p + val_size - buf) == (unsigned)encoded_len);
  if (kv_prot_info != nullptr) {
    Slice encoded(buf, encoded_len);
    TEST_SYNC_POINT_CALLBACK("MemTable::Add:Encoded", &encoded);
    Status status = VerifyEncodedEntry(encoded, *kv_prot_info);
    if (!status.ok()) {
      return status;
    }
  }

  size_t ts_sz = GetInternalKeyComparator().user_comparator()->timestamp_size();
  Slice key_without_ts = StripTimestampFromUserKey(key, ts_sz);

  if (!allow_concurrent) {
    // Extract prefix for insert with hint.
    if (insert_with_hint_prefix_extractor_ != nullptr &&
        insert_with_hint_prefix_extractor_->InDomain(key_slice)) {
      Slice prefix = insert_with_hint_prefix_extractor_->Transform(key_slice);
      bool res = table->InsertKeyWithHint(handle, &insert_hints_[prefix]);
      if (UNLIKELY(!res)) {
        return Status::TryAgain("key+seq exists");
      }
    } else {
      bool res = table->InsertKey(handle);
      if (UNLIKELY(!res)) {
        return Status::TryAgain("key+seq exists");
      }
    }

    // this is a bit ugly, but is the way to avoid locked instructions
    // when incrementing an atomic
    num_entries_.store(num_entries_.load(std::memory_order_relaxed) + 1,
                       std::memory_order_relaxed);
    data_size_.store(data_size_.load(std::memory_order_relaxed) + encoded_len,
                     std::memory_order_relaxed);
    if (type == kTypeDeletion) {
      num_deletes_.store(num_deletes_.load(std::memory_order_relaxed) + 1,
                         std::memory_order_relaxed);
    }

    if (bloom_filter_ && prefix_extractor_ &&
        prefix_extractor_->InDomain(key_without_ts)) {
      bloom_filter_->Add(prefix_extractor_->Transform(key_without_ts));
    }
    if (bloom_filter_ && moptions_.memtable_whole_key_filtering) {
      bloom_filter_->Add(key_without_ts);
    }

    // The first sequence number inserted into the memtable
    assert(first_seqno_ == 0 || s >= first_seqno_);
    if (first_seqno_ == 0) {
      first_seqno_.store(s, std::memory_order_relaxed);

      if (earliest_seqno_ == kMaxSequenceNumber) {
        earliest_seqno_.store(GetFirstSequenceNumber(),
                              std::memory_order_relaxed);
      }
      assert(first_seqno_.load() >= earliest_seqno_.load());
    }
    assert(post_process_info == nullptr);
    UpdateFlushState();
  } else {
    bool res = (hint == nullptr)
                   ? table->InsertKeyConcurrently(handle)
                   : table->InsertKeyWithHintConcurrently(handle, hint);
    if (UNLIKELY(!res)) {
      return Status::TryAgain("key+seq exists");
    }

    assert(post_process_info != nullptr);
    post_process_info->num_entries++;
    post_process_info->data_size += encoded_len;
    if (type == kTypeDeletion) {
      post_process_info->num_deletes++;
    }

    if (bloom_filter_ && prefix_extractor_ &&
        prefix_extractor_->InDomain(key_without_ts)) {
      bloom_filter_->AddConcurrently(
          prefix_extractor_->Transform(key_without_ts));
    }
    if (bloom_filter_ && moptions_.memtable_whole_key_filtering) {
      bloom_filter_->AddConcurrently(key_without_ts);
    }

    // atomically update first_seqno_ and earliest_seqno_.
    uint64_t cur_seq_num = first_seqno_.load(std::memory_order_relaxed);
    while ((cur_seq_num == 0 || s < cur_seq_num) &&
           !first_seqno_.compare_exchange_weak(cur_seq_num, s)) {
    }
    uint64_t cur_earliest_seqno =
        earliest_seqno_.load(std::memory_order_relaxed);
    while (
        (cur_earliest_seqno == kMaxSequenceNumber || s < cur_earliest_seqno) &&
        !first_seqno_.compare_exchange_weak(cur_earliest_seqno, s)) {
    }
  }
  if (type == kTypeRangeDeletion) {
    is_range_del_table_empty_.store(false, std::memory_order_relaxed);
  }
  UpdateOldestKeyTime();
  return Status::OK();
}

// Callback from MemTable::Get()
namespace {

struct Saver {
  Status* status;
  const LookupKey* key;
  bool* found_final_value;  // Is value set correctly? Used by KeyMayExist
  bool* merge_in_progress;
  std::string* value;
  SequenceNumber seq;
  std::string* timestamp;
  const MergeOperator* merge_operator;
  // the merge operations encountered;
  MergeContext* merge_context;
  SequenceNumber max_covering_tombstone_seq;
  MemTable* mem;
  Logger* logger;
  Statistics* statistics;
  bool inplace_update_support;
  bool do_merge;
  SystemClock* clock;

  ReadCallback* callback_;
  bool* is_blob_index;
  bool allow_data_in_errors;
  bool CheckCallback(SequenceNumber _seq) {
    if (callback_) {
      return callback_->IsVisible(_seq);
    }
    return true;
  }
};
}  // namespace

static bool SaveValue(void* arg, const char* entry) {
  Saver* s = reinterpret_cast<Saver*>(arg);
  assert(s != nullptr);
  MergeContext* merge_context = s->merge_context;
  SequenceNumber max_covering_tombstone_seq = s->max_covering_tombstone_seq;
  const MergeOperator* merge_operator = s->merge_operator;

  assert(merge_context != nullptr);

  // entry format is:
  //    klength  varint32
  //    userkey  char[klength-8]
  //    tag      uint64
  //    vlength  varint32f
  //    value    char[vlength]
  // Check that it belongs to same user key.  We do not check the
  // sequence number since the Seek() call above should have skipped
  // all entries with overly large sequence numbers.
  uint32_t key_length = 0;
  const char* key_ptr = GetVarint32Ptr(entry, entry + 5, &key_length);
  assert(key_length >= 8);
  Slice user_key_slice = Slice(key_ptr, key_length - 8);
  const Comparator* user_comparator =
      s->mem->GetInternalKeyComparator().user_comparator();
  size_t ts_sz = user_comparator->timestamp_size();
  if (user_comparator->EqualWithoutTimestamp(user_key_slice,
                                             s->key->user_key())) {
    // Correct user key
    const uint64_t tag = DecodeFixed64(key_ptr + key_length - 8);
    ValueType type;
    SequenceNumber seq;
    UnPackSequenceAndType(tag, &seq, &type);
    // If the value is not in the snapshot, skip it
    if (!s->CheckCallback(seq)) {
      return true;  // to continue to the next seq
    }

    s->seq = seq;

    if ((type == kTypeValue || type == kTypeMerge || type == kTypeBlobIndex) &&
        max_covering_tombstone_seq > seq) {
      type = kTypeRangeDeletion;
    }
    switch (type) {
      case kTypeBlobIndex:
        if (s->is_blob_index == nullptr) {
          ROCKS_LOG_ERROR(s->logger, "Encounter unexpected blob index.");
          *(s->status) = Status::NotSupported(
              "Encounter unsupported blob value. Please open DB with "
              "ROCKSDB_NAMESPACE::blob_db::BlobDB instead.");
        } else if (*(s->merge_in_progress)) {
          *(s->status) =
              Status::NotSupported("Blob DB does not support merge operator.");
        }
        if (!s->status->ok()) {
          *(s->found_final_value) = true;
          return false;
        }
        FALLTHROUGH_INTENDED;
      case kTypeValue: {
        if (s->inplace_update_support) {
          s->mem->GetLock(s->key->user_key())->ReadLock();
        }
        Slice v = GetLengthPrefixedSlice(key_ptr + key_length);
        *(s->status) = Status::OK();
        if (*(s->merge_in_progress)) {
          if (s->do_merge) {
            if (s->value != nullptr) {
              *(s->status) = MergeHelper::TimedFullMerge(
                  merge_operator, s->key->user_key(), &v,
                  merge_context->GetOperands(), s->value, s->logger,
                  s->statistics, s->clock, nullptr /* result_operand */, true);
            }
          } else {
            // Preserve the value with the goal of returning it as part of
            // raw merge operands to the user
            merge_context->PushOperand(
                v, s->inplace_update_support == false /* operand_pinned */);
          }
        } else if (!s->do_merge) {
          // Preserve the value with the goal of returning it as part of
          // raw merge operands to the user
          merge_context->PushOperand(
              v, s->inplace_update_support == false /* operand_pinned */);
        } else if (s->value != nullptr) {
          s->value->assign(v.data(), v.size());
        }
        if (s->inplace_update_support) {
          s->mem->GetLock(s->key->user_key())->ReadUnlock();
        }
        *(s->found_final_value) = true;
        if (s->is_blob_index != nullptr) {
          *(s->is_blob_index) = (type == kTypeBlobIndex);
        }

        if (ts_sz > 0 && s->timestamp != nullptr) {
          Slice ts = ExtractTimestampFromUserKey(user_key_slice, ts_sz);
          s->timestamp->assign(ts.data(), ts.size());
        }
        return false;
      }
      case kTypeDeletion:
      case kTypeDeletionWithTimestamp:
      case kTypeSingleDeletion:
      case kTypeRangeDeletion: {
        if (*(s->merge_in_progress)) {
          if (s->value != nullptr) {
            *(s->status) = MergeHelper::TimedFullMerge(
                merge_operator, s->key->user_key(), nullptr,
                merge_context->GetOperands(), s->value, s->logger,
                s->statistics, s->clock, nullptr /* result_operand */, true);
          }
        } else {
          *(s->status) = Status::NotFound();
        }
        *(s->found_final_value) = true;
        return false;
      }
      case kTypeMerge: {
        if (!merge_operator) {
          *(s->status) = Status::InvalidArgument(
              "merge_operator is not properly initialized.");
          // Normally we continue the loop (return true) when we see a merge
          // operand.  But in case of an error, we should stop the loop
          // immediately and pretend we have found the value to stop further
          // seek.  Otherwise, the later call will override this error status.
          *(s->found_final_value) = true;
          return false;
        }
        Slice v = GetLengthPrefixedSlice(key_ptr + key_length);
        *(s->merge_in_progress) = true;
        merge_context->PushOperand(
            v, s->inplace_update_support == false /* operand_pinned */);
        if (s->do_merge && merge_operator->ShouldMerge(
                               merge_context->GetOperandsDirectionBackward())) {
          *(s->status) = MergeHelper::TimedFullMerge(
              merge_operator, s->key->user_key(), nullptr,
              merge_context->GetOperands(), s->value, s->logger, s->statistics,
              s->clock, nullptr /* result_operand */, true);
          *(s->found_final_value) = true;
          return false;
        }
        return true;
      }
      default: {
        std::string msg("Corrupted value not expected.");
        if (s->allow_data_in_errors) {
          msg.append("Unrecognized value type: " +
                     std::to_string(static_cast<int>(type)) + ". ");
          msg.append("User key: " + user_key_slice.ToString(/*hex=*/true) +
                     ". ");
          msg.append("seq: " + std::to_string(seq) + ".");
        }
        *(s->status) = Status::Corruption(msg.c_str());
        return false;
      }
    }
  }

  // s->state could be Corrupt, merge or notfound
  return false;
}

bool MemTable::Get(const LookupKey& key, std::string* value,
                   std::string* timestamp, Status* s,
                   MergeContext* merge_context,
                   SequenceNumber* max_covering_tombstone_seq,
                   SequenceNumber* seq, const ReadOptions& read_opts,
                   ReadCallback* callback, bool* is_blob_index, bool do_merge) {
  // The sequence number is updated synchronously in version_set.h
  if (IsEmpty()) {
    // Avoiding recording stats for speed.
    return false;
  }
  PERF_TIMER_GUARD(get_from_memtable_time);

  std::unique_ptr<FragmentedRangeTombstoneIterator> range_del_iter(
      NewRangeTombstoneIterator(read_opts,
                                GetInternalKeySeqno(key.internal_key())));
  if (range_del_iter != nullptr) {
    *max_covering_tombstone_seq =
        std::max(*max_covering_tombstone_seq,
                 range_del_iter->MaxCoveringTombstoneSeqnum(key.user_key()));
  }

  bool found_final_value = false;
  bool merge_in_progress = s->IsMergeInProgress();
  bool may_contain = true;
  size_t ts_sz = GetInternalKeyComparator().user_comparator()->timestamp_size();
  Slice user_key_without_ts = StripTimestampFromUserKey(key.user_key(), ts_sz);
  if (bloom_filter_) {
    // when both memtable_whole_key_filtering and prefix_extractor_ are set,
    // only do whole key filtering for Get() to save CPU
    if (moptions_.memtable_whole_key_filtering) {
      may_contain = bloom_filter_->MayContain(user_key_without_ts);
    } else {
      assert(prefix_extractor_);
      may_contain = !prefix_extractor_->InDomain(user_key_without_ts) ||
                    bloom_filter_->MayContain(
                        prefix_extractor_->Transform(user_key_without_ts));
    }
  }

  if (bloom_filter_ && !may_contain) {
    // iter is null if prefix bloom says the key does not exist
    PERF_COUNTER_ADD(bloom_memtable_miss_count, 1);
    *seq = kMaxSequenceNumber;
  } else {
    if (bloom_filter_) {
      PERF_COUNTER_ADD(bloom_memtable_hit_count, 1);
    }
    GetFromTable(key, *max_covering_tombstone_seq, do_merge, callback,
                 is_blob_index, value, timestamp, s, merge_context, seq,
                 &found_final_value, &merge_in_progress);
  }

  // No change to value, since we have not yet found a Put/Delete
  if (!found_final_value && merge_in_progress) {
    *s = Status::MergeInProgress();
  }
  PERF_COUNTER_ADD(get_from_memtable_count, 1);
  return found_final_value;
}

void MemTable::GetFromTable(const LookupKey& key,
                            SequenceNumber max_covering_tombstone_seq,
                            bool do_merge, ReadCallback* callback,
                            bool* is_blob_index, std::string* value,
                            std::string* timestamp, Status* s,
                            MergeContext* merge_context, SequenceNumber* seq,
                            bool* found_final_value, bool* merge_in_progress) {
  Saver saver;
  saver.status = s;
  saver.found_final_value = found_final_value;
  saver.merge_in_progress = merge_in_progress;
  saver.key = &key;
  saver.value = value;
  saver.timestamp = timestamp;
  saver.seq = kMaxSequenceNumber;
  saver.mem = this;
  saver.merge_context = merge_context;
  saver.max_covering_tombstone_seq = max_covering_tombstone_seq;
  saver.merge_operator = moptions_.merge_operator;
  saver.logger = moptions_.info_log;
  saver.inplace_update_support = moptions_.inplace_update_support;
  saver.statistics = moptions_.statistics;
  saver.clock = clock_;
  saver.callback_ = callback;
  saver.is_blob_index = is_blob_index;
  saver.do_merge = do_merge;
  saver.allow_data_in_errors = moptions_.allow_data_in_errors;
  table_->Get(key, &saver, SaveValue);
  *seq = saver.seq;
}

void MemTable::MultiGet(const ReadOptions& read_options, MultiGetRange* range,
                        ReadCallback* callback) {
  // The sequence number is updated synchronously in version_set.h
  if (IsEmpty()) {
    // Avoiding recording stats for speed.
    return;
  }
  PERF_TIMER_GUARD(get_from_memtable_time);

  MultiGetRange temp_range(*range, range->begin(), range->end());
  if (bloom_filter_) {
    std::array<Slice*, MultiGetContext::MAX_BATCH_SIZE> keys;
    std::array<bool, MultiGetContext::MAX_BATCH_SIZE> may_match = {{true}};
    autovector<Slice, MultiGetContext::MAX_BATCH_SIZE> prefixes;
    int num_keys = 0;
    for (auto iter = temp_range.begin(); iter != temp_range.end(); ++iter) {
      if (!prefix_extractor_) {
        keys[num_keys++] = &iter->ukey_without_ts;
      } else if (prefix_extractor_->InDomain(iter->ukey_without_ts)) {
        prefixes.emplace_back(
            prefix_extractor_->Transform(iter->ukey_without_ts));
        keys[num_keys++] = &prefixes.back();
      }
    }
    bloom_filter_->MayContain(num_keys, &keys[0], &may_match[0]);
    int idx = 0;
    for (auto iter = temp_range.begin(); iter != temp_range.end(); ++iter) {
      if (prefix_extractor_ &&
          !prefix_extractor_->InDomain(iter->ukey_without_ts)) {
        PERF_COUNTER_ADD(bloom_memtable_hit_count, 1);
        continue;
      }
      if (!may_match[idx]) {
        temp_range.SkipKey(iter);
        PERF_COUNTER_ADD(bloom_memtable_miss_count, 1);
      } else {
        PERF_COUNTER_ADD(bloom_memtable_hit_count, 1);
      }
      idx++;
    }
  }
  for (auto iter = temp_range.begin(); iter != temp_range.end(); ++iter) {
    SequenceNumber seq = kMaxSequenceNumber;
    bool found_final_value{false};
    bool merge_in_progress = iter->s->IsMergeInProgress();
    std::unique_ptr<FragmentedRangeTombstoneIterator> range_del_iter(
        NewRangeTombstoneIterator(
            read_options, GetInternalKeySeqno(iter->lkey->internal_key())));
    if (range_del_iter != nullptr) {
      iter->max_covering_tombstone_seq = std::max(
          iter->max_covering_tombstone_seq,
          range_del_iter->MaxCoveringTombstoneSeqnum(iter->lkey->user_key()));
    }
    GetFromTable(*(iter->lkey), iter->max_covering_tombstone_seq, true,
                 callback, &iter->is_blob_index, iter->value->GetSelf(),
                 iter->timestamp, iter->s, &(iter->merge_context), &seq,
                 &found_final_value, &merge_in_progress);

    if (!found_final_value && merge_in_progress) {
      *(iter->s) = Status::MergeInProgress();
    }

    if (found_final_value) {
      iter->value->PinSelf();
      range->AddValueSize(iter->value->size());
      range->MarkKeyDone(iter);
      RecordTick(moptions_.statistics, MEMTABLE_HIT);
      if (range->GetValueSize() > read_options.value_size_soft_limit) {
        // Set all remaining keys in range to Abort
        for (auto range_iter = range->begin(); range_iter != range->end();
             ++range_iter) {
          range->MarkKeyDone(range_iter);
          *(range_iter->s) = Status::Aborted();
        }
        break;
      }
    }
  }
  PERF_COUNTER_ADD(get_from_memtable_count, 1);
}

Status MemTable::Update(SequenceNumber seq, const Slice& key,
                        const Slice& value,
                        const ProtectionInfoKVOS64* kv_prot_info) {
  LookupKey lkey(key, seq);
  Slice mem_key = lkey.memtable_key();

  std::unique_ptr<MemTableRep::Iterator> iter(
      table_->GetDynamicPrefixIterator());
  iter->Seek(lkey.internal_key(), mem_key.data());

  if (iter->Valid()) {
    // entry format is:
    //    key_length  varint32
    //    userkey  char[klength-8]
    //    tag      uint64
    //    vlength  varint32
    //    value    char[vlength]
    // Check that it belongs to same user key.  We do not check the
    // sequence number since the Seek() call above should have skipped
    // all entries with overly large sequence numbers.
    const char* entry = iter->key();
    uint32_t key_length = 0;
    const char* key_ptr = GetVarint32Ptr(entry, entry + 5, &key_length);
    if (comparator_.comparator.user_comparator()->Equal(
            Slice(key_ptr, key_length - 8), lkey.user_key())) {
      // Correct user key
      const uint64_t tag = DecodeFixed64(key_ptr + key_length - 8);
      ValueType type;
      SequenceNumber existing_seq;
      UnPackSequenceAndType(tag, &existing_seq, &type);
      assert(existing_seq != seq);
      if (type == kTypeValue) {
        Slice prev_value = GetLengthPrefixedSlice(key_ptr + key_length);
        uint32_t prev_size = static_cast<uint32_t>(prev_value.size());
        uint32_t new_size = static_cast<uint32_t>(value.size());

        // Update value, if new value size  <= previous value size
        if (new_size <= prev_size) {
          char* p =
              EncodeVarint32(const_cast<char*>(key_ptr) + key_length, new_size);
          WriteLock wl(GetLock(lkey.user_key()));
          memcpy(p, value.data(), value.size());
          assert((unsigned)((p + value.size()) - entry) ==
                 (unsigned)(VarintLength(key_length) + key_length +
                            VarintLength(value.size()) + value.size()));
          RecordTick(moptions_.statistics, NUMBER_KEYS_UPDATED);
          if (kv_prot_info != nullptr) {
            ProtectionInfoKVOS64 updated_kv_prot_info(*kv_prot_info);
            // `seq` is swallowed and `existing_seq` prevails.
            updated_kv_prot_info.UpdateS(seq, existing_seq);
            Slice encoded(entry, p + value.size() - entry);
            return VerifyEncodedEntry(encoded, updated_kv_prot_info);
          }
          return Status::OK();
        }
      }
    }
  }

  // The latest value is not `kTypeValue` or key doesn't exist
  return Add(seq, kTypeValue, key, value, kv_prot_info);
}

Status MemTable::UpdateCallback(SequenceNumber seq, const Slice& key,
                                const Slice& delta,
                                const ProtectionInfoKVOS64* kv_prot_info) {
  LookupKey lkey(key, seq);
  Slice memkey = lkey.memtable_key();

  std::unique_ptr<MemTableRep::Iterator> iter(
      table_->GetDynamicPrefixIterator());
  iter->Seek(lkey.internal_key(), memkey.data());

  if (iter->Valid()) {
    // entry format is:
    //    key_length  varint32
    //    userkey  char[klength-8]
    //    tag      uint64
    //    vlength  varint32
    //    value    char[vlength]
    // Check that it belongs to same user key.  We do not check the
    // sequence number since the Seek() call above should have skipped
    // all entries with overly large sequence numbers.
    const char* entry = iter->key();
    uint32_t key_length = 0;
    const char* key_ptr = GetVarint32Ptr(entry, entry + 5, &key_length);
    if (comparator_.comparator.user_comparator()->Equal(
            Slice(key_ptr, key_length - 8), lkey.user_key())) {
      // Correct user key
      const uint64_t tag = DecodeFixed64(key_ptr + key_length - 8);
      ValueType type;
      uint64_t existing_seq;
      UnPackSequenceAndType(tag, &existing_seq, &type);
      switch (type) {
        case kTypeValue: {
          Slice prev_value = GetLengthPrefixedSlice(key_ptr + key_length);
          uint32_t prev_size = static_cast<uint32_t>(prev_value.size());

          char* prev_buffer = const_cast<char*>(prev_value.data());
          uint32_t new_prev_size = prev_size;

          std::string str_value;
          WriteLock wl(GetLock(lkey.user_key()));
          auto status = moptions_.inplace_callback(prev_buffer, &new_prev_size,
                                                   delta, &str_value);
          if (status == UpdateStatus::UPDATED_INPLACE) {
            // Value already updated by callback.
            assert(new_prev_size <= prev_size);
            if (new_prev_size < prev_size) {
              // overwrite the new prev_size
              char* p = EncodeVarint32(const_cast<char*>(key_ptr) + key_length,
                                       new_prev_size);
              if (VarintLength(new_prev_size) < VarintLength(prev_size)) {
                // shift the value buffer as well.
                memcpy(p, prev_buffer, new_prev_size);
              }
            }
            RecordTick(moptions_.statistics, NUMBER_KEYS_UPDATED);
            UpdateFlushState();
            if (kv_prot_info != nullptr) {
              ProtectionInfoKVOS64 updated_kv_prot_info(*kv_prot_info);
              // `seq` is swallowed and `existing_seq` prevails.
              updated_kv_prot_info.UpdateS(seq, existing_seq);
              updated_kv_prot_info.UpdateV(delta,
                                           Slice(prev_buffer, new_prev_size));
              Slice encoded(entry, prev_buffer + new_prev_size - entry);
              return VerifyEncodedEntry(encoded, updated_kv_prot_info);
            }
            return Status::OK();
          } else if (status == UpdateStatus::UPDATED) {
            Status s;
            if (kv_prot_info != nullptr) {
              ProtectionInfoKVOS64 updated_kv_prot_info(*kv_prot_info);
              updated_kv_prot_info.UpdateV(delta, str_value);
              s = Add(seq, kTypeValue, key, Slice(str_value),
                      &updated_kv_prot_info);
            } else {
              s = Add(seq, kTypeValue, key, Slice(str_value),
                      nullptr /* kv_prot_info */);
            }
            RecordTick(moptions_.statistics, NUMBER_KEYS_WRITTEN);
            UpdateFlushState();
            return s;
          } else if (status == UpdateStatus::UPDATE_FAILED) {
            // `UPDATE_FAILED` is named incorrectly. It indicates no update
            // happened. It does not indicate a failure happened.
            UpdateFlushState();
            return Status::OK();
          }
        }
        default:
          break;
      }
    }
  }
  // The latest value is not `kTypeValue` or key doesn't exist
  return Status::NotFound();
}

size_t MemTable::CountSuccessiveMergeEntries(const LookupKey& key) {
  Slice memkey = key.memtable_key();

  // A total ordered iterator is costly for some memtablerep (prefix aware
  // reps). By passing in the user key, we allow efficient iterator creation.
  // The iterator only needs to be ordered within the same user key.
  std::unique_ptr<MemTableRep::Iterator> iter(
      table_->GetDynamicPrefixIterator());
  iter->Seek(key.internal_key(), memkey.data());

  size_t num_successive_merges = 0;

  for (; iter->Valid(); iter->Next()) {
    const char* entry = iter->key();
    uint32_t key_length = 0;
    const char* iter_key_ptr = GetVarint32Ptr(entry, entry + 5, &key_length);
    if (!comparator_.comparator.user_comparator()->Equal(
            Slice(iter_key_ptr, key_length - 8), key.user_key())) {
      break;
    }

    const uint64_t tag = DecodeFixed64(iter_key_ptr + key_length - 8);
    ValueType type;
    uint64_t unused;
    UnPackSequenceAndType(tag, &unused, &type);
    if (type != kTypeMerge) {
      break;
    }

    ++num_successive_merges;
  }

  return num_successive_merges;
}

void MemTableRep::Get(const LookupKey& k, void* callback_args,
                      bool (*callback_func)(void* arg, const char* entry)) {
  auto iter = GetDynamicPrefixIterator();
  for (iter->Seek(k.internal_key(), k.memtable_key().data());
       iter->Valid() && callback_func(callback_args, iter->key());
       iter->Next()) {
  }
}

void MemTable::RefLogContainingPrepSection(uint64_t log) {
  assert(log > 0);
  auto cur = min_prep_log_referenced_.load();
  while ((log < cur || cur == 0) &&
         !min_prep_log_referenced_.compare_exchange_strong(cur, log)) {
    cur = min_prep_log_referenced_.load();
  }
}

uint64_t MemTable::GetMinLogContainingPrepSection() {
  return min_prep_log_referenced_.load();
}

}  // namespace ROCKSDB_NAMESPACE
