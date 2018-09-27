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

#include "Context.h"
#include "Basics/Common.h"
#include "VocBase/vocbase.h"

struct TRI_vocbase_t;

namespace arangodb {

class TransactionState;

namespace transaction {

/// transaction context that will manage the creation or acquisition of a TransactionState
/// for transaction::Methods instances for cluster wide transactions. Cluster wide transactions
/// essentially just mean that all operations will use a consistent transaction ID and
/// on the same server the same TransactionState instance will be used.
/// The class supports three different use-cases
/// (1) Constructor with TID and Type::Default can be used to share a TransactionState between
///     multiple transaction::Methods instances
/// (2) Constructor with TID and Type::Global will try to lease an already existing TransactionState
///     from the TransactionManager. This supports global transaction with explicit begin / end requests
/// (3) Construcor with TransactionState* is used to manage a global transaction
class SmartContext final : public Context {
 public:
  
  /// @brief create the context, with given TID
  explicit SmartContext(TRI_vocbase_t& vocbase);

  /// @brief destroy the context
  ~SmartContext() = default;

  /// @brief order a custom type handler
  std::shared_ptr<arangodb::velocypack::CustomTypeHandler>
  orderCustomTypeHandler() override final;

  /// @brief return the resolver
  CollectionNameResolver const& resolver() override final;

  /// @brief get parent transaction (if any) and increase nesting
  TransactionState* getParentTransaction() const override;

  /// @brief register the transaction,
  void registerTransaction(TransactionState*) override;

  /// @brief unregister the transaction
  void unregisterTransaction() noexcept override;

  /// @brief whether or not the transaction is embeddable
  bool isEmbeddable() const override;
  
  static std::shared_ptr<Context> Create(TRI_vocbase_t&);
  
private:
  /// @brief managed TransactionState
  TransactionState *_state;
};

}
}

#endif
