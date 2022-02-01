////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/RocksDBTransactionCollection.h"

namespace arangodb {
class RocksDBTransactionMethods;
class ReplicatedRocksDBTransactionState;

class ReplicatedRocksDBTransactionCollection final
    : public RocksDBTransactionCollection {
 public:
  ReplicatedRocksDBTransactionCollection(ReplicatedRocksDBTransactionState* trx,
                                         DataSourceId cid,
                                         AccessMode::Type accessType);
  ~ReplicatedRocksDBTransactionCollection();

  Result beginTransaction();
  Result commitTransaction();
  Result abortTransaction();

  RocksDBTransactionMethods* rocksdbMethods() const {
    return _rocksMethods.get();
  }

  void beginQuery(bool isModificationQuery);
  void endQuery(bool isModificationQuery) noexcept;

  /// @returns tick of last operation in a transaction
  /// @note the value is guaranteed to be valid only after
  ///       transaction is committed
  TRI_voc_tick_t lastOperationTick() const noexcept;

  /// @brief number of commits, including intermediate commits
  uint64_t numCommits() const;

  uint64_t numOperations() const noexcept;

  bool ensureSnapshot();

 private:
  void maybeDisableIndexing();

  /// @brief wrapper to use outside this class to access rocksdb
  std::unique_ptr<RocksDBTransactionMethods> _rocksMethods;
};

}  // namespace arangodb
