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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_TRANSACTION_WINDOW_H
#define ARANGODB_CACHE_TRANSACTION_WINDOW_H

#include <atomic>
#include <cstdint>

#include "Basics/ReadWriteSpinLock.h"
#include "Cache/Transaction.h"

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
  void end(Transaction* tx) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return the current window identifier.
  //////////////////////////////////////////////////////////////////////////////
  std::uint64_t term();

 private:
  struct Counters {
    uint64_t openReads : 21;
    uint64_t openWrites : 21;
    uint64_t openSensitive : 21;
  };
  static_assert(sizeof(Counters) == sizeof(uint64_t), "unexpected size");

  struct alignas(16) State {
    Counters counters;
    uint64_t term;
  };

  std::atomic<State> _state;
};

};  // end namespace cache
};  // end namespace arangodb

#endif
