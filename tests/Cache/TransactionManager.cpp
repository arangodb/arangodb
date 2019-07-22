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

#include "Basics/Common.h"
#include "Cache/Transaction.h"
#include "Cache/TransactionManager.h"

#include "gtest/gtest.h"

#include <stdint.h>

using namespace arangodb::cache;

TEST(CacheTransactionalManagerTest, verify_that_transaction_term_is_maintained_correctly) {
  TransactionManager transactions;
  Transaction* tx1;
  Transaction* tx2;
  Transaction* tx3;

  ASSERT_TRUE(0ULL == transactions.term());

  tx1 = transactions.begin(false);
  ASSERT_TRUE(1ULL == transactions.term());
  transactions.end(tx1);
  ASSERT_TRUE(2ULL == transactions.term());

  tx1 = transactions.begin(false);
  ASSERT_TRUE(3ULL == transactions.term());
  tx2 = transactions.begin(false);
  ASSERT_TRUE(3ULL == transactions.term());
  transactions.end(tx1);
  ASSERT_TRUE(3ULL == transactions.term());
  transactions.end(tx2);
  ASSERT_TRUE(4ULL == transactions.term());

  tx1 = transactions.begin(true);
  ASSERT_TRUE(4ULL == transactions.term());
  tx2 = transactions.begin(false);
  ASSERT_TRUE(5ULL == transactions.term());
  transactions.end(tx2);
  ASSERT_TRUE(5ULL == transactions.term());
  transactions.end(tx1);
  ASSERT_TRUE(6ULL == transactions.term());

  tx1 = transactions.begin(true);
  ASSERT_TRUE(6ULL == transactions.term());
  tx2 = transactions.begin(false);
  ASSERT_TRUE(7ULL == transactions.term());
  transactions.end(tx2);
  ASSERT_TRUE(7ULL == transactions.term());
  tx3 = transactions.begin(true);
  ASSERT_TRUE(7ULL == transactions.term());
  transactions.end(tx1);
  ASSERT_TRUE(8ULL == transactions.term());
  transactions.end(tx3);
  ASSERT_TRUE(8ULL == transactions.term());

  tx1 = transactions.begin(true);
  ASSERT_TRUE(8ULL == transactions.term());
  tx2 = transactions.begin(false);
  ASSERT_TRUE(9ULL == transactions.term());
  transactions.end(tx2);
  ASSERT_TRUE(9ULL == transactions.term());
  tx3 = transactions.begin(true);
  ASSERT_TRUE(9ULL == transactions.term());
  transactions.end(tx3);
  ASSERT_TRUE(9ULL == transactions.term());
  tx2 = transactions.begin(false);
  ASSERT_TRUE(9ULL == transactions.term());
  tx3 = transactions.begin(false);
  ASSERT_TRUE(9ULL == transactions.term());
  transactions.end(tx3);
  ASSERT_TRUE(9ULL == transactions.term());
  transactions.end(tx2);
  ASSERT_TRUE(9ULL == transactions.term());
  transactions.end(tx1);
  ASSERT_TRUE(10ULL == transactions.term());
}
