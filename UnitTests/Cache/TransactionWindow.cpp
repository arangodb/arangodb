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

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Cache/TransactionWindow.h"

#include <stdint.h>
#include <iostream>

using namespace arangodb::cache;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CCacheTransactionWindowSetup {
  CCacheTransactionWindowSetup() {
    BOOST_TEST_MESSAGE("setup TransactionWindow");
  }

  ~CCacheTransactionWindowSetup() {
    BOOST_TEST_MESSAGE("tear-down TransactionWindow");
  }
};
// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CCacheTransactionWindowTest,
                         CCacheTransactionWindowSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test transaction term management
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(tst_transaction_term) {
  TransactionWindow transactions;

  BOOST_CHECK_EQUAL(0ULL, transactions.term());

  transactions.start();
  BOOST_CHECK_EQUAL(1ULL, transactions.term());
  transactions.end();
  BOOST_CHECK_EQUAL(2ULL, transactions.term());

  transactions.start();
  BOOST_CHECK_EQUAL(3ULL, transactions.term());
  transactions.start();
  BOOST_CHECK_EQUAL(3ULL, transactions.term());
  transactions.end();
  BOOST_CHECK_EQUAL(3ULL, transactions.term());
  transactions.end();
  BOOST_CHECK_EQUAL(4ULL, transactions.term());
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
