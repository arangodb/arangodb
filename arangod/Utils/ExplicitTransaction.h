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

#ifndef ARANGOD_UTILS_EXPLICIT_TRANSACTION_H
#define ARANGOD_UTILS_EXPLICIT_TRANSACTION_H 1

#include "Basics/Common.h"

#include "Utils/Transaction.h"
#include "Utils/V8TransactionContext.h"
#include "VocBase/ticks.h"
#include "VocBase/transaction.h"

namespace arangodb {

class ExplicitTransaction : public Transaction {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the transaction
  //////////////////////////////////////////////////////////////////////////////

  ExplicitTransaction(std::shared_ptr<V8TransactionContext> transactionContext,
                      std::vector<std::string> const& readCollections,
                      std::vector<std::string> const& writeCollections,
                      double lockTimeout, bool waitForSync,
                      bool allowImplicitCollections)
      : Transaction(transactionContext) {
    this->addHint(TRI_TRANSACTION_HINT_LOCK_ENTIRELY, false);

    if (lockTimeout >= 0.0) {
      this->setTimeout(lockTimeout);
    }

    if (waitForSync) {
      this->setWaitForSync();
    }

    for (auto const& it : writeCollections) {
      this->addCollection(it, TRI_TRANSACTION_WRITE);
    }

    for (auto const& it : readCollections) {
      this->addCollection(it, TRI_TRANSACTION_READ);
    }
    
    this->setAllowImplicitCollections(allowImplicitCollections);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief end the transaction
  //////////////////////////////////////////////////////////////////////////////

  ~ExplicitTransaction() {}
};
}

#endif
