////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
#include "../IResearch/StorageEngineMock.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "velocypack/Parser.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct LogicalDataSourceSetup {
  StorageEngineMock engine;

  LogicalDataSourceSetup() {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
  }

  ~LogicalDataSourceSetup() {
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("LogicalDataSourceTest", "[vocbase]") {
  LogicalDataSourceSetup s;
  (void)(s);

SECTION("test_category") {
  // LogicalCollection
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    arangodb::LogicalCollection instance(nullptr, json->slice(), true);

    CHECK((arangodb::LogicalCollection::category() == instance.category()));
  }

  // LogicalView
  {
    class LogicalViewImpl: public arangodb::LogicalView {
     public:
      LogicalViewImpl(TRI_vocbase_t* vocbase, arangodb::velocypack::Slice const& definition)
        : LogicalView(vocbase, definition, 0) {
      }
      virtual arangodb::Result create() noexcept override { return arangodb::Result(); }
      virtual arangodb::Result drop() override { return arangodb::Result(); }
      virtual void open() override {}
      virtual arangodb::Result rename(std::string&& newName, bool doSync) override { return arangodb::Result(); }
      virtual void toVelocyPack(arangodb::velocypack::Builder& result, bool includeProperties, bool includeSystem) const override {}
      virtual arangodb::Result updateProperties(arangodb::velocypack::Slice const& properties, bool partialUpdate, bool doSync) override { return arangodb::Result(); }
      virtual bool visitCollections(CollectionVisitor const& visitor) const override { return true; }
    };

    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    LogicalViewImpl instance(nullptr, json->slice());

    CHECK((arangodb::LogicalView::category() == instance.category()));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------