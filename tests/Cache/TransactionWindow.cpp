////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::TransactionWindow
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

#include "Cache/TransactionWindow.h"

#include <stdint.h>
#include <iostream>

using namespace arangodb::cache;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CCacheTransactionWindowTest", "[cache]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test transaction term management
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_transaction_term") {
  TransactionWindow transactions;

  CHECK(0ULL == transactions.term());

  transactions.start();
  CHECK(1ULL == transactions.term());
  transactions.end();
  CHECK(2ULL == transactions.term());

  transactions.start();
  CHECK(3ULL == transactions.term());
  transactions.start();
  CHECK(3ULL == transactions.term());
  transactions.end();
  CHECK(3ULL == transactions.term());
  transactions.end();
  CHECK(4ULL == transactions.term());
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
