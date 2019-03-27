////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_TRANSACTION_GLOBAL_CONTEXT_H
#define ARANGOD_TRANSACTION_GLOBAL_CONTEXT_H 1

#include "Basics/Common.h"
#include "Transaction/Context.h"
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
/// same TransactionState instance will be used across shards on the same server.
class SmartContext : public Context {
 public:

  SmartContext(TRI_vocbase_t& vocbase, TRI_voc_tid_t globalId,
               TransactionState* state);
    
  /// @brief destroy the context
  ~SmartContext();

  /// @brief order a custom type handler
  std::shared_ptr<arangodb::velocypack::CustomTypeHandler> orderCustomTypeHandler() override final;

  /// @brief return the resolver
  CollectionNameResolver const& resolver() override final;

  /// @brief whether or not the transaction is embeddable
  bool isEmbeddable() const override final {
    return true;
  }
  
  /// @brief locally persisted transaction ID
  TRI_voc_tid_t generateId() const override final;
  
 protected:
  /// @brief ID of the transaction to use
  TRI_voc_tid_t const _globalId;
  arangodb::TransactionState* _state;
};
  
/// @brief Acquire a transaction from the Manager
struct ManagedContext final : public SmartContext {
  
  ManagedContext(TRI_voc_tid_t globalId, TransactionState* state,
                 AccessMode::Type mode);
  
  ~ManagedContext();
  
  /// @brief get parent transaction (if any)
  TransactionState* getParentTransaction() const override;

  /// @brief register the transaction,
  void registerTransaction(TransactionState*) override {
    TRI_ASSERT(false);
  }

  /// @brief unregister the transaction
  void unregisterTransaction() noexcept override;
  
private:
  AccessMode::Type _mode;
};

/// Used for a standalone AQL query. Always creates the state first.
/// Registers the TransactionState with the manager
struct AQLStandaloneContext final : public SmartContext {
  
  AQLStandaloneContext(TRI_vocbase_t& vocbase, TRI_voc_tid_t globalId)
    : SmartContext(vocbase, globalId, nullptr) {}

  /// @brief get parent transaction (if any)
  TransactionState* getParentTransaction() const override;

  /// @brief register the transaction,
  void registerTransaction(TransactionState*) override;

  /// @brief unregister the transaction
  void unregisterTransaction() noexcept override;
};
  
/// Can be used to reuse transaction state between multiple
/// transaction::Methods instances. Mainly for legacy clients
/// that do not send the transaction ID header
struct StandaloneSmartContext final : public SmartContext {
  
  explicit StandaloneSmartContext(TRI_vocbase_t& vocbase);
  
  /// @brief get parent transaction (if any)
  TransactionState* getParentTransaction() const override;
  
  /// @brief register the transaction,
  void registerTransaction(TransactionState*) override;
  
  /// @brief unregister the transaction
  void unregisterTransaction() noexcept override;
};

}  // namespace transaction
}  // namespace arangodb

#endif
