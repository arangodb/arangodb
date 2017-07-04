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

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/AttributeScorer.h"
#include "IResearch/IResearchView.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalView.h"
#include "velocypack/Parser.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchAttributeScorerSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchAttributeScorerSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // must be first
    features.emplace_back(new arangodb::AuthenticationFeature(arangodb::application_features::ApplicationServer::server), true);
    features.emplace_back(new arangodb::DatabaseFeature(arangodb::application_features::ApplicationServer::server), false);
    features.emplace_back(new arangodb::FeatureCacheFeature(arangodb::application_features::ApplicationServer::server), true);
    features.emplace_back(new arangodb::ViewTypesFeature(arangodb::application_features::ApplicationServer::server), true);

    arangodb::ViewTypesFeature::registerViewImplementation(
      arangodb::iresearch::IResearchView::type(),
      arangodb::iresearch::IResearchView::make
    );

    for (auto& entry: features) {
      arangodb::application_features::ApplicationServer::server->addFeature(entry.first);
      entry.first->prepare();

      if (entry.second) {
        entry.first->start();
      }
    }
  }

  ~IResearchAttributeScorerSetup() {
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto& entry : features) {
      if (entry.second) {
        entry.first->stop();
      }

      entry.first->unprepare();
    }

    arangodb::FeatureCacheFeature::reset();
  }
}; // IResearchAttributeScorerSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchAttributeScorerTest", "[iresearch][iresearch-scorer][iresearch-attribute_scorer]") {
  IResearchAttributeScorerSetup s;
  UNUSED(s);

SECTION("test_query") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"links\": { \"testCollection\": { \"includeAllFields\": true } } \
  }");
  auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
  CHECK((nullptr != logicalCollection));
  auto logicalView = vocbase.createView(viewJson->slice(), 0);
  CHECK((false == !logicalView));
  auto* view = logicalView->getImplementation();
  CHECK((false == !view));

  // fill with test data
  {
    // FIXME TODO
  }
  
  // query view
  {
    std::string query = "FOR d IN testCollection SORT 'testAttr' RETURN d";
    // FIXME TODO
  }
  
  // query view ascending
  {
    std::string query = "FOR d IN testCollection SORT 'testAttr' ASC RETURN d";
    // FIXME TODO
  }

  // query view descending
  {
    std::string query = "FOR d IN testCollection SORT 'testAttr' DESC RETURN d";
    // FIXME TODO
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// ----------------------------------------------------------------------------