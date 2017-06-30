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
#include "StorageEngineMock.h"

#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "IResearch/IResearchDocument.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

NS_LOCAL

void assertOrderSuccess(std::string const& queryString, irs::order const& expected) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  std::shared_ptr<arangodb::velocypack::Builder> bindVars;
  auto options = std::make_shared<arangodb::velocypack::Builder>();

  arangodb::aql::Query query(
     false, &vocbase, arangodb::aql::QueryString(queryString),
     bindVars, options,
     arangodb::aql::PART_MAIN
  );
/*
  auto const parseResult = query.parse();
  REQUIRE(TRI_ERROR_NO_ERROR == parseResult.code);

  auto* root = query.ast()->root();
  REQUIRE(root);
  auto* orderNode = root->getMember(2);
  REQUIRE(orderNode);

  irs::order actual;
  CHECK((arangodb::iresearch::OrderFactory::order(nullptr, *orderNode)));
  CHECK((arangodb::iresearch::OrderFactory::order(&actual, *orderNode)));
  CHECK((expected.size() == actual.size()));

  for (auto itrExp = expected.begin(), itrAct = actual.begin();
       itrExp != expected.end() && itrAct != actual.end();
       ++itrExp, ++itrAct) {
    CHECK((*itrExp == *itrAct));
  }
  */
}

void assertOrderFail(std::string const& queryString) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  arangodb::aql::Query query(
     false, &vocbase, arangodb::aql::QueryString(queryString),
     nullptr, nullptr,
     arangodb::aql::PART_MAIN
  );
/*
  auto const parseResult = query.parse();
  REQUIRE(TRI_ERROR_NO_ERROR == parseResult.code);

  auto* root = query.ast()->root();
  REQUIRE(root);
  auto* orderNode = root->getMember(2);
  REQUIRE(orderNode);

  irs::order actual;
  CHECK((!arangodb::iresearch::OrderFactory::order(nullptr, *orderNode)));
  CHECK((!arangodb::iresearch::OrderFactory::order(&actual, *orderNode)));
  */
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchOrderSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;

  IResearchOrderSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::application_features::ApplicationFeature* feature;

    // AqlFeature
    arangodb::application_features::ApplicationServer::server->addFeature(
      feature = new arangodb::AqlFeature(&server)
    );
    feature->start();
    feature->prepare();

    // QueryRegistryFeature
    arangodb::application_features::ApplicationServer::server->addFeature(
      feature = new arangodb::QueryRegistryFeature(&server)
    );
    feature->start();
    feature->prepare();

    // TraverserEngineRegistryFeature (required for AqlFeature::stop() to work)
    arangodb::application_features::ApplicationServer::server->addFeature(
      feature = new arangodb::TraverserEngineRegistryFeature(&server)
    );
    feature->start();
    feature->prepare();
  }

  ~IResearchOrderSetup() {
    arangodb::AqlFeature(&server).stop(); // unset singleton instance
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
}; // IResearchOrderSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchOrderTest", "[iresearch][iresearch-order]") {
  IResearchOrderSetup s;
  UNUSED(s);

SECTION("test_FCall") {
  //std::string const queryString = "FOR d IN collection FILTER '1' SORT tfidf() RETURN d";

  //irs::Or expected;
  //expected.add<irs::all>();
  

}

SECTION("test_FCallUser") {
  // function
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf() RETURN d";
    irs::order expected;

    assertOrderSuccess(query, expected);
  }

  // function ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf() ASC RETURN d";
    irs::order expected;

    assertOrderSuccess(query, expected);
  }

  // function DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf() DESC RETURN d";
    irs::order expected;

    assertOrderSuccess(query, expected);
  }

  // invalid function
  {
    std::string query = "FOR d IN collection FILTER '1' SORT invalid() DESC RETURN d";

    assertOrderFail(query);
  }
}

SECTION("test_StringValue") {
  // simple field
  {
    std::string query = "FOR d IN collection FILTER '1' SORT a RETURN d";
    irs::order expected;

    assertOrderSuccess(query, expected);
  }

  // simple field ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT a ASC RETURN d";
    irs::order expected;

    assertOrderSuccess(query, expected);
  }

  // simple field DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT a DESC RETURN d";
    irs::order expected;

    assertOrderSuccess(query, expected);
  }

  // nested field
  {
    std::string query = "FOR d IN collection FILTER '1' SORT a.b.c RETURN d";
    irs::order expected;

    assertOrderSuccess(query, expected);
  }

  // nested field ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT a.b.c ASC RETURN d";
    irs::order expected;

    assertOrderSuccess(query, expected);
  }

  // nested field DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT a.b.c DESC RETURN d";
    irs::order expected;

    assertOrderSuccess(query, expected);
  }

  // invalid field
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 1 RETURN d";

    assertOrderFail(query);
  }
}

SECTION("test_order") {

// test empty
// test mutiple
// test reverse string/bool

}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// ----------------------------------------------------------------------------