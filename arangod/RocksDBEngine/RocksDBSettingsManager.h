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
  /// Retrieve initial settings values from database on engine startup
  void retrieveInitialValues();

  /// Thread-Safe force sync
  Result sync(bool force);

  // Earliest sequence number needed for recovery (don't throw out newer WALs)
  rocksdb::SequenceNumber earliestSeqNeeded() const;

 private:
  void loadSettings();

  bool lockForSync(bool force);

 private:
  /// @brief a reusable builder, used inside sync() to serialize objects
  arangodb::velocypack::Builder _tmpBuilder;

  /// @brief last sync sequence number
  std::atomic<rocksdb::SequenceNumber> _lastSync;

  /// @brief currently syncing
  std::atomic<bool> _syncing;

  /// @brief rocksdb instance
  rocksdb::DB* _db;

  TRI_voc_tick_t _initialReleasedTick;
};
}  // namespace arangodb

#endif
