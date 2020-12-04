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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchQueryCommon.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/OptimizerRulesFeature.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchView.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>

namespace {

static const char* collectionName1 = "collection_1";
static const char* collectionName2 = "collection_2";

static const char* viewName = "view";

class IResearchViewCountApproximateTest : public IResearchQueryTest {
 protected:
  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  void addLinkToCollection(std::shared_ptr<arangodb::iresearch::IResearchView>& view) {
    auto updateJson = VPackParser::fromJson(
      std::string("{") +
        "\"links\": {"
        "\"" + collectionName1 + "\": {\"includeAllFields\": true, \"storeValues\": \"id\"},"
        "\"" + collectionName2 + "\": {\"includeAllFields\": true, \"storeValues\": \"id\"}"
      "}}");
    EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
    builder.close();

    auto slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(slice.get("type").copyString() ==
                arangodb::iresearch::DATA_SOURCE_TYPE.name());
    EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
    auto tmpSlice = slice.get("links");
    EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
  }

  void SetUp() override {
    // add collection_1
    std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
    {
      auto collectionJson =
          VPackParser::fromJson(std::string("{\"name\": \"") + collectionName1 + "\"}");
      logicalCollection1 = vocbase().createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection1);
    }

    // add collection_2
    std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
    {
      auto collectionJson =
          VPackParser::fromJson(std::string("{\"name\": \"") + collectionName2 + "\"}");
      logicalCollection2 = vocbase().createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection2);
    }
    // create view
    std::shared_ptr<arangodb::iresearch::IResearchView> view;
    {
      auto createJson = VPackParser::fromJson(
          std::string("{") +
          "\"name\": \"" + viewName + "\", \
           \"type\": \"arangosearch\", \
           \"primarySort\": [{\"field\": \"value\", \"direction\": \"asc\"}], \
           \"storedValues\": [] \
        }");
      view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
          vocbase().createView(createJson->slice()));
      ASSERT_FALSE(!view);

