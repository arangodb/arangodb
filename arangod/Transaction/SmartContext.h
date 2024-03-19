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

#include "Transaction/Context.h"
#include "Transaction/OperationOrigin.h"
#include "VocBase/vocbase.h"
#include "VocBase/AccessMode.h"

struct TRI_vocbase_t;

namespace arangodb {

class TransactionState;

namespace transaction {

/// Context that will manage the creation or acquisition of a
/// TransactionState for transaction::Methods instances for cluster wide
/// transactions. Cluster wide transactions essentially just mean that all
/// operations will use a consistent transaction ID and on the same server the
/// same TransactionState instance will be used across shards on the same
/// server.
class SmartContext : public Context {
 public:
  SmartContext(TRI_vocbase_t& vocbase, TransactionId globalId,
               std::shared_ptr<TransactionState> state,
               OperationOrigin operationOrigin);

  /// @brief destroy the context
  ~SmartContext();

  /// @brief order a custom type handler
  velocypack::CustomTypeHandler* orderCustomTypeHandler() override final;

  /// @brief whether or not the transaction is embeddable
  bool isEmbeddable() const override final { return true; }

  /// @brief locally persisted transaction ID
  TransactionId generateId() const override final;

  bool isStateSet() const noexcept { return _state != nullptr; }

  void setState(
      std::shared_ptr<arangodb::TransactionState> const& state) noexcept {
    _state = state;
  }

 protected:
  /// @brief ID of the transaction to use
  TransactionId const _globalId;
  std::shared_ptr<arangodb::TransactionState> _state;
};

struct TransactionContextSideUser {};

/// Used for a standalone AQL query. Always creates the state first.
/// Registers the TransactionState with the manager
struct AQLStandaloneContext final : public SmartContext {
  AQLStandaloneContext(TRI_vocbase_t& vocbase, TransactionId globalId,
                       OperationOrigin operationOrigin)
      : SmartContext(vocbase, globalId, nullptr, operationOrigin) {}

  /// @brief get transaction state, determine commit responsiblity
  std::shared_ptr<TransactionState> acquireState(
      transaction::Options const& options, bool& responsibleForCommit) override;

  /// @brief unregister the transaction
  void unregisterTransaction() noexcept override;

  std::shared_ptr<Context> clone() const override;
};

}  // namespace transaction
}  // namespace arangodb
