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

#include "utils/misc.hpp"
#include "utils/string.hpp"

#include "Aql/AqlFunctionFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchFeatureSetup {
  StorageEngineMock engine;

  IResearchFeatureSetup() {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
  }

  ~IResearchFeatureSetup() {
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearcFeatureTest", "[iresearch][iresearch-feature]") {
  IResearchFeatureSetup s;
  UNUSED(s);

SECTION("test_start") {
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  auto* functions = new arangodb::aql::AqlFunctionFeature(&server);
  arangodb::iresearch::IResearchFeature iresearch(&server);
  auto cleanup = irs::make_finally([functions]()->void{ functions->unprepare(); });
  std::map<irs::string_ref, irs::string_ref> expected = {
    // filter functions
    { "EXISTS", ".|.,." },
    { "PHRASE", ".,.,.|.+" },
    { "STARTS_WITH", ".,.|." },

    // scorer functions
    { "@", ".|+" },
    { "BM25", ".|+" },
    { "TFIDF", ".|+" },
  };

  server.addFeature(functions);
  functions->prepare();

  for(auto& entry: expected) {
    auto* function = arangodb::iresearch::getFunction(*functions, entry.first);
    CHECK((nullptr == function));
  };

  iresearch.start();

  for(auto& entry: expected) {
    auto* function = arangodb::iresearch::getFunction(*functions, entry.first);
    CHECK((nullptr != function));
    CHECK((entry.second == function->arguments));
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------