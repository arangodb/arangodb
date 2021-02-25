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

#ifndef ARANGOD_CLUSTER_CLUSTER_TRANSACTION_STATE_H
#define ARANGOD_CLUSTER_CLUSTER_TRANSACTION_STATE_H 1

#include "StorageEngine/TransactionState.h"

namespace arangodb {

/// @brief transaction type
class ClusterTransactionState final : public TransactionState {
 public:
  ClusterTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                          transaction::Options const& options);
  ~ClusterTransactionState() = default;

  /// @brief begin a transaction
  Result beginTransaction(transaction::Hints hints) override;

  /// @brief commit a transaction
  Result commitTransaction(transaction::Methods* trx) override;

  /// @brief abort a transaction
  Result abortTransaction(transaction::Methods* trx) override;
  
  /// @brief return number of commits, including intermediate commits
  uint64_t numCommits() const override;

  bool hasFailedOperations() const override { return false; }
};

}  // namespace arangodb

#endif
