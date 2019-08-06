////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for transaction Manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Query.h"
#include "Rest/GeneralResponse.h"
#include "Transaction/Manager.h"
#include "Transaction/SmartContext.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/Status.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include "ManagerSetup.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "gtest/gtest.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test transaction::Context
class TransactionContextTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::TransactionManagerSetup setup;
  TRI_vocbase_t vocbase;

  TransactionContextTest()
      : vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase") {}
};

TEST_F(TransactionContextTest, StandaloneContext) {
  transaction::StandaloneContext ctx(vocbase);
  EXPECT_TRUE(!ctx.isEmbeddable());
  EXPECT_TRUE(ctx.getParentTransaction() == nullptr);

  std::vector<std::string*> strings;
  for (int i = 0; i < 10; i++) {
    std::string* str = ctx.leaseString();
    if (i > 0) {
      EXPECT_TRUE(strings.back() != str);
    }
    strings.push_back(str);
  }

  while (!strings.empty()) {
    std::string* str = strings.back();
    ctx.returnString(str);
    strings.pop_back();
  }

  std::vector<velocypack::Builder*> builders;
  for (int i = 0; i < 10; i++) {
    auto* b = ctx.leaseBuilder();
    if (i > 0) {
      EXPECT_TRUE(builders.back() != b);
    }
    builders.push_back(b);
  }
  while (!builders.empty()) {
    auto* b = builders.back();
    ctx.returnBuilder(b);
    builders.pop_back();
  }
}

TEST_F(TransactionContextTest, StandaloneSmartContext) {
  auto const cname = "testCollection";
  auto params = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  vocbase.createCollection(params->slice());

  auto ctx = std::make_shared<transaction::StandaloneSmartContext>(vocbase);
  transaction::Options trxOpts;
  transaction::Methods trx{ctx, {}, std::vector<std::string>{cname}, {}, trxOpts};

  Result res = trx.begin();
  ASSERT_TRUE(res.ok());

  auto docs = arangodb::velocypack::Parser::fromJson(
      "[{ \"hello\": \"world1\" }, { \"hello\": \"world2\" }]");

  OperationOptions opOpts;
  OperationResult result = trx.insert(cname, docs->slice(), opOpts);
  ASSERT_TRUE(result.ok());

  VPackSlice trxSlice = result.slice();
  ASSERT_TRUE(trxSlice.isArray());
  ASSERT_TRUE(trxSlice.length() == 2);

  aql::QueryString queryString{
      "FOR doc IN @@collection "
      "FILTER doc.hello != '' "
      "RETURN doc"};

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->add(VPackValue(VPackValueType::Object));
  bindVars->add("@collection", VPackValue(cname));
  bindVars->close();

  {
    arangodb::aql::Query query(false, vocbase, queryString, bindVars, nullptr,
                               arangodb::aql::PART_DEPENDENT);
    query.setTransactionContext(ctx);  // shares the same transaction

    auto qres = query.executeSync(QueryRegistryFeature::registry());
    ASSERT_TRUE(qres.ok());
    VPackSlice aqlSlice = qres.data->slice();
    ASSERT_TRUE(aqlSlice.isArray());
    ASSERT_TRUE(aqlSlice.length() == 2);
    ASSERT_TRUE(aqlSlice.at(0).get("hello").isEqualString("world1"));
  }

  ASSERT_TRUE(trxSlice.at(1).hasKey(StaticStrings::KeyString));
  OperationResult result2 = trx.remove(cname, trxSlice.at(0), opOpts);
  ASSERT_TRUE(result2.ok());

  {
    arangodb::aql::Query query(false, vocbase, queryString, bindVars, nullptr,
                               arangodb::aql::PART_DEPENDENT);
    query.setTransactionContext(ctx);  // shares the same transaction

    auto qres = query.executeSync(QueryRegistryFeature::registry());
    ASSERT_TRUE(qres.ok());
    VPackSlice aqlSlice = qres.data->slice();
    ASSERT_TRUE(aqlSlice.isArray());
    ASSERT_TRUE(aqlSlice.length() == 1);
    ASSERT_TRUE(aqlSlice.at(0).get("hello").isEqualString("world2"));
  }
}  // SECTION("StandaloneSmartContext")
