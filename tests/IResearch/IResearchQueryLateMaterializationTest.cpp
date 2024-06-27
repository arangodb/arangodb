////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <absl/strings/str_replace.h>

#include <velocypack/Iterator.h>

#include "Aql/OptimizerRule.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchView.h"
#include "IResearchQueryCommon.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb::tests {
namespace {

class QueryLateMaterialization : public QueryTest {
 protected:
  std::deque<std::shared_ptr<velocypack::Buffer<uint8_t>>> insertedDocs;

  virtual void createView() = 0;

  void commitView() {
    auto r = executeQuery(vocbase(),
                          "FOR d IN view_1 SEARCH 1 == 1"
                          " OPTIONS { waitForSync: true } RETURN d");
    EXPECT_TRUE(r.result.ok()) << r.result.errorMessage();
    r = executeQuery(vocbase(),
                     "FOR d IN view_2 SEARCH 1 == 1"
                     " OPTIONS { waitForSync: true } RETURN d");
    EXPECT_TRUE(r.result.ok()) << r.result.errorMessage();
  }

  void SetUp() override {
    // add collection_1
    std::shared_ptr<LogicalCollection> logicalCollection1;
    {
      auto collectionJson =
          VPackParser::fromJson(R"({"name": "collection_1" })");
      logicalCollection1 = vocbase().createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection1);
    }
    // add collection_2
    std::shared_ptr<LogicalCollection> logicalCollection2;
    {
      auto collectionJson =
          VPackParser::fromJson(R"({"name": "collection_2" })");
      logicalCollection2 = vocbase().createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection2);
    }
    createView();
    // populate view with the data
    {
      OperationOptions opt;
      static std::vector<std::string> const kEmpty;
      transaction::Methods trx(
          transaction::StandaloneContext::create(
              vocbase(), arangodb::transaction::OperationOriginTestCase{}),
          kEmpty, {logicalCollection1->name(), logicalCollection2->name()},
          kEmpty, transaction::Options());
      EXPECT_TRUE(trx.begin().ok());
      // insert into collection_1
      {
        auto builder = VPackParser::fromJson(
            "["
            "{\"_key\": \"c0\", \"str\": \"cat\", \"foo\": \"foo0\", "
            "\"value\": 0},"
            "{\"_key\": \"c1\", \"str\": \"cat\", \"foo\": \"foo1\", "
            "\"value\": 1},"
            "{\"_key\": \"c2\", \"str\": \"cat\", \"foo\": \"foo2\", "
            "\"value\": 2},"
            "{\"_key\": \"c3\", \"str\": \"cat\", \"foo\": \"foo3\", "
            "\"value\": 3}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : velocypack::ArrayIterator(root)) {
          auto res = trx.insert(logicalCollection1->name(), doc, opt);
          EXPECT_TRUE(res.ok());

          res = trx.document(logicalCollection1->name(), res.slice(), opt);
          EXPECT_TRUE(res.ok());
          insertedDocs.emplace_back(std::move(res.buffer));
        }
      }
      // insert into collection_2
      {
        auto builder = VPackParser::fromJson(
            "["
            "{\"_key\": \"c_0\", \"str\": \"cat\", \"foo\": \"foo_0\", "
            "\"value\": 10},"
            "{\"_key\": \"c_1\", \"str\": \"cat\", \"foo\": \"foo_1\", "
            "\"value\": 11},"
            "{\"_key\": \"c_2\", \"str\": \"cat\", \"foo\": \"foo_2\", "
            "\"value\": 12},"
            "{\"_key\": \"c_3\", \"str\": \"cat\", \"foo\": \"foo_3\", "
            "\"value\": 13}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : velocypack::ArrayIterator(root)) {
          auto res = trx.insert(logicalCollection2->name(), doc, opt);
          EXPECT_TRUE(res.ok());

          res = trx.document(logicalCollection2->name(), res.slice(), opt);
          EXPECT_TRUE(res.ok());
          insertedDocs.emplace_back(std::move(res.buffer));
        }
      }
      EXPECT_TRUE(trx.commit().ok());
      commitView();
    }
  }

  void executeAndCheck(std::string const& query,
                       std::vector<velocypack::Slice> const& expectedDocs,
                       bool checkRuleOnly) {
    EXPECT_TRUE(tests::assertRules(
        vocbase(), query, {aql::OptimizerRule::handleArangoSearchViewsRule},
        nullptr,
        R"({"optimizer":{"rules":["-arangosearch-constrained-sort"]}})"));

    EXPECT_TRUE(tests::assertRules(
        vocbase(), query,
        {aql::OptimizerRule::lateDocumentMaterializationArangoSearchRule},
        nullptr,
        R"({"optimizer":{"rules":["-arangosearch-constrained-sort"]}})"));

    auto queryResult = tests::executeQuery(
        vocbase(), query, nullptr,
        R"({"optimizer":{"rules":["-arangosearch-constrained-sort"]}})");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    if (checkRuleOnly) {
      return;
    }

    velocypack::ArrayIterator resultIt(result);

    ASSERT_EQ(expectedDocs.size(), resultIt.size());
    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE(0 == basics::VelocyPackHelper::compare(
                           velocypack::Slice(*expectedDoc), resolved, true));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }

  void queryTests() {
    // tests
    {
      auto const query =
          "FOR d IN view_2"
          " SEARCH d.value IN [1, 2, 11, 12]"
          " LET a = NOOPT(d.foo)"
          " LET e = SUM(FOR c IN view_1 LET p = CONCAT(c.foo, c.foo) RETURN p)"
          " SORT CONCAT(a, e) LIMIT 10 RETURN d";

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocs[5]->data()),
          velocypack::Slice(insertedDocs[6]->data()),
          velocypack::Slice(insertedDocs[1]->data()),
          velocypack::Slice(insertedDocs[2]->data())};

      executeAndCheck(query, expectedDocs, false);
    }
    {
      auto const query =
          "FOR d IN view_2"
          " FILTER d.value IN [1, 2]"
          " SORT d.foo DESC LIMIT 10 RETURN d";

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocs[2]->data()),
          velocypack::Slice(insertedDocs[1]->data())};

      executeAndCheck(query, expectedDocs, false);
    }
    {
      auto const query =
          "FOR d IN view_2"
          " SEARCH d.value IN [1, 2, 11, 12]"
          " SORT d.value DESC LET c = BM25(d) * 2"
          " SORT CONCAT(BM25(d), c, d.value) LIMIT 10 RETURN d";

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocs[1]->data()),
          velocypack::Slice(insertedDocs[5]->data()),
          velocypack::Slice(insertedDocs[6]->data()),
          velocypack::Slice(insertedDocs[2]->data())};

      executeAndCheck(query, expectedDocs, false);
    }
    {
      auto const query =
          "FOR d IN view_2"
          " SEARCH d.value IN [1, 2, 11, 12]"
          " SORT RAND(), d.value DESC LIMIT 10 RETURN d";

      executeAndCheck(query, std::vector<velocypack::Slice>{}, true);
    }
    {
      auto const query =
          "FOR d IN view_2"
          " SEARCH d.value IN [1, 2, 11, 12]"
          " SORT d.value DESC, d.foo LIMIT 10 RETURN d";

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocs[6]->data()),
          velocypack::Slice(insertedDocs[5]->data()),
          velocypack::Slice(insertedDocs[2]->data()),
          velocypack::Slice(insertedDocs[1]->data())};

      executeAndCheck(query, expectedDocs, false);
    }
    {
      auto const query =
          "FOR d IN view_2"
          " SEARCH d.value IN [1, 2, 11, 12]"
          " SORT d.value DESC LIMIT 10 RETURN d";

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocs[6]->data()),
          velocypack::Slice(insertedDocs[5]->data()),
          velocypack::Slice(insertedDocs[2]->data()),
          velocypack::Slice(insertedDocs[1]->data())};

      executeAndCheck(query, expectedDocs, false);
    }
    {
      auto const query =
          "FOR d IN view_2"
          " SEARCH d.value IN [1, 2, 11, 12]"
          " SORT d.value DESC"
          " LIMIT 10"
          " SORT NOOPT(d.value) ASC RETURN d";

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocs[1]->data()),
          velocypack::Slice(insertedDocs[2]->data()),
          velocypack::Slice(insertedDocs[5]->data()),
          velocypack::Slice(insertedDocs[6]->data())};

      executeAndCheck(query, expectedDocs, false);
    }
    {
      auto const query =
          "FOR d IN view_2"
          " SEARCH d.value IN [1, 2, 11, 12]"
          " SORT d.value DESC"
          " LIMIT 10 SORT TFIDF(d) DESC LIMIT 4 RETURN d";

      executeAndCheck(query, std::vector<velocypack::Slice>{}, true);
    }
    {
      auto const query =
          "FOR d IN view_2"
          " SEARCH d.value IN [1, 2, 11, 12]"
          " SORT d.value DESC"
          " LIMIT 10 LET c = CONCAT(NOOPT(d._key), '-C') RETURN c";

      executeAndCheck(query, std::vector<velocypack::Slice>{}, true);
    }
    {
      auto const query =
          "FOR d IN view_2"
          " SEARCH d.value IN [1, 2, 11, 12]"
          " SORT d.value DESC LIMIT 3, 1 RETURN d";

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocs[1]->data())};

      executeAndCheck(query, expectedDocs, false);
    }
    {
      auto const query =
          "FOR d IN view_2"
          " SEARCH d.value IN [1, 2, 11, 12]"
          " SORT d.value DESC LIMIT 5, 10 RETURN d";

      executeAndCheck(query, std::vector<velocypack::Slice>{}, false);
    }
    {
      auto const query =
          "FOR c IN view_1"
          " SEARCH c.value == 1"
          " FOR d IN view_2"
          "  SEARCH d.value IN [c.value, c.value + 1]"
          "  SORT d.value DESC LIMIT 10 RETURN d";

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocs[2]->data()),
          velocypack::Slice(insertedDocs[1]->data())};

      executeAndCheck(query, expectedDocs, false);
    }
  }
};

