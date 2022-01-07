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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "V8Context.h"
#include "Basics/Exceptions.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "V8Server/V8DealerFeature.h"

#include <v8.h>
#include "V8/v8-globals.h"

using namespace arangodb;

/// @brief create the context
transaction::V8Context::V8Context(TRI_vocbase_t& vocbase, bool embeddable)
    : Context(vocbase), _currentTransaction(nullptr), _embeddable(embeddable) {}

transaction::V8Context::~V8Context() noexcept {
  auto v8g = getV8State();
  if (v8g != nullptr && v8g->_transactionContext == this) {
    this->exitV8Context();
  }
}

/// @brief order a custom type handler for the collection
VPackCustomTypeHandler* transaction::V8Context::orderCustomTypeHandler() {
  if (_customTypeHandler == nullptr) {
    _customTypeHandler =
        transaction::Context::createCustomTypeHandler(_vocbase, resolver());
    _options.customTypeHandler = _customTypeHandler.get();
  }

  TRI_ASSERT(_customTypeHandler != nullptr);
  TRI_ASSERT(_options.customTypeHandler != nullptr);

  return _customTypeHandler.get();
}

/// @brief get transaction state, determine commit responsibility
/*virtual*/ std::shared_ptr<TransactionState>
transaction::V8Context::acquireState(transaction::Options const& options,
                                     bool& responsibleForCommit) {
  if (_currentTransaction) {
    responsibleForCommit = false;
    return _currentTransaction;
  }

  auto v8g = getV8State();
  TRI_ASSERT(v8g != nullptr);

  if (v8g->_transactionContext != nullptr) {
    _currentTransaction = v8g->_transactionContext->_currentTransaction;
  } else if (v8g->_transactionState) {
    _currentTransaction = v8g->_transactionState;
  }

  if (!_currentTransaction) {
    _currentTransaction = transaction::Context::createState(options);
    responsibleForCommit = true;
  } else {
    if (!isEmbeddable()) {
      // we are embedded but this is disallowed...
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_NESTED);
    }
    responsibleForCommit = false;
  }

  return _currentTransaction;
}

void transaction::V8Context::enterV8Context() {
  // registerTransaction
  auto v8g = getV8State();
  TRI_ASSERT(v8g != nullptr);

  TRI_ASSERT(_currentTransaction != nullptr);
  TRI_ASSERT(v8g->_transactionContext == nullptr ||
             v8g->_transactionContext == this);

  v8g->_transactionContext = this;
}

void transaction::V8Context::exitV8Context() noexcept {
  auto v8g = getV8State();
  if (v8g != nullptr && v8g->_transactionContext == this) {
    v8g->_transactionContext = nullptr;
  }
}

/// @brief unregister the transaction from the context
void transaction::V8Context::unregisterTransaction() noexcept {
  exitV8Context();
}

std::shared_ptr<transaction::Context> transaction::V8Context::clone() const {
  // intentionally create a StandaloneContext and no V8Context.
  // We cannot clone V8Contexts into V8Contexts, as this will mess up the
  // v8 isolates and stuff.
  // This comes with the consequence that one cannot run any JavaScript
  // code in the cloned context!
  TRI_ASSERT(_currentTransaction != nullptr);
  auto clone = std::make_shared<transaction::StandaloneContext>(_vocbase);
  clone->setState(_currentTransaction);
  return clone;
}

/// @brief whether or not the transaction is embeddable
bool transaction::V8Context::isEmbeddable() const { return _embeddable; }

/// @brief return parent transaction state or none
/*static*/ std::shared_ptr<TransactionState>
transaction::V8Context::getParentState() {
  auto v8g = getV8State();
  if (v8g == nullptr || v8g->_transactionContext == nullptr) {
    return nullptr;
  }
  return static_cast<transaction::V8Context*>(v8g->_transactionContext)
      ->_currentTransaction;
}

/// @brief check whether the transaction is embedded
/*static*/ bool transaction::V8Context::isEmbedded() {
  return (getParentState() != nullptr);
}

/// @brief create a context, returned in a shared ptr
std::shared_ptr<transaction::V8Context> transaction::V8Context::Create(
    TRI_vocbase_t& vocbase, bool embeddable) {
  return std::make_shared<transaction::V8Context>(vocbase, embeddable);
}

std::shared_ptr<transaction::Context>
transaction::V8Context::CreateWhenRequired(TRI_vocbase_t& vocbase,
                                           bool embeddable) {
  // is V8 enabled and are currently in a V8 scope ?
  if (vocbase.server().hasFeature<V8DealerFeature>() &&
      vocbase.server().isEnabled<V8DealerFeature>() &&
      v8::Isolate::GetCurrent() != nullptr) {
    return transaction::V8Context::Create(vocbase, embeddable);
  }

  return transaction::StandaloneContext::Create(vocbase);
}

/*static*/ TRI_v8_global_t* transaction::V8Context::getV8State() noexcept {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (isolate == nullptr) {
    return nullptr;
  }
  auto v8g = static_cast<TRI_v8_global_t*>(
      isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT));
  return v8g;
}
