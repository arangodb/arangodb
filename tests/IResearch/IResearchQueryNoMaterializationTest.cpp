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

#include "Aql/AqlFunctionFeature.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/OptimizerRule.h"
#include "Aql/Query.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewStoredValues.h"
#include "IResearchQueryCommon.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb::tests {
namespace {

class QueryTestMulti
    : public ::testing::TestWithParam<
          std::tuple<arangodb::ViewType, arangodb::iresearch::LinkVersion>>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
 private:
  TRI_vocbase_t* _vocbase{nullptr};

 protected:
  arangodb::tests::mocks::MockAqlServer server;

  virtual arangodb::ViewType type() const { return std::get<0>(GetParam()); }

  QueryTestMulti() : server{false} {
    arangodb::tests::init(true);

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();

    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    // required for IResearchAnalyzerFeature::emplace(...)
    dbFeature.createDatabase(testDBInfo(server.server()), _vocbase);

    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(
        *_vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        unused);
    unused = nullptr;

    auto res = analyzers.emplace(
        result, "testVocbase::test_analyzer", "TestAnalyzer",
        VPackParser::fromJson("\"abc\"")->slice(),
        arangodb::transaction::OperationOriginTestCase{},
        arangodb::iresearch::Features(
            {}, irs::IndexFeatures::FREQ |
                    irs::IndexFeatures::POS));  // required for PHRASE
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result, "testVocbase::test_csv_analyzer", "TestDelimAnalyzer",
        VPackParser::fromJson("\",\"")->slice(),
        arangodb::transaction::OperationOriginTestCase{});  // cache analyzer
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result, "testVocbase::text_en", "text",
        VPackParser::fromJson(
            "{ \"locale\": \"en.UTF-8\", \"stopwords\": [ ] }")
            ->slice(),
        arangodb::transaction::OperationOriginTestCase{},
        arangodb::iresearch::Features{
            arangodb::iresearch::FieldFeatures::NORM,
            irs::IndexFeatures::FREQ |
                irs::IndexFeatures::POS});  // cache analyzer
    EXPECT_TRUE(res.ok());

    auto sysVocbase =
        server.getFeature<arangodb::SystemDatabaseFeature>().use();
    arangodb::methods::Collections::createSystem(
        *sysVocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        unused);
    unused = nullptr;

    res =
        analyzers.emplace(result, "_system::test_analyzer", "TestAnalyzer",
                          VPackParser::fromJson("\"abc\"")->slice(),
                          arangodb::transaction::OperationOriginTestCase{},
                          arangodb::iresearch::Features{
                              irs::IndexFeatures::FREQ |
                              irs::IndexFeatures::POS});  // required for PHRASE

    res = analyzers.emplace(
        result, "_system::ngram_test_analyzer13", "ngram",
        VPackParser::fromJson("{\"min\":1, \"max\":3, \"streamType\":\"utf8\", "
                              "\"preserveOriginal\":false}")
            ->slice(),
        arangodb::transaction::OperationOriginTestCase{},
        arangodb::iresearch::Features{
            irs::IndexFeatures::FREQ |
            irs::IndexFeatures::POS});  // required for PHRASE

    res = analyzers.emplace(
        result, "_system::ngram_test_analyzer2", "ngram",
        VPackParser::fromJson("{\"min\":2, \"max\":2, \"streamType\":\"utf8\", "
                              "\"preserveOriginal\":false}")
            ->slice(),
        arangodb::transaction::OperationOriginTestCase{},
        arangodb::iresearch::Features{
            irs::IndexFeatures::FREQ |
            irs::IndexFeatures::POS});  // required for PHRASE

    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result, "_system::test_csv_analyzer", "TestDelimAnalyzer",
        VPackParser::fromJson("\",\"")->slice(),
        arangodb::transaction::OperationOriginTestCase{});  // cache analyzer
    EXPECT_TRUE(res.ok());

    auto& functions = server.getFeature<arangodb::aql::AqlFunctionFeature>();
    // register fake non-deterministic function in order to suppress
    // optimizations
    functions.add(arangodb::aql::Function{
        "_NONDETERM_", ".",
        arangodb::aql::Function::makeFlags(
            // fake non-deterministic
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::aql::AstNode const&,
           arangodb::aql::functions::VPackFunctionParametersView params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // register fake non-deterministic function in order to suppress
    // optimizations
    functions.add(arangodb::aql::Function{
        "_FORWARD_", ".",
        arangodb::aql::Function::makeFlags(
            // fake deterministic
            arangodb::aql::Function::Flags::Deterministic,
            arangodb::aql::Function::Flags::Cacheable,
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::aql::AstNode const&,
           arangodb::aql::functions::VPackFunctionParametersView params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // external function names must be registred in upper-case
    // user defined functions have ':' in the external function name
    // function arguments string format:
    // requiredArg1[,requiredArg2]...[|optionalArg1[,optionalArg2]...]
    arangodb::aql::Function customScorer(
        "CUSTOMSCORER", ".|+",
        arangodb::aql::Function::makeFlags(
            arangodb::aql::Function::Flags::Deterministic,
            arangodb::aql::Function::Flags::Cacheable,
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        nullptr);
    arangodb::iresearch::addFunction(functions, customScorer);

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(
        dbPathFeature);  // ensure test data is stored in a unique directory
  }

  TRI_vocbase_t& vocbase() {
    TRI_ASSERT(_vocbase != nullptr);
    return *_vocbase;
  }

  arangodb::iresearch::LinkVersion linkVersion() const noexcept {
    return std::get<1>(GetParam());
  }

  arangodb::iresearch::LinkVersion version() const noexcept {
    return std::get<1>(GetParam());
  }
};

constexpr const char* collectionName1 = "collection_1";
constexpr const char* collectionName2 = "collection_2";
constexpr const char* viewName = "view";

class QueryNoMaterialization : public QueryTestMulti {
 protected:
  void addLinkToCollection(
      std::shared_ptr<arangodb::iresearch::IResearchView>& view) {
    auto versionStr = std::to_string(static_cast<uint32_t>(linkVersion()));

    auto updateJson =
        VPackParser::fromJson(std::string("{") +
                              "\"links\": {"
                              "\"" +
                              collectionName1 +
                              "\": {\"includeAllFields\": true, "
                              "\"storeValues\": \"id\", \"version\": " +
                              versionStr +
                              "},"
                              "\"" +
                              collectionName2 +
                              "\": {\"includeAllFields\": true, "
                              "\"storeValues\": \"id\", \"version\": " +
                              versionStr +
                              "}"
                              "}}");
    EXPECT_TRUE(view->properties(updateJson->slice(), true, true).ok());

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder,
                     arangodb::LogicalDataSource::Serialization::Properties);
    builder.close();

    auto slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(slice.get("type").copyString() ==
                arangodb::iresearch::StaticStrings::ViewArangoSearchType);
    EXPECT_TRUE(slice.get("deleted").isNone());  // no system properties
    auto tmpSlice = slice.get("links");
    EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
  }

  void SetUp() override {
    // add collection_1
    std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
    {
      auto collectionJson = VPackParser::fromJson(std::string("{\"name\": \"") +
                                                  collectionName1 + "\"}");
      logicalCollection1 = vocbase().createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection1);
    }

    // add collection_2
    std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
    {
      auto collectionJson = VPackParser::fromJson(std::string("{\"name\": \"") +
                                                  collectionName2 + "\"}");
      logicalCollection2 = vocbase().createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection2);
    }

    auto createIndexes = [&](int index, std::string_view addition) {
      bool created = false;
      auto createJson = VPackParser::fromJson(absl::Substitute(
          R"({ "name": "index_$0", "type": "inverted",
               "version": $1, $2
               "includeAllFields": true })",
          index, version(), addition));
      logicalCollection1->createIndex(createJson->slice(), created)
          .waitAndGet();
      ASSERT_TRUE(created);
      created = false;
      logicalCollection2->createIndex(createJson->slice(), created)
          .waitAndGet();
      ASSERT_TRUE(created);
    };

    auto addIndexes = [](auto& view, int index) {
      auto const viewDefinition = absl::Substitute(R"({ "indexes": [
        { "collection": "collection_1", "index": "index_$0"},
        { "collection": "collection_2", "index": "index_$0"}
      ]})",
                                                   index);
      auto updateJson = arangodb::velocypack::Parser::fromJson(viewDefinition);
      auto r = view.properties(updateJson->slice(), true, true);
      EXPECT_TRUE(r.ok()) << r.errorMessage();
    };

    // create view
    if (type() == ViewType::kArangoSearch) {
      auto createJson =
          VPackParser::fromJson(std::string("{") + "\"name\": \"" + viewName +
                                "\", \
           \"type\": \"arangosearch\", \
           \"primarySort\": [{\"field\": \"value\", \"direction\": \"asc\"}, {\"field\": \"foo\", \"direction\": \"desc\"}, {\"field\": \"boo\", \"direction\": \"desc\"}], \
           \"storedValues\": [{\"fields\":[\"str\"], \"compression\":\"none\"}, [\"value\"], [\"_id\"], [\"str\", \"value\"], [\"exist\"]] \
        }");
      auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
          vocbase().createView(createJson->slice(), false));
      ASSERT_FALSE(!view);

      // add links to collections
      addLinkToCollection(view);
    } else {
      auto createJson = VPackParser::fromJson(
          "{\"name\": \"view\", \"type\": \"search-alias\" }");
      auto view = std::dynamic_pointer_cast<iresearch::Search>(
          vocbase().createView(createJson->slice(), false));
      ASSERT_FALSE(!view);
      createIndexes(1, R"("primarySort": {"fields": [
                            {"field": "value", "direction": "asc"},
                            {"field": "foo",   "direction": "desc"},
                            {"field": "boo",   "direction": "desc"}]},
                          "storedValues": [{"fields":["str"], "compression":"none"}, ["value"], ["_id"], ["str", "value"], ["exist"]],)");
      addIndexes(*view, 1);
    }
    // create view2
    if (type() == ViewType::kArangoSearch) {
      auto createJson =
          VPackParser::fromJson(std::string("{") + "\"name\": \"" + viewName +
                                "2\", \
           \"type\": \"arangosearch\", \
           \"primarySort\": [{\"field\": \"value\", \"direction\": \"asc\"}], \
           \"storedValues\": [] \
        }");
      auto view2 =
          std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
              vocbase().createView(createJson->slice(), false));
      ASSERT_FALSE(!view2);

      // add links to collections
      addLinkToCollection(view2);
    } else {
      auto createJson = VPackParser::fromJson(
          "{\"name\": \"view2\", \"type\": \"search-alias\" }");
      auto view = std::dynamic_pointer_cast<iresearch::Search>(
          vocbase().createView(createJson->slice(), false));
      ASSERT_FALSE(!view);
      createIndexes(
          2,
          R"("primarySort": {"fields": [{"field": "value", "direction": "asc"}]},
                          "storedValues": [],)");
      addIndexes(*view, 2);
    }

    // populate view with the data
    {
      arangodb::OperationOptions opt;
      static std::vector<std::string> const EMPTY;
      arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::create(
              vocbase(), arangodb::transaction::OperationOriginTestCase{}),
          EMPTY, {logicalCollection1->name(), logicalCollection2->name()},
          EMPTY, arangodb::transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collection_1
      {
        auto builder = VPackParser::fromJson(
            "["
            "{\"_key\": \"c0\", \"str\": \"cat\", \"foo\": \"foo0\", "
            "\"value\": 0, \"exist\": \"ex0\"},"
            "{\"_key\": \"c1\", \"str\": \"cat\", \"foo\": \"foo1\", "
            "\"value\": 1},"
            "{\"_key\": \"c2\", \"str\": \"cat\", \"foo\": \"foo2\", "
            "\"value\": 2, \"exist\": \"ex2\"},"
            "{\"_key\": \"c3\", \"str\": \"cat\", \"foo\": \"foo3\", "
            "\"value\": 3}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          auto res = trx.insert(logicalCollection1->name(), doc, opt);
          EXPECT_TRUE(res.ok());
        }
      }

      // insert into collection_2
      {
        auto builder = VPackParser::fromJson(
            "["
            "{\"_key\": \"c_0\", \"str\": \"cat\", \"foo\": \"foo_0\", "
            "\"value\": 10, \"exist\": \"ex_10\"},"
            "{\"_key\": \"c_1\", \"str\": \"cat\", \"foo\": \"foo_1\", "
            "\"value\": 11},"
            "{\"_key\": \"c_2\", \"str\": \"cat\", \"foo\": \"foo_2\", "
            "\"value\": 12, \"exist\": \"ex_12\"},"
            "{\"_key\": \"c_3\", \"str\": \"cat\", \"foo\": \"foo_3\", "
            "\"value\": 13}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          auto res = trx.insert(logicalCollection2->name(), doc, opt);
          EXPECT_TRUE(res.ok());
        }
      }

      EXPECT_TRUE(trx.commit().ok());
      EXPECT_TRUE(
          (arangodb::tests::executeQuery(vocbase(),
                                         "FOR d IN view SEARCH 1 ==1 OPTIONS "
                                         "{ waitForSync: true } RETURN d")
               .result.ok()));  // commit
      EXPECT_TRUE(
          (arangodb::tests::executeQuery(vocbase(),
                                         "FOR d IN view2 SEARCH 1 ==1 OPTIONS "
                                         "{ waitForSync: true } RETURN d")
               .result.ok()));  // commit
    }
  }

  void executeAndCheck(std::string const& queryString,
                       std::vector<VPackValue> const& expectedValues,
                       arangodb::velocypack::ValueLength numOfColumns,
                       std::set<std::pair<ptrdiff_t, size_t>>&& fields) {
    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase(), queryString,
        {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    auto query = arangodb::aql::Query::create(
        arangodb::transaction::StandaloneContext::create(
            vocbase(), arangodb::transaction::OperationOriginTestCase{}),
        arangodb::aql::QueryString(queryString), nullptr);
    auto const res = query->explain();
    ASSERT_TRUE(res.data);
    auto const explanation = res.data->slice();
    arangodb::velocypack::ArrayIterator nodes(explanation.get("nodes"));
    auto found = false;
    for (auto const node : nodes) {
      if (node.hasKey("type") && node.get("type").isString() &&
          node.get("type").stringView() == "EnumerateViewNode") {
        EXPECT_TRUE(node.hasKey("noMaterialization") &&
                    node.get("noMaterialization").isBool() &&
                    node.get("noMaterialization").getBool());
        ASSERT_TRUE(node.hasKey("viewValuesVars") &&
                    node.get("viewValuesVars").isArray());
        ASSERT_EQ(numOfColumns, node.get("viewValuesVars").length());
        arangodb::velocypack::ArrayIterator columnFields(
            node.get("viewValuesVars"));
        for (auto const cf : columnFields) {
          ASSERT_TRUE(cf.isObject());
          if (cf.hasKey("fieldNumber")) {
            auto fieldNumber = cf.get("fieldNumber");
            ASSERT_TRUE(fieldNumber.isNumber<size_t>());
            auto it = fields.find(std::make_pair(
                arangodb::iresearch::IResearchViewNode::kSortColumnNumber,
                fieldNumber.getNumber<size_t>()));
            ASSERT_TRUE(it != fields.end());
            fields.erase(it);
          } else {
            ASSERT_TRUE(cf.hasKey("columnNumber") &&
                        cf.get("columnNumber").isNumber());
            auto columnNumber = cf.get("columnNumber").getNumber<int>();
            ASSERT_TRUE(cf.hasKey("viewStoredValuesVars") &&
                        cf.get("viewStoredValuesVars").isArray());
            arangodb::velocypack::ArrayIterator fs(
                cf.get("viewStoredValuesVars"));
            for (auto const f : fs) {
              ASSERT_TRUE(f.hasKey("fieldNumber") &&
                          f.get("fieldNumber").isNumber<size_t>());
              auto it = fields.find(std::make_pair(
                  columnNumber, f.get("fieldNumber").getNumber<size_t>()));
              ASSERT_TRUE(it != fields.end());
              fields.erase(it);
            }
          }
        }
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(fields.empty());

    auto queryResult = arangodb::tests::executeQuery(vocbase(), queryString);
    ASSERT_TRUE(queryResult.result.ok());

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

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_P(QueryNoMaterialization, sortColumnPriority) {
  auto const queryString =
      std::string("FOR d IN ") + viewName +
      " SEARCH d.value IN [1, 2, 11, 12] SORT d.value RETURN d.value";

  std::vector<VPackValue> expectedValues{VPackValue(1), VPackValue(2),
                                         VPackValue(11), VPackValue(12)};

  executeAndCheck(
      queryString, expectedValues, 1,
      {{arangodb::iresearch::IResearchViewNode::kSortColumnNumber, 0}});
}

TEST_P(QueryNoMaterialization, sortColumnPriorityViewsSubquery) {
  // this checks proper stored variables buffer resizing uring optimization
  auto const queryString =
      std::string("FOR c IN ") + viewName +
      "2 SEARCH c.value IN [1, 2, 11, 12] SORT c.value FOR d IN " + viewName +
      " SEARCH d.value == c.value SORT d.value RETURN d.value";

  std::vector<VPackValue> expectedValues{VPackValue(1), VPackValue(2),
                                         VPackValue(11), VPackValue(12)};

  auto queryResult = arangodb::tests::executeQuery(vocbase(), queryString);
  ASSERT_TRUE(queryResult.result.ok());

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

TEST_P(QueryNoMaterialization, maxMatchColumnPriority) {
  auto const queryString = std::string("FOR d IN ") + viewName +
                           " FILTER d.str == 'cat' SORT d.value RETURN d.value";

  std::vector<VPackValue> expectedValues{
      VPackValue(0),  VPackValue(1),  VPackValue(2),  VPackValue(3),
      VPackValue(10), VPackValue(11), VPackValue(12), VPackValue(13)};

  executeAndCheck(queryString, expectedValues, 1, {{3, 0}, {3, 1}});
}

TEST_P(QueryNoMaterialization, sortAndStoredValues) {
  auto const queryString =
      std::string("FOR d IN ") + viewName + " SORT d._id RETURN d.foo";

  std::vector<VPackValue> expectedValues{
      VPackValue("foo0"),  VPackValue("foo1"),  VPackValue("foo2"),
      VPackValue("foo3"),  VPackValue("foo_0"), VPackValue("foo_1"),
      VPackValue("foo_2"), VPackValue("foo_3")};

  executeAndCheck(
      queryString, expectedValues, 2,
      {{arangodb::iresearch::IResearchViewNode::kSortColumnNumber, 1}, {2, 0}});
}

TEST_P(QueryNoMaterialization, fieldExistence) {
  auto const queryString =
      std::string("FOR d IN ") + viewName +
      " SEARCH EXISTS(d.exist) SORT d.value RETURN d.value";

  std::vector<VPackValue> expectedValues{VPackValue(0), VPackValue(2),
                                         VPackValue(10), VPackValue(12)};

  executeAndCheck(
      queryString, expectedValues, 1,
      {{arangodb::iresearch::IResearchViewNode::kSortColumnNumber, 0}});
}

TEST_P(QueryNoMaterialization, storedFieldExistence) {
  auto const queryString =
      std::string("FOR d IN ") + viewName +
      " SEARCH EXISTS(d.exist) SORT d.value RETURN d.exist";

  std::vector<VPackValue> expectedValues{VPackValue("ex0"), VPackValue("ex2"),
                                         VPackValue("ex_10"),
                                         VPackValue("ex_12")};

  executeAndCheck(
      queryString, expectedValues, 2,
      {{arangodb::iresearch::IResearchViewNode::kSortColumnNumber, 0}, {4, 0}});
}

TEST_P(QueryNoMaterialization, emptyField) {
  auto const queryString = std::string("FOR d IN ") + viewName +
                           " SORT d.exist DESC LIMIT 1 RETURN d.exist";

  std::vector<VPackValue> expectedValues{VPackValue("ex2")};

  executeAndCheck(queryString, expectedValues, 1, {{4, 0}});
}

TEST_P(QueryNoMaterialization, testStoredValuesRecord) {
  static std::vector<std::string> const EMPTY;
  auto doc = arangodb::velocypack::Parser::fromJson(
      "{ \"str\": \"abc\", \"value\": 10 }");
  std::string collectionName("testCollection");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\":\"" + collectionName + "\"}");
  auto logicalCollection = vocbase().createCollection(collectionJson->slice());
  ASSERT_TRUE(logicalCollection);
  size_t const columnsCount = 6;  // PK + storedValues
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \
        \"id\": 42, \
        \"name\": \"testView\", \
        \"type\": \"arangosearch\", \
        \"storedValues\": [{\"fields\":[\"str\"]}, {\"fields\":[\"foo\"]}, {\"fields\":[\"value\"]},\
                          {\"fields\":[\"_id\"]}, {\"fields\":[\"str\", \"foo\", \"value\"]}] \
      }");
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase().createView(viewJson->slice(), false));
  ASSERT_TRUE(view);

  auto updateJson =
      VPackParser::fromJson("{\"links\": {\"" + collectionName +
                            "\": {\"includeAllFields\": true} }}");
  EXPECT_TRUE(view->properties(updateJson->slice(), true, true).ok());

  arangodb::velocypack::Builder builder;

  builder.openObject();
  view->properties(builder,
                   arangodb::LogicalDataSource::Serialization::Properties);
  builder.close();

  auto slice = builder.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE(slice.get("type").copyString() ==
              arangodb::iresearch::StaticStrings::ViewArangoSearchType);
  EXPECT_TRUE(slice.get("deleted").isNone());  // no system properties
  auto tmpSlice = slice.get("links");
  EXPECT_TRUE(tmpSlice.isObject() && 1 == tmpSlice.length());

  {
    arangodb::OperationOptions opt;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase(), arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, {logicalCollection->name()}, EMPTY,
        arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    auto const res = trx.insert(logicalCollection->name(), doc->slice(), opt);
    EXPECT_TRUE(res.ok());

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(
                    *logicalCollection, *view)
                    ->commit()
                    .ok());
  }

  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase(), arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    auto link = arangodb::iresearch::IResearchLinkHelper::find(
        *logicalCollection, *view);
    ASSERT_TRUE(link);
    auto const snapshot = link->snapshot();
    auto const& snapshotReader = snapshot.getDirectoryReader();
    std::string const columns[] = {
        arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            std::string("_id"),
        arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            std::string("foo"),
        arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            std::string("foo") +
            arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            "str" +
            arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            "value",
        arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            std::string("str"),
        arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            std::string("value"),
        "@_PK"};
    for (auto const& segment : snapshotReader) {
      auto col = segment.columns();
      auto doc = segment.docs_iterator();
      ASSERT_TRUE(doc);
      ASSERT_TRUE(doc->next());
      size_t counter = 0;
      while (col->next()) {
        auto const& val = col->value();
        ASSERT_TRUE(counter < columnsCount);
        EXPECT_EQ(columns[counter], val.name());
        if (5 == counter) {  // skip PK
          ++counter;
          continue;
        }
        auto columnReader = segment.column(val.id());
        ASSERT_TRUE(columnReader);
        auto valReader = columnReader->iterator(irs::ColumnHint::kNormal);
        ASSERT_TRUE(valReader);
        auto* value = irs::get<irs::payload>(*valReader);
        ASSERT_TRUE(value);
        ASSERT_EQ(doc->value(), valReader->seek(doc->value()));
        if (1 == counter) {  // foo
          EXPECT_TRUE(irs::IsNull(value->value));
          ++counter;
          continue;
        }
        size_t valueSize = value->value.size();
        auto slice = VPackSlice(value->value.data());
        switch (counter) {
          case 0: {
            ASSERT_TRUE(slice.isString());
            arangodb::velocypack::ValueLength length = 0;
            auto str = slice.getString(length);
            std::string strVal(str, length);
            ASSERT_TRUE(length > collectionName.size());
            EXPECT_EQ(collectionName + "/",
                      strVal.substr(0, collectionName.size() + 1));
            break;
          }
          case 2: {
            arangodb::velocypack::ValueLength size = slice.byteSize();
            ASSERT_TRUE(slice.isString());
            arangodb::velocypack::ValueLength length = 0;
            auto str = slice.getString(length);
            EXPECT_EQ("abc", std::string(str, length));
            slice = VPackSlice(slice.start() + slice.byteSize());
            size += slice.byteSize();
            EXPECT_TRUE(slice.isNull());
            slice = VPackSlice(slice.start() + slice.byteSize());
            size += slice.byteSize();
            ASSERT_TRUE(slice.isNumber());
            EXPECT_EQ(10, slice.getNumber<int>());
            EXPECT_EQ(valueSize, size);
            break;
          }
          case 3: {
            ASSERT_TRUE(slice.isString());
            arangodb::velocypack::ValueLength length = 0;
            auto str = slice.getString(length);
            EXPECT_EQ("abc", std::string(str, length));
            break;
          }
          case 4:
            ASSERT_TRUE(slice.isNumber());
            EXPECT_EQ(10, slice.getNumber<int>());
            break;
          default:
            ASSERT_TRUE(false);
            break;
        }
        ++counter;
      }
      EXPECT_EQ(columnsCount, counter);
    }
  }
}

TEST_P(QueryNoMaterialization, testStoredValuesRecordWithCompression) {
  static std::vector<std::string> const EMPTY;
  auto doc = arangodb::velocypack::Parser::fromJson(
      "{ \"str\": \"abc\", \"value\": 10 }");
  std::string collectionName("testCollection");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\":\"" + collectionName + "\"}");
  auto logicalCollection = vocbase().createCollection(collectionJson->slice());
  ASSERT_TRUE(logicalCollection);
  size_t const columnsCount = 6;  // PK + storedValues
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \
        \"id\": 42, \
        \"name\": \"testView\", \
        \"type\": \"arangosearch\", \
        \"storedValues\": [{\"fields\":[\"str\"], \"compression\":\"none\"}, [\"foo\"],\
        {\"fields\":[\"value\"], \"compression\":\"lz4\"}, [\"_id\"], {\"fields\":[\"str\", \"foo\", \"value\"]}] \
      }");
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase().createView(viewJson->slice(), false));
  ASSERT_TRUE(view);

  auto updateJson =
      VPackParser::fromJson("{\"links\": {\"" + collectionName +
                            "\": {\"includeAllFields\": true} }}");
  EXPECT_TRUE(view->properties(updateJson->slice(), true, true).ok());

  arangodb::velocypack::Builder builder;

  builder.openObject();
  view->properties(builder,
                   arangodb::LogicalDataSource::Serialization::Properties);
  builder.close();

  auto slice = builder.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE(slice.get("type").copyString() ==
              arangodb::iresearch::StaticStrings::ViewArangoSearchType);
  EXPECT_TRUE(slice.get("deleted").isNone());  // no system properties
  auto tmpSlice = slice.get("links");
  EXPECT_TRUE(tmpSlice.isObject() && 1 == tmpSlice.length());

  {
    arangodb::OperationOptions opt;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase(), arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, {logicalCollection->name()}, EMPTY,
        arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    auto const res = trx.insert(logicalCollection->name(), doc->slice(), opt);
    EXPECT_TRUE(res.ok());

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(
                    *logicalCollection, *view)
                    ->commit()
                    .ok());
  }

  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase(), arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    auto link = arangodb::iresearch::IResearchLinkHelper::find(
        *logicalCollection, *view);
    ASSERT_TRUE(link);
    auto const snapshot = link->snapshot();
    auto const& snapshotReader = snapshot.getDirectoryReader();
    std::string const columns[] = {
        arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            std::string("_id"),
        arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            std::string("foo"),
        arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            std::string("foo") +
            arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            "str" +
            arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            "value",
        arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            std::string("str"),
        arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER +
            std::string("value"),
        "@_PK"};
    for (auto const& segment : snapshotReader) {
      auto col = segment.columns();
      auto doc = segment.docs_iterator();
      ASSERT_TRUE(doc);
      ASSERT_TRUE(doc->next());
      size_t counter = 0;
      while (col->next()) {
        auto const& val = col->value();
        ASSERT_TRUE(counter < columnsCount);
        EXPECT_EQ(columns[counter], val.name());
        if (5 == counter) {  // skip PK
          ++counter;
          continue;
        }
        auto columnReader = segment.column(val.id());
        ASSERT_TRUE(columnReader);
        auto valReader = columnReader->iterator(irs::ColumnHint::kNormal);
        ASSERT_TRUE(valReader);
        auto* value = irs::get<irs::payload>(*valReader);
        ASSERT_TRUE(value);
        ASSERT_EQ(doc->value(), valReader->seek(doc->value()));
        if (1 == counter) {  // foo
          EXPECT_TRUE(irs::IsNull(value->value));
          ++counter;
          continue;
        }
        size_t valueSize = value->value.size();
        auto slice = VPackSlice(value->value.data());
        switch (counter) {
          case 0: {
            ASSERT_TRUE(slice.isString());
            arangodb::velocypack::ValueLength length = 0;
            auto str = slice.getString(length);
            std::string strVal(str, length);
            ASSERT_TRUE(length > collectionName.size());
            EXPECT_EQ(collectionName + "/",
                      strVal.substr(0, collectionName.size() + 1));
            break;
          }
          case 2: {
            arangodb::velocypack::ValueLength size = slice.byteSize();
            ASSERT_TRUE(slice.isString());
            arangodb::velocypack::ValueLength length = 0;
            auto str = slice.getString(length);
            EXPECT_EQ("abc", std::string(str, length));
            slice = VPackSlice(slice.start() + slice.byteSize());
            size += slice.byteSize();
            EXPECT_TRUE(slice.isNull());
            slice = VPackSlice(slice.start() + slice.byteSize());
            size += slice.byteSize();
            ASSERT_TRUE(slice.isNumber());
            EXPECT_EQ(10, slice.getNumber<int>());
            EXPECT_EQ(valueSize, size);
            break;
          }
          case 3: {
            ASSERT_TRUE(slice.isString());
            arangodb::velocypack::ValueLength length = 0;
            auto str = slice.getString(length);
            EXPECT_EQ("abc", std::string(str, length));
            break;
          }
          case 4:
            ASSERT_TRUE(slice.isNumber());
            EXPECT_EQ(10, slice.getNumber<int>());
            break;
          default:
            ASSERT_TRUE(false);
            break;
        }
        ++counter;
      }
      EXPECT_EQ(columnsCount, counter);
    }
  }
}

