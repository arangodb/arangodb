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

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Cache/Transaction.h"
#include "Cache/TransactionManager.h"

#include <stdint.h>
#include <iostream>

using namespace arangodb::cache;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CCacheTransactionManagerSetup {
  CCacheTransactionManagerSetup() {
    BOOST_TEST_MESSAGE("setup TransactionManager");
  }

  ~CCacheTransactionManagerSetup() {
    BOOST_TEST_MESSAGE("tear-down TransactionManager");
  }
};
// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CCacheTransactionManagerTest,
                         CCacheTransactionManagerSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test transaction term management
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(tst_transaction_term) {
  TransactionManager transactions;
  Transaction* tx1;
  Transaction* tx2;
  Transaction* tx3;

  BOOST_CHECK_EQUAL(0ULL, transactions.term());

  tx1 = transactions.begin(false);
  BOOST_CHECK_EQUAL(1ULL, transactions.term());
  transactions.end(tx1);
  BOOST_CHECK_EQUAL(2ULL, transactions.term());

  tx1 = transactions.begin(false);
  BOOST_CHECK_EQUAL(3ULL, transactions.term());
  tx2 = transactions.begin(false);
  BOOST_CHECK_EQUAL(3ULL, transactions.term());
  transactions.end(tx1);
  BOOST_CHECK_EQUAL(3ULL, transactions.term());
  transactions.end(tx2);
  BOOST_CHECK_EQUAL(4ULL, transactions.term());

  tx1 = transactions.begin(true);
  BOOST_CHECK_EQUAL(4ULL, transactions.term());
  tx2 = transactions.begin(false);
  BOOST_CHECK_EQUAL(5ULL, transactions.term());
  transactions.end(tx2);
  BOOST_CHECK_EQUAL(5ULL, transactions.term());
  transactions.end(tx1);
  BOOST_CHECK_EQUAL(6ULL, transactions.term());

  tx1 = transactions.begin(true);
  BOOST_CHECK_EQUAL(6ULL, transactions.term());
  tx2 = transactions.begin(false);
  BOOST_CHECK_EQUAL(7ULL, transactions.term());
  transactions.end(tx2);
  BOOST_CHECK_EQUAL(7ULL, transactions.term());
  tx3 = transactions.begin(true);
  BOOST_CHECK_EQUAL(7ULL, transactions.term());
  transactions.end(tx1);
  BOOST_CHECK_EQUAL(8ULL, transactions.term());
  transactions.end(tx3);
  BOOST_CHECK_EQUAL(8ULL, transactions.term());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|//
// --SECTION--\\|/// @\\}\\)"
// End:
