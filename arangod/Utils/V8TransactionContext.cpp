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
#include "Storage/Options.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/transaction.h"

#include "V8/v8-globals.h"
#include <v8.h>

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the context
////////////////////////////////////////////////////////////////////////////////

V8TransactionContext::V8TransactionContext(bool embeddable)
    : TransactionContext(),
      _sharedTransactionContext(static_cast<V8TransactionContext*>(
          static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData(
                                            V8DataSlot))->_transactionContext)),
      _resolver(nullptr),
      _options(),
      _currentTransaction(nullptr),
      _ownResolver(false),
      _ownOptions(false),
      _embeddable(embeddable) {
  // std::cout << TRI_CurrentThreadId() << ", V8TRANSACTIONCONTEXT " << this <<
  // " CTOR\r\n";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the context
////////////////////////////////////////////////////////////////////////////////

V8TransactionContext::~V8TransactionContext() {
  // std::cout << TRI_CurrentThreadId() << ", V8TRANSACTIONCONTEXT " << this <<
  // " DTOR\r\n";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the resolver
////////////////////////////////////////////////////////////////////////////////

CollectionNameResolver const* V8TransactionContext::getResolver() const {
  TRI_ASSERT(_sharedTransactionContext != nullptr);
  TRI_ASSERT(_sharedTransactionContext->_resolver != nullptr);
  return _sharedTransactionContext->_resolver;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the VPackOptions
////////////////////////////////////////////////////////////////////////////////

VPackOptions const* V8TransactionContext::getVPackOptions() const {
  TRI_ASSERT(_sharedTransactionContext != nullptr);
  return &_sharedTransactionContext->_options;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get parent transaction (if any)
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_t* V8TransactionContext::getParentTransaction() const {
  // std::cout << TRI_CurrentThreadId() << ", V8TRANSACTIONCONTEXT " << this <<
  // " GETPARENT: " << _sharedTransactionContext->_currentTransaction << "\r\n";
  TRI_ASSERT(_sharedTransactionContext != nullptr);
  return _sharedTransactionContext->_currentTransaction;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register the transaction in the context
////////////////////////////////////////////////////////////////////////////////

int V8TransactionContext::registerTransaction(TRI_transaction_t* trx) {
  TRI_ASSERT(_sharedTransactionContext != nullptr);
  TRI_ASSERT(_sharedTransactionContext->_currentTransaction ==
                       nullptr);
  _sharedTransactionContext->_currentTransaction = trx;

  // std::cout << TRI_CurrentThreadId() << ", V8TRANSACTIONCONTEXT " << this <<
  // " REGISTER: " << trx << "\r\n";
  if (_sharedTransactionContext->_resolver == nullptr) {
    _sharedTransactionContext->_resolver =
        new CollectionNameResolver(trx->_vocbase);
    _ownResolver = true;
  }

  if (_sharedTransactionContext->_options.customTypeHandler == nullptr) {
    _sharedTransactionContext->_options =
        arangodb::StorageOptions::getJsonToDocumentTemplate();
    _sharedTransactionContext->_options.customTypeHandler =
        arangodb::StorageOptions::createCustomHandler(
            _sharedTransactionContext->_resolver);
    _ownOptions = true;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the transaction from the context
////////////////////////////////////////////////////////////////////////////////

int V8TransactionContext::unregisterTransaction() {
  TRI_ASSERT(_sharedTransactionContext != nullptr);
  _sharedTransactionContext->_currentTransaction = nullptr;

  // std::cout << TRI_CurrentThreadId() << ", V8TRANSACTIONCONTEXT " << this <<
  // " UNREGISTER\r\n";

  if (_ownResolver && _sharedTransactionContext->_resolver != nullptr) {
    _ownResolver = false;
    delete _sharedTransactionContext->_resolver;
    _sharedTransactionContext->_resolver = nullptr;
  }

  if (_ownOptions &&
      _sharedTransactionContext->_options.customTypeHandler != nullptr) {
    _ownOptions = false;
    delete _sharedTransactionContext->_options.customTypeHandler;
    _sharedTransactionContext->_options.customTypeHandler = nullptr;
  }

  return TRI_ERROR_NO_ERROR;
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
/// @brief delete the resolver from the context
////////////////////////////////////////////////////////////////////////////////

void V8TransactionContext::deleteResolver() {
  TRI_ASSERT(hasResolver());
  delete _resolver;
  _resolver = nullptr;
  _ownResolver = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the transaction is embedded
////////////////////////////////////////////////////////////////////////////////

bool V8TransactionContext::IsEmbedded() {
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(
      v8::Isolate::GetCurrent()->GetData(V8DataSlot));
  if (v8g->_transactionContext == nullptr) {
    return false;
  }
  return static_cast<V8TransactionContext*>(v8g->_transactionContext)
             ->_currentTransaction != nullptr;
}