class QueryLateMaterializationView : public QueryLateMaterialization {
 protected:
  ViewType type() const final { return ViewType::kArangoSearch; }

  void addLinkToCollection(std::shared_ptr<iresearch::IResearchView>& view) {
    auto v = static_cast<uint32_t>(version());
    auto updateJson = VPackParser::fromJson(absl::Substitute(
        R"({"links": {"collection_1": {"includeAllFields": true, "version": $0},
                      "collection_2": {"includeAllFields": true, "version":
$0}}})",
        v));
    EXPECT_TRUE(view->properties(updateJson->slice(), true, true).ok());

    velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, LogicalDataSource::Serialization::Properties);
    builder.close();

    auto slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(slice.get("type").copyString() ==
                iresearch::StaticStrings::ViewArangoSearchType);
    EXPECT_TRUE(slice.get("deleted").isNone());  // no system properties
    auto tmpSlice = slice.get("links");
    EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
  }

  void createView() final {
    // create view_1
    std::shared_ptr<iresearch::IResearchView> view1;
    {
      auto createJson = VPackParser::fromJson(
          R"({"name": "view_1", "type": "arangosearch"})");
      auto view1 = std::dynamic_pointer_cast<iresearch::IResearchView>(
          vocbase().createView(createJson->slice(), false));
      ASSERT_FALSE(!view1);

      // add links to collections
      addLinkToCollection(view1);
    }
    // create view_2
    std::shared_ptr<iresearch::IResearchView> view2;
    {
      auto createJson =
          VPackParser::fromJson(R"({"name": "view_2", "type": "arangosearch",
                                   "primarySort": [{"field": "value",
"direction": "asc"}, {"field": "foo", "direction": "desc"}]})");
      view2 = std::dynamic_pointer_cast<iresearch::IResearchView>(
          vocbase().createView(createJson->slice(), false));
      ASSERT_FALSE(!view2);

      // add links to collections
      addLinkToCollection(view2);
    }
  }
};

