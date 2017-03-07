////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::TransactionManager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Daniel H. Larkin
/// @author Copyright 2017, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "catch.hpp"

#include "Cache/Transaction.h"
#include "Cache/TransactionManager.h"

#include <stdint.h>
#include <iostream>

using namespace arangodb::cache;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CCacheTransactionManagerTest", "[cache]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test transaction term management
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_transaction_term") {
  TransactionManager transactions;
  Transaction* tx1;
  Transaction* tx2;
  Transaction* tx3;

  CHECK(0ULL == transactions.term());

  tx1 = transactions.begin(false);
  CHECK(1ULL == transactions.term());
  transactions.end(tx1);
  CHECK(2ULL == transactions.term());

  tx1 = transactions.begin(false);
  CHECK(3ULL == transactions.term());
  tx2 = transactions.begin(false);
  CHECK(3ULL == transactions.term());
  transactions.end(tx1);
  CHECK(3ULL == transactions.term());
  transactions.end(tx2);
  CHECK(4ULL == transactions.term());

  tx1 = transactions.begin(true);
  CHECK(4ULL == transactions.term());
  tx2 = transactions.begin(false);
  CHECK(5ULL == transactions.term());
  transactions.end(tx2);
  CHECK(5ULL == transactions.term());
  transactions.end(tx1);
  CHECK(6ULL == transactions.term());

  tx1 = transactions.begin(true);
  CHECK(6ULL == transactions.term());
  tx2 = transactions.begin(false);
  CHECK(7ULL == transactions.term());
  transactions.end(tx2);
  CHECK(7ULL == transactions.term());
  tx3 = transactions.begin(true);
  CHECK(7ULL == transactions.term());
  transactions.end(tx1);
  CHECK(8ULL == transactions.term());
  transactions.end(tx3);
  CHECK(8ULL == transactions.term());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|//
// --SECTION--\\|/// @\\}\\)"
// End:
