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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <velocypack/Parser.h>

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/StorageEngineMock.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Mocks/ExecContextFactory.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalView.h"
#include "VocBase/VocbaseInfo.h"
#include "VocBase/vocbase.h"
#include "Cluster/ClusterFeature.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"

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
  arangodb::application_features::ApplicationServer server;
  StorageEngineMock& engine;
  std::vector<
      std::pair<arangodb::application_features::ApplicationFeature&, bool>>
      features;
  ViewFactory viewFactory;

  LogicalViewTest()
      : server(nullptr, nullptr),
        engine(
            server.addFeature<arangodb::StorageEngine, StorageEngineMock>()) {
    features.emplace_back(server.addFeature<arangodb::AuthenticationFeature>(),
                          false);  // required for ExecContext
    auto& dbFeature = server.addFeature<arangodb::DatabaseFeature>();
    features.emplace_back(dbFeature, false);
    features.emplace_back(
        server.addFeature<arangodb::metrics::MetricsFeature>(
            arangodb::LazyApplicationFeatureReference<
                arangodb::QueryRegistryFeature>(server),
            arangodb::LazyApplicationFeatureReference<
                arangodb::StatisticsFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<
                arangodb::DatabaseFeature>(dbFeature),
            arangodb::LazyApplicationFeatureReference<
                arangodb::metrics::ClusterMetricsFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<arangodb::ClusterFeature>(
                nullptr)),
        false);
    features.emplace_back(
        server.addFeature<arangodb::QueryRegistryFeature>(
            server.getFeature<arangodb::metrics::MetricsFeature>()),
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

  // no ExecContext (implicitly superuser!)
  {
    TRI_vocbase_t vocbase(testDBInfo(server), engine);
    auto logicalView = vocbase.createView(viewJson->slice(), false);
    EXPECT_TRUE(arangodb::ExecContext::current()
                    .canReadView(vocbase.name(), logicalView->name())
                    .ok());
  }

  // no read access
  {
    TRI_vocbase_t vocbase(testDBInfo(server), engine);
    auto logicalView = vocbase.createView(viewJson->slice(), false);
    auto classicCtx = arangodb::tests::mocks::makeClassicExecContext(
        "", "testVocbase", arangodb::auth::Level::NONE,
        arangodb::auth::Level::NONE);
    EXPECT_FALSE(
        classicCtx.execContext->canReadView(vocbase.name(), logicalView->name())
            .ok());
  }

  // read access
  {
    TRI_vocbase_t vocbase(testDBInfo(server), engine);
    auto logicalView = vocbase.createView(viewJson->slice(), false);
    auto classicCtx = arangodb::tests::mocks::makeClassicExecContext(
        "", "testVocbase", arangodb::auth::Level::NONE,
        arangodb::auth::Level::RO);
    EXPECT_TRUE(
        classicCtx.execContext->canReadView(vocbase.name(), logicalView->name())
            .ok());
  }

  // write access
  {
    TRI_vocbase_t vocbase(testDBInfo(server), engine);
    auto logicalView = vocbase.createView(viewJson->slice(), false);
    auto classicCtx = arangodb::tests::mocks::makeClassicExecContext(
        "", "testVocbase", arangodb::auth::Level::NONE,
        arangodb::auth::Level::RW);
    EXPECT_TRUE(
        classicCtx.execContext->canReadView(vocbase.name(), logicalView->name())
            .ok());
  }
}
