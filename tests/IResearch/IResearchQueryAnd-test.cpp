//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"
#include "common.h"

#include "StorageEngineMock.h"

#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "Transaction/UserTransaction.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/V8Context.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "ApplicationFeatures/JemallocFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "Basics/VelocyPackHelper.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "tests/Basics/icu-helper.h"
#include "3rdParty/iresearch/tests/tests_config.hpp"

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

NS_LOCAL

struct TestTermAttribute: public irs::term_attribute {
 public:
  void value(irs::bytes_ref const& value) {
    value_ = value;
  }
};

class TestDelimAnalyzer: public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();

  static ptr make(irs::string_ref const& args) {
    if (args.null()) throw std::exception();
    if (args.empty()) return nullptr;
    PTR_NAMED(TestDelimAnalyzer, ptr, args);
    return ptr;
  }

  TestDelimAnalyzer(irs::string_ref const& delim)
    : irs::analysis::analyzer(TestDelimAnalyzer::type()),
      _delim(irs::ref_cast<irs::byte_type>(delim)) {
    _attrs.emplace(_term);
  }

  virtual irs::attribute_view const& attributes() const NOEXCEPT override { return _attrs; }

  virtual bool next() override {
    if (_data.empty()) {
      return false;
    }

    size_t i = 0;

    for (size_t count = _data.size(); i < count; ++i) {
      auto data = irs::ref_cast<char>(_data);
      auto delim = irs::ref_cast<char>(_delim);

      if (0 == strncmp(&(data.c_str()[i]), delim.c_str(), delim.size())) {
        _term.value(irs::bytes_ref(_data.c_str(), i));
        _data = irs::bytes_ref(_data.c_str() + i + (std::max)(size_t(1), _delim.size()), _data.size() - i - (std::max)(size_t(1), _delim.size()));
        return true;
      }
    }

    _term.value(_data);
    _data = irs::bytes_ref::nil;
    return true;
  }

  virtual bool reset(irs::string_ref const& data) override {
    _data = irs::ref_cast<irs::byte_type>(data);
    return true;
  }

 private:
  irs::attribute_view _attrs;
  irs::bytes_ref _delim;
  irs::bytes_ref _data;
  TestTermAttribute _term;
};

DEFINE_ANALYZER_TYPE_NAMED(TestDelimAnalyzer, "TestDelimAnalyzer");
REGISTER_ANALYZER(TestDelimAnalyzer);

