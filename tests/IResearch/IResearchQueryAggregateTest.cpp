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

#include "IResearch/IResearchView.h"
#include "IResearch/Search.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include "Containers/FlatHashSet.h"
#include "Containers/FlatHashMap.h"

#include <velocypack/Iterator.h>

#include <absl/strings/substitute.h>

namespace arangodb::tests {
namespace {

class QueryAggregate : public IResearchQueryTest {
 protected:
  void createCollections() {
    {
      auto createJson =
          velocypack::Parser::fromJson(R"({ "name": "testCollection0" })");
      auto collection = _vocbase.createCollection(createJson->slice());
      ASSERT_TRUE(collection);
      std::vector<std::shared_ptr<velocypack::Builder>> docs{
          velocypack::Parser::fromJson(R"({ "seq": -6, "value": null })"),
          velocypack::Parser::fromJson(R"({ "seq": -5, "value": true })"),
          velocypack::Parser::fromJson(R"({ "seq": -4, "value": "abc" })"),
          velocypack::Parser::fromJson(R"({ "seq": -3, "value": 3.14 })"),
          velocypack::Parser::fromJson(
              R"({ "seq": -2, "value": [ 1, "abc" ] })"),
          velocypack::Parser::fromJson(
              R"({ "seq": -1, "value": { "a": 7, "b": "c" } })"),
      };

      OperationOptions options;
      options.returnNew = true;
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::Create(_vocbase), *collection,
          AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : docs) {
        auto r = trx.insert(collection->name(), entry->slice(), options);
        EXPECT_TRUE(r.ok());
        _insertedDocs.emplace_back(r.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }
    {
      auto createJson =
          velocypack::Parser::fromJson(R"({ "name": "testCollection1" })");
      auto collection = _vocbase.createCollection(createJson->slice());
      ASSERT_TRUE(collection);

      irs::utf8_path resource;
      resource /= std::string_view{testResourceDir};
      resource /= std::string_view{"simple_sequential.json"};

      auto builder =
          basics::VelocyPackHelper::velocyPackFromFile(resource.string());
      auto slice = builder.slice();
      ASSERT_TRUE(slice.isArray());

      OperationOptions options;
      options.returnNew = true;
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::Create(_vocbase), *collection,
          AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (velocypack::ArrayIterator it{slice}; it.valid(); ++it) {
        auto r = trx.insert(collection->name(), it.value(), options);
        EXPECT_TRUE(r.ok());
        _insertedDocs.emplace_back(r.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }
  }

  static void checkView(LogicalView const& view) {
    containers::FlatHashSet<std::pair<DataSourceId, IndexId>> cids;
    size_t count = 0;
    view.visitCollections([&](DataSourceId cid, LogicalView::Indexes* indexes) {
      if (indexes != nullptr) {
        for (auto indexId : *indexes) {
          cids.emplace(cid, indexId);
          ++count;
        }
      } else {
        cids.emplace(cid, IndexId::none());
        ++count;
      }
      return true;
    });
    EXPECT_EQ(2, cids.size());
    EXPECT_EQ(count, cids.size());
  }

  void queryTests() {
    EXPECT_TRUE(executeQuery(_vocbase,
                             "FOR d IN testView SEARCH 1 == 1 OPTIONS "
                             "{ waitForSync: true } RETURN d")
                    .result.ok());  // commit

    // test grouping with counting
    {
      containers::FlatHashMap<double_t, size_t> expected{
          {100., 5}, {12., 2}, {95., 1},   {90.564, 1}, {1., 1},
          {0., 1},   {50., 1}, {-32.5, 1}, {3.14, 1}  // null
      };

      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value <= 100 COLLECT value = d.value "
          "WITH COUNT INTO size RETURN { 'value' : value, 'names' : size }");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      velocypack::ArrayIterator itr{slice};
      ASSERT_EQ(expected.size(), itr.size());

      for (; itr.valid(); itr.next()) {
        auto const value = itr.value();
        auto const key = value.get("value").getNumber<double_t>();

        auto expectedValue = expected.find(key);
        ASSERT_NE(expectedValue, expected.end());
        ASSERT_EQ(expectedValue->second,
                  value.get("names").getNumber<size_t>());
        ASSERT_EQ(1, expected.erase(key));
      }
      ASSERT_TRUE(expected.empty());
    }

    // test grouping
    {
      containers::FlatHashMap<double_t,
                              containers::FlatHashSet<std::string_view>>
          expected{
              {100., {"A", "E", "G", "I", "J"}},
              {12., {"D", "K"}},
              {95., {"L"}},
              {90.564, {"M"}},
              {1., {"N"}},
              {0., {"O"}},
              {50., {"P"}},
              {-32.5, {"Q"}},
              {3.14, {}}  // null
          };

      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.value <= 100 COLLECT value = d.value "
          "INTO name = d.name RETURN { 'value' : value, 'names' : name }");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());

      velocypack::ArrayIterator itr{slice};
      ASSERT_EQ(expected.size(), itr.size());

      for (; itr.valid(); itr.next()) {
        auto const value = itr.value();
        auto const key = value.get("value").getNumber<double_t>();

        auto expectedValue = expected.find(key);
        ASSERT_NE(expectedValue, expected.end());

        velocypack::ArrayIterator name{value.get("names")};
        auto& expectedNames = expectedValue->second;

        if (expectedNames.empty()) {
          // array must contain singe 'null' value
          ASSERT_EQ(1, name.size());
          ASSERT_TRUE(name.valid());
          ASSERT_TRUE(name.value().isNull());
          name.next();
          ASSERT_FALSE(name.valid());
        } else {
          ASSERT_EQ(expectedNames.size(), name.size());

          for (; name.valid(); name.next()) {
            auto const actualName = name.value().stringView();
            auto expectedName = expectedNames.find(actualName);
            ASSERT_NE(expectedName, expectedNames.end());
            expectedNames.erase(expectedName);
          }
        }

        ASSERT_TRUE(expectedNames.empty());
        ASSERT_EQ(1, expected.erase(key));
      }
      ASSERT_TRUE(expected.empty());
    }

    // test aggregation
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView SEARCH d.seq < 7 COLLECT AGGREGATE sumSeq = "
          "SUM(d.seq) RETURN sumSeq");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());

