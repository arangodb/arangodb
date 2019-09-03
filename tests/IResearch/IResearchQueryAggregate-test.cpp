////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "common.h"
#include "gtest/gtest.h"

#include "../Mocks/StorageEngineMock.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "3rdParty/iresearch/tests/tests_config.hpp"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchView.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

extern const char* ARGV0;  // defined in main.cpp

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryAggregateTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;

  IResearchQueryAggregateTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR);  // suppress WARNING DefaultCustomTypeHandler called
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    server.addFeature<arangodb::FlushFeature>(
        std::make_unique<arangodb::FlushFeature>(server));
    features.emplace_back(server.getFeature<arangodb::FlushFeature>(), false);

    server.addFeature<arangodb::ViewTypesFeature>(
        std::make_unique<arangodb::ViewTypesFeature>(server));
    features.emplace_back(server.getFeature<arangodb::ViewTypesFeature>(), true);

    server.addFeature<arangodb::AuthenticationFeature>(
        std::make_unique<arangodb::AuthenticationFeature>(server));
    features.emplace_back(server.getFeature<arangodb::AuthenticationFeature>(), true);

    server.addFeature<arangodb::DatabasePathFeature>(
        std::make_unique<arangodb::DatabasePathFeature>(server));
    features.emplace_back(server.getFeature<arangodb::DatabasePathFeature>(), false);

    server.addFeature<arangodb::DatabaseFeature>(
        std::make_unique<arangodb::DatabaseFeature>(server));
    features.emplace_back(server.getFeature<arangodb::DatabaseFeature>(), false);

    server.addFeature<arangodb::ShardingFeature>(
        std::make_unique<arangodb::ShardingFeature>(server));
    features.emplace_back(server.getFeature<arangodb::ShardingFeature>(), false);

    server.addFeature<arangodb::QueryRegistryFeature>(
        std::make_unique<arangodb::QueryRegistryFeature>(server));
    features.emplace_back(server.getFeature<arangodb::QueryRegistryFeature>(), false);  // must be first

    system = irs::memory::make_unique<TRI_vocbase_t>(server, TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                                     0, TRI_VOC_SYSTEM_DATABASE);
    server.addFeature<arangodb::SystemDatabaseFeature>(
        std::make_unique<arangodb::SystemDatabaseFeature>(server, system.get()));
    features.emplace_back(server.getFeature<arangodb::SystemDatabaseFeature>(),
                          false);  // required for IResearchAnalyzerFeature

    server.addFeature<arangodb::TraverserEngineRegistryFeature>(
        std::make_unique<arangodb::TraverserEngineRegistryFeature>(server));
    features.emplace_back(server.getFeature<arangodb::TraverserEngineRegistryFeature>(),
                          false);  // must be before AqlFeature

    server.addFeature<arangodb::AqlFeature>(std::make_unique<arangodb::AqlFeature>(server));
    features.emplace_back(server.getFeature<arangodb::AqlFeature>(), true);

    server.addFeature<arangodb::aql::OptimizerRulesFeature>(
        std::make_unique<arangodb::aql::OptimizerRulesFeature>(server));
    features.emplace_back(server.getFeature<arangodb::aql::OptimizerRulesFeature>(), true);

    server.addFeature<arangodb::aql::AqlFunctionFeature>(
        std::make_unique<arangodb::aql::AqlFunctionFeature>(server));
    features.emplace_back(server.getFeature<arangodb::aql::AqlFunctionFeature>(),
                          true);  // required for IResearchAnalyzerFeature

    server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(
        std::make_unique<arangodb::iresearch::IResearchAnalyzerFeature>(server));
    features.emplace_back(server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>(),
                          true);

    server.addFeature<arangodb::iresearch::IResearchFeature>(
        std::make_unique<arangodb::iresearch::IResearchFeature>(server));
    features.emplace_back(server.getFeature<arangodb::iresearch::IResearchFeature>(), true);

