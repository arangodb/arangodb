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
#include "VocBase/server.h"
#include "VocBase/transaction.h"

struct TRI_vocbase_t;

namespace arangodb {

class ExplicitTransaction : public Transaction {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the transaction
  //////////////////////////////////////////////////////////////////////////////

  ExplicitTransaction(TRI_vocbase_t* vocbase,
                      std::vector<std::string> const& readCollections,
                      std::vector<std::string> const& writeCollections,
                      double lockTimeout, bool waitForSync, bool embed,
                      bool allowImplicitCollections)
      : Transaction(new V8TransactionContext(embed), vocbase, 0) {
    this->addHint(TRI_TRANSACTION_HINT_LOCK_ENTIRELY, false);

    if (lockTimeout >= 0.0) {
      this->setTimeout(lockTimeout);
    }

    if (waitForSync) {
      this->setWaitForSync();
    }

    this->setAllowImplicitCollections(allowImplicitCollections);

    for (auto const& it : readCollections) {
      this->addCollection(it, TRI_TRANSACTION_READ);
    }

    for (auto const& it : writeCollections) {
      this->addCollection(it, TRI_TRANSACTION_WRITE);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create the transaction with cids
  //////////////////////////////////////////////////////////////////////////////

  ExplicitTransaction(TRI_vocbase_t* vocbase,
                      std::vector<TRI_voc_cid_t> const& readCollections,
                      std::vector<TRI_voc_cid_t> const& writeCollections,
                      double lockTimeout, bool waitForSync, bool embed)
      : Transaction(new V8TransactionContext(embed), vocbase, 0) {
    this->addHint(TRI_TRANSACTION_HINT_LOCK_ENTIRELY, false);

    if (lockTimeout >= 0.0) {
      this->setTimeout(lockTimeout);
    }

    if (waitForSync) {
      this->setWaitForSync();
    }

    for (auto const& it : readCollections) {
      this->addCollection(it, TRI_TRANSACTION_READ);
    }

    for (auto const& it : writeCollections) {
      this->addCollection(it, TRI_TRANSACTION_WRITE);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief end the transaction
  //////////////////////////////////////////////////////////////////////////////

  ~ExplicitTransaction() {}
};
}

#endif
