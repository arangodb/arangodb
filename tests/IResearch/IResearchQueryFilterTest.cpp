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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchQueryCommon.h"

#include "Aql/OptimizerRule.h"

namespace arangodb::tests {
namespace {

class QueryFilter : public QueryTest {
 protected:
  void createCollections() {
    {
      auto collectionJson =
          velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
      auto logicalCollection1 =
          _vocbase.createCollection(collectionJson->slice());
      ASSERT_TRUE(logicalCollection1);
    }
    {
      auto collectionJson =
          velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
      auto logicalCollection2 =
          _vocbase.createCollection(collectionJson->slice());
      ASSERT_TRUE(logicalCollection2);
    }
  }
  void queryTests() {
    // ==, !=, <, <=, >, >=, range
    static std::vector<std::string> const EMPTY;

    auto logicalCollection1 = _vocbase.lookupCollection("testCollection0");
    auto logicalCollection2 = _vocbase.lookupCollection("testCollection1");

    std::deque<std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
        insertedDocs;
    // populate view with the data
    {
      OperationOptions opt;

      transaction::Methods trx(
          transaction::StandaloneContext::Create(_vocbase), EMPTY,
          {logicalCollection1->name(), logicalCollection2->name()}, EMPTY,
          transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collections
      {
        irs::utf8_path resource;
        resource /= std::string_view(testResourceDir);
        resource /= std::string_view("simple_sequential.json");

        auto builder =
            basics::VelocyPackHelper::velocyPackFromFile(resource.string());
        auto root = builder.slice();
        ASSERT_TRUE(root.isArray());

        size_t i = 0;

        std::shared_ptr<LogicalCollection> collections[]{logicalCollection1,
                                                         logicalCollection2};

        for (auto doc : velocypack::ArrayIterator(root)) {
          auto res = trx.insert(collections[i % 2]->name(), doc, opt);
          EXPECT_TRUE(res.ok());

          res = trx.document(collections[i % 2]->name(), res.slice(), opt);
          EXPECT_TRUE(res.ok());
          insertedDocs.emplace_back(std::move(res.buffer));
          ++i;
        }
      }

      EXPECT_TRUE(trx.commit().ok());
      EXPECT_TRUE((executeQuery(_vocbase,
                                "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                "{ waitForSync: true } RETURN d")
                       .result.ok()));  // commit
    }

    // -----------------------------------------------------------------------------
    // --SECTION--                                              'collections'
    // option
    // -----------------------------------------------------------------------------

    // collection by name
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' || d.name == 'B' "
          "FILTER d.seq == 0 "
          "RETURN d";

      EXPECT_TRUE(assertRules(
          _vocbase, query, {aql::OptimizerRule::handleArangoSearchViewsRule}));

      std::map<irs::string_ref, std::shared_ptr<velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", insertedDocs[0]}};

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = iresearch::getStringRef(keySlice);

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(
                             velocypack::Slice(expectedDoc->second->data()),
                             resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // FILTER must always follow SEARCH
    {
      std::string const query =
          "FOR d IN testView FILTER d.seq == 1' SEARCH d.name == 'A' RETURN d";

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_QUERY_PARSE));
    }
  }
};

class QueryFilterView : public QueryFilter {
 protected:
  ViewType type() const final { return ViewType::kArangoSearch; }
};

class QueryFilterSearch : public QueryFilter {
 protected:
  ViewType type() const final { return ViewType::kSearchAlias; }
};

TEST_P(QueryFilterView, Test) {
  createCollections();
  createView(R"("storeValues":"id",)", R"("storeValues":"id",)");
  queryTests();
}

TEST_P(QueryFilterView, TestWithoutStoreValues) {
  createCollections();
  createView(R"()", R"()");
  queryTests();
}

TEST_P(QueryFilterSearch, Test) {
  createCollections();
  createIndexes(R"()", R"()");
  createSearch();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryFilterView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryFilterSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
