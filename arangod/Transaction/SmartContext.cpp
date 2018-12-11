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

#include "SmartContext.h"
#include "StorageEngine/TransactionState.h"
#include "StorageEngine/TransactionManager.h"
#include "Utils/CollectionNameResolver.h"

struct TRI_vocbase_t;

namespace arangodb {
  
/// @brief create the context
transaction::SmartContext::SmartContext(TRI_vocbase_t& vocbase)
  : Context(vocbase), _state(nullptr) {}

/// @brief order a custom type handler for the collection
std::shared_ptr<arangodb::velocypack::CustomTypeHandler> transaction::SmartContext::orderCustomTypeHandler() {
  if (_customTypeHandler == nullptr) {
    _customTypeHandler.reset(
      transaction::Context::createCustomTypeHandler(_vocbase, resolver())
    );
    _options.customTypeHandler = _customTypeHandler.get();
    _dumpOptions.customTypeHandler = _customTypeHandler.get();
  }

  TRI_ASSERT(_customTypeHandler != nullptr);

  return _customTypeHandler;
}

/// @brief return the resolver
CollectionNameResolver const& transaction::SmartContext::resolver() {
  if (_resolver == nullptr) {
    createResolver();
  }
  
  TRI_ASSERT(_resolver != nullptr);
  
  return *_resolver;
}
  
/// @brief get parent transaction (if any) and increase nesting
TransactionState* transaction::SmartContext::getParentTransaction() const {
  return _state;
}

/// @brief register the transaction, so other Method instances can get it
void transaction::SmartContext::registerTransaction(TransactionState* state) {
  TRI_ASSERT(_state == nullptr);
  _state = state;
}
  
/// @brief unregister the transaction
void transaction::SmartContext::unregisterTransaction() noexcept {
  _state = nullptr;
}
  
/// @brief whether or not the transaction is embeddable
bool transaction::SmartContext::isEmbeddable() const {
  return true;
}

std::shared_ptr<transaction::Context> transaction::SmartContext::Create(TRI_vocbase_t& vocbase) {
    return std::make_shared<transaction::SmartContext>(vocbase);
}
} // arangodb
