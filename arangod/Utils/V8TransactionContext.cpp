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

#include "V8TransactionContext.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/transaction.h"

#include <v8.h>
#include "V8/v8-globals.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the context
////////////////////////////////////////////////////////////////////////////////

V8TransactionContext::V8TransactionContext(TRI_vocbase_t* vocbase,
                                           bool embeddable)
    : TransactionContext(vocbase),
      _sharedTransactionContext(static_cast<V8TransactionContext*>(
          static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData(
                                            V8PlatformFeature::V8_DATA_SLOT))
              ->_transactionContext)),
      _mainScope(nullptr),
      _currentTransaction(nullptr),
      _embeddable(embeddable) {}

//////////////////////////////////////////////////////////////////////////////
/// @brief order a custom type handler for the collection
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackCustomTypeHandler>
V8TransactionContext::orderCustomTypeHandler() {
  if (_customTypeHandler == nullptr) {
    V8TransactionContext* main = _sharedTransactionContext->_mainScope;

    if (main != nullptr && main != this && !main->isGlobal()) {
      _customTypeHandler = main->orderCustomTypeHandler();
    } else {
      _customTypeHandler.reset(
          TransactionContext::createCustomTypeHandler(_vocbase, getResolver()));
    }
    _options.customTypeHandler = _customTypeHandler.get();
    _dumpOptions.customTypeHandler = _customTypeHandler.get();
  }

  TRI_ASSERT(_customTypeHandler != nullptr);
  TRI_ASSERT(_options.customTypeHandler != nullptr);
  TRI_ASSERT(_dumpOptions.customTypeHandler != nullptr);
  return _customTypeHandler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the resolver
////////////////////////////////////////////////////////////////////////////////

CollectionNameResolver const* V8TransactionContext::getResolver() {
  if (_resolver == nullptr) {
    V8TransactionContext* main = _sharedTransactionContext->_mainScope;

    if (main != nullptr && main != this && !main->isGlobal()) {
      _resolver = main->getResolver();
    } else {
      TRI_ASSERT(_resolver == nullptr);
      _resolver = createResolver();
    }
  }

  TRI_ASSERT(_resolver != nullptr);
  return _resolver;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get parent transaction (if any)
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_t* V8TransactionContext::getParentTransaction() const {
  TRI_ASSERT(_sharedTransactionContext != nullptr);
  return _sharedTransactionContext->_currentTransaction;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register the transaction in the context
////////////////////////////////////////////////////////////////////////////////

int V8TransactionContext::registerTransaction(TRI_transaction_t* trx) {
  TRI_ASSERT(_sharedTransactionContext != nullptr);
  TRI_ASSERT(_sharedTransactionContext->_currentTransaction == nullptr);
  TRI_ASSERT(_sharedTransactionContext->_mainScope == nullptr);
  _sharedTransactionContext->_currentTransaction = trx;
  _sharedTransactionContext->_mainScope = this;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the transaction from the context
////////////////////////////////////////////////////////////////////////////////

void V8TransactionContext::unregisterTransaction() {
  TRI_ASSERT(_sharedTransactionContext != nullptr);
  _sharedTransactionContext->_currentTransaction = nullptr;
  _sharedTransactionContext->_mainScope = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction is embeddable
////////////////////////////////////////////////////////////////////////////////

bool V8TransactionContext::isEmbeddable() const { return _embeddable; }

////////////////////////////////////////////////////////////////////////////////
/// @brief make this context a global context
/// this is only called upon V8 context initialization
////////////////////////////////////////////////////////////////////////////////

void V8TransactionContext::makeGlobal() { _sharedTransactionContext = this; }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction context is a global one
////////////////////////////////////////////////////////////////////////////////

bool V8TransactionContext::isGlobal() const {
  return _sharedTransactionContext == this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the transaction is embedded
////////////////////////////////////////////////////////////////////////////////

bool V8TransactionContext::IsEmbedded() {
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(
      v8::Isolate::GetCurrent()->GetData(V8PlatformFeature::V8_DATA_SLOT));
  if (v8g->_transactionContext == nullptr) {
    return false;
  }
  return static_cast<V8TransactionContext*>(v8g->_transactionContext)
             ->_currentTransaction != nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a context, returned in a shared ptr
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<V8TransactionContext> V8TransactionContext::Create(
    TRI_vocbase_t* vocbase, bool embeddable) {
  return std::make_shared<V8TransactionContext>(vocbase, embeddable);
}
