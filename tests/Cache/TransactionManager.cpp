////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::TransactionManager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cache/TransactionManager.h"
#include "Basics/Common.h"
#include "Cache/Transaction.h"

#include "catch.hpp"

#include <stdint.h>

using namespace arangodb::cache;

TEST_CASE("cache::TransactionManager", "[cache]") {
  SECTION("verify that transaction term is maintained correctly") {
    TransactionManager transactions;
    Transaction* tx1;
    Transaction* tx2;
    Transaction* tx3;

    REQUIRE(0ULL == transactions.term());

    tx1 = transactions.begin(false);
    REQUIRE(1ULL == transactions.term());
    transactions.end(tx1);
    REQUIRE(2ULL == transactions.term());

    tx1 = transactions.begin(false);
    REQUIRE(3ULL == transactions.term());
    tx2 = transactions.begin(false);
    REQUIRE(3ULL == transactions.term());
    transactions.end(tx1);
    REQUIRE(3ULL == transactions.term());
    transactions.end(tx2);
    REQUIRE(4ULL == transactions.term());

    tx1 = transactions.begin(true);
    REQUIRE(4ULL == transactions.term());
    tx2 = transactions.begin(false);
    REQUIRE(5ULL == transactions.term());
    transactions.end(tx2);
    REQUIRE(5ULL == transactions.term());
    transactions.end(tx1);
    REQUIRE(6ULL == transactions.term());

    tx1 = transactions.begin(true);
    REQUIRE(6ULL == transactions.term());
    tx2 = transactions.begin(false);
    REQUIRE(7ULL == transactions.term());
    transactions.end(tx2);
    REQUIRE(7ULL == transactions.term());
    tx3 = transactions.begin(true);
    REQUIRE(7ULL == transactions.term());
    transactions.end(tx1);
    REQUIRE(8ULL == transactions.term());
    transactions.end(tx3);
    REQUIRE(8ULL == transactions.term());

    tx1 = transactions.begin(true);
    REQUIRE(8ULL == transactions.term());
    tx2 = transactions.begin(false);
    REQUIRE(9ULL == transactions.term());
    transactions.end(tx2);
    REQUIRE(9ULL == transactions.term());
    tx3 = transactions.begin(true);
    REQUIRE(9ULL == transactions.term());
    transactions.end(tx3);
    REQUIRE(9ULL == transactions.term());
    tx2 = transactions.begin(false);
    REQUIRE(9ULL == transactions.term());
    tx3 = transactions.begin(false);
    REQUIRE(9ULL == transactions.term());
    transactions.end(tx3);
    REQUIRE(9ULL == transactions.term());
    transactions.end(tx2);
    REQUIRE(9ULL == transactions.term());
    transactions.end(tx1);
    REQUIRE(10ULL == transactions.term());
  }
}
