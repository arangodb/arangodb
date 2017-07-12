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

#include "analysis/token_attributes.hpp"
#include "search/scorers.hpp"
#include "utils/locale_utils.hpp"
#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

#include "ApplicationFeatures/JemallocFeature.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/files.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchView.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "Random/RandomFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "V8Server/V8DealerFeature.h"
#include "Views/ViewIterator.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

NS_LOCAL

struct DocIdScorer: public irs::sort {
  DECLARE_SORT_TYPE() { static irs::sort::type_id type("test_doc_id"); return type; }
  static ptr make(const irs::string_ref&) { PTR_NAMED(DocIdScorer, ptr); return ptr; }
  DocIdScorer(): irs::sort(DocIdScorer::type()) { }
  virtual sort::prepared::ptr prepare() const override { PTR_NAMED(Prepared, ptr); return ptr; }

  struct Prepared: public irs::sort::prepared_base<uint64_t> {
    virtual void add(score_t& dst, const score_t& src) const override { dst = src; }
    virtual irs::flags const& features() const override { return irs::flags::empty_instance(); }
    virtual bool less(const score_t& lhs, const score_t& rhs) const override { return lhs < rhs; }
    virtual irs::sort::collector::ptr prepare_collector() const override { return nullptr; }
    virtual void prepare_score(score_t& score) const override { }
    virtual irs::sort::scorer::ptr prepare_scorer(
      irs::sub_reader const& segment,
      irs::term_reader const& field,
      irs::attribute_store const& query_attrs,
      irs::attribute_store const& doc_attrs
    ) const override {
      return irs::sort::scorer::make<Scorer>(doc_attrs.get<irs::document>());
    }
  };

  struct Scorer: public irs::sort::scorer {
    irs::attribute_store::ref<irs::document> const& _doc;
    Scorer(irs::attribute_store::ref<irs::document> const& doc): _doc(doc) { }
    virtual void score(irs::byte_type* score_buf) override {
      reinterpret_cast<uint64_t&>(*score_buf) = *(_doc.get()->value);
    }
  };
};

REGISTER_SCORER(DocIdScorer);

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchViewSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::string testFilesystemPath;

  IResearchViewSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // setup required application features
    features.push_back(std::make_pair(new arangodb::V8DealerFeature(arangodb::application_features::ApplicationServer::server), false));
    features.push_back(std::make_pair(new arangodb::ViewTypesFeature(arangodb::application_features::ApplicationServer::server), true));
    features.push_back(std::make_pair(new arangodb::QueryRegistryFeature(&server), false));
    features.push_back(std::make_pair(new arangodb::FeatureCacheFeature(arangodb::application_features::ApplicationServer::server), true));
    features.push_back(std::make_pair(new arangodb::RandomFeature(&server), false)); // required by AuthenticationFeature
    features.push_back(std::make_pair(new arangodb::AuthenticationFeature(arangodb::application_features::ApplicationServer::server), true));
    features.push_back(std::make_pair(new arangodb::DatabaseFeature(arangodb::application_features::ApplicationServer::server), false));
    features.push_back(std::make_pair(new arangodb::iresearch::IResearchFeature(&server), true));

    arangodb::ViewTypesFeature::registerViewImplementation(
      arangodb::iresearch::IResearchView::type(),
      arangodb::iresearch::IResearchView::make
    );

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

    PhysicalViewMock::persistPropertiesResult = TRI_ERROR_NO_ERROR;
    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;
    testFilesystemPath = (
      irs::utf8_path()/
      TRI_GetTempPath()/
      (std::string("arangodb_tests.") + std::to_string(TRI_microtime()))
    ).utf8();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchViewSetup() {
    TRI_RemoveDirectory(testFilesystemPath.c_str());
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
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchViewTest", "[iresearch][iresearch-view]") {
  IResearchViewSetup s;
  UNUSED(s);


SECTION("test_defaults") {
  auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\" \
  }");
  arangodb::iresearch::IResearchViewMeta expectedMeta;

  expectedMeta._name = "testView";

  // existing view definition
  {
    auto view = arangodb::iresearch::IResearchView::make(nullptr, json->slice(), false);
    CHECK(false == (!view));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((9U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // existing view definition with LogicalView
  {
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK(false == (!view));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // new view definition
  {
    auto view = arangodb::iresearch::IResearchView::make(nullptr, json->slice(), true);
    CHECK(false == (!view));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((9U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // new view definition with LogicalView
  {
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), true);
    CHECK(false == (!view));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }
}

SECTION("test_drop") {
  std::string dataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("deleteme")).utf8();
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"properties\": { \"name\": \"testView\", \
    \"links\": { \"testCollection\": {} }, \
    \"dataPath\": \"" + arangodb::basics::StringUtils::replace(dataPath, "\\", "/") + "\" \
    } \
  }");

  CHECK((false == TRI_IsDirectory(dataPath.c_str())));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
  CHECK((nullptr != logicalCollection));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  auto logicalView = vocbase.createView(json->slice(), 0);
  REQUIRE((false == !logicalView));
  auto view = logicalView->getImplementation();
  REQUIRE((false == !view));

  CHECK((false == logicalCollection->getIndexes().empty()));
  CHECK((false == !vocbase.lookupView("testView")));
  CHECK((false == TRI_IsDirectory(dataPath.c_str())));
  view->open();
  CHECK((true == TRI_IsDirectory(dataPath.c_str())));
  CHECK((TRI_ERROR_NO_ERROR == vocbase.dropView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((false == TRI_IsDirectory(dataPath.c_str())));
}

SECTION("test_move_datapath") {
  std::string createDataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("deleteme0")).utf8();
  std::string updateDataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("deleteme1")).utf8();
  auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"properties\": { \"name\": \"testView\", \
    \"dataPath\": \"" + arangodb::basics::StringUtils::replace(createDataPath, "\\", "/") + "\" \
    } \
  }");
  auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
    \"dataPath\": \"" + arangodb::basics::StringUtils::replace(updateDataPath, "\\", "/") + "\" \
  }");

  CHECK((false == TRI_IsDirectory(createDataPath.c_str())));
  CHECK((false == TRI_IsDirectory(updateDataPath.c_str())));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalView = vocbase.createView(createJson->slice(), 0);
  REQUIRE((false == !logicalView));
  auto view = logicalView->getImplementation();
  REQUIRE((false == !view));

  CHECK((false == TRI_IsDirectory(createDataPath.c_str())));
  view->open();
  CHECK((true == TRI_IsDirectory(createDataPath.c_str())));
  CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
  CHECK((false == TRI_IsDirectory(createDataPath.c_str())));
  CHECK((true == TRI_IsDirectory(updateDataPath.c_str())));
}

SECTION("test_open") {
  // absolute data path
  {
    std::string dataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("deleteme")).utf8();
    auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"name\": \"testView\", \
      \"dataPath\": \"" + arangodb::basics::StringUtils::replace(dataPath, "\\", "/") + "\" \
    }");

    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    auto view = arangodb::iresearch::IResearchView::make(nullptr, json->slice(), false);

    REQUIRE((false == !view));
    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    view->open();
    CHECK((true == TRI_IsDirectory(dataPath.c_str())));
  }

  auto* dbPathFeature = new arangodb::DatabasePathFeature(&s.server);
  auto* jemallocFeature = new arangodb::JemallocFeature(&s.server); // required for DatabasePathFeature

  arangodb::application_features::ApplicationServer::server->addFeature(dbPathFeature);
  arangodb::application_features::ApplicationServer::server->addFeature(jemallocFeature);

  // relative data path
  {
    arangodb::options::ProgramOptions options("", "", "", nullptr);

    options.addPositional((irs::utf8_path()/s.testFilesystemPath).utf8());
    dbPathFeature->validateOptions(std::shared_ptr<decltype(options)>(&options, [](decltype(options)*){})); // set data directory

    std::string dataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("databases")/std::string("deleteme")).utf8();
    auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"name\": \"testView\", \
      \"dataPath\": \"deleteme\" \
    }");

    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    auto view = arangodb::iresearch::IResearchView::make(nullptr, json->slice(), false);
    CHECK(false == (!view));
    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    view->open();
    CHECK((true == TRI_IsDirectory(dataPath.c_str())));
  }

  // default data path
  {
    arangodb::options::ProgramOptions options("", "", "", nullptr);

    options.addPositional((irs::utf8_path()/s.testFilesystemPath).utf8());
    dbPathFeature->validateOptions(std::shared_ptr<decltype(options)>(&options, [](decltype(options)*){})); // set data directory

    std::string dataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("databases")/std::string("iresearch-0")).utf8();
    auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"name\": \"testView\" \
    }");

    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    auto view = arangodb::iresearch::IResearchView::make(nullptr, json->slice(), false);
    CHECK(false == (!view));
    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    view->open();
    CHECK((true == TRI_IsDirectory(dataPath.c_str())));
  }
}

SECTION("test_query") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"properties\": { \"name\": \"testView\" } \
  }");
  static std::vector<std::string> const EMPTY;
  arangodb::aql::AstNode noop(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
  arangodb::aql::AstNode noopChild(arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_OR);

  noop.addMember(&noopChild);

  // no transaction provided
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    CHECK((nullptr == dynamic_cast<arangodb::iresearch::IResearchView*>(view)->iteratorForCondition(nullptr, &noop, nullptr, nullptr)));
  }

  // no filter provided
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((nullptr == dynamic_cast<arangodb::iresearch::IResearchView*>(view)->iteratorForCondition(&trx, nullptr, nullptr, nullptr)));
  }

  // empty ordered iterator
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto* view = logicalView->getImplementation();
    REQUIRE((false == !view));

    std::vector<std::pair<arangodb::aql::Variable const*, bool>> sorts;
    std::vector<std::vector<arangodb::basics::AttributeName>> constAttributes;
    std::unordered_map<arangodb::aql::VariableId, arangodb::aql::AstNode const*> variableDefinitions;

    irs::string_ref attribute("testAttribute");
    arangodb::aql::AstNode nodeExpression(attribute.c_str(), attribute.size(), arangodb::aql::AstNodeValueType::VALUE_TYPE_STRING);
    arangodb::aql::Variable variable("testVariable", 0);

    variableDefinitions.emplace(variable.id, &nodeExpression); // add node for condition
    sorts.emplace_back(std::make_pair(&variable, true)); // add one condition

    arangodb::aql::SortCondition order(nullptr, sorts, constAttributes, variableDefinitions);
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(dynamic_cast<arangodb::iresearch::IResearchView*>(view)->iteratorForCondition(&trx, &noop, nullptr, &order));

    CHECK((false == !itr));
    CHECK((std::string("iresearch-ordered-iterator") == itr->typeName()));
    CHECK((&trx == itr->transaction()));
    CHECK((view == itr->view()));
    CHECK((false == itr->hasExtra()));
    CHECK_THROWS(itr->nextExtra([](arangodb::DocumentIdentifierToken const&, arangodb::velocypack::Slice)->void{}, 42));
    size_t count = 0;
    CHECK((false == itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 42)));
    CHECK((0 == count));
    uint64_t skipped = 0;
    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((0 == skipped));
    CHECK_NOTHROW((itr->reset()));
  }

  // empty unordered iterator (nullptr sort condition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto* view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(dynamic_cast<arangodb::iresearch::IResearchView*>(view)->iteratorForCondition(&trx, &noop, nullptr, nullptr));

    CHECK((false == !itr));
    CHECK((std::string("iresearch-unordered-iterator") == itr->typeName()));
    CHECK((&trx == itr->transaction()));
    CHECK((view == itr->view()));
    CHECK((false == itr->hasExtra()));
    CHECK_THROWS(itr->nextExtra([](arangodb::DocumentIdentifierToken const&, arangodb::velocypack::Slice)->void{}, 42));
    size_t count = 0;
    CHECK((false == itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 42)));
    CHECK((0 == count));
    uint64_t skipped = 0;
    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((0 == skipped));
    CHECK_NOTHROW((itr->reset()));
  }

  // empty unordered iterator (empty sort condition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto* view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::aql::SortCondition order;
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(dynamic_cast<arangodb::iresearch::IResearchView*>(view)->iteratorForCondition(&trx, &noop, nullptr, &order));

    CHECK((false == !itr));
    CHECK((std::string("iresearch-unordered-iterator") == itr->typeName()));
    CHECK((&trx == itr->transaction()));
    CHECK((view == itr->view()));
    CHECK((false == itr->hasExtra()));
    CHECK_THROWS(itr->nextExtra([](arangodb::DocumentIdentifierToken const&, arangodb::velocypack::Slice)->void{}, 42));
    size_t count = 0;
    CHECK((false == itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 42)));
    CHECK((0 == count));
    uint64_t skipped = 0;
    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((0 == skipped));
    CHECK_NOTHROW((itr->reset()));
  }

  // ordered iterator
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
    CHECK((false == !view));

    // fill with test data
    {
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
      arangodb::iresearch::IResearchLinkMeta meta;
      meta._includeAllFields = true;
      arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
      CHECK((trx.begin().ok()));

      for (size_t i = 0; i < 12; ++i) {
        view->insert(trx, 1, i, doc->slice(), meta);
      }

      CHECK((trx.commit().ok()));
      view->sync();
    }

    arangodb::aql::AstNode filter(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
    arangodb::aql::AstNode filterGe(arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_GE);
    arangodb::aql::AstNode filterAttr(arangodb::aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS);
    arangodb::aql::AstNode filterValue(int64_t(1), arangodb::aql::AstNodeValueType::VALUE_TYPE_INT);
    irs::string_ref attr("key");

    filterAttr.setStringValue(attr.c_str(), attr.size());
    filterGe.addMember(&filterAttr);
    filterGe.addMember(&filterValue);
    filter.addMember(&filterGe);

    std::vector<std::pair<arangodb::aql::Variable const*, bool>> sorts;
    std::vector<std::vector<arangodb::basics::AttributeName>> constAttributes;
    std::unordered_map<arangodb::aql::VariableId, arangodb::aql::AstNode const*> variableDefinitions;

    irs::string_ref attribute("testAttribute");
    arangodb::aql::AstNode nodeArgs(arangodb::aql::AstNodeType::NODE_TYPE_ARRAY);
    arangodb::aql::AstNode nodeExpression(arangodb::aql::AstNodeType::NODE_TYPE_FCALL);
    arangodb::aql::Function nodeFunction("test_doc_id", "internalName", "", false, false, true, true, false);
    arangodb::aql::Variable variable("testVariable", 0);

    nodeExpression.addMember(&nodeArgs);
    nodeExpression.setData(&nodeFunction);
    variableDefinitions.emplace(variable.id, &nodeExpression); // add node for condition
    sorts.emplace_back(std::make_pair(&variable, true)); // add one condition

    arangodb::aql::SortCondition order(nullptr, sorts, constAttributes, variableDefinitions);
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(dynamic_cast<arangodb::iresearch::IResearchView*>(view)->iteratorForCondition(&trx, &filter, nullptr, &order));

    CHECK((false == !itr));
    CHECK((std::string("iresearch-ordered-iterator") == itr->typeName()));
    CHECK((&trx == itr->transaction()));
    CHECK((view == itr->view()));
    CHECK((false == itr->hasExtra()));
    CHECK_THROWS(itr->nextExtra([](arangodb::DocumentIdentifierToken const&, arangodb::velocypack::Slice)->void{}, 42));

    {
      std::vector<size_t> const expected = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
      size_t next = 0;
      CHECK((true == itr->next([&expected, &next](arangodb::DocumentIdentifierToken const& token)->void{ CHECK((token._data == expected[next++]));}, 10)));
      CHECK((expected.size() == next));
    }

    CHECK_NOTHROW((itr->reset()));
    uint64_t skipped = 0;
    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((5 == skipped));

    {
      std::vector<size_t> const expected = { 6, 7, 8, 9, 10, 11, 12 };
      size_t next = 0;
      CHECK((false == itr->next([&expected, &next](arangodb::DocumentIdentifierToken const& token)->void{ CHECK((token._data == expected[next++])); std::cerr << ", " << token._data;  }, 10)));
      CHECK((expected.size() == next));
    }

    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((0 == skipped));

    CHECK_NOTHROW((itr->reset()));
  }

  // unordered iterator (nullptr sort condition)
  {
    // FIXME TODO implement
  }

  // unordered iterator (empty sort condition)
  {
    // FIXME TODO implement
  }

  // FIXME TODO implement
}

SECTION("test_register_link") {
  // ApplicationServer in IN_WAIT (known link)
  {
    // FIXME TODO implement
  }

  // ApplicationServer in IN_WAIT (new link)
  {
    // FIXME TODO implement
  }

  // ApplicationServer not in IN_WAIT (known link)
  {
    // FIXME TODO implement
  }

  // ApplicationServer not in IN_WAIT (new link)
  {
    // FIXME TODO implement
  }
}

SECTION("test_update_overwrite") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"properties\": { \"name\": \"testView\" } \
  }");

  // modify meta params
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    // initial update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"locale\": \"en\", \
        \"name\": \"<invalid and ignored>\", \
        \"threadsMaxIdle\": 10, \
        \"threadsMaxTotal\": 20 \
      }");

      expectedMeta._name = "testView";
      expectedMeta._locale = irs::locale_utils::locale("en", true);
      expectedMeta._threadsMaxIdle = 10;
      expectedMeta._threadsMaxTotal = 20;
      CHECK((view->updateProperties(updateJson->slice(), false, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((10U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // subsequent update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"locale\": \"ru\", \
        \"name\": \"<invalid and ignored>\" \
      }");

      expectedMeta._name = "testView";
      expectedMeta._locale = irs::locale_utils::locale("ru", true);
      CHECK((view->updateProperties(updateJson->slice(), false, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((10U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }
  }
}

SECTION("test_update_partial") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"properties\": { \"name\": \"testView\" } \
  }");

  // modify meta params
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"locale\": \"en\", \
      \"name\": \"<invalid and ignored>\", \
      \"threadsMaxIdle\": 10, \
      \"threadsMaxTotal\": 20 \
    }");

    expectedMeta._name = "testView";
    expectedMeta._locale = irs::locale_utils::locale("en", true);
    expectedMeta._threadsMaxIdle = 10;
    expectedMeta._threadsMaxTotal = 20;
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // test rollback on meta modification failure
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    std::string dataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("deleteme")).utf8();
    auto res = TRI_CreateDatafile(dataPath, 1); // create a file where the data path directory should be
    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson(std::string() + "{ \
      \"dataPath\": \"" + arangodb::basics::StringUtils::replace(dataPath, "\\", "/") + "\", \
      \"locale\": \"en\", \
      \"threadsMaxIdle\": 10, \
      \"threadsMaxTotal\": 20 \
    }");

    expectedMeta._name = "testView";
    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // test rollback on persist failure
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"locale\": \"en\", \
      \"threadsMaxIdle\": 10, \
      \"threadsMaxTotal\": 20 \
    }");

    expectedMeta._name = "testView";
    PhysicalViewMock::persistPropertiesResult = TRI_ERROR_INTERNAL; // test fail
    CHECK((TRI_ERROR_INTERNAL == view->updateProperties(updateJson->slice(), true, false).errorNumber()));
    PhysicalViewMock::persistPropertiesResult = TRI_ERROR_NO_ERROR; // revert to valid

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // add a new link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": {} \
      }}");

    expectedMeta._collections.insert(logicalCollection->cid());
    expectedMeta._name = "testView";
    expectedLinkMeta["testCollection"]; // use defaults
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      auto key = itr.key();
      auto value = itr.value();
      CHECK((true == key.isString()));

      auto expectedItr = expectedLinkMeta.find(key.copyString());
      CHECK((
        true == value.isObject()
        && expectedItr != expectedLinkMeta.end()
        && linkMeta.init(value, error)
        && expectedItr->second == linkMeta
      ));
      expectedLinkMeta.erase(expectedItr);
    }

    CHECK((true == expectedLinkMeta.empty()));
  }

  // add new link to non-existant collection
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": {} \
      }}");

    expectedMeta._name = "testView";
    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // remove link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._collections.insert(logicalCollection->cid());
    expectedMeta._name = "testView";

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"links\": { \
          \"testCollection\": {} \
      }}");

      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((10U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
    }

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"links\": { \
          \"testCollection\": null \
      }}");

      expectedMeta._collections.clear();
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((10U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }
  }

  // remove link from non-existant collection
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": null \
      }}");

    expectedMeta._name = "testView";
    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // remove non-existant link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": null \
    }}");
    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._name = "testView";

    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
