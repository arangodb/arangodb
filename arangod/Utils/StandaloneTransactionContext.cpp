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

#include "StandaloneTransactionContext.h"
#include "Storage/Options.h"
#include "Utils/CollectionNameResolver.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the context
////////////////////////////////////////////////////////////////////////////////

StandaloneTransactionContext::StandaloneTransactionContext()
    : TransactionContext(), _resolver(nullptr), _options() {
  // std::cout << TRI_CurrentThreadId() << ", STANDALONETRANSACTIONCONTEXT
  // CTOR\r\n";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the context
////////////////////////////////////////////////////////////////////////////////

StandaloneTransactionContext::~StandaloneTransactionContext() {
  // std::cout << TRI_CurrentThreadId() << ", STANDALONETRANSACTIONCONTEXT
  // DTOR\r\n";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the resolver
////////////////////////////////////////////////////////////////////////////////

CollectionNameResolver const* StandaloneTransactionContext::getResolver()
    const {
  TRI_ASSERT(_resolver != nullptr);
  return _resolver;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the VPackOptions
////////////////////////////////////////////////////////////////////////////////

VPackOptions const* StandaloneTransactionContext::getVPackOptions() const {
  return &_options;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get parent transaction (if any)
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_t* StandaloneTransactionContext::getParentTransaction() const {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register the transaction in the context
////////////////////////////////////////////////////////////////////////////////

int StandaloneTransactionContext::registerTransaction(TRI_transaction_t* trx) {
  if (_resolver == nullptr) {
    _resolver = new CollectionNameResolver(trx->_vocbase);

    _options = arangodb::StorageOptions::getJsonToDocumentTemplate();
    _options.customTypeHandler =
        arangodb::StorageOptions::createCustomHandler(_resolver);
  }
  // std::cout << TRI_CurrentThreadId() << ", STANDALONETRANSACTIONCONTEXT
  // REGISTER: " << trx << "\r\n";

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the transaction from the context
////////////////////////////////////////////////////////////////////////////////

int StandaloneTransactionContext::unregisterTransaction() {
  delete _resolver;
  _resolver = nullptr;

  if (_options.customTypeHandler != nullptr) {
    delete _options.customTypeHandler;
    _options.customTypeHandler = nullptr;
  }
  // std::cout << TRI_CurrentThreadId() << ", STANDALONETRANSACTIONCONTEXT
  // UNREGISTER\r\n";

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction is embeddable
////////////////////////////////////////////////////////////////////////////////

bool StandaloneTransactionContext::isEmbeddable() const { return false; }
