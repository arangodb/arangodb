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

#ifndef ARANGOD_STORAGE_ENGINE_TRANSACTION_MANAGER_H
#define ARANGOD_STORAGE_ENGINE_TRANSACTION_MANAGER_H 1

#include <chrono>

#include "Basics/Common.h"
#include "VocBase/voc-types.h"

namespace arangodb {

// to be derived by storage engines
struct TransactionData {
  virtual ~TransactionData() = default;
};

class TransactionManager {
 public:
  TransactionManager() {}
  virtual ~TransactionManager() = default;

  // register a transaction
  virtual void registerTransaction(TRI_voc_tid_t transactionId,
                                   bool isReadOnlyTransaction) = 0;

  // unregister a transaction
  virtual void unregisterTransaction(TRI_voc_tid_t transactionId, bool isReadOnlyTransaction) = 0;

  virtual uint64_t getActiveTransactionCount() = 0;

  // functions to block transactions during hotbackup or restore
  //  (not yet implemented/needed in all storage engines)
  // holdTransactions: true on successful hold, false on timeout or error
  virtual bool holdTransactions(uint64_t timeout) {return true;}
  virtual bool holdTransactions(std::chrono::microseconds timeout) {return true;}

  virtual void releaseTransactions() {}
};

}  // namespace arangodb

#endif
