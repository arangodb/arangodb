////////////////////////////////////////////////////////////////////////////////
/// @brief standalone transaction context
///
/// @file 
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Utils/StandaloneTransactionContext.h"
#include "Utils/CollectionNameResolver.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the context
////////////////////////////////////////////////////////////////////////////////

StandaloneTransactionContext::StandaloneTransactionContext () 
  : TransactionContext(),
    _resolver(nullptr) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the context
////////////////////////////////////////////////////////////////////////////////
        
StandaloneTransactionContext::~StandaloneTransactionContext () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the resolver
////////////////////////////////////////////////////////////////////////////////

CollectionNameResolver const* StandaloneTransactionContext::getResolver () const { 
  TRI_ASSERT(_resolver != nullptr);
  return _resolver;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get parent transaction (if any)
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_t* StandaloneTransactionContext::getParentTransaction () const {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register the transaction in the context
////////////////////////////////////////////////////////////////////////////////

int StandaloneTransactionContext::registerTransaction (TRI_transaction_t* trx) {
  TRI_ASSERT(_resolver == nullptr);
  _resolver = new CollectionNameResolver(trx->_vocbase);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister the transaction from the context
////////////////////////////////////////////////////////////////////////////////

int StandaloneTransactionContext::unregisterTransaction () {
  if (_resolver != nullptr) {
    delete _resolver;
    _resolver = nullptr;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the transaction is embeddable
////////////////////////////////////////////////////////////////////////////////

bool StandaloneTransactionContext::isEmbeddable () const {
  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
