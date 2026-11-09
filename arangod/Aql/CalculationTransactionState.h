////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "VocBase/vocbase.h"

namespace arangodb::aql {

/// @brief Dummy transaction state which does nothing but provides valid
/// statuses to keep ASSERT happy
class CalculationTransactionState final : public arangodb::TransactionState {
 public:
  explicit CalculationTransactionState(TRI_vocbase_t& vocbase,
                                       transaction::OperationOrigin trxType);
  ~CalculationTransactionState();

  [[nodiscard]] bool ensureSnapshot() override { return false; }

  /// @brief begin a transaction
  [[nodiscard]] futures::Future<Result> beginTransaction(
      arangodb::transaction::Hints) override;

  /// @brief commit a transaction
  [[nodiscard]] futures::Future<arangodb::Result> commitTransaction(
      arangodb::transaction::Methods*) override;

  /// @brief abort a transaction
  [[nodiscard]] arangodb::Result abortTransaction(
      arangodb::transaction::Methods*) override;

  Result triggerIntermediateCommit() override;

  [[nodiscard]] futures::Future<Result> performIntermediateCommitIfRequired(
      arangodb::DataSourceId collectionId) override;

  [[nodiscard]] uint64_t numPrimitiveOperations() const noexcept override {
    return 0;
  }

  [[nodiscard]] bool hasFailedOperations() const noexcept override {
    return false;
  }

  /// @brief number of commits, including intermediate commits
  [[nodiscard]] uint64_t numCommits() const noexcept override { return 0; }

  /// @brief number of intermediate commits
  [[nodiscard]] uint64_t numIntermediateCommits() const noexcept override {
    return 0;
  }

  void addIntermediateCommits(uint64_t /*value*/) override;

  [[nodiscard]] TRI_voc_tick_t lastOperationTick() const noexcept override {
    return 0;
  }

  std::unique_ptr<arangodb::TransactionCollection> createTransactionCollection(
      arangodb::DataSourceId cid,
      arangodb::AccessMode::Type accessType) override;
};

}  // namespace arangodb::aql