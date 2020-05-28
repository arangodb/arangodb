////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for Supervision
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
/// @author Kaveh Vahedipour
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Agency/Job.h"
#include "Agency/Supervision.h"

#include "gtest/gtest.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>
#include <typeinfo>

using namespace arangodb;
using namespace arangodb::consensus;

std::vector<std::string> servers{"XXX-XXX-XXX", "XXX-XXX-XXY"};

TEST(SupervisionTest, checking_for_the_delete_transaction_0_servers) {
  std::vector<std::string> todelete;
  auto const& transaction = removeTransactionBuilder(todelete);
  auto const& slice = transaction->slice();

  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);
  ASSERT_TRUE(slice[0].isArray());
  ASSERT_EQ(slice[0].length(), 1);
  ASSERT_TRUE(slice[0][0].isObject());
  ASSERT_EQ(slice[0][0].length(), 0);
}

TEST(SupervisionTest, checking_for_the_delete_transaction_1_server) {
  std::vector<std::string> todelete{servers[0]};
  auto const& transaction = removeTransactionBuilder(todelete);
  auto const& slice = transaction->slice();

  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);
  ASSERT_TRUE(slice[0].isArray());
  ASSERT_EQ(slice[0].length(), 1);
  ASSERT_TRUE(slice[0][0].isObject());
  ASSERT_EQ(slice[0][0].length(), 1);
  for (size_t i = 0; i < slice[0][0].length(); ++i) {
    ASSERT_TRUE(slice[0][0].keyAt(i).copyString() ==
                Supervision::agencyPrefix() +
                    arangodb::consensus::healthPrefix + servers[i]);
    ASSERT_TRUE(slice[0][0].valueAt(i).isObject());
    ASSERT_EQ(slice[0][0].valueAt(i).keyAt(0).copyString(), "op");
    ASSERT_EQ(slice[0][0].valueAt(i).valueAt(0).copyString(), "delete");
  }
}

TEST(SupervisionTest, checking_for_the_delete_transaction_2_servers) {
  std::vector<std::string> todelete = servers;
  auto const& transaction = removeTransactionBuilder(todelete);
  auto const& slice = transaction->slice();

  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);
  ASSERT_TRUE(slice[0].isArray());
  ASSERT_EQ(slice[0].length(), 1);
  ASSERT_TRUE(slice[0][0].isObject());
  ASSERT_EQ(slice[0][0].length(), 2);
  for (size_t i = 0; i < slice[0][0].length(); ++i) {
    ASSERT_TRUE(slice[0][0].keyAt(i).copyString() ==
                Supervision::agencyPrefix() +
                    arangodb::consensus::healthPrefix + servers[i]);
    ASSERT_TRUE(slice[0][0].valueAt(i).isObject());
    ASSERT_EQ(slice[0][0].valueAt(i).keyAt(0).copyString(), "op");
    ASSERT_EQ(slice[0][0].valueAt(i).valueAt(0).copyString(), "delete");
  }
}
