////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include <velocypack/Parser.h>

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/StorageEngineMock.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryRegistry.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalView.h"
#include "VocBase/VocbaseInfo.h"
#include "VocBase/vocbase.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

namespace {
struct TestView : public arangodb::LogicalView {
  arangodb::Result _appendVelocyPackResult;
  arangodb::velocypack::Builder _properties;

  TestView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition)
      : arangodb::LogicalView(vocbase, definition) {}
  virtual arangodb::Result appendVelocyPackImpl(
      arangodb::velocypack::Builder& builder,
      Serialization) const override {
    builder.add("properties", _properties.slice());
    return _appendVelocyPackResult;
  }
  virtual arangodb::Result dropImpl() override {
    return arangodb::LogicalViewHelperStorageEngine::drop(*this);
  }
  virtual void open() override {}
  virtual arangodb::Result renameImpl(std::string const& oldName) override {
    return arangodb::LogicalViewHelperStorageEngine::rename(*this, oldName);
  }
  virtual arangodb::Result properties(arangodb::velocypack::Slice const& properties,
                                      bool partialUpdate) override {
    _properties = arangodb::velocypack::Builder(properties);
    return arangodb::Result();
  }
  virtual bool visitCollections(CollectionVisitor const& visitor) const override {
    return true;
  }
};

struct ViewFactory : public arangodb::ViewFactory {
  virtual arangodb::Result create(arangodb::LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                                  arangodb::velocypack::Slice const& definition) const override {
    view = vocbase.createView(definition);

    return arangodb::Result();
  }

  virtual arangodb::Result instantiate(arangodb::LogicalView::ptr& view,
                                       TRI_vocbase_t& vocbase,
                                       arangodb::velocypack::Slice const& definition) const override {
    view = std::make_shared<TestView>(vocbase, definition);

    return arangodb::Result();
  }
};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class LogicalViewTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;
  ViewFactory viewFactory;

  LogicalViewTest() : engine(server), server(nullptr, nullptr) {
    auto& selector = server.addFeature<arangodb::EngineSelectorFeature>();
    features.emplace_back(selector, false);
    selector.setEngineTesting(&engine);
    features.emplace_back(server.addFeature<arangodb::AuthenticationFeature>(), false);  // required for ExecContext
    features.emplace_back(server.addFeature<arangodb::MetricsFeature>(), false);  
    features.emplace_back(server.addFeature<arangodb::QueryRegistryFeature>(), false);  // required for TRI_vocbase_t
    features.emplace_back(server.addFeature<arangodb::ViewTypesFeature>(), false);  // required for LogicalView::create(...)

#if USE_ENTERPRISE
    features.emplace_back(server.addFeature<arangodb::LdapFeature>(), false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    for (auto& f : features) {
      f.first.prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first.start();
      }
    }

    auto& viewTypesFeature = server.getFeature<arangodb::ViewTypesFeature>();
    viewTypesFeature.emplace(arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef(
                                 "testViewType")),
                             viewFactory);
  }

  ~LogicalViewTest() {
    server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(nullptr);

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first.stop();
      }
    }

    for (auto& f : features) {
      f.first.unprepare();
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(LogicalViewTest, test_auth) {
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"testViewType\" }");

  // no ExecContext
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));
    auto logicalView = vocbase.createView(viewJson->slice());
    EXPECT_TRUE(logicalView->canUse(arangodb::auth::Level::RW));
  }

  // no read access
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));
    auto logicalView = vocbase.createView(viewJson->slice());
    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "",
                                  "testVocbase", arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    EXPECT_FALSE(logicalView->canUse(arangodb::auth::Level::RO));
  }

  // no write access
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));
    auto logicalView = vocbase.createView(viewJson->slice());
    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "",
                                  "testVocbase", arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::RO, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    EXPECT_TRUE(logicalView->canUse(arangodb::auth::Level::RO));
    EXPECT_FALSE(logicalView->canUse(arangodb::auth::Level::RW));
  }

  // write access (view access is db access as per https://github.com/arangodb/backlog/issues/459)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));
    auto logicalView = vocbase.createView(viewJson->slice());
    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "",
                                  "testVocbase", arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::RW, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    EXPECT_TRUE(logicalView->canUse(arangodb::auth::Level::RO));
    EXPECT_TRUE(logicalView->canUse(arangodb::auth::Level::RW));
  }
}
