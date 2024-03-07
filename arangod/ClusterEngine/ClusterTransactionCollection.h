////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "StorageEngine/TransactionCollection.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace transaction {
class Methods;
}
class TransactionState;

/// @brief collection used in a transaction
class ClusterTransactionCollection final : public TransactionCollection {
 public:
  ClusterTransactionCollection(TransactionState* trx, DataSourceId cid,
                               AccessMode::Type accessType);
  ~ClusterTransactionCollection();

  /// @brief whether or not any write operations for the collection happened
  bool hasOperations() const override;

  bool canAccess(AccessMode::Type accessType) const override;
  futures::Future<Result> lockUsage() override;
  void releaseUsage() override;

 private:
  /// @brief request a lock for a collection
  /// returns TRI_ERROR_LOCKED in case the lock was successfully acquired
  /// returns TRI_ERROR_NO_ERROR in case the lock does not need to be acquired
  /// and no other error occurred returns any other error code otherwise
  futures::Future<Result> doLock(AccessMode::Type) override;

  /// @brief request an unlock for a collection
  Result doUnlock(AccessMode::Type) override;
};
}  // namespace arangodb