class QueryLateMaterializationSearch : public QueryLateMaterialization {
 protected:
  ViewType type() const final { return ViewType::kSearchAlias; }

  void addLinkToCollection(std::shared_ptr<iresearch::Search>& view,
                           std::string_view name) {
    auto const viewDefinition = absl::Substitute(R"({
      "indexes": [
        { "collection": "collection_1", "index": "index$0_1"},
        { "collection": "collection_2", "index": "index$0_2"}
      ]})",
                                                 name);
    auto updateJson = arangodb::velocypack::Parser::fromJson(viewDefinition);
    auto r = view->properties(updateJson->slice(), true, true);
    EXPECT_TRUE(r.ok()) << r.errorMessage();
  }

  void createView() final {
    // create indexes
    auto createIndex = [this](int name, bool sorted) {
      bool created = false;
      auto createJson = VPackParser::fromJson(absl::Substitute(
          R"({ "name": "index$0_$1", "type": "inverted",
               "version": $2, $3
               "includeAllFields": true })",
          (sorted ? "Sorted" : "Unordered"), name, version(),
          (sorted
               ? R"("primarySort": { "fields": [{"field": "value", "direction": "asc"}, {"field": "foo", "direction": "desc"}] },)"
               : "")));
      auto collection =
          vocbase().lookupCollection(absl::Substitute("collection_$0", name));
      ASSERT_TRUE(collection);
      collection->createIndex(createJson->slice(), created).waitAndGet();
      ASSERT_TRUE(created);
    };
    createIndex(1, false);
    createIndex(2, false);
    createIndex(1, true);
    createIndex(2, true);

    // add view_1
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"view_1\", \"type\": \"search-alias\" }");

      auto view = std::dynamic_pointer_cast<arangodb::iresearch::Search>(
          vocbase().createView(createJson->slice(), false));
      ASSERT_FALSE(!view);
      addLinkToCollection(view, "Unordered");
    }
    // add view_2
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"view_2\", \"type\": \"search-alias\" }");

      auto view = std::dynamic_pointer_cast<arangodb::iresearch::Search>(
          vocbase().createView(createJson->slice(), false));
      ASSERT_FALSE(!view);
      addLinkToCollection(view, "Sorted");
    }
  }
};

TEST_P(QueryLateMaterializationView, test) {
  // view
  queryTests();
}

TEST_P(QueryLateMaterializationSearch, test) {
  // search
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryLateMaterializationView,
                        GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryLateMaterializationSearch,
                        GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
