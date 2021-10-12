////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Transaction/Hints.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class Result;

namespace transaction {
class Methods;
}

class ITransactionable {
 public:
  virtual ~ITransactionable() = default;

  /// @brief begin a transaction
  [[nodiscard]] virtual arangodb::Result beginTransaction(transaction::Hints hints) = 0;

  /// @brief commit a transaction
  [[nodiscard]] virtual arangodb::Result commitTransaction(transaction::Methods* trx) = 0;

  /// @brief abort a transaction
  [[nodiscard]] virtual arangodb::Result abortTransaction(transaction::Methods* trx) = 0;

  /// @brief return number of commits.
  /// for cluster transactions on coordinator, this either returns 0 or 1.
  /// for leader, follower or single-server transactions, this can include any
  /// number, because it will also include intermediate commits.
  [[nodiscard]] virtual uint64_t numCommits() const = 0;

  [[nodiscard]] virtual bool hasFailedOperations() const = 0;

  [[nodiscard]] virtual TRI_voc_tick_t lastOperationTick() const noexcept = 0;

  virtual void beginQuery(bool isModificationQuery) = 0;
  virtual void endQuery(bool isModificationQuery) noexcept = 0;
};

}  // namespace arangodb
