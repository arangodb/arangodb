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

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "Basics/Thread.h"
#include "Basics/Result.h"
#include <rocksdb/types.h>

namespace rocksdb {class Transaction; class Snapshot;}

namespace arangodb {

class RocksDBCounterManager : Thread {
public:
  
  struct Counter {
    rocksdb::SequenceNumber sequenceNumber;
    uint64_t count;
  };
  
  /// Constructor needs to be called synchrunously,
  /// will load counts from the db and scan the WAL
  RocksDBCounterManager(uint64_t interval);
  
  /// Thread-Safe load a counter
  uint64_t loadCounter(uint64_t objectId);
  
  /// collections / views / indexes can call this method to update
  /// their total counts. Thread-Safe needs the snapshot so we know
  /// the sequence number used
  void updateCounter(uint64_t objectId, rocksdb::Snapshot const* snapshot,
                     uint64_t counter);
  /// Thread-Safe force sync
  arangodb::Result sync();
  
 private:
  
  void loadCounterValues();
  
  basics::ReadWriteLock _rwLock;
  std::unordered_map<uint64_t, Counter> _counters;
  
  bool _syncing;
  uint64_t const _interval;
};
}

#endif
