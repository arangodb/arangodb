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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchQueryCommon.h"
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

static const char* viewName1 = "view_1";
static const char* viewName2 = "view_2";
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryLateMaterializationTest : public IResearchQueryTest {
 protected:
  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  void addLinkToCollection(std::shared_ptr<arangodb::iresearch::IResearchView>& view) {
    auto updateJson = VPackParser::fromJson(
      std::string("{") +
        "\"links\": {"
        "\"" + collectionName1 + "\": {\"includeAllFields\": true},"
        "\"" + collectionName2 + "\": {\"includeAllFields\": true}"
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

    // create view_1
    std::shared_ptr<arangodb::iresearch::IResearchView> view1;
    {
      auto createJson = VPackParser::fromJson(
          std::string("{") +
          "\"name\": \"" + viewName1 + "\", \
           \"type\": \"arangosearch\" \
        }");
      view1 = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
          vocbase().createView(createJson->slice()));
      ASSERT_FALSE(!view1);

      // add links to collections
      addLinkToCollection(view1);
    }

    // create view_2
    std::shared_ptr<arangodb::iresearch::IResearchView> view2;
    {
       auto createJson = VPackParser::fromJson(
           std::string("{") +
           "\"name\": \"" + viewName2 + "\", \
            \"type\": \"arangosearch\", \
            \"primarySort\": [{\"field\": \"value\", \"direction\": \"asc\"}, {\"field\": \"foo\", \"direction\": \"desc\"}] \
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
              "{\"_key\": \"c0\", \"str\": \"cat\", \"foo\": \"foo0\", \"value\": 0},"
              "{\"_key\": \"c1\", \"str\": \"cat\", \"foo\": \"foo1\", \"value\": 1},"
              "{\"_key\": \"c2\", \"str\": \"cat\", \"foo\": \"foo2\", \"value\": 2},"
              "{\"_key\": \"c3\", \"str\": \"cat\", \"foo\": \"foo3\", \"value\": 3}"
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
              "{\"_key\": \"c_0\", \"str\": \"cat\", \"foo\": \"foo_0\", \"value\": 10},"
              "{\"_key\": \"c_1\", \"str\": \"cat\", \"foo\": \"foo_1\", \"value\": 11},"
              "{\"_key\": \"c_2\", \"str\": \"cat\", \"foo\": \"foo_2\", \"value\": 12},"
              "{\"_key\": \"c_3\", \"str\": \"cat\", \"foo\": \"foo_3\", \"value\": 13}"
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

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection1, *view1)
                  ->commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection2, *view1)
                  ->commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection1, *view2)
                  ->commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection2, *view2)
                  ->commit().ok());
    }
  }

  void executeAndCheck(std::string const& query, std::vector<arangodb::velocypack::Slice> const& expectedDocs, bool checkRuleOnly) {
    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      {arangodb::aql::OptimizerRule::lateDocumentMaterializationArangoSearchRule}));

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    if (checkRuleOnly) {
      return;
    }

    arangodb::velocypack::ArrayIterator resultIt(result);

    ASSERT_EQ(expectedDocs.size(), resultIt.size());
    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                    arangodb::velocypack::Slice(*expectedDoc), resolved, true));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }
};

}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryLateMaterializationTest, test_1) {
  auto const query = std::string("FOR d IN ") + viewName2 +
      " SEARCH d.value IN [1, 2, 11, 12] LET a = NOOPT(d.foo) LET e = SUM(FOR c IN " + viewName1 +
      " LET p = CONCAT(c.foo, c.foo) RETURN p) SORT CONCAT(a, e) LIMIT 10 RETURN d";

  std::vector<arangodb::velocypack::Slice> expectedDocs{
    arangodb::velocypack::Slice(insertedDocs[5].vpack()),
    arangodb::velocypack::Slice(insertedDocs[6].vpack()),
    arangodb::velocypack::Slice(insertedDocs[1].vpack()),
    arangodb::velocypack::Slice(insertedDocs[2].vpack())
  };

  executeAndCheck(query, expectedDocs, false);
}

TEST_F(IResearchQueryLateMaterializationTest, test_2) {
  auto const query = std::string("FOR d IN ") + viewName2 +
      " FILTER d.value IN [1, 2] SORT d.foo DESC LIMIT 10 RETURN d";

  std::vector<arangodb::velocypack::Slice> expectedDocs{
    arangodb::velocypack::Slice(insertedDocs[2].vpack()),
    arangodb::velocypack::Slice(insertedDocs[1].vpack())
  };

  executeAndCheck(query, expectedDocs, false);
}

