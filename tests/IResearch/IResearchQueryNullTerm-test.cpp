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

class IResearchQueryNullTermTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;

  IResearchQueryNullTermTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AQL.name(), arangodb::LogLevel::ERR);  // suppress WARNING {aql} Suboptimal AqlItemMatrix index lookup:
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

  ~IResearchQueryNullTermTest() {
    system.reset();  // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(server).stop();  // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AQL.name(), arangodb::LogLevel::DEFAULT);
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
};  // IResearchQuerySetup

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryNullTermTest, test) {
  TRI_vocbase_t vocbase(server, TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  arangodb::LogicalView* view{};
  std::vector<arangodb::velocypack::Builder> insertedDocs;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_TRUE((nullptr != collection));

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        arangodb::velocypack::Parser::fromJson("{ \"seq\": -7 }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -6, \"value\": null}"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -5, \"value\": null}"),
        arangodb::velocypack::Parser::fromJson("{ \"seq\": -4 }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -3, \"value\": null}"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -2, \"value\": null}"),
        arangodb::velocypack::Parser::fromJson("{ \"seq\": -1 }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": 0, \"value\": null }"),
        arangodb::velocypack::Parser::fromJson("{ \"seq\": 1 }")};

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

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": 2, \"value\": null}"),
        arangodb::velocypack::Parser::fromJson("{ \"seq\": 3 }"),
        arangodb::velocypack::Parser::fromJson("{ \"seq\": 4 }"),
        arangodb::velocypack::Parser::fromJson("{ \"seq\": 5 }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": 6, \"value\": null}"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": 7, \"value\": null}"),
        arangodb::velocypack::Parser::fromJson("{ \"seq\": 8 }")};

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

  // create view
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));

    view = logicalView.get();
    ASSERT_TRUE(nullptr != view);
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

  // -----------------------------------------------------------------------------
  // --SECTION-- ==
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value == 'null' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value == 0 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.value == null, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");

      if (valueSlice.isNone() || !valueSlice.isNull()) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value == null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // d.value == null, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");

      if (valueSlice.isNone() || !valueSlice.isNull()) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value == null SORT BM25(d), TFIDF(d), "
        "d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION-- !=
  // -----------------------------------------------------------------------------

  // invalid type
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const keySlice = docSlice.get("seq");
      auto const fieldSlice = docSlice.get("value");

      if (!fieldSlice.isNone() && "null" == arangodb::iresearch::getStringRef(fieldSlice)) {
        continue;
      }

      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value != 'null' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // invalid type
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const keySlice = docSlice.get("seq");
      auto const fieldSlice = docSlice.get("value");

      if (!fieldSlice.isNone() &&
          (fieldSlice.isNumber() && 0. == fieldSlice.getNumber<double>())) {
        continue;
      }

      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value != 0 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // d.value != null, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");

      if (!valueSlice.isNone() && valueSlice.isNull()) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value != null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // d.value != null, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");

      if (!valueSlice.isNone() && valueSlice.isNull()) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value != null SORT BM25(d), TFIDF(d), "
        "d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION-- <
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value < 'null' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value < false RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value < 0 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.value < null
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value < null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // -----------------------------------------------------------------------------
  // --SECTION-- <=
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value <= 'null' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value <= false RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value <= 0 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.value <= null, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || !valueSlice.isNull()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value <= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // d.value <= null, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || !valueSlice.isNull()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value <= null SORT BM25(d), TFIDF(d), "
        "d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION-- >
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value > 'null' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value > false RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value > 0 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.value > null
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value > null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // -----------------------------------------------------------------------------
  // --SECTION-- >=
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value >= 'null' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value >= 0 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value >= false RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.value >= null, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || !valueSlice.isNull()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value >= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // d.value >= null, BM25(), TFIDF(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || !valueSlice.isNull()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value >= null SORT BM25(d), TFIDF(d), "
        "d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                       Range(>, <)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH d.value > "
                                      "'null' and d.value < null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value > 0 and d.value < null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value > false and d.value < null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // empty range
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value > null and d.value < null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                      Range(>=, <)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH d.value >= "
                                      "'null' and d.value < null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value >= 0 and d.value < null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH d.value >= "
                                      "false and d.value < null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // empty range
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value >= null and d.value < null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                      Range(>, <=)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH d.value > "
                                      "'null' and d.value <= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value > 0 and d.value <= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH d.value > "
                                      "false and d.value <= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // empty range
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value > null and d.value <= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range(>=, <=)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH d.value >= "
                                      "'null' and d.value <= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value >= 0 and d.value <= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH d.value >= "
                                      "false and d.value <= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.value >= null and d.value <= null, unordered
  {
    std::map<size_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || !valueSlice.isNull()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH d.value >= "
                                      "null and d.value <= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("seq");
      auto const key = keySlice.getNumber<ptrdiff_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // d.value >= null and d.value <= null, BM25(), TFIDF(), d.seq DESC
  // (will be converted to d.value >= 0 AND d.value <= 0)
  {
    std::map<ptrdiff_t, arangodb::velocypack::Slice> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice = doc.slice().resolveExternals();
      auto const valueSlice = docSlice.get("value");
      if (valueSlice.isNone() || !valueSlice.isNull()) {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), docSlice);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value >= null and d.value <= null SORT "
        "BM25(d), TFIDF(d), d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      ++expectedDoc;
    }
    EXPECT_TRUE(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range(>=, <=)
  // -----------------------------------------------------------------------------

  // d.value >= null and d.value <= null, unordered
  // (will be converted to d.value >= 0 AND d.value <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.value IN null..null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());
    EXPECT_TRUE(!resultIt.valid());
  }

  // d.seq >= nullptr AND d.seq <= nullptr, unordered
  // (will be converted to d.seq >= 0 AND d.seq <= 0)
  {
    std::unordered_map<size_t, arangodb::velocypack::Slice> expectedDocs{
        {0, insertedDocs[7].slice()},  // seq == 0
    };

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.seq IN null..null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_TRUE(expectedDocs.size() == resultIt.size());
    EXPECT_TRUE(resultIt.valid());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto key = resolved.get("seq");
      auto expectedDoc = expectedDocs.find(key.getNumber<size_t>());
      EXPECT_TRUE(expectedDoc != expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(expectedDoc->second,
                                                                   resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // d.value >= null and d.value <= null, BM25(), TFIDF(), d.seq DESC
  // (will be converted to d.value >= 0 AND d.value <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value IN null..null SORT BM25(d), "
        "TFIDF(d), d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());
    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_TRUE(0 == resultIt.size());
    EXPECT_TRUE(!resultIt.valid());
  }
}
