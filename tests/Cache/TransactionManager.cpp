////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <cstdint>

#include "Cache/Transaction.h"
#include "Cache/TransactionManager.h"

using namespace arangodb::cache;

TEST(CacheTransactionalManagerTest, verify_that_transaction_term_is_maintained_correctly) {
  TransactionManager transactions;
  Transaction* tx1;
  Transaction* tx2;
  Transaction* tx3;

  ASSERT_EQ(0ULL, transactions.term());

  tx1 = transactions.begin(false);
  ASSERT_EQ(1ULL, transactions.term());
  transactions.end(tx1);
  ASSERT_EQ(2ULL, transactions.term());

  tx1 = transactions.begin(false);
  ASSERT_EQ(3ULL, transactions.term());
  tx2 = transactions.begin(false);
  ASSERT_EQ(3ULL, transactions.term());
  transactions.end(tx1);
  ASSERT_EQ(3ULL, transactions.term());
  transactions.end(tx2);
  ASSERT_EQ(4ULL, transactions.term());

  tx1 = transactions.begin(true);
  ASSERT_EQ(4ULL, transactions.term());
  tx2 = transactions.begin(false);
  ASSERT_EQ(5ULL, transactions.term());
  transactions.end(tx2);
  ASSERT_EQ(5ULL, transactions.term());
  transactions.end(tx1);
  ASSERT_EQ(6ULL, transactions.term());

  tx1 = transactions.begin(true);
  ASSERT_EQ(6ULL, transactions.term());
  tx2 = transactions.begin(false);
  ASSERT_EQ(7ULL, transactions.term());
  transactions.end(tx2);
  ASSERT_EQ(7ULL, transactions.term());
  tx3 = transactions.begin(true);
  ASSERT_EQ(7ULL, transactions.term());
  transactions.end(tx1);
  ASSERT_EQ(8ULL, transactions.term());
  transactions.end(tx3);
  ASSERT_EQ(8ULL, transactions.term());

  tx1 = transactions.begin(true);
  ASSERT_EQ(8ULL, transactions.term());
  tx2 = transactions.begin(false);
  ASSERT_EQ(9ULL, transactions.term());
  transactions.end(tx2);
  ASSERT_EQ(9ULL, transactions.term());
  tx3 = transactions.begin(true);
  ASSERT_EQ(9ULL, transactions.term());
  transactions.end(tx3);
  ASSERT_EQ(9ULL, transactions.term());
  tx2 = transactions.begin(false);
  ASSERT_EQ(9ULL, transactions.term());
  tx3 = transactions.begin(false);
  ASSERT_EQ(9ULL, transactions.term());
  transactions.end(tx3);
  ASSERT_EQ(9ULL, transactions.term());
  transactions.end(tx2);
  ASSERT_EQ(9ULL, transactions.term());
  transactions.end(tx1);
  ASSERT_EQ(10ULL, transactions.term());
}
