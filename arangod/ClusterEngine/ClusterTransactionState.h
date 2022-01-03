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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "StorageEngine/TransactionState.h"
#include "VocBase/Identifiers/TransactionId.h"

struct TRI_vocbase_t;

namespace arangodb {
class Result;

namespace transaction {
struct Options;
}

/// @brief transaction type
class ClusterTransactionState final : public TransactionState {
 public:
  ClusterTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                          transaction::Options const& options);
  ~ClusterTransactionState() override = default;

  /// @brief begin a transaction
  [[nodiscard]] Result beginTransaction(transaction::Hints hints) override;

  /// @brief commit a transaction
  [[nodiscard]] Result commitTransaction(transaction::Methods* trx) override;

  /// @brief abort a transaction
  [[nodiscard]] Result abortTransaction(transaction::Methods* trx) override;

  [[nodiscard]] Result performIntermediateCommitIfRequired(
      DataSourceId cid) override;

  /// @brief return number of commits, including intermediate commits
  [[nodiscard]] uint64_t numCommits() const override;

  [[nodiscard]] bool hasFailedOperations() const override { return false; }

  [[nodiscard]] TRI_voc_tick_t lastOperationTick() const noexcept override;

 protected:
  std::unique_ptr<TransactionCollection> createTransactionCollection(
      DataSourceId cid, AccessMode::Type accessType) override;
};

}  // namespace arangodb