      velocypack::ArrayIterator itr{slice};
      ASSERT_TRUE(itr.valid());
      EXPECT_EQ(0, itr.value().getNumber<size_t>());
      itr.next();
      EXPECT_FALSE(itr.valid());
    }

    // test aggregation without filter condition
    {
      auto result = executeQuery(_vocbase,
                                 "FOR d IN testView COLLECT AGGREGATE "
                                 "sumSeq = SUM(d.seq) RETURN sumSeq");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());

      velocypack::ArrayIterator itr{slice};
      ASSERT_TRUE(itr.valid());
      EXPECT_EQ(475, itr.value().getNumber<size_t>());
      itr.next();
      EXPECT_FALSE(itr.valid());
    }
    // total number of documents in a view
    {
      auto result = executeQuery(
          _vocbase,
          "FOR d IN testView COLLECT WITH COUNT INTO count RETURN count");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());

      velocypack::ArrayIterator itr{slice};
      ASSERT_TRUE(itr.valid());
      EXPECT_EQ(38, itr.value().getNumber<size_t>());
      itr.next();
      EXPECT_FALSE(itr.valid());
    }
  }

  TRI_vocbase_t _vocbase{TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                         testDBInfo(server.server())};
  std::vector<velocypack::Builder> _insertedDocs;
};

class QueryAggregateView : public QueryAggregate {
 protected:
  void createView() {
    auto createJson = velocypack::Parser::fromJson(
        R"({ "name": "testView", "type": "arangosearch" })");
    auto logicalView = _vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);
    auto& implView = basics::downCast<iresearch::IResearchView>(*logicalView);
    auto updateJson =
        velocypack::Parser::fromJson(absl::Substitute(R"({ "links": {
          "testCollection0": {
            "includeAllFields": true,
            "trackListPositions": true,
            "version": $0 },
          "testCollection1": {
            "includeAllFields": true,
            "version": $0 } } })",
                                                      version()));
    auto r = implView.properties(updateJson->slice(), true, true);
    EXPECT_TRUE(r.ok()) << r.errorMessage();
    checkView(implView);
  }
};

class QueryAggregateSearch : public QueryAggregate {
 protected:
  void createIndexes() {
    {
      bool created = false;
      // TODO remove fields, also see SEARCH-334
      auto createJson = velocypack::Parser::fromJson(absl::Substitute(
          R"({ "name": "testIndex0", "type": "inverted", "version": $0,
               "includeAllFields": true, "fields": [ { "name": "seq" } ],
               "trackListPositions": true })",
          version()));
      auto collection = _vocbase.lookupCollection("testCollection0");
      EXPECT_TRUE(collection);
      collection->createIndex(createJson->slice(), created);
      EXPECT_TRUE(created);
    }
    {
      bool created = false;
      // TODO remove fields, also see SEARCH-334
      auto createJson = velocypack::Parser::fromJson(absl::Substitute(
          R"({ "name": "testIndex1", "type": "inverted", "version": $0,
               "includeAllFields": true, "fields": [ { "name": "seq" } ] })",
          version()));
      auto collection = _vocbase.lookupCollection("testCollection1");
      EXPECT_TRUE(collection);
      collection->createIndex(createJson->slice(), created);
      EXPECT_TRUE(created);
    }
  }

  void createSearch() {
    auto createJson = velocypack::Parser::fromJson(
        R"({ "name": "testView", "type": "search" })");
    auto logicalView = _vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);
    auto& implView = basics::downCast<iresearch::Search>(*logicalView);
    auto updateJson = velocypack::Parser::fromJson(R"({ "indexes": [
      { "collection": "testCollection0", "index": "testIndex0" },
      { "collection": "testCollection1", "index": "testIndex1" } ] })");
    auto r = implView.properties(updateJson->slice(), true, true);
    EXPECT_TRUE(r.ok()) << r.errorMessage();
    checkView(implView);
  }
};

TEST_P(QueryAggregateView, Test) {
  createCollections();
  createView();
  queryTests();
}

TEST_P(QueryAggregateSearch, Test) {
  createCollections();
  createIndexes();
  createSearch();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryAggregateView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryAggregateSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
