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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Transaction/SmartContext.h"

namespace arangodb::transaction {

/// @brief Acquire a transaction from the Manager
struct ManagedContext final : public SmartContext {
  ManagedContext(TransactionId globalId,
                 std::shared_ptr<TransactionState> state,
                 bool responsibleForCommit, bool cloned);

  ManagedContext(TransactionId globalId,
                 std::shared_ptr<TransactionState> state,
                 TransactionContextSideUser /*sideUser*/);

  ~ManagedContext();

  /// @brief get transaction state, determine commit responsiblity
  std::shared_ptr<TransactionState> acquireState(
      transaction::Options const& options, bool& responsibleForCommit) override;

  /// @brief unregister the transaction
  void unregisterTransaction() noexcept override;

  std::shared_ptr<Context> clone() const override;

 private:
  bool const _responsibleForCommit;
  bool const _cloned;
  bool const _isSideUser;
};

}  // namespace arangodb::transaction
