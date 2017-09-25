////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_TRANSACTION_WINDOW_H
#define ARANGODB_CACHE_TRANSACTION_WINDOW_H

#include "Basics/Common.h"
#include "Basics/ReadWriteSpinLock.h"
#include "Cache/Transaction.h"

#include <stdint.h>
#include <atomic>

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Manage global cache transactions.
///
/// Allows clients to start a transaction, end a transaction, and query an
/// identifier for the current window. If the identifier is even, there are no
/// ongoing sensitive transactions, and it is safe to store any values retrieved
/// from the backing store to transactional caches. If the identifier is odd,
/// then some values may be blacklisted by transactional caches (if they have
/// been written to the backing store in the current window).
////////////////////////////////////////////////////////////////////////////////
class TransactionManager {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initialize state with no open transactions.
  //////////////////////////////////////////////////////////////////////////////
  TransactionManager();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Open a new transaction.
  ///
  /// The transaction is considered read-only if it is guaranteed not to write
  /// to the backing store. A read-only transaction may, however, write to the
  /// cache.
  //////////////////////////////////////////////////////////////////////////////
  Transaction* begin(bool readOnly);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Signal the end of a transaction. Deletes the passed Transaction.
  //////////////////////////////////////////////////////////////////////////////
  void end(Transaction* tx);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return the current window identifier.
  //////////////////////////////////////////////////////////////////////////////
  uint64_t term();

 private:
  std::atomic<uint64_t> _openReads;
  std::atomic<uint64_t> _openSensitive;
  std::atomic<uint64_t> _openWrites;
  std::atomic<uint64_t> _term;
  std::atomic<bool> _lock;

  void lock();
  void unlock();
};

};  // end namespace cache
};  // end namespace arangodb

#endif
