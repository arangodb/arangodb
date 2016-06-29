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
#include "Utils/CollectionNameResolver.h"
#include "VocBase/transaction.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the context
////////////////////////////////////////////////////////////////////////////////

StandaloneTransactionContext::StandaloneTransactionContext(TRI_vocbase_t* vocbase)
    : TransactionContext(vocbase) {
}

//////////////////////////////////////////////////////////////////////////////
/// @brief order a custom type handler for the collection
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackCustomTypeHandler> StandaloneTransactionContext::orderCustomTypeHandler() {
  if (_customTypeHandler == nullptr) {
    _customTypeHandler.reset(TransactionContext::createCustomTypeHandler(_vocbase, getResolver()));
    _options.customTypeHandler = _customTypeHandler.get();
    _dumpOptions.customTypeHandler = _customTypeHandler.get();
  }

  TRI_ASSERT(_customTypeHandler != nullptr);
  return _customTypeHandler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the resolver
////////////////////////////////////////////////////////////////////////////////

CollectionNameResolver const* StandaloneTransactionContext::getResolver() {
  if (_resolver == nullptr) {
    createResolver();
  }
  TRI_ASSERT(_resolver != nullptr);
  return _resolver;
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
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the transaction from the context
////////////////////////////////////////////////////////////////////////////////

void StandaloneTransactionContext::unregisterTransaction() {
  // nothing special to do. cleanup will be done by the parent's destructor
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction is embeddable
////////////////////////////////////////////////////////////////////////////////

bool StandaloneTransactionContext::isEmbeddable() const { return false; }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a context, returned in a shared ptr
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<StandaloneTransactionContext> StandaloneTransactionContext::Create(TRI_vocbase_t* vocbase) {
  return std::make_shared<StandaloneTransactionContext>(vocbase);
}