TEST_P(QueryNoMaterialization, matchSortButNotEnoughAttributes) {
  auto const queryString =
      std::string("FOR d IN ") + viewName +
      " SEARCH d.value IN [1, 2, 11, 12] FILTER d.boo == '12312' SORT d.boo "
      "ASC "
      " RETURN DISTINCT  {resource_type: d.foo, version: d.not_in_stored}";

  std::vector<VPackValue> expectedValues{};
  EXPECT_TRUE(arangodb::tests::assertRules(
      vocbase(), queryString,
      {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

  auto query = arangodb::aql::Query::create(
      arangodb::transaction::StandaloneContext::create(
          vocbase(), arangodb::transaction::OperationOriginTestCase{}),
      arangodb::aql::QueryString(queryString), nullptr);
  auto const res = query->explain();  // this should not crash!
  ASSERT_TRUE(res.data);
  auto const explanation = res.data->slice();
  arangodb::velocypack::ArrayIterator nodes(explanation.get("nodes"));
  auto found = false;
  for (auto const node : nodes) {
    if (node.hasKey("type") && node.get("type").isString() &&
        node.get("type").stringView() == "EnumerateViewNode") {
      EXPECT_FALSE(node.hasKey("noMaterialization"));
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

INSTANTIATE_TEST_CASE_P(
    IResearch, QueryNoMaterialization,
    testing::Values(std::tuple{ViewType::kArangoSearch,
                               arangodb::iresearch::LinkVersion::MIN},
                    std::tuple{ViewType::kArangoSearch,
                               arangodb::iresearch::LinkVersion::MAX},
                    std::tuple{ViewType::kSearchAlias,
                               arangodb::iresearch::LinkVersion::MAX}));

}  // namespace
}  // namespace arangodb::tests
