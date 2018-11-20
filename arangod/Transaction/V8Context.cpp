////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "V8Context.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "V8Server/V8DealerFeature.h"

#include <v8.h>
#include "V8/v8-globals.h"

using namespace arangodb;

/// @brief create the context
transaction::V8Context::V8Context(TRI_vocbase_t& vocbase, bool embeddable)
  : Context(vocbase),
      _sharedTransactionContext(nullptr),
      _mainScope(nullptr),
      _currentTransaction(nullptr),
      _embeddable(embeddable) {
      // need to set everything here
      TRI_GET_GLOBALS2(v8::Isolate::GetCurrent());
      _sharedTransactionContext = static_cast<transaction::V8Context*>(v8g->_transactionContext);
    }

/// @brief order a custom type handler for the collection
std::shared_ptr<VPackCustomTypeHandler>
transaction::V8Context::orderCustomTypeHandler() {
  if (_customTypeHandler == nullptr) {
    transaction::V8Context* main = _sharedTransactionContext->_mainScope;

    if (main != nullptr && main != this && !main->isGlobal()) {
      _customTypeHandler = main->orderCustomTypeHandler();
    } else {
      _customTypeHandler.reset(transaction::Context::createCustomTypeHandler(
        _vocbase, resolver()
      ));
    }

    _options.customTypeHandler = _customTypeHandler.get();
    _dumpOptions.customTypeHandler = _customTypeHandler.get();
  }

  TRI_ASSERT(_customTypeHandler != nullptr);
  TRI_ASSERT(_options.customTypeHandler != nullptr);
  TRI_ASSERT(_dumpOptions.customTypeHandler != nullptr);

  return _customTypeHandler;
}

/// @brief return the resolver
CollectionNameResolver const& transaction::V8Context::resolver() {
  if (_resolver == nullptr) {
    transaction::V8Context* main = _sharedTransactionContext->_mainScope;

    if (main != nullptr && main != this && !main->isGlobal()) {
      _resolver = &(main->resolver());
    } else {
      TRI_ASSERT(_resolver == nullptr);
      createResolver(); // sets _resolver
    }
  }

  TRI_ASSERT(_resolver != nullptr);

  return *_resolver;
}

/// @brief get parent transaction (if any)
TransactionState* transaction::V8Context::getParentTransaction() const {
  TRI_ASSERT(_sharedTransactionContext != nullptr);
  return _sharedTransactionContext->_currentTransaction;
}

/// @brief register the transaction in the context
void transaction::V8Context::registerTransaction(TransactionState* trx) {
  TRI_ASSERT(_sharedTransactionContext != nullptr);
  TRI_ASSERT(_sharedTransactionContext->_currentTransaction == nullptr);
  TRI_ASSERT(_sharedTransactionContext->_mainScope == nullptr);
  _sharedTransactionContext->_currentTransaction = trx;
  _sharedTransactionContext->_mainScope = this;
}

/// @brief unregister the transaction from the context
void transaction::V8Context::unregisterTransaction() noexcept {
  TRI_ASSERT(_sharedTransactionContext != nullptr);
  _sharedTransactionContext->_currentTransaction = nullptr;
  _sharedTransactionContext->_mainScope = nullptr;
}

/// @brief whether or not the transaction is embeddable
bool transaction::V8Context::isEmbeddable() const { return _embeddable; }

/// @brief make this context a global context
/// this is only called upon V8 context initialization
void transaction::V8Context::makeGlobal() { _sharedTransactionContext = this; }

/// @brief whether or not the transaction context is a global one
bool transaction::V8Context::isGlobal() const {
  return _sharedTransactionContext == this;
}

/// @brief return parent transaction state or none
TransactionState* transaction::V8Context::getParentState() {
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(
      v8::Isolate::GetCurrent()->GetData(V8PlatformFeature::V8_DATA_SLOT));
  if (v8g == nullptr ||
      v8g->_transactionContext == nullptr) {
    return nullptr;
  }
  return static_cast<transaction::V8Context*>(v8g->_transactionContext)
             ->_currentTransaction;
}

/// @brief check whether the transaction is embedded
bool transaction::V8Context::isEmbedded() {
  return (getParentState() != nullptr);
}

/// @brief create a context, returned in a shared ptr
std::shared_ptr<transaction::V8Context> transaction::V8Context::Create(
    TRI_vocbase_t& vocbase,
    bool embeddable
) {
  return std::make_shared<transaction::V8Context>(vocbase, embeddable);
}

std::shared_ptr<transaction::Context> transaction::V8Context::CreateWhenRequired(
    TRI_vocbase_t& vocbase,
    bool embeddable
) {
  // is V8 enabled and are currently in a V8 scope ?
  if (V8DealerFeature::DEALER != nullptr &&
      v8::Isolate::GetCurrent() != nullptr) {
    return Create(vocbase, embeddable);
  }

  return transaction::StandaloneContext::Create(vocbase);
}
