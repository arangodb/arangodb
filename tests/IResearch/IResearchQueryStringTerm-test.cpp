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

#include "catch.hpp"
#include "common.h"

#include "StorageEngineMock.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Aql/Ast.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Transaction/Methods.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Basics/VelocyPackHelper.h"
#include "3rdParty/iresearch/tests/tests_config.hpp"

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

extern const char* ARGV0; // defined in main.cpp

NS_LOCAL

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchQueryStringTermSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQueryStringTermSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::aql::AqlFunctionFeature* functions = nullptr;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR); // suppress WARNING DefaultCustomTypeHandler called
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true);
    features.emplace_back(functions = new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true);

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

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

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
      "_NONDETERM_",
      ".",
      arangodb::aql::Function::makeFlags(
        // fake non-deterministic
        arangodb::aql::Function::Flags::CanRunOnDBServer
      ),
      [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*, arangodb::aql::VPackFunctionParameters const& params) {
        TRI_ASSERT(!params.empty());
        return params[0];
    }});

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
      "_FORWARD_",
      ".",
      arangodb::aql::Function::makeFlags(
        // fake deterministic
        arangodb::aql::Function::Flags::Deterministic,
        arangodb::aql::Function::Flags::Cacheable,
        arangodb::aql::Function::Flags::CanRunOnDBServer
      ),
      [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*, arangodb::aql::VPackFunctionParameters const& params) {
        TRI_ASSERT(!params.empty());
        return params[0];
    }});

    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();

    analyzers->emplace("test_analyzer", "TestAnalyzer", "abc"); // cache analyzer
    analyzers->emplace("test_csv_analyzer", "TestDelimAnalyzer", ","); // cache analyzer

    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
  }

  ~IResearchQueryStringTermSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
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

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
  }
}; // IResearchQuerySetup

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchQueryTestStringTerm", "[iresearch][iresearch-query]") {
  IResearchQueryStringTermSetup s;
  UNUSED(s);

  // ArangoDB specific string comparer
  struct StringComparer {
    bool operator()(irs::string_ref const& lhs, irs::string_ref const& rhs) const {
      return arangodb::basics::VelocyPackHelper::compareStringValues(
        lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size(), true
      ) < 0;
    }
  }; // StringComparer

  // ==, !=, <, <=, >, >=, range
  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;

  // add collection_1
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection1));
  }

  // add collection_2
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_2\" }");
    logicalCollection2 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection2));
  }

  // add view
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(createJson->slice())
  );
  REQUIRE((false == !view));

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\" : {"
        "\"collection_1\" : { \"includeAllFields\" : true },"
        "\"collection_2\" : { \"includeAllFields\" : true }"
      "}}"
    );
    CHECK((view->properties(updateJson->slice(), true).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
  }

  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  // populate view with the data
  {
    arangodb::OperationOptions opt;
    TRI_voc_tick_t tick;

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));

    // insert into collections
    {
      irs::utf8_path resource;
      resource/=irs::string_ref(IResearch_test_resource_dir);
      resource/=irs::string_ref("simple_sequential.json");

      auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      REQUIRE(root.isArray());

      size_t i = 0;

      std::shared_ptr<arangodb::LogicalCollection> collections[] {
        logicalCollection1, logicalCollection2
      };

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocs.emplace_back();
        auto const res = collections[i % 2]->insert(&trx, doc, insertedDocs.back(), opt, tick, false);
        CHECK(res.ok());
        ++i;
      }
    }

    CHECK((trx.commit().ok()));
    CHECK(view->commit().ok());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                ==
  // -----------------------------------------------------------------------------

  // missing term
  {
    std::string const query = "FOR d IN testView SEARCH d.name == 'invalid_value' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    std::string const query = "FOR d IN testView SEARCH d.name == 0 RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    std::string const query = "FOR d IN testView SEARCH d.name == null RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    std::string const query = "FOR d IN testView SEARCH d.name == false RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name == true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name == @name RETURN d",
      arangodb::velocypack::Parser::fromJson("{ \"name\" : true }")
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.name == 'A', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name == 'A' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.same == 'same', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.same == 'xyz' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.same == 'same', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.same == CONCAT('xy', @param) RETURN d",
      arangodb::velocypack::Parser::fromJson("{ \"param\" : \"z\" }")
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.duplicated == 'abcd', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] },
      { "E", &insertedDocs[4] },
      { "K", &insertedDocs[10] },
      { "U", &insertedDocs[20] },
      { "~", &insertedDocs[26] },
      { "$", &insertedDocs[30] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.duplicated == 'abcd' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.duplicated == 'abcd', name DESC
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*, StringComparer> expectedDocs {
      { "A", &insertedDocs[0] },
      { "E", &insertedDocs[4] },
      { "K", &insertedDocs[10] },
      { "U", &insertedDocs[20] },
      { "~", &insertedDocs[26] },
      { "$", &insertedDocs[30] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.duplicated == 'abcd' SORT d.name DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator actualDocs(result);
    CHECK(expectedDocs.size() == actualDocs.size());

    for (auto expectedDoc = expectedDocs.rbegin(), end = expectedDocs.rend(); expectedDoc != end; ++expectedDoc) {
      CHECK(actualDocs.valid());
      auto actualDoc = actualDocs.value();
      auto const resolved = actualDoc.resolveExternals();
      std::string const actualName = resolved.get("name").toString();
      std::string const expectedName = arangodb::velocypack::Slice(expectedDoc->second->vpack()).get("name").toString();

      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      actualDocs.next();
    }
    CHECK(!actualDocs.valid());
  }

  // d.duplicated == 'abcd', TFIDF() ASC, name DESC
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*, StringComparer> expectedDocs {
      { "A", &insertedDocs[0] },
      { "E", &insertedDocs[4] },
      { "K", &insertedDocs[10] },
      { "U", &insertedDocs[20] },
      { "~", &insertedDocs[26] },
      { "$", &insertedDocs[30] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.duplicated == 'abcd' SORT TFIDF(d) ASC, d.name DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator actualDocs(result);
    CHECK(expectedDocs.size() == actualDocs.size());

    for (auto expectedDoc = expectedDocs.rbegin(), end = expectedDocs.rend(); expectedDoc != end; ++expectedDoc) {
      CHECK(actualDocs.valid());
      auto actualDoc = actualDocs.value();
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      actualDocs.next();
    }
    CHECK(!actualDocs.valid());
  }

  // d.same == 'same', BM25() ASC, TFIDF() ASC, seq DESC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) DESC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // expression  (invalid value)
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "LET x = RAND()"
      "LET z = {} "
      "FOR d IN testView SEARCH z.name == (x + (RAND() + 1)) RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

// FIXME support expression with self-reference
//  // expression  (invalid value)
//  {
//    auto queryResult = arangodb::tests::executeQuery(
//      vocbase,
//      "LET x = RAND()"
//      "FOR d IN testView SEARCH d.name == (x + (RAND() + 1)) RETURN d"
//    );
//    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
//
//    auto result = queryResult.result->slice();
//    CHECK(result.isArray());
//
//    arangodb::velocypack::ArrayIterator resultIt(result);
//    CHECK(0 == resultIt.size());
//
//    for (auto const actualDoc : resultIt) {
//      UNUSED(actualDoc);
//      CHECK(false);
//    }
//  }

  // expression, d.duplicated == 'abcd', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] },
      { "E", &insertedDocs[4] },
      { "K", &insertedDocs[10] },
      { "U", &insertedDocs[20] },
      { "~", &insertedDocs[26] },
      { "$", &insertedDocs[30] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "LET x = _NONDETERM_('abcd') "
      "FOR d IN testView SEARCH d.duplicated == x RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // expression+variable, d.duplicated == 'abcd', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] },
      { "E", &insertedDocs[4] },
      { "K", &insertedDocs[10] },
      { "U", &insertedDocs[20] },
      { "~", &insertedDocs[26] },
      { "$", &insertedDocs[30] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "LET x = _NONDETERM_('abc') "
      "FOR d IN testView SEARCH d.duplicated == CONCAT(x, 'd') RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // expression+variable, d.duplicated == 'abcd', unordered, LIMIT 2
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] },
      { "E", &insertedDocs[4] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "LET x = _NONDETERM_('abc') "
      "FOR d IN testView SEARCH d.duplicated == CONCAT(x, 'd') LIMIT 2 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // expression, d.duplicated == 'abcd', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] },
      { "E", &insertedDocs[4] },
      { "K", &insertedDocs[10] },
      { "U", &insertedDocs[20] },
      { "~", &insertedDocs[26] },
      { "$", &insertedDocs[30] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.duplicated == CONCAT(_FORWARD_('abc'), 'd') RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // subquery, d.name == (FOR i IN collection_1 SEARCH i.name == 'A' RETURN i)[0].name), unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "LET x=(FOR i IN collection_1 FILTER i.name=='A' RETURN i)[0].name FOR d IN testView SEARCH d.name==x RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // subquery, d.name == (FOR i IN collection_1 SEARCH i.name == 'A' RETURN i)[0]), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "LET x=(FOR i IN collection_1 FILTER i.name=='A' RETURN i)[0] FOR d IN testView SEARCH d.name==x RETURN d"
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code); // unsupported type: object
  }

  // subquery, d.name == (FOR i IN collection_1 SEARCH i.name == 'A' RETURN i)[0].name), unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs {
      { "A", &insertedDocs[0] }
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name==(FOR i IN collection_1 FILTER i.name=='A' RETURN i)[0].name RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // subquery, d.name == (FOR i IN collection_1 SEARCH i.name == 'A' RETURN i)[0]), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name==(FOR i IN collection_1 FILTER i.name=='A' RETURN i)[0] RETURN d"
    );
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code); // unsupported type: object
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                !=
  // -----------------------------------------------------------------------------

  // invalid type, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name != 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // invalid type, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name != false RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // invalid type, d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name != null SORT d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();

      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // missing term, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name != 'invalid_term' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // existing duplicated term, unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.same != 'xyz' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // existing unique term, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    expectedDocs.erase("C");

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name != 'C' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // existing unique term, unordered (not all documents contain field)
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;

    for (auto const& doc: insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const fieldSlice = docSlice.get("duplicated");

      if (!fieldSlice.isNone() && "vczc" == arangodb::iresearch::getStringRef(fieldSlice)) {
        continue;
      }

      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.duplicated != 'vczc' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // missing term, seq DESC
  {
    auto& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name != 'invalid_term' SORT d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // existing duplicated term, TFIDF() ASC, BM25() ASC, seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const fieldSlice = docSlice.get("duplicated");

      if (!fieldSlice.isNone() && "abcd" == arangodb::iresearch::getStringRef(fieldSlice)) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.duplicated != 'abcd' SORT TFIDF(d) ASC, BM25(d) ASC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(resultIt.size() == expectedDocs.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // expression: invalid type, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "LET x = _NONDETERM_(0) "
      "FOR d IN testView SEARCH d.name != x RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // expression: existing duplicated term, TFIDF() ASC, BM25() ASC, seq DESC LIMIT 10
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    size_t limit = 5;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const fieldSlice = docSlice.get("duplicated");

      if (!fieldSlice.isNone() && "abcd" == arangodb::iresearch::getStringRef(fieldSlice)) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    // limit results
    while (expectedDocs.size() > limit) {
      expectedDocs.erase(expectedDocs.begin());
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "LET x = _NONDETERM_('abc') "
      "FOR d IN testView SEARCH d.duplicated != CONCAT(x,'d') SORT TFIDF(d) ASC, BM25(d) ASC, d.seq DESC LIMIT 5 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(resultIt.size() == expectedDocs.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      ++expectedDoc;

      if (!limit--) {
        break;
      }
    }

    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                 <
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name < null RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name < true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name < 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.name < 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key >= "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name < 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name < '!' (less than min term), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name < '!' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.name < '~' (less than max term), BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const nameSlice = docSlice.get("name");
      if (arangodb::iresearch::getStringRef(nameSlice) >= "~") {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name < '~' SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                <=
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name <= null RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name <= true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name <= 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.name <= 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key > "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name <= 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name <= '!' (less than min term), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name <= '!' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());

    auto const actualDoc = resultIt.value();
    auto const resolved = actualDoc.resolveExternals();
    CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(insertedDocs[27].vpack()), resolved, true));

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.name <= '~' (less than max term), BM25() ASC, TFIDF() ASC seq DESC
  {
    auto& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name <= '~' SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                 >
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > null RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.name > 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key <= "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name > '~' (greater than max term), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > '~' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name > '!' (greater than min term), BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const nameSlice = docSlice.get("name");
      if (arangodb::iresearch::getStringRef(nameSlice) <= "!") {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > '!' SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                                >=
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= null RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= true RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= 0 RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.name >= 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key < "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name >= '~' (greater or equal than max term), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= '~' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());

    auto const actualDoc = resultIt.value();
    auto const resolved = actualDoc.resolveExternals();
    CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(insertedDocs[26].vpack()), resolved, true));

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.name >= '!' (greater or equal than min term), BM25() ASC, TFIDF() ASC seq DESC
  {
    auto& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= '!' SORT BM25(d), TFIDF(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                      Range (>, <)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > null AND d.name < 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > true AND d.name < 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > 0 AND d.name < 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.name > 'H' AND d.name < 'S', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key <= "H" || key >= "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > 'H' AND d.name < 'S' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name > 'S' AND d.name < 'N' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > 'S' AND d.name < 'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name > 'H' AND d.name < 'H' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > 'H' AND d.name < 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name > '!' AND d.name < '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name <= "!" || name >= "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > '!' AND d.name < '~' SORT tfidf(d), BM25(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range (>=, <)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= null AND d.name < 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= true AND d.name < 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= 0 AND d.name < 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.name >= 'H' AND d.name < 'S' , unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key < "H" || key >= "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= 'H' AND d.name < 'S' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name >= 'S' AND d.name < 'N' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= 'S' AND d.name < 'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name >= 'H' AND d.name < 'H' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= 'H' AND d.name < 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name >= '!' AND d.name < '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name < "!" || name >= "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= '!' AND d.name < '~' SORT tfidf(d), BM25(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range (>, <=)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > null AND d.name <= 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > true AND d.name <= 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > 0 AND d.name <= 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.name >= 'H' AND d.name <= 'S' , unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key <= "H" || key > "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > 'H' AND d.name <= 'S' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name > 'S' AND d.name <= 'N' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > 'S' AND d.name <= 'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name > 'H' AND d.name <= 'H' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > 'H' AND d.name <= 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name > '!' AND d.name <= '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name <= "!" || name > "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name > '!' AND d.name <= '~' SORT tfidf(d), BM25(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                    Range (>=, <=)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= null AND d.name <= 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= true AND d.name <= 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= 0 AND d.name <= 'Z' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      CHECK(false);
    }
  }

  // d.name >= 'H' AND d.name <= 'S' , unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key < "H" || key > "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= 'H' AND d.name <= 'S' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      REQUIRE(expectedDoc != expectedDocs.end());
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    CHECK(expectedDocs.empty());
  }

  // d.name >= 'S' AND d.name <= 'N' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= 'S' AND d.name <= 'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.name >= 'H' AND d.name <= 'H' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= 'H' AND d.name <= 'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());
    CHECK(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(insertedDocs[7].vpack()), resolved, true));

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.name > '!' AND d.name <= '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name < "!" || name > "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name >= '!' AND d.name <= '~' SORT tfidf(d), BM25(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(expectedDocs.size() == resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedDoc->second->vpack()), resolved, true));
      ++expectedDoc;
    }
    CHECK(expectedDoc == expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                    Range (>=, <=)
  // -----------------------------------------------------------------------------

  // d.name >= 'H' AND d.name <= 'S' , unordered
  // (will be converted to d.name >= 0 AND d.name <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name IN 'H'..'S' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq >= 'H' AND d.seq <= 'S' , unordered
  // (will be converted to d.seq >= 0 AND d.seq <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.seq IN 'H'..'S' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());
    CHECK(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(insertedDocs[0].vpack()), resolved, true));

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.name >= 'S' AND d.name <= 'N' , unordered
  // (will be converted to d.name >= 0 AND d.name <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name IN 'S'..'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
  }

  // d.seq >= 'S' AND d.seq <= 'N' , unordered
  // (will be converted to d.seq >= 0 AND d.seq <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.seq IN 'S'..'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());
    CHECK(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(insertedDocs[0].vpack()), resolved, true));

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.name >= 'H' AND d.name <= 'H' , unordered
  // (will be converted to d.name >= 0 AND d.name <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name IN 'H'..'H' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
  }

  // d.seq >= 'H' AND d.seq <= 'N' , unordered
  // (will be converted to d.seq >= 0 AND d.seq <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.seq IN 'H'..'N' RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(1 == resultIt.size());
    CHECK(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    CHECK(0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(insertedDocs[0].vpack()), resolved, true));

    resultIt.next();
    CHECK(!resultIt.valid());
  }

  // d.name >= '!' AND d.name <= '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  // (will be converted to d.name >= 0 AND d.name <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.name IN '!'..'~' SORT tfidf(d), BM25(d), d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    CHECK(0 == resultIt.size());
    CHECK(!resultIt.valid());
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
