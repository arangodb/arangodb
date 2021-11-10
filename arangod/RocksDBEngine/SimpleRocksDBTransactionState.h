////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBEngine/RocksDBTransactionState.h"
#include "VocBase/Identifiers/DataSourceId.h"

namespace arangodb {
class RocksDBTransactionMethods;

class SimpleRocksDBTransactionState final : public RocksDBTransactionState {
 public:
  SimpleRocksDBTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                                transaction::Options const& options);

  ~SimpleRocksDBTransactionState() override;

  /// @brief begin a transaction
  Result beginTransaction(transaction::Hints hints) override;

  RocksDBTransactionMethods* rocksdbMethods(DataSourceId) const override { return _rocksMethods.get(); }

  void beginQuery(bool isModificationQuery) override;
  void endQuery(bool isModificationQuery) noexcept override;

  /// @returns tick of last operation in a transaction
  /// @note the value is guaranteed to be valid only after
  ///       transaction is committed
  TRI_voc_tick_t lastOperationTick() const noexcept override;

  /// @brief number of commits, including intermediate commits
  uint64_t numCommits() const override;

  bool hasOperations() const noexcept override;

  uint64_t numOperations() const noexcept override;

  bool ensureSnapshot() override;

  rocksdb::SequenceNumber beginSeq() const override;

 protected:
  std::unique_ptr<TransactionCollection> createTransactionCollection(
      DataSourceId cid, AccessMode::Type accessType) override;

  Result doCommit() override;
  Result doAbort() override;

 private:
  void maybeDisableIndexing();

  /// @brief wrapper to use outside this class to access rocksdb
  std::unique_ptr<RocksDBTransactionMethods> _rocksMethods;
};

}  // namespace arangodb

