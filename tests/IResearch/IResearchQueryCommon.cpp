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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchQueryCommon.h"

#include "Aql/AqlFunctionFeature.h"
#include "Basics/DownCast.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchView.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

IResearchQueryTest::IResearchQueryTest() : server{false} {
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
      VPackParser::fromJson("{ \"locale\": \"en.UTF-8\", \"stopwords\": [ ] }")
          ->slice(),
      arangodb::transaction::OperationOriginTestCase{},
      arangodb::iresearch::Features{
          arangodb::iresearch::FieldFeatures::NORM,
          irs::IndexFeatures::FREQ |
              irs::IndexFeatures::POS});  // cache analyzer
  EXPECT_TRUE(res.ok());

  auto sysVocbase = server.getFeature<arangodb::SystemDatabaseFeature>().use();
  arangodb::methods::Collections::createSystem(
      *sysVocbase, options, arangodb::tests::AnalyzerCollectionName, false,
      unused);
  unused = nullptr;

  res = analyzers.emplace(result, "_system::test_analyzer", "TestAnalyzer",
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

namespace arangodb::tests {

void QueryTest::createCollections() {
  // testCollection0
  {
    auto createJson = VPackParser::fromJson(R"({ "name": "testCollection0" })");
    auto collection = _vocbase.createCollection(createJson->slice());
    ASSERT_TRUE(collection);

    std::vector<std::shared_ptr<VPackBuilder>> docs{
        VPackParser::fromJson(R"({ "seq": -6, "value": null })"),
        VPackParser::fromJson(R"({ "seq": -5, "value": true })"),
        VPackParser::fromJson(R"({ "seq": -4, "value": "abc" })"),
        VPackParser::fromJson(R"({ "seq": -3, "value": 3.14 })"),
        VPackParser::fromJson(R"({ "seq": -2, "value": [ 1, "abc" ] })"),
        VPackParser::fromJson(
            R"({ "seq": -1, "value": { "a": 7, "b": "c" } })"),
    };

    OperationOptions options;
    options.returnNew = true;
    SingleCollectionTransaction trx{
        transaction::StandaloneContext::create(
            _vocbase, arangodb::transaction::OperationOriginTestCase{}),
        *collection, AccessMode::Type::WRITE};
    {
      auto r = trx.begin();
      EXPECT_TRUE(r.ok()) << r.errorMessage();
    }
    for (auto& entry : docs) {
      auto r = trx.insert(collection->name(), entry->slice(), options);
      EXPECT_TRUE(r.ok()) << r.errorMessage();
      _insertedDocs.emplace_back(r.slice().get("new"));
    }
    {
      auto r = trx.commit();
      EXPECT_TRUE(r.ok()) << r.errorMessage();
    }
  }
  // testCollection1
  {
    auto createJson = VPackParser::fromJson(R"({ "name": "testCollection1" })");
    auto collection = _vocbase.createCollection(createJson->slice());
    ASSERT_TRUE(collection);

    std::filesystem::path resource;
    resource /= std::string_view{testResourceDir};
    resource /= std::string_view{"simple_sequential.json"};
    auto builder =
        basics::VelocyPackHelper::velocyPackFromFile(resource.string());
    auto slice = builder.slice();
    ASSERT_TRUE(slice.isArray()) << slice.toString();

    OperationOptions options;
    options.returnNew = true;
    SingleCollectionTransaction trx{
        transaction::StandaloneContext::create(
            _vocbase, arangodb::transaction::OperationOriginTestCase{}),
        *collection, AccessMode::Type::WRITE};
    {
      auto r = trx.begin();
      EXPECT_TRUE(r.ok()) << r.errorMessage();
    }
    for (velocypack::ArrayIterator it{slice}; it.valid(); ++it) {
      auto r = trx.insert(collection->name(), it.value(), options);
      EXPECT_TRUE(r.ok()) << r.errorMessage();
      _insertedDocs.emplace_back(r.slice().get("new"));
    }
    {
      auto r = trx.commit();
      EXPECT_TRUE(r.ok()) << r.errorMessage();
    }
  }
}

void QueryTest::checkView(LogicalView const& view, size_t expected,
                          std::string_view viewName) {
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
  EXPECT_EQ(expected, count);
  EXPECT_EQ(count, cids.size());
  auto r = executeQuery(
      _vocbase, absl::Substitute("FOR d IN $0 SEARCH 1 == 1"
                                 " OPTIONS { waitForSync: true } RETURN d",
                                 viewName));
  EXPECT_TRUE(r.result.ok()) << r.result.errorMessage();
}

void QueryTest::createView(std::string_view definition1,
                           std::string_view definition2) {
  auto createJson = VPackParser::fromJson(
      R"({ "name": "testView", "type": "arangosearch" })");
  auto logicalView = _vocbase.createView(createJson->slice(), false);
  ASSERT_FALSE(!logicalView);
  auto& implView = basics::downCast<iresearch::IResearchView>(*logicalView);
  auto updateJson = VPackParser::fromJson(absl::Substitute(
      R"({ "links": {
          "testCollection0": {
            "version": $0, $1
            "includeAllFields": true },
          "testCollection1": {
            "version": $0, $2
            "includeAllFields": true } } })",
      version(), definition1, definition2));
  auto r = implView.properties(updateJson->slice(), true, true);
  EXPECT_TRUE(r.ok()) << r.errorMessage();
  checkView(implView);
}