      // add links to collections
      addLinkToCollection(view);
    }

    std::shared_ptr<arangodb::iresearch::IResearchView> view2;
    {
      auto createJson = VPackParser::fromJson(
          std::string("{") +
          "\"name\": \"" + viewName + "2\", \
           \"type\": \"arangosearch\", \
           \"primarySort\": [{\"field\": \"value\", \"direction\": \"asc\"}], \
           \"storedValues\": [] \
        }");
      view2 = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
          vocbase().createView(createJson->slice()));
      ASSERT_FALSE(!view2);

      // add links to collections
      addLinkToCollection(view2);
    }

    // populate view with the data
    {
      arangodb::OperationOptions opt;
      static std::vector<std::string> const EMPTY;
      arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(vocbase()),
          EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collection_1
      {
        auto builder = VPackParser::fromJson(
            "["
              "{\"_key\": \"c0\", \"value\": 0},"
              "{\"_key\": \"c1\", \"value\": 1},"
              "{\"_key\": \"c2\", \"value\": 2},"
              "{\"_key\": \"c3\", \"value\": 3}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          insertedDocs.emplace_back();
          auto const res =
              logicalCollection1->insert(&trx, doc, insertedDocs.back(), opt);
          EXPECT_TRUE(res.ok());
        }
      }

      // insert into collection_2
      {
        auto builder = VPackParser::fromJson(
            "["
              "{\"_key\": \"c_0\", \"value\": 10},"
              "{\"_key\": \"c_1\", \"value\": 11},"
              "{\"_key\": \"c_2\", \"value\": 12},"
              "{\"_key\": \"c_3\", \"value\": 13}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          insertedDocs.emplace_back();
          auto const res =
              logicalCollection2->insert(&trx, doc, insertedDocs.back(), opt);
          EXPECT_TRUE(res.ok());
        }
      }

      EXPECT_TRUE(trx.commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection1, *view)
                  ->commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection2, *view)
                  ->commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection1, *view2)
                  ->commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection2, *view2)
                  ->commit().ok());
    }
    // now we need to have at least 2 segments per index to check proper inter segment switches. So - another round
    // of commits to force creating new segment. And as we have no consolidations - segments will be not merged
    {
      arangodb::OperationOptions opt;
      static std::vector<std::string> const EMPTY;
      arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(vocbase()),
          EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collection_1
      {
        auto builder = VPackParser::fromJson(
            "["
              "{\"_key\": \"c4\", \"value\": 4},"
              "{\"_key\": \"c5\", \"value\": 5},"
              "{\"_key\": \"c6\", \"value\": 6},"
              "{\"_key\": \"c7\", \"value\": 7},"
              "{\"_key\": \"c8\", \"value\": 10}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          insertedDocs.emplace_back();
          auto const res =
              logicalCollection1->insert(&trx, doc, insertedDocs.back(), opt);
          EXPECT_TRUE(res.ok());
        }
      }

      // insert into collection_2
      {
        auto builder = VPackParser::fromJson(
            "["
              "{\"_key\": \"c_4\", \"value\": 14},"
              "{\"_key\": \"c_5\", \"value\": 15},"
              "{\"_key\": \"c_6\", \"value\": 16},"
              "{\"_key\": \"c_7\", \"value\": 17}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          insertedDocs.emplace_back();
          auto const res =
              logicalCollection2->insert(&trx, doc, insertedDocs.back(), opt);
          EXPECT_TRUE(res.ok());
        }
      }

      EXPECT_TRUE(trx.commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection1, *view)
                  ->commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection2, *view)
                  ->commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection1, *view2)
                  ->commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection2, *view2)
                  ->commit().ok());
    }
  }

  void executeAndCheck(std::string const& queryString,
                       std::vector<VPackValue> const& expectedValues,
                       int64_t expectedFullCount) {
    SCOPED_TRACE(testing::Message("Query:") << queryString);
    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), queryString,
      {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase()),
                               arangodb::aql::QueryString(queryString), nullptr,
                               arangodb::velocypack::Parser::fromJson("{}"));

    auto queryResult = arangodb::tests::executeQuery(vocbase(), queryString, nullptr, expectedFullCount >= 0 ? "{\"fullCount\":true}" : "{}");
    ASSERT_TRUE(queryResult.result.ok());
    if (expectedFullCount >= 0) {
      ASSERT_NE(nullptr, queryResult.extra);
      auto statsSlice = queryResult.extra->slice().get("stats");
      ASSERT_TRUE(statsSlice.isObject());
      auto fullCountSlice  = statsSlice.get("fullCount");
      ASSERT_TRUE(fullCountSlice.isInteger());
      ASSERT_EQ(expectedFullCount, fullCountSlice.getInt());
    }
   

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);

    ASSERT_EQ(expectedValues.size(), resultIt.size());
    // Check values
    auto expectedValue = expectedValues.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedValue) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      if (resolved.isString()) {
        ASSERT_TRUE(expectedValue->isString());
        arangodb::velocypack::ValueLength length = 0;
        auto resStr = resolved.getString(length);
        EXPECT_TRUE(memcmp(expectedValue->getCharPtr(), resStr, length) == 0);
      } else {
        ASSERT_TRUE(resolved.isNumber());
        EXPECT_EQ(expectedValue->getInt64(), resolved.getInt());
      }
    }
    EXPECT_EQ(expectedValue, expectedValues.end());
  }
};
}

TEST_F(IResearchViewCountApproximateTest, fullCountExact) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " COLLECT WITH COUNT INTO c RETURN c ";

  std::vector<VPackValue> expectedValues{
    VPackValue(17),
  };
  executeAndCheck(queryString, expectedValues, -1);
}

