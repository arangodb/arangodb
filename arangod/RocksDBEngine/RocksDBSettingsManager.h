////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_SETTINGS_MANAGER_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_SETTINGS_MANAGER_H 1

#include <rocksdb/types.h>
#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <atomic>

namespace rocksdb {
class DB;
class Transaction;
}  // namespace rocksdb

namespace arangodb {

class RocksDBSettingsManager {
  friend class RocksDBEngine;

  /// Constructor needs to be called synchronously,
  /// will load counts from the db and scan the WAL
  explicit RocksDBSettingsManager(rocksdb::TransactionDB* db);

 public:
//  struct CounterAdjustment {
//    rocksdb::SequenceNumber _sequenceNum = 0;
//    uint64_t _added = 0;
//    uint64_t _removed = 0;
//    TRI_voc_rid_t _revisionId = 0;  // used for revision id
//
//    CounterAdjustment() {}
//    CounterAdjustment(rocksdb::SequenceNumber seq, uint64_t added,
//                      uint64_t removed, TRI_voc_rid_t revisionId)
//        : _sequenceNum(seq),
//          _added(added),
//          _removed(removed),
//          _revisionId(revisionId) {}
//
//    rocksdb::SequenceNumber sequenceNumber() const { return _sequenceNum; };
//    uint64_t added() const { return _added; }
//    uint64_t removed() const { return _removed; }
//    TRI_voc_rid_t revisionId() const { return _revisionId; }
//  };
//
//  struct CMValue {
//    /// ArangoDB transaction ID
//    rocksdb::SequenceNumber _sequenceNum;
//    /// used for number of documents
//    uint64_t _added;
//    uint64_t _removed;
//    /// used for revision id
//    TRI_voc_rid_t _revisionId;
//
//    CMValue(rocksdb::SequenceNumber sq, uint64_t added, uint64_t removed, TRI_voc_rid_t rid)
//        : _sequenceNum(sq), _added(added), _removed(removed), _revisionId(rid) {}
//    explicit CMValue(arangodb::velocypack::Slice const&);
//    void serialize(arangodb::velocypack::Builder&) const;
//  };

 public:
  /// Retrieve initial settings values from database on engine startup
  void retrieveInitialValues();

  /// Thread-Safe force sync
  Result sync(bool force);

  // Earliest sequence number needed for recovery (don't throw out newer WALs)
  rocksdb::SequenceNumber earliestSeqNeeded() const;

 private:
  /// bump up the value of the last rocksdb::SequenceNumber we have seen
  /// and that is pending a sync update
  void setMaxUpdateSequenceNumber(rocksdb::SequenceNumber seqNo);
  
//  void loadCounterValues();
  void loadSettings();
//  void loadIndexEstimates();
//  void loadKeyGenerators();

  bool lockForSync(bool force);

  /// @brief a reusable builder, used inside sync() to serialize objects
  arangodb::velocypack::Builder _tmpBuilder;

  /// @brief counter values
//  std::unordered_map<uint64_t, CMValue> _counters;
//
//  /// @brief Key generator container
//  std::unordered_map<uint64_t, uint64_t> _generators;
//
//  /// @brief Index Estimator contianer.
//  ///        Note the elements in this container will be moved into the
//  ///        index classes and are only temporarily stored here during recovery.
//  std::unordered_map<uint64_t,
//                     std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>>
//      _estimators;

  /// @brief synced sequence numbers per object
//  std::unordered_map<uint64_t, rocksdb::SequenceNumber> _syncedSeqNums;

  /// @brief last sync sequence number
  rocksdb::SequenceNumber _lastSync;
  
  /// @brief currently syncing
  std::atomic<bool> _syncing;

  /// @brief rocksdb instance
  rocksdb::TransactionDB* _db;

  /// @brief protect _syncing and _counters
  mutable basics::ReadWriteLock _rwLock;

  TRI_voc_tick_t _initialReleasedTick;
  
  /// @brief the maximum sequence number that we have encountered
  /// when updating a counter value
  std::atomic<rocksdb::SequenceNumber> _maxUpdateSeqNo;
  
  /// @brief the last maximum sequence number we stored when we last synced
  /// all counters back to persistent storage
  /// if this is identical to _maxUpdateSeqNo, we do not need to write
  /// back any counter values to disk and can save I/O
  rocksdb::SequenceNumber _lastSyncedSeqNo;
};
}  // namespace arangodb

#endif
