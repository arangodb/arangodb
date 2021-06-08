////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <velocypack/Buffer.h>

namespace rocksdb {
class Slice;
}

namespace arangodb {
namespace transaction {
class Methods;
}

/// @brief last state (last looked-at-key, transaction state) used by a
/// RocksDB iterator. the purpose of the state tracker is to find out if an
/// existing RocksDB Iterator needs to be rebuilt after an intermediate
/// commit. Intermediate commits tamper with the rocksdb::Transaction's
/// internals, which can also affect Iterators handed out by this transaction.
/// the only safe way to continue working with such Iterators is to recreate
/// and reposition them.
class RocksDBIteratorStateTracker {
 public:
  RocksDBIteratorStateTracker(RocksDBIteratorStateTracker const&) = delete;
  RocksDBIteratorStateTracker& operator=(RocksDBIteratorStateTracker const&) = delete;

  explicit RocksDBIteratorStateTracker(transaction::Methods* trx);
  ~RocksDBIteratorStateTracker();

  /// @brief whether or not tracking is active. as tracking can have a minimal
  /// performance overhead, it is turned off where not needed (read-only transactions)
  bool isActive() const noexcept;

  /// @brief track last seen key by a RocksDB Iterator
  void trackKey(rocksdb::Slice key);
  
  /// @brief reset our state tracking (i.e. forget about last seen key)
  void reset();

  /// @brief whether or not existing rocksdb::Iterators should be rebuilt
  bool mustRebuildIterator() const;

  /// @brief last tracked key
  rocksdb::Slice key() const;

 private:
  /// @brief return the transaction's current intermediate commit id
  uint64_t currentIntermediateCommitId() const;

  /// @brief deactivate the tracking
  void deactivate();

 private:
  transaction::Methods* _trx;
  arangodb::velocypack::Buffer<uint8_t> _key;
  uint64_t _intermediateCommitId;
};

}  // namespace arangodb
