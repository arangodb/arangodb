////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalView.h"
#include "VocBase/VocbaseInfo.h"
#include "VocBase/vocbase.h"
#include "Cluster/ClusterFeature.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

namespace {
struct TestView : public arangodb::LogicalView {
  static constexpr auto typeInfo() noexcept {
    return std::pair{static_cast<arangodb::ViewType>(42),
                     std::string_view{"testViewType"}};
  }

  arangodb::Result _appendVelocyPackResult;
  arangodb::velocypack::Builder _properties;

  TestView(TRI_vocbase_t& vocbase,
           arangodb::velocypack::Slice const& definition)
      : arangodb::LogicalView(*this, vocbase, definition, false) {}
  arangodb::Result appendVPackImpl(arangodb::velocypack::Builder& build,
                                   Serialization, bool) const override {
    build.add("properties", _properties.slice());
    return _appendVelocyPackResult;
  }
  virtual arangodb::Result dropImpl() override {
    return arangodb::storage_helper::drop(*this);
  }
  virtual void open() override {}
  virtual arangodb::Result renameImpl(std::string const& oldName) override {
    return arangodb::storage_helper::rename(*this, oldName);
  }
  virtual arangodb::Result properties(arangodb::velocypack::Slice properties,
                                      bool /*isUserRequest*/,
                                      bool /*partialUpdate*/) override {
    _properties = arangodb::velocypack::Builder(properties);
    return arangodb::Result();
  }
  virtual bool visitCollections(
      CollectionVisitor const& /*visitor*/) const override {
    return true;
  }
};

struct ViewFactory : public arangodb::ViewFactory {
  virtual arangodb::Result create(arangodb::LogicalView::ptr& view,
                                  TRI_vocbase_t& vocbase,
                                  arangodb::velocypack::Slice definition,
                                  bool isUserRequest) const override {
    view = vocbase.createView(definition, isUserRequest);

    return arangodb::Result();
  }

  virtual arangodb::Result instantiate(arangodb::LogicalView::ptr& view,
                                       TRI_vocbase_t& vocbase,
                                       arangodb::velocypack::Slice definition,
                                       bool /*isUserRequest*/) const override {
    view = std::make_shared<TestView>(vocbase, definition);

    return arangodb::Result();
  }
};

}  // namespace

class LogicalViewTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
 protected:
  arangodb::ArangodServer server;
  StorageEngineMock engine;
  std::vector<
      std::pair<arangodb::application_features::ApplicationFeature&, bool>>
      features;
  ViewFactory viewFactory;

  LogicalViewTest() : server(nullptr, nullptr), engine(server) {
    auto& selector = server.addFeature<arangodb::EngineSelectorFeature>();
    features.emplace_back(selector, false);
    selector.setEngineTesting(&engine);
    features.emplace_back(server.addFeature<arangodb::AuthenticationFeature>(),
                          false);  // required for ExecContext
    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(),
                          false);
    features.emplace_back(
        server.addFeature<arangodb::metrics::MetricsFeature>(
            arangodb::LazyApplicationFeatureReference<
                arangodb::QueryRegistryFeature>(server),
            arangodb::LazyApplicationFeatureReference<
                arangodb::StatisticsFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<
                arangodb::EngineSelectorFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<
                arangodb::metrics::ClusterMetricsFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<arangodb::ClusterFeature>(
                nullptr)),
        false);
    features.emplace_back(
        server.addFeature<arangodb::QueryRegistryFeature>(
            server.template getFeature<arangodb::metrics::MetricsFeature>()),
        false);  // required for TRI_vocbase_t
    features.emplace_back(server.addFeature<arangodb::ViewTypesFeature>(),
                          false);  // required for LogicalView::create(...)

    for (auto& f : features) {
      f.first.prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first.start();
      }
    }

    auto& viewTypesFeature = server.getFeature<arangodb::ViewTypesFeature>();
    viewTypesFeature.emplace(TestView::typeInfo().second, viewFactory);
  }

  ~LogicalViewTest() {
    server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(
        nullptr);

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

TEST_F(LogicalViewTest, test_auth) {
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"testViewType\" }");

  // no ExecContext
  {
    TRI_vocbase_t vocbase(testDBInfo(server));
    auto logicalView = vocbase.createView(viewJson->slice(), false);
    EXPECT_TRUE(logicalView->canUse(arangodb::auth::Level::RW));
  }

  // no read access
  {
    TRI_vocbase_t vocbase(testDBInfo(server));
    auto logicalView = vocbase.createView(viewJson->slice(), false);
    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::ConstructorToken{},
                                  arangodb::ExecContext::Type::Default, "",
                                  "testVocbase", arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE, false) {}
    };
    auto execContext = std::make_shared<ExecContext>();
    arangodb::ExecContextScope execContextScope(execContext);
    EXPECT_FALSE(logicalView->canUse(arangodb::auth::Level::RO));
  }

  // no write access
  {
    TRI_vocbase_t vocbase(testDBInfo(server));
    auto logicalView = vocbase.createView(viewJson->slice(), false);
    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::ConstructorToken{},
                                  arangodb::ExecContext::Type::Default, "",
                                  "testVocbase", arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::RO, false) {}
    };
    auto execContext = std::make_shared<ExecContext>();
    arangodb::ExecContextScope execContextScope(execContext);
    EXPECT_TRUE(logicalView->canUse(arangodb::auth::Level::RO));
    EXPECT_FALSE(logicalView->canUse(arangodb::auth::Level::RW));
  }

  // write access (view access is db access as per
  // https://github.com/arangodb/backlog/issues/459)
  {
    TRI_vocbase_t vocbase(testDBInfo(server));
    auto logicalView = vocbase.createView(viewJson->slice(), false);
    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::ConstructorToken{},
                                  arangodb::ExecContext::Type::Default, "",
                                  "testVocbase", arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::RW, false) {}
    };
    auto execContext = std::make_shared<ExecContext>();
    arangodb::ExecContextScope execContextScope(execContext);
    EXPECT_TRUE(logicalView->canUse(arangodb::auth::Level::RO));
    EXPECT_TRUE(logicalView->canUse(arangodb::auth::Level::RW));
  }
}