#if USE_ENTERPRISE
    server.addFeature<arangodb::LdapFeature>(std::make_unique<arangodb::LdapFeature>(server));
    features.emplace_back(server.getFeature<arangodb::LdapFeature>(), false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    for (auto& f : features) {
      f.first.prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first.start();
      }
    }

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  }

  ~IResearchQueryAggregateTest() {
    system.reset();  // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(server).stop();  // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first.stop();
      }
    }

    for (auto& f : features) {
      f.first.unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
  }
};  // IResearchQueryTest

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_F(IResearchQueryAggregateTest, test) {
  TRI_vocbase_t vocbase(server, TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != collection));

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -6, \"value\": null }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -5, \"value\": true }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -4, \"value\": \"abc\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -3, \"value\": 3.14 }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -1, \"value\": { \"a\": 7, \"b\": \"c\" } }"),
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE((trx.begin().ok()));

    for (auto& entry : docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      EXPECT_TRUE((res.ok()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE((trx.commit().ok()));
  }

  // create collection1
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != collection));

    irs::utf8_path resource;
    resource /= irs::string_ref(arangodb::tests::testResourceDir);
    resource /= irs::string_ref("simple_sequential.json");

    auto builder =
        arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
    auto slice = builder.slice();
    ASSERT_TRUE(slice.isArray());

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE((trx.begin().ok()));

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto res = trx.insert(collection->name(), itr.value(), options);
      EXPECT_TRUE((res.ok()));
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE((trx.commit().ok()));
  }

  // create view
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_TRUE((false == !impl));

    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"includeAllFields\": true, "
        "\"trackListPositions\": true },"
        "\"testCollection1\": { \"includeAllFields\": true }"
        "}}");
    EXPECT_TRUE((impl->properties(updateJson->slice(), true).ok()));
    std::set<TRI_voc_cid_t> cids;
    impl->visitCollections([&cids](TRI_voc_cid_t cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_TRUE((2 == cids.size()));
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // test grouping with counting
  {
    std::map<double_t, size_t> expected{
        {100., 5}, {12., 2}, {95., 1},   {90.564, 1}, {1., 1},
        {0., 1},   {50., 1}, {-32.5, 1}, {3.14, 1}  // null
    };

    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value <= 100 COLLECT value = d.value WITH "
        "COUNT INTO size RETURN { 'value' : value, 'names' : size }");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator itr(slice);
    ASSERT_TRUE(expected.size() == itr.size());

    for (; itr.valid(); itr.next()) {
      auto const value = itr.value();
      auto const key = value.get("value").getNumber<double_t>();

      auto expectedValue = expected.find(key);
      ASSERT_TRUE(expectedValue != expected.end());
      ASSERT_TRUE(expectedValue->second == value.get("names").getNumber<size_t>());
      ASSERT_TRUE(1 == expected.erase(key));
    }
    ASSERT_TRUE(expected.empty());
  }

  // test grouping
  {
    std::map<double_t, std::set<std::string>> expected{
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

    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value <= 100 COLLECT value = d.value INTO "
        "name = d.name RETURN { 'value' : value, 'names' : name }");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator itr(slice);
    ASSERT_TRUE(expected.size() == itr.size());

    for (; itr.valid(); itr.next()) {
      auto const value = itr.value();
      auto const key = value.get("value").getNumber<double_t>();

      auto expectedValue = expected.find(key);
      ASSERT_TRUE(expectedValue != expected.end());

      arangodb::velocypack::ArrayIterator name(value.get("names"));
      auto& expectedNames = expectedValue->second;

      if (expectedNames.empty()) {
        // array must contain singe 'null' value
        ASSERT_TRUE(1 == name.size());
        ASSERT_TRUE(name.valid());
        ASSERT_TRUE(name.value().isNull());
        name.next();
        ASSERT_TRUE(!name.valid());
      } else {
        ASSERT_TRUE(expectedNames.size() == name.size());

        for (; name.valid(); name.next()) {
          auto const actualName = arangodb::iresearch::getStringRef(name.value());
          auto expectedName = expectedNames.find(actualName);
          ASSERT_TRUE(expectedName != expectedNames.end());
          expectedNames.erase(expectedName);
        }
      }

      ASSERT_TRUE(expectedNames.empty());
      ASSERT_TRUE(1 == expected.erase(key));
    }
    ASSERT_TRUE(expected.empty());
  }

  // test aggregation
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.seq < 7 COLLECT AGGREGATE sumSeq = "
        "SUM(d.seq) RETURN sumSeq");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator itr(slice);
    ASSERT_TRUE(itr.valid());
    EXPECT_TRUE(0 == itr.value().getNumber<size_t>());
    itr.next();
    EXPECT_TRUE(!itr.valid());
  }

  // test aggregation without filter condition
  {
    auto result =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView COLLECT AGGREGATE "
                                      "sumSeq = SUM(d.seq) RETURN sumSeq");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator itr(slice);
    ASSERT_TRUE(itr.valid());
    EXPECT_TRUE(475 == itr.value().getNumber<size_t>());
    itr.next();
    EXPECT_TRUE(!itr.valid());
  }

  // total number of documents in a view
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView COLLECT WITH COUNT INTO count RETURN count");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator itr(slice);
    ASSERT_TRUE(itr.valid());
    EXPECT_TRUE(38 == itr.value().getNumber<size_t>());
    itr.next();
    EXPECT_TRUE(!itr.valid());
  }
}
