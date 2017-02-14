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

#include "Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/TransactionContext.h"

#include <velocypack/Builder.h>

using namespace arangodb;
  
/// @brief constructor, leases a StringBuffer
transaction::StringBufferLeaser::StringBufferLeaser(transaction::Methods* trx) 
      : _transactionContext(trx->transactionContextPtr()),
        _stringBuffer(_transactionContext->leaseStringBuffer(32)) {
}

/// @brief constructor, leases a StringBuffer
transaction::StringBufferLeaser::StringBufferLeaser(TransactionContext* transactionContext) 
      : _transactionContext(transactionContext), 
        _stringBuffer(_transactionContext->leaseStringBuffer(32)) {
}

/// @brief destructor
transaction::StringBufferLeaser::~StringBufferLeaser() { 
  _transactionContext->returnStringBuffer(_stringBuffer);
}
  
/// @brief constructor, leases a builder
transaction::BuilderLeaser::BuilderLeaser(transaction::Methods* trx) 
      : _transactionContext(trx->transactionContextPtr()), 
        _builder(_transactionContext->leaseBuilder()) {
  TRI_ASSERT(_builder != nullptr);
}

/// @brief constructor, leases a builder
transaction::BuilderLeaser::BuilderLeaser(TransactionContext* transactionContext) 
      : _transactionContext(transactionContext), 
        _builder(_transactionContext->leaseBuilder()) {
  TRI_ASSERT(_builder != nullptr);
}

/// @brief destructor
transaction::BuilderLeaser::~BuilderLeaser() { 
  if (_builder != nullptr) {
    _transactionContext->returnBuilder(_builder); 
  }
}