TEST_F(IResearchViewCountApproximateTest, fullCountCost) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " OPTIONS {countApproximate:'cost'} COLLECT WITH COUNT INTO c RETURN c ";

  std::vector<VPackValue> expectedValues{
    VPackValue(17),
  };
  executeAndCheck(queryString, expectedValues, -1);
}

TEST_F(IResearchViewCountApproximateTest, fullCountWithFilter) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " SEARCH d.value >= 10 COLLECT WITH COUNT INTO c RETURN c ";

  std::vector<VPackValue> expectedValues{
    VPackValue(9),
  };
  executeAndCheck(queryString, expectedValues, -1);
}

TEST_F(IResearchViewCountApproximateTest, fullCountWithFilterCost) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " SEARCH d.value >= 10 OPTIONS {countApproximate:'cost'} COLLECT WITH COUNT INTO c RETURN c ";

  std::vector<VPackValue> expectedValues{
    VPackValue(9),
  };
  executeAndCheck(queryString, expectedValues, -1);
}

TEST_F(IResearchViewCountApproximateTest, forcedFullCountWithFilter) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " SEARCH d.value >= 10 OPTIONS {countApproximate:'exact'} LIMIT 2, 1  RETURN  d.value ";

  std::vector<VPackValue> expectedValues{
    VPackValue(11),
  };
  executeAndCheck(queryString, expectedValues, 9);
}

TEST_F(IResearchViewCountApproximateTest, forcedFullCountWithFilterSorted) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " SEARCH d.value >= 2 OPTIONS {countApproximate:'exact'} SORT d.value ASC LIMIT 1  RETURN  d.value ";

  std::vector<VPackValue> expectedValues{
    VPackValue(2),
  };
  executeAndCheck(queryString, expectedValues, 15);
}

TEST_F(IResearchViewCountApproximateTest, forcedFullCountSorted) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " OPTIONS {countApproximate:'exact'} SORT d.value ASC LIMIT 7, 1  RETURN  d.value ";

  std::vector<VPackValue> expectedValues{
    VPackValue(7),
  };
  executeAndCheck (queryString, expectedValues, 17);
}

TEST_F(IResearchViewCountApproximateTest, forcedFullCountSortedCost) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " OPTIONS {countApproximate:'cost'} SORT d.value ASC LIMIT 7, 1  RETURN  d.value ";

  std::vector<VPackValue> expectedValues{
    VPackValue(7),
  };
  executeAndCheck(queryString, expectedValues, 17);
}

TEST_F(IResearchViewCountApproximateTest, forcedFullCountNotSorted) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " OPTIONS {countApproximate:'exact'} SORT d.value DESC LIMIT 7, 1  RETURN  d.value ";

  std::vector<VPackValue> expectedValues{
    VPackValue(10),
  };
  executeAndCheck(queryString, expectedValues, 17);
}

TEST_F(IResearchViewCountApproximateTest, forcedFullCountNotSortedCost) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " OPTIONS {countApproximate:'cost'} SORT d.value DESC LIMIT 7, 1  RETURN  d.value ";

  std::vector<VPackValue> expectedValues{
    VPackValue(10),
  };
  executeAndCheck(queryString, expectedValues, 17);
}

TEST_F(IResearchViewCountApproximateTest, forcedFullCountWithFilterSortedCost) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " SEARCH d.value >= 2 OPTIONS {countApproximate:'cost'} SORT d.value ASC LIMIT 8, 1  RETURN  d.value ";

  std::vector<VPackValue> expectedValues{
    VPackValue(11),
  };
  executeAndCheck(queryString, expectedValues, 15);
}

TEST_F(IResearchViewCountApproximateTest, forcedFullCountWithFilterNoOffsetSortedCost) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " SEARCH d.value >= 2 OPTIONS {countApproximate:'cost'} SORT d.value ASC LIMIT  2  RETURN  d.value ";

  std::vector<VPackValue> expectedValues{
    VPackValue(2),
    VPackValue(3),
  };
  executeAndCheck(queryString, expectedValues, 15);
}