arangodb::aql::QueryResult executeQuery(
    TRI_vocbase_t& vocbase,
    const std::string& queryString
) {
  std::shared_ptr<arangodb::velocypack::Builder> bindVars;
  auto options = std::make_shared<arangodb::velocypack::Builder>();

  arangodb::aql::Query query(
    false, &vocbase, arangodb::aql::QueryString(queryString),
    bindVars, options,
    arangodb::aql::PART_MAIN
  );

  return query.execute(arangodb::QueryRegistryFeature::QUERY_REGISTRY);
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

extern const char* ARGV0; // defined in main.cpp

struct IResearchQuerySetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQuerySetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();
    IcuInitializer::setup(ARGV0); // initialize ICU, required for Utf8Helper which is using by optimizer

    // setup required application features
    features.emplace_back(new arangodb::ViewTypesFeature(&server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(&server), true); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::DatabasePathFeature(&server), false);
    features.emplace_back(new arangodb::JemallocFeature(&server), false); // required for DatabasePathFeature
    features.emplace_back(new arangodb::DatabaseFeature(&server), false); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::FeatureCacheFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first);
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(&server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(&server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(&server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    auto* analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

    analyzers->emplace("test_analyzer", "TestAnalyzer", "abc"); // cache analyzer
    analyzers->emplace("test_csv_analyzer", "TestDelimAnalyzer", ","); // cache analyzer

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchQuerySetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(&server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }

    arangodb::FeatureCacheFeature::reset();
  }
}; // IResearchQuerySetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchQueryTestAnd", "[iresearch][iresearch-query]") {
  IResearchQuerySetup s;
  UNUSED(s);

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
    auto* collection = vocbase.createCollection(createJson->slice());
    REQUIRE((nullptr != collection));

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs {
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -6, \"value\": null }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -5, \"value\": true }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -4, \"value\": \"abc\" }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -3, \"value\": 3.14 }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
      arangodb::velocypack::Parser::fromJson("{ \"seq\": -1, \"value\": { \"a\": 7, \"b\": \"c\" } }"),
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase),
      collection->cid(),
      arangodb::AccessMode::Type::WRITE
    );
    CHECK((trx.begin().ok()));

    for (auto& entry: docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      CHECK((res.successful()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    CHECK((trx.commit().ok()));
  }

  // create collection1
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
    auto* collection = vocbase.createCollection(createJson->slice());
    REQUIRE((nullptr != collection));

    irs::utf8_path resource;
    resource/=irs::string_ref(IResearch_test_resource_dir);
    resource/=irs::string_ref("simple_sequential.json");

    auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
    auto slice = builder->slice();
    REQUIRE(slice.isArray());

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase),
      collection->cid(),
      arangodb::AccessMode::Type::WRITE
    );
    CHECK((trx.begin().ok()));

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto res = trx.insert(collection->name(), itr.value(), options);
      CHECK((res.successful()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    CHECK((trx.commit().ok()));
  }

  // create view
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"iresearch\" }");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view->getImplementation());
    REQUIRE((false == !impl));

    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
        "\"testCollection0\": { \"includeAllFields\": true, \"trackListPositions\": true, \"tokenizers\": [ \"test_analyzer\", \"identity\" ] },"
        "\"testCollection1\": { \"includeAllFields\": true, \"tokenizers\": [ \"test_analyzer\", \"identity\" ] }"
      "}}"
    );
    CHECK((impl->updateProperties(updateJson->slice(), true, false).ok()));
    CHECK((2 == impl->linkCount()));
    impl->sync();
  }

  // field and missing field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d['same'] == 'xyz' AND d.invalid == 2 SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // two different fields
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(),
      insertedDocs[10].slice(),
      insertedDocs[12].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d['same'] == 'xyz' AND d.value == 100 SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // not field and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(),
      insertedDocs[10].slice(),
      insertedDocs[12].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER NOT (d['same'] == 'abc') AND d.value == 100 SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // field and phrase
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[13].slice(),
      insertedDocs[19].slice(),
      insertedDocs[22].slice(),
      insertedDocs[24].slice(),
      insertedDocs[29].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.same == 'xyz' AND PHRASE(d['duplicated'], 'z', 'test_analyzer') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // not phrase and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(),
      insertedDocs[9].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[23].slice(),
      insertedDocs[25].slice(),
      insertedDocs[26].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[30].slice(),
      insertedDocs[31].slice(),
      insertedDocs[32].slice(),
      insertedDocs[33].slice(),
      insertedDocs[34].slice(),
      insertedDocs[35].slice(),
      insertedDocs[36].slice(),
      insertedDocs[37].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER NOT PHRASE(d['duplicated'], 'z', 'test_analyzer') AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // field and prefix
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[36].slice(),
      insertedDocs[37].slice(),
      insertedDocs[6].slice(),
      insertedDocs[9].slice(),
      insertedDocs[26].slice(),
      insertedDocs[31].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.same == 'xyz' AND STARTS_WITH(d['prefix'], 'abc') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // not prefix and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[14].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[21].slice(),
      insertedDocs[22].slice(),
      insertedDocs[23].slice(),
      insertedDocs[24].slice(),
      insertedDocs[25].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[29].slice(),
      insertedDocs[30].slice(),
      insertedDocs[32].slice(),
      insertedDocs[33].slice(),
      insertedDocs[34].slice(),
      insertedDocs[35].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER NOT STARTS_WITH(d['prefix'], 'abc') AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // field and exists
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[6].slice(),
      insertedDocs[9].slice(),
      insertedDocs[14].slice(),
      insertedDocs[21].slice(),
      insertedDocs[26].slice(),
      insertedDocs[29].slice(),
      insertedDocs[31].slice(),
      insertedDocs[34].slice(),
      insertedDocs[36].slice(),
      insertedDocs[37].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER d.same == 'xyz' AND EXISTS(d['prefix']) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // not exists and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[7].slice(),
      insertedDocs[8].slice(),
      insertedDocs[10].slice(),
      insertedDocs[11].slice(),
      insertedDocs[12].slice(),
      insertedDocs[13].slice(),
      insertedDocs[15].slice(),
      insertedDocs[16].slice(),
      insertedDocs[17].slice(),
      insertedDocs[18].slice(),
      insertedDocs[19].slice(),
      insertedDocs[20].slice(),
      insertedDocs[22].slice(),
      insertedDocs[23].slice(),
      insertedDocs[24].slice(),
      insertedDocs[25].slice(),
      insertedDocs[27].slice(),
      insertedDocs[28].slice(),
      insertedDocs[30].slice(),
      insertedDocs[32].slice(),
      insertedDocs[33].slice(),
      insertedDocs[35].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER NOT EXISTS(d['prefix']) AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // phrase and not field and exists
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[29].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER PHRASE(d['duplicated'], 'z', 'test_analyzer') AND NOT (d.same == 'abc') AND EXISTS(d['prefix']) SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // prefix and not exists and field
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[37].slice(),
      insertedDocs[9].slice(),
      insertedDocs[31].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER STARTS_WITH(d['prefix'], 'abc') AND NOT EXISTS(d.duplicated) AND d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }

  // exists and not prefix and phrase and not field and range
  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[29].slice(),
    };
    auto result = executeQuery(
      vocbase,
      "FOR d IN VIEW testView FILTER EXISTS(d.name) AND NOT STARTS_WITH(d['prefix'], 'abc') AND PHRASE(d['duplicated'], 'z', 'test_analyzer') AND NOT (d.same == 'abc') AND d.seq >= 23 SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      CHECK((i < expected.size()));
      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++], resolved, true)));
    }

    CHECK((i == expected.size()));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