TEST_F(IResearchQueryLateMaterializationTest, test_3) {
  auto const query = std::string("FOR d IN ") + viewName2 +
      " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LET c = BM25(d) * 2 SORT CONCAT(BM25(d), c, d.value) LIMIT 10 RETURN d";

  std::vector<arangodb::velocypack::Slice> expectedDocs{
    arangodb::velocypack::Slice(insertedDocs[1].vpack()),
    arangodb::velocypack::Slice(insertedDocs[5].vpack()),
    arangodb::velocypack::Slice(insertedDocs[6].vpack()),
    arangodb::velocypack::Slice(insertedDocs[2].vpack())
  };

  executeAndCheck(query, expectedDocs, false);
}

TEST_F(IResearchQueryLateMaterializationTest, test_4) {
  auto const query = std::string("FOR d IN ") + viewName2 +
      " SEARCH d.value IN [1, 2, 11, 12] SORT RAND(), d.value DESC LIMIT 10 RETURN d";

  executeAndCheck(query, std::vector<arangodb::velocypack::Slice>{}, true);
}

TEST_F(IResearchQueryLateMaterializationTest, test_5) {
  auto const query = std::string("FOR d IN ") + viewName2 +
      " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC, d.foo LIMIT 10 RETURN d";

  std::vector<arangodb::velocypack::Slice> expectedDocs{
    arangodb::velocypack::Slice(insertedDocs[6].vpack()),
    arangodb::velocypack::Slice(insertedDocs[5].vpack()),
    arangodb::velocypack::Slice(insertedDocs[2].vpack()),
    arangodb::velocypack::Slice(insertedDocs[1].vpack())
  };

  executeAndCheck(query, expectedDocs, false);
}

TEST_F(IResearchQueryLateMaterializationTest, test_6) {
  auto const query = std::string("FOR d IN ") + viewName2 +
      " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LIMIT 10 RETURN d";

  std::vector<arangodb::velocypack::Slice> expectedDocs{
    arangodb::velocypack::Slice(insertedDocs[6].vpack()),
    arangodb::velocypack::Slice(insertedDocs[5].vpack()),
    arangodb::velocypack::Slice(insertedDocs[2].vpack()),
    arangodb::velocypack::Slice(insertedDocs[1].vpack())
  };

  executeAndCheck(query, expectedDocs, false);
}

TEST_F(IResearchQueryLateMaterializationTest, test_7) {
  auto const query = std::string("FOR d IN ") + viewName2 +
      " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LIMIT 10 SORT NOOPT(d.value) ASC RETURN d";

  std::vector<arangodb::velocypack::Slice> expectedDocs{
    arangodb::velocypack::Slice(insertedDocs[1].vpack()),
    arangodb::velocypack::Slice(insertedDocs[2].vpack()),
    arangodb::velocypack::Slice(insertedDocs[5].vpack()),
    arangodb::velocypack::Slice(insertedDocs[6].vpack())
  };

  executeAndCheck(query, expectedDocs, false);
}

TEST_F(IResearchQueryLateMaterializationTest, test_8) {
  auto const query = std::string("FOR d IN ") + viewName2 +
      " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LIMIT 10 SORT TFIDF(d) DESC LIMIT 4 RETURN d";

   executeAndCheck(query, std::vector<arangodb::velocypack::Slice>{}, true);
}

TEST_F(IResearchQueryLateMaterializationTest, test_9) {
  auto const query = std::string("FOR d IN ") + viewName2 +
      " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LIMIT 10 LET c = CONCAT(NOOPT(d._key), '-C') RETURN c";

   executeAndCheck(query, std::vector<arangodb::velocypack::Slice>{}, true);
}

TEST_F(IResearchQueryLateMaterializationTest, test_10) {
  auto const query = std::string("FOR d IN ") + viewName2 +
      " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LIMIT 3, 1 RETURN d";

  std::vector<arangodb::velocypack::Slice> expectedDocs{
    arangodb::velocypack::Slice(insertedDocs[1].vpack())
  };

  executeAndCheck(query, expectedDocs, false);
}

TEST_F(IResearchQueryLateMaterializationTest, test_11) {
  auto const query = std::string("FOR d IN ") + viewName2 +
      " SEARCH d.value IN [1, 2, 11, 12] SORT d.value DESC LIMIT 5, 10 RETURN d";

  executeAndCheck(query, std::vector<arangodb::velocypack::Slice>{}, false);
}

TEST_F(IResearchQueryLateMaterializationTest, test_12) {
  auto const query = std::string("FOR c IN ") + viewName1 +
      " SEARCH c.value == 1 FOR d IN " + viewName2 +
      " SEARCH d.value IN [c.value, c.value + 1] SORT d.value DESC LIMIT 10 RETURN d";

  std::vector<arangodb::velocypack::Slice> expectedDocs{
    arangodb::velocypack::Slice(insertedDocs[2].vpack()),
    arangodb::velocypack::Slice(insertedDocs[1].vpack())
  };

  executeAndCheck(query, expectedDocs, false);
}
