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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SmartContext.h"

#include "Transaction/OperationOrigin.h"
#include "VocBase/vocbase.h"

#include <memory>

struct TRI_vocbase_t;

namespace arangodb {
class TransactionState;

namespace transaction {

/// Can be used to reuse transaction state between multiple
/// transaction::Methods instances.
struct StandaloneContext final : public SmartContext {
  explicit StandaloneContext(TRI_vocbase_t& vocbase,
                             OperationOrigin operationOrigin);

  /// @brief get transaction state, determine commit responsiblity
  std::shared_ptr<TransactionState> acquireState(
      transaction::Options const& options, bool& responsibleForCommit) override;

  /// @brief unregister the transaction
  void unregisterTransaction() noexcept override;

  std::shared_ptr<Context> clone() const override;

  /// @brief create a context, returned in a shared ptr
  static std::shared_ptr<transaction::Context> create(
      TRI_vocbase_t& vocbase, OperationOrigin operationOrigin);
};

}  // namespace transaction
}  // namespace arangodb