void QueryTest::createIndexes(std::string_view definition1,
                              std::string_view definition2) {
  // testIndex0
  {
    bool created = false;
    // TODO kSearch remove fields, also see SEARCH-334
    auto createJson = VPackParser::fromJson(absl::Substitute(
        R"({ "name": "testIndex0", "type": "inverted",
               "version": $0, $1
               "includeAllFields": true })",
        version(), definition1));
    auto collection = _vocbase.lookupCollection("testCollection0");
    ASSERT_TRUE(collection);
    collection->createIndex(createJson->slice(), created).waitAndGet();
    ASSERT_TRUE(created);
  }
  // testIndex1
  {
    bool created = false;
    // TODO kSearch remove fields, also see SEARCH-334
    auto createJson = VPackParser::fromJson(absl::Substitute(
        R"({ "name": "testIndex1", "type": "inverted",
               "version": $0, $1
               "includeAllFields": true })",
        version(), definition2));
    auto collection = _vocbase.lookupCollection("testCollection1");
    ASSERT_TRUE(collection);
    collection->createIndex(createJson->slice(), created).waitAndGet();
    EXPECT_TRUE(created);
  }
}

void QueryTest::createSearch() {
  auto createJson = VPackParser::fromJson(
      R"({ "name": "testView", "type": "search-alias" })");
  auto logicalView = _vocbase.createView(createJson->slice(), false);
  ASSERT_TRUE(logicalView);
  auto& implView = basics::downCast<iresearch::Search>(*logicalView);
  auto updateJson = VPackParser::fromJson(R"({ "indexes": [
      { "collection": "testCollection0", "index": "testIndex0" },
      { "collection": "testCollection1", "index": "testIndex1" } ] })");
  auto r = implView.properties(updateJson->slice(), true, true);
  EXPECT_TRUE(r.ok()) << r.errorMessage();
  checkView(implView);
}

bool QueryTest::runQuery(std::string_view query) {
  return runQuery(query, _insertedDocs.begin(), _insertedDocs.size());
}

bool QueryTest::runQuery(std::string_view query,
                         std::span<VPackSlice const> expected) {
  return runQuery(query, expected.begin(), expected.size());
}

bool QueryTest::runQuery(std::string_view query, VPackValue v) {
  VPackBuilder builder;
  builder.add(v);
  return runQuery(query, &builder, 1);
}

bool QueryTest::checkSlices(VPackSlice actual, VPackSlice expected) {
  int const r = basics::VelocyPackHelper::compare(actual, expected, true);
  EXPECT_EQ(r, 0) << "actual:\n"
                  << actual.toString() << "\nexpected:\n"
                  << expected.toString();
  return r == 0;
}

}  // namespace arangodb::tests
