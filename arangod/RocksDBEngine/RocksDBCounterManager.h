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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COUNTMANAGER_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COUNTMANAGER_H 1

#include <rocksdb/types.h>
#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Basics/Thread.h"
#include "RocksDBEngine/RocksDBTypes.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace rocksdb {
class DB;
class Transaction;
class Snapshot;
}

namespace arangodb {

class RocksDBCounterManager : Thread {
  friend class RocksDBEngine;
  RocksDBCounterManager(rocksdb::DB* db, double interval);

 public:
  struct Counter {
    rocksdb::SequenceNumber _sequenceNumber;
    uint64_t _count; // used for number of documents
    uint64_t _revisionId; // used for revision id

    Counter();
    explicit Counter(VPackSlice const&);
    Counter(rocksdb::SequenceNumber seq, uint64_t count, uint64_t revisionId)
      : _sequenceNumber(seq), _count(count), _revisionId(revisionId) {}

    void serialize(VPackBuilder&) const;
    void reset();
    uint64_t count() { return _count; }
    uint64_t revisionId() { return _revisionId; }
  };

  /// Constructor needs to be called synchrunously,
  /// will load counts from the db and scan the WAL

  /// Thread-Safe load a counter
  /// FIXME: is it possible to return a const& to a counter?
  Counter const loadCounter(uint64_t objectId);

  /// collections / views / indexes can call this method to update
  /// their total counts. Thread-Safe needs the snapshot so we know
  /// the sequence number used
  void updateCounter(uint64_t objectId, rocksdb::Snapshot const* snapshot,
                     int64_t delta, uint64_t revisionId);

  /// Thread-Safe remove a counter
  void removeCounter(uint64_t objectId);

  /// Thread-Safe force sync
  arangodb::Result sync();

  void beginShutdown() override;

 protected:
  void run() override;

 private:
  void readCounterValues();
  bool parseRocksWAL();
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief counter values
  //////////////////////////////////////////////////////////////////////////////
  std::unordered_map<uint64_t, Counter> _counters;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief synced sequence numbers
  //////////////////////////////////////////////////////////////////////////////
  std::unordered_map<uint64_t, rocksdb::SequenceNumber> _syncedSeqNum;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief currently syncing
  //////////////////////////////////////////////////////////////////////////////
  bool _syncing = false;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief rocsdb instance
  //////////////////////////////////////////////////////////////////////////////
  rocksdb::DB* _db;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief interval i which we will sync
  //////////////////////////////////////////////////////////////////////////////
  double const _interval;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief condition variable for heartbeat
  //////////////////////////////////////////////////////////////////////////////
  arangodb::basics::ConditionVariable _condition;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief protect _syncing and _counters
  //////////////////////////////////////////////////////////////////////////////
  basics::ReadWriteLock _rwLock;
};
}

#endif
