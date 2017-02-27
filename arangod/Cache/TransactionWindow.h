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

#include <stdint.h>
#include <atomic>

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Manage windows in time when there are either no ongoing transactions
/// or some.
///
/// Allows clients to start a transaction, end a transaction, and query an
/// identifier for the current window.
////////////////////////////////////////////////////////////////////////////////
class TransactionWindow {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initialize state with no open transactions.
  //////////////////////////////////////////////////////////////////////////////
  TransactionWindow();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Signal the beginning of a transaction.
  //////////////////////////////////////////////////////////////////////////////
  void start();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Signal the end of a transaction.
  //////////////////////////////////////////////////////////////////////////////
  void end();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return the current window identifier.
  //////////////////////////////////////////////////////////////////////////////
  uint64_t term();

 private:
  std::atomic<uint64_t> _open;
  std::atomic<uint64_t> _term;
};

};  // end namespace cache
};  // end namespace arangodb

#endif
