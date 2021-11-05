//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#ifndef ROCKSDB_LITE

#include "utilities/transactions/pessimistic_transaction.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "db/column_family.h"
#include "db/db_impl/db_impl.h"
#include "logging/logging.h"
#include "rocksdb/comparator.h"
#include "rocksdb/db.h"
#include "rocksdb/snapshot.h"
#include "rocksdb/status.h"
#include "rocksdb/utilities/transaction_db.h"
#include "test_util/sync_point.h"
#include "util/cast_util.h"
#include "util/string_util.h"
#include "utilities/transactions/pessimistic_transaction_db.h"
#include "utilities/transactions/transaction_util.h"

namespace ROCKSDB_NAMESPACE {

struct WriteOptions;

std::atomic<TransactionID> PessimisticTransaction::txn_id_counter_(1);

TransactionID PessimisticTransaction::GenTxnID() {
  return txn_id_counter_.fetch_add(1);
}

PessimisticTransaction::PessimisticTransaction(
    TransactionDB* txn_db, const WriteOptions& write_options,
    const TransactionOptions& txn_options, const bool init)
    : TransactionBaseImpl(
          txn_db->GetRootDB(), write_options,
          static_cast_with_check<PessimisticTransactionDB>(txn_db)
              ->GetLockTrackerFactory()),
      txn_db_impl_(nullptr),
      expiration_time_(0),
      txn_id_(0),
      waiting_cf_id_(0),
      waiting_key_(nullptr),
      lock_timeout_(0),
      deadlock_detect_(false),
      deadlock_detect_depth_(0),
      skip_concurrency_control_(false) {
  txn_db_impl_ = static_cast_with_check<PessimisticTransactionDB>(txn_db);
  db_impl_ = static_cast_with_check<DBImpl>(db_);
  if (init) {
    Initialize(txn_options);
  }
}

void PessimisticTransaction::Initialize(const TransactionOptions& txn_options) {
  txn_id_ = GenTxnID();

  txn_state_ = STARTED;

  deadlock_detect_ = txn_options.deadlock_detect;
  deadlock_detect_depth_ = txn_options.deadlock_detect_depth;
  write_batch_.SetMaxBytes(txn_options.max_write_batch_size);
  skip_concurrency_control_ = txn_options.skip_concurrency_control;

  lock_timeout_ = txn_options.lock_timeout * 1000;
  if (lock_timeout_ < 0) {
    // Lock timeout not set, use default
    lock_timeout_ =
        txn_db_impl_->GetTxnDBOptions().transaction_lock_timeout * 1000;
  }

  if (txn_options.expiration >= 0) {
    expiration_time_ = start_time_ + txn_options.expiration * 1000;
  } else {
    expiration_time_ = 0;
  }

  if (txn_options.set_snapshot) {
    SetSnapshot();
  }

  if (expiration_time_ > 0) {
    txn_db_impl_->InsertExpirableTransaction(txn_id_, this);
  }
  use_only_the_last_commit_time_batch_for_recovery_ =
      txn_options.use_only_the_last_commit_time_batch_for_recovery;
  skip_prepare_ = txn_options.skip_prepare;
}

PessimisticTransaction::~PessimisticTransaction() {
  txn_db_impl_->UnLock(this, *tracked_locks_);
  if (expiration_time_ > 0) {
    txn_db_impl_->RemoveExpirableTransaction(txn_id_);
  }
  if (!name_.empty() && txn_state_ != COMMITTED) {
    txn_db_impl_->UnregisterTransaction(this);
  }
}

void PessimisticTransaction::Clear() {
  txn_db_impl_->UnLock(this, *tracked_locks_);
  TransactionBaseImpl::Clear();
}

void PessimisticTransaction::Reinitialize(
    TransactionDB* txn_db, const WriteOptions& write_options,
    const TransactionOptions& txn_options) {
  if (!name_.empty() && txn_state_ != COMMITTED) {
    txn_db_impl_->UnregisterTransaction(this);
  }
  TransactionBaseImpl::Reinitialize(txn_db->GetRootDB(), write_options);
  Initialize(txn_options);
}

bool PessimisticTransaction::IsExpired() const {
  if (expiration_time_ > 0) {
    if (dbimpl_->GetSystemClock()->NowMicros() >= expiration_time_) {
      // Transaction is expired.
      return true;
    }
  }

  return false;
}

WriteCommittedTxn::WriteCommittedTxn(TransactionDB* txn_db,
                                     const WriteOptions& write_options,
                                     const TransactionOptions& txn_options)
    : PessimisticTransaction(txn_db, write_options, txn_options){};

Status PessimisticTransaction::CommitBatch(WriteBatch* batch) {
  std::unique_ptr<LockTracker> keys_to_unlock(lock_tracker_factory_.Create());
  Status s = LockBatch(batch, keys_to_unlock.get());

  if (!s.ok()) {
    return s;
  }

  bool can_commit = false;

  if (IsExpired()) {
    s = Status::Expired();
  } else if (expiration_time_ > 0) {
    TransactionState expected = STARTED;
    can_commit = std::atomic_compare_exchange_strong(&txn_state_, &expected,
                                                     AWAITING_COMMIT);
  } else if (txn_state_ == STARTED) {
    // lock stealing is not a concern
    can_commit = true;
  }

  if (can_commit) {
    txn_state_.store(AWAITING_COMMIT);
    s = CommitBatchInternal(batch);
    if (s.ok()) {
      txn_state_.store(COMMITTED);
    }
  } else if (txn_state_ == LOCKS_STOLEN) {
    s = Status::Expired();
  } else {
    s = Status::InvalidArgument("Transaction is not in state for commit.");
  }

  txn_db_impl_->UnLock(this, *keys_to_unlock);

  return s;
}

Status PessimisticTransaction::Prepare() {

  if (name_.empty()) {
    return Status::InvalidArgument(
        "Cannot prepare a transaction that has not been named.");
  }

  if (IsExpired()) {
    return Status::Expired();
  }

  Status s;
  bool can_prepare = false;

  if (expiration_time_ > 0) {
    // must concern ourselves with expiraton and/or lock stealing
    // need to compare/exchange bc locks could be stolen under us here
    TransactionState expected = STARTED;
    can_prepare = std::atomic_compare_exchange_strong(&txn_state_, &expected,
                                                      AWAITING_PREPARE);
  } else if (txn_state_ == STARTED) {
    // expiration and lock stealing is not possible
    txn_state_.store(AWAITING_PREPARE);
    can_prepare = true;
  }

  if (can_prepare) {
    // transaction can't expire after preparation
    expiration_time_ = 0;
    assert(log_number_ == 0 ||
           txn_db_impl_->GetTxnDBOptions().write_policy == WRITE_UNPREPARED);

    s = PrepareInternal();
    if (s.ok()) {
      txn_state_.store(PREPARED);
    }
  } else if (txn_state_ == LOCKS_STOLEN) {
    s = Status::Expired();
  } else if (txn_state_ == PREPARED) {
    s = Status::InvalidArgument("Transaction has already been prepared.");
  } else if (txn_state_ == COMMITTED) {
    s = Status::InvalidArgument("Transaction has already been committed.");
  } else if (txn_state_ == ROLLEDBACK) {
    s = Status::InvalidArgument("Transaction has already been rolledback.");
  } else {
    s = Status::InvalidArgument("Transaction is not in state for commit.");
  }

  return s;
}

Status WriteCommittedTxn::PrepareInternal() {
  WriteOptions write_options = write_options_;
  write_options.disableWAL = false;
  auto s = WriteBatchInternal::MarkEndPrepare(GetWriteBatch()->GetWriteBatch(),
                                              name_);
  assert(s.ok());
  class MarkLogCallback : public PreReleaseCallback {
   public:
    MarkLogCallback(DBImpl* db, bool two_write_queues)
        : db_(db), two_write_queues_(two_write_queues) {
      (void)two_write_queues_;  // to silence unused private field warning
    }
    virtual Status Callback(SequenceNumber, bool is_mem_disabled,
                            uint64_t log_number, size_t /*index*/,
                            size_t /*total*/) override {
#ifdef NDEBUG
      (void)is_mem_disabled;
#endif
      assert(log_number != 0);
      assert(!two_write_queues_ || is_mem_disabled);  // implies the 2nd queue
      db_->logs_with_prep_tracker()->MarkLogAsContainingPrepSection(log_number);
      return Status::OK();
    }

   private:
    DBImpl* db_;
    bool two_write_queues_;
  } mark_log_callback(db_impl_,
                      db_impl_->immutable_db_options().two_write_queues);

  WriteCallback* const kNoWriteCallback = nullptr;
  const uint64_t kRefNoLog = 0;
  const bool kDisableMemtable = true;
  SequenceNumber* const KIgnoreSeqUsed = nullptr;
  const size_t kNoBatchCount = 0;
  s = db_impl_->WriteImpl(write_options, GetWriteBatch()->GetWriteBatch(),
                          kNoWriteCallback, &log_number_, kRefNoLog,
                          kDisableMemtable, KIgnoreSeqUsed, kNoBatchCount,
                          &mark_log_callback);
  return s;
}

Status PessimisticTransaction::Commit() {
  bool commit_without_prepare = false;
  bool commit_prepared = false;

  if (IsExpired()) {
    return Status::Expired();
  }

  if (expiration_time_ > 0) {
    // we must atomicaly compare and exchange the state here because at
    // this state in the transaction it is possible for another thread
    // to change our state out from under us in the even that we expire and have
    // our locks stolen. In this case the only valid state is STARTED because
    // a state of PREPARED would have a cleared expiration_time_.
    TransactionState expected = STARTED;
    commit_without_prepare = std::atomic_compare_exchange_strong(
        &txn_state_, &expected, AWAITING_COMMIT);
    TEST_SYNC_POINT("TransactionTest::ExpirableTransactionDataRace:1");
  } else if (txn_state_ == PREPARED) {
    // expiration and lock stealing is not a concern
    commit_prepared = true;
  } else if (txn_state_ == STARTED) {
    // expiration and lock stealing is not a concern
    if (skip_prepare_) {
      commit_without_prepare = true;
    } else {
      return Status::TxnNotPrepared();
    }
  }

  Status s;
  if (commit_without_prepare) {
    assert(!commit_prepared);
    if (WriteBatchInternal::Count(GetCommitTimeWriteBatch()) > 0) {
      s = Status::InvalidArgument(
          "Commit-time batch contains values that will not be committed.");
    } else {
      txn_state_.store(AWAITING_COMMIT);
      if (log_number_ > 0) {
        dbimpl_->logs_with_prep_tracker()->MarkLogAsHavingPrepSectionFlushed(
            log_number_);
      }
      s = CommitWithoutPrepareInternal();
      if (!name_.empty()) {
        txn_db_impl_->UnregisterTransaction(this);
      }
      Clear();
      if (s.ok()) {
        txn_state_.store(COMMITTED);
      }
    }
  } else if (commit_prepared) {
    txn_state_.store(AWAITING_COMMIT);

    s = CommitInternal();

    if (!s.ok()) {
      ROCKS_LOG_WARN(db_impl_->immutable_db_options().info_log,
                     "Commit write failed");
      return s;
    }

    // FindObsoleteFiles must now look to the memtables
    // to determine what prep logs must be kept around,
    // not the prep section heap.
    assert(log_number_ > 0);
    dbimpl_->logs_with_prep_tracker()->MarkLogAsHavingPrepSectionFlushed(
        log_number_);
    txn_db_impl_->UnregisterTransaction(this);

    Clear();
    txn_state_.store(COMMITTED);
  } else if (txn_state_ == LOCKS_STOLEN) {
    s = Status::Expired();
  } else if (txn_state_ == COMMITTED) {
    s = Status::InvalidArgument("Transaction has already been committed.");
  } else if (txn_state_ == ROLLEDBACK) {
    s = Status::InvalidArgument("Transaction has already been rolledback.");
  } else {
    s = Status::InvalidArgument("Transaction is not in state for commit.");
  }

  return s;
}

Status WriteCommittedTxn::CommitWithoutPrepareInternal() {
  uint64_t seq_used = kMaxSequenceNumber;
  auto s =
      db_impl_->WriteImpl(write_options_, GetWriteBatch()->GetWriteBatch(),
                          /*callback*/ nullptr, /*log_used*/ nullptr,
                          /*log_ref*/ 0, /*disable_memtable*/ false, &seq_used);
  assert(!s.ok() || seq_used != kMaxSequenceNumber);
  if (s.ok()) {
    SetId(seq_used);
  }
  return s;
}

Status WriteCommittedTxn::CommitBatchInternal(WriteBatch* batch, size_t) {
  uint64_t seq_used = kMaxSequenceNumber;
  auto s = db_impl_->WriteImpl(write_options_, batch, /*callback*/ nullptr,
                               /*log_used*/ nullptr, /*log_ref*/ 0,
                               /*disable_memtable*/ false, &seq_used);
  assert(!s.ok() || seq_used != kMaxSequenceNumber);
  if (s.ok()) {
    SetId(seq_used);
  }
  return s;
}

Status WriteCommittedTxn::CommitInternal() {
  // We take the commit-time batch and append the Commit marker.
  // The Memtable will ignore the Commit marker in non-recovery mode
  WriteBatch* working_batch = GetCommitTimeWriteBatch();
  auto s = WriteBatchInternal::MarkCommit(working_batch, name_);
  assert(s.ok());

  // any operations appended to this working_batch will be ignored from WAL
  working_batch->MarkWalTerminationPoint();

  // insert prepared batch into Memtable only skipping WAL.
  // Memtable will ignore BeginPrepare/EndPrepare markers
  // in non recovery mode and simply insert the values
  s = WriteBatchInternal::Append(working_batch,
                                 GetWriteBatch()->GetWriteBatch());
  assert(s.ok());

  uint64_t seq_used = kMaxSequenceNumber;
  s = db_impl_->WriteImpl(write_options_, working_batch, /*callback*/ nullptr,
                          /*log_used*/ nullptr, /*log_ref*/ log_number_,
                          /*disable_memtable*/ false, &seq_used);
  assert(!s.ok() || seq_used != kMaxSequenceNumber);
  if (s.ok()) {
    SetId(seq_used);
  }
  return s;
}

Status PessimisticTransaction::Rollback() {
  Status s;
  if (txn_state_ == PREPARED) {
    txn_state_.store(AWAITING_ROLLBACK);

    s = RollbackInternal();

    if (s.ok()) {
      // we do not need to keep our prepared section around
      assert(log_number_ > 0);
      dbimpl_->logs_with_prep_tracker()->MarkLogAsHavingPrepSectionFlushed(
          log_number_);
      Clear();
      txn_state_.store(ROLLEDBACK);
    }
  } else if (txn_state_ == STARTED) {
    if (log_number_ > 0) {
      assert(txn_db_impl_->GetTxnDBOptions().write_policy == WRITE_UNPREPARED);
      assert(GetId() > 0);
      s = RollbackInternal();

      if (s.ok()) {
        dbimpl_->logs_with_prep_tracker()->MarkLogAsHavingPrepSectionFlushed(
            log_number_);
      }
    }
    // prepare couldn't have taken place
    Clear();
  } else if (txn_state_ == COMMITTED) {
    s = Status::InvalidArgument("This transaction has already been committed.");
  } else {
    s = Status::InvalidArgument(
        "Two phase transaction is not in state for rollback.");
  }

  return s;
}

Status WriteCommittedTxn::RollbackInternal() {
  WriteBatch rollback_marker;
  auto s = WriteBatchInternal::MarkRollback(&rollback_marker, name_);
  assert(s.ok());
  s = db_impl_->WriteImpl(write_options_, &rollback_marker);
  return s;
}

Status PessimisticTransaction::RollbackToSavePoint() {
  if (txn_state_ != STARTED) {
    return Status::InvalidArgument("Transaction is beyond state for rollback.");
  }

  if (save_points_ != nullptr && !save_points_->empty()) {
    // Unlock any keys locked since last transaction
    auto& save_point_tracker = *save_points_->top().new_locks_;
    std::unique_ptr<LockTracker> t(
        tracked_locks_->GetTrackedLocksSinceSavePoint(save_point_tracker));
    if (t) {
      txn_db_impl_->UnLock(this, *t);
    }
  }

  return TransactionBaseImpl::RollbackToSavePoint();
}

// Lock all keys in this batch.
// On success, caller should unlock keys_to_unlock
Status PessimisticTransaction::LockBatch(WriteBatch* batch,
                                         LockTracker* keys_to_unlock) {
  class Handler : public WriteBatch::Handler {
   public:
    // Sorted map of column_family_id to sorted set of keys.
    // Since LockBatch() always locks keys in sorted order, it cannot deadlock
    // with itself.  We're not using a comparator here since it doesn't matter
    // what the sorting is as long as it's consistent.
    std::map<uint32_t, std::set<std::string>> keys_;

    Handler() {}

    void RecordKey(uint32_t column_family_id, const Slice& key) {
      std::string key_str = key.ToString();

      auto& cfh_keys = keys_[column_family_id];
      auto iter = cfh_keys.find(key_str);
      if (iter == cfh_keys.end()) {
        // key not yet seen, store it.
        cfh_keys.insert({std::move(key_str)});
      }
    }

    Status PutCF(uint32_t column_family_id, const Slice& key,
                 const Slice& /* unused */) override {
      RecordKey(column_family_id, key);
      return Status::OK();
    }
    Status MergeCF(uint32_t column_family_id, const Slice& key,
                   const Slice& /* unused */) override {
      RecordKey(column_family_id, key);
      return Status::OK();
    }
    Status DeleteCF(uint32_t column_family_id, const Slice& key) override {
      RecordKey(column_family_id, key);
      return Status::OK();
    }
  };

  // Iterating on this handler will add all keys in this batch into keys
  Handler handler;
  Status s = batch->Iterate(&handler);
  if (!s.ok()) {
    return s;
  }

  // Attempt to lock all keys
  for (const auto& cf_iter : handler.keys_) {
    uint32_t cfh_id = cf_iter.first;
    auto& cfh_keys = cf_iter.second;

    for (const auto& key_iter : cfh_keys) {
      const std::string& key = key_iter;

      s = txn_db_impl_->TryLock(this, cfh_id, key, true /* exclusive */);
      if (!s.ok()) {
        break;
      }
      PointLockRequest r;
      r.column_family_id = cfh_id;
      r.key = key;
      r.seq = kMaxSequenceNumber;
      r.read_only = false;
      r.exclusive = true;
      keys_to_unlock->Track(r);
    }

    if (!s.ok()) {
      break;
    }
  }

  if (!s.ok()) {
    txn_db_impl_->UnLock(this, *keys_to_unlock);
  }

  return s;
}

// Attempt to lock this key.
// Returns OK if the key has been successfully locked.  Non-ok, otherwise.
// If check_shapshot is true and this transaction has a snapshot set,
// this key will only be locked if there have been no writes to this key since
// the snapshot time.
Status PessimisticTransaction::TryLock(ColumnFamilyHandle* column_family,
                                       const Slice& key, bool read_only,
                                       bool exclusive, const bool do_validate,
                                       const bool assume_tracked) {
  assert(!assume_tracked || !do_validate);
  Status s;
  if (UNLIKELY(skip_concurrency_control_)) {
    return s;
  }
  uint32_t cfh_id = GetColumnFamilyID(column_family);
  std::string key_str = key.ToString();

  PointLockStatus status;
  bool lock_upgrade;
  bool previously_locked;
  if (tracked_locks_->IsPointLockSupported()) {
    status = tracked_locks_->GetPointLockStatus(cfh_id, key_str);
    previously_locked = status.locked;
    lock_upgrade = previously_locked && exclusive && !status.exclusive;
  } else {
    // If the record is tracked, we can assume it was locked, too.
    previously_locked = assume_tracked;
    status.locked = false;
    lock_upgrade = false;
  }

  // Lock this key if this transactions hasn't already locked it or we require
  // an upgrade.
  if (!previously_locked || lock_upgrade) {
    s = txn_db_impl_->TryLock(this, cfh_id, key_str, exclusive);
  }

  SetSnapshotIfNeeded();

  // Even though we do not care about doing conflict checking for this write,
  // we still need to take a lock to make sure we do not cause a conflict with
  // some other write.  However, we do not need to check if there have been
  // any writes since this transaction's snapshot.
  // TODO(agiardullo): could optimize by supporting shared txn locks in the
  // future
  SequenceNumber tracked_at_seq =
      status.locked ? status.seq : kMaxSequenceNumber;
  if (!do_validate || snapshot_ == nullptr) {
    if (assume_tracked && !previously_locked &&
        tracked_locks_->IsPointLockSupported()) {
      s = Status::InvalidArgument(
          "assume_tracked is set but it is not tracked yet");
    }
    // Need to remember the earliest sequence number that we know that this
    // key has not been modified after.  This is useful if this same
    // transaction
    // later tries to lock this key again.
    if (tracked_at_seq == kMaxSequenceNumber) {
      // Since we haven't checked a snapshot, we only know this key has not
      // been modified since after we locked it.
      // Note: when last_seq_same_as_publish_seq_==false this is less than the
      // latest allocated seq but it is ok since i) this is just a heuristic
      // used only as a hint to avoid actual check for conflicts, ii) this would
      // cause a false positive only if the snapthot is taken right after the
      // lock, which would be an unusual sequence.
      tracked_at_seq = db_->GetLatestSequenceNumber();
    }
  } else {
    // If a snapshot is set, we need to make sure the key hasn't been modified
    // since the snapshot.  This must be done after we locked the key.
    // If we already have validated an earilier snapshot it must has been
    // reflected in tracked_at_seq and ValidateSnapshot will return OK.
    if (s.ok()) {
      s = ValidateSnapshot(column_family, key, &tracked_at_seq);

      if (!s.ok()) {
        // Failed to validate key
        // Unlock key we just locked
        if (lock_upgrade) {
          s = txn_db_impl_->TryLock(this, cfh_id, key_str,
                                    false /* exclusive */);
          assert(s.ok());
        } else if (!previously_locked) {
          txn_db_impl_->UnLock(this, cfh_id, key.ToString());
        }
      }
    }
  }

  if (s.ok()) {
    // We must track all the locked keys so that we can unlock them later. If
    // the key is already locked, this func will update some stats on the
    // tracked key. It could also update the tracked_at_seq if it is lower
    // than the existing tracked key seq. These stats are necessary for
    // RollbackToSavePoint to determine whether a key can be safely removed
    // from tracked_keys_. Removal can only be done if a key was only locked
    // during the current savepoint.
    //
    // Recall that if assume_tracked is true, we assume that TrackKey has been
    // called previously since the last savepoint, with the same exclusive
    // setting, and at a lower sequence number, so skipping here should be
    // safe.
    if (!assume_tracked) {
      TrackKey(cfh_id, key_str, tracked_at_seq, read_only, exclusive);
    } else {
#ifndef NDEBUG
      if (tracked_locks_->IsPointLockSupported()) {
        PointLockStatus lock_status =
            tracked_locks_->GetPointLockStatus(cfh_id, key_str);
        assert(lock_status.locked);
        assert(lock_status.seq <= tracked_at_seq);
        assert(lock_status.exclusive == exclusive);
      }
#endif
    }
  }

  return s;
}

Status PessimisticTransaction::GetRangeLock(ColumnFamilyHandle* column_family,
                                            const Endpoint& start_endp,
                                            const Endpoint& end_endp) {
  ColumnFamilyHandle* cfh =
      column_family ? column_family : db_impl_->DefaultColumnFamily();
  uint32_t cfh_id = GetColumnFamilyID(cfh);

  Status s = txn_db_impl_->TryRangeLock(this, cfh_id, start_endp, end_endp);

  if (s.ok()) {
    RangeLockRequest req{cfh_id, start_endp, end_endp};
    tracked_locks_->Track(req);
  }
  return s;
}

// Return OK() if this key has not been modified more recently than the
// transaction snapshot_.
// tracked_at_seq is the global seq at which we either locked the key or already
// have done ValidateSnapshot.
Status PessimisticTransaction::ValidateSnapshot(
    ColumnFamilyHandle* column_family, const Slice& key,
    SequenceNumber* tracked_at_seq) {
  assert(snapshot_);

  SequenceNumber snap_seq = snapshot_->GetSequenceNumber();
  if (*tracked_at_seq <= snap_seq) {
    // If the key has been previous validated (or locked) at a sequence number
    // earlier than the current snapshot's sequence number, we already know it
    // has not been modified aftter snap_seq either.
    return Status::OK();
  }
  // Otherwise we have either
  // 1: tracked_at_seq == kMaxSequenceNumber, i.e., first time tracking the key
  // 2: snap_seq < tracked_at_seq: last time we lock the key was via
  // do_validate=false which means we had skipped ValidateSnapshot. In both
  // cases we should do ValidateSnapshot now.

  *tracked_at_seq = snap_seq;

  ColumnFamilyHandle* cfh =
      column_family ? column_family : db_impl_->DefaultColumnFamily();

  return TransactionUtil::CheckKeyForConflicts(
      db_impl_, cfh, key.ToString(), snap_seq, false /* cache_only */);
}

bool PessimisticTransaction::TryStealingLocks() {
  assert(IsExpired());
  TransactionState expected = STARTED;
  return std::atomic_compare_exchange_strong(&txn_state_, &expected,
                                             LOCKS_STOLEN);
}

void PessimisticTransaction::UnlockGetForUpdate(
    ColumnFamilyHandle* column_family, const Slice& key) {
  txn_db_impl_->UnLock(this, GetColumnFamilyID(column_family), key.ToString());
}

Status PessimisticTransaction::SetName(const TransactionName& name) {
  Status s;
  if (txn_state_ == STARTED) {
    if (name_.length()) {
      s = Status::InvalidArgument("Transaction has already been named.");
    } else if (txn_db_impl_->GetTransactionByName(name) != nullptr) {
      s = Status::InvalidArgument("Transaction name must be unique.");
    } else if (name.length() < 1 || name.length() > 512) {
      s = Status::InvalidArgument(
          "Transaction name length must be between 1 and 512 chars.");
    } else {
      name_ = name;
      txn_db_impl_->RegisterTransaction(this);
    }
  } else {
    s = Status::InvalidArgument("Transaction is beyond state for naming.");
  }
  return s;
}

}  // namespace ROCKSDB_NAMESPACE

#endif  // ROCKSDB_LITE
