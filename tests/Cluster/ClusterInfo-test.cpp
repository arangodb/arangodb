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

#include "gtest/gtest.h"

#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"

#include "IResearch/AgencyMock.h"
#include "Mocks/LogLevels.h"
#include "Mocks/StorageEngineMock.h"

#include "Agency/Store.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalView.h"
#include "VocBase/vocbase.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

namespace {

struct TestView : public arangodb::LogicalView {
  arangodb::velocypack::Builder _definition;
  TestView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition, uint64_t planVersion)
      : arangodb::LogicalView(vocbase, definition, planVersion),
        _definition(definition) {}
  virtual arangodb::Result appendVelocyPackImpl(
      arangodb::velocypack::Builder& builder,
      std::underlying_type<LogicalDataSource::Serialize>::type flags) const override {
    return arangodb::iresearch::mergeSlice(builder, _definition.slice())
               ? TRI_ERROR_NO_ERROR
               : TRI_ERROR_INTERNAL;
  }
  virtual arangodb::Result dropImpl() override {
    return arangodb::LogicalViewHelperClusterInfo::drop(*this);
  }
  virtual void open() override {}
  virtual arangodb::Result properties(arangodb::velocypack::Slice const&, bool) override {
    return arangodb::Result();
  }
  virtual arangodb::Result renameImpl(std::string const& oldName) override {
    return arangodb::LogicalViewHelperStorageEngine::rename(*this, oldName);
  }
  virtual bool visitCollections(CollectionVisitor const& visitor) const override {
    return true;
  }
};

struct ViewFactory : public arangodb::ViewFactory {
  virtual arangodb::Result create(arangodb::LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                                  arangodb::velocypack::Slice const& definition) const override {
    auto* ci = arangodb::ClusterInfo::instance();

    if (!ci) {
      return TRI_ERROR_INTERNAL;
    }

    auto res = instantiate(view, vocbase, definition, 0);

    if (!res.ok()) {
      return res;
    }

    if (!view) {
      return TRI_ERROR_INTERNAL;
    }

    arangodb::velocypack::Builder builder;

    builder.openObject();
    res = view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                        arangodb::LogicalDataSource::Serialize::Detailed,
                                        arangodb::LogicalDataSource::Serialize::ForPersistence));

    if (!res.ok()) {
      return res;
    }

    builder.close();

    return ci->createViewCoordinator(vocbase.name(), std::to_string(view->id()),
                                     builder.slice());
  }

  virtual arangodb::Result instantiate(arangodb::LogicalView::ptr& view,
                                       TRI_vocbase_t& vocbase,
                                       arangodb::velocypack::Slice const& definition,
                                       uint64_t planVersion) const override {
    view = std::make_shared<TestView>(vocbase, definition, planVersion);

    return arangodb::Result();
  }
};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class ClusterInfoTest : public ::testing::Test,
                        public LogSuppressor<Logger::AGENCY, LogLevel::FATAL>,
                        public LogSuppressor<Logger::AUTHENTICATION, LogLevel::ERR>,
                        public LogSuppressor<Logger::CLUSTER, LogLevel::FATAL>,
{
 protected:
  struct ClusterCommControl : arangodb::ClusterComm {
    static void reset() { arangodb::ClusterComm::_theInstanceInit.store(0); }
  };

  arangodb::application_features::ApplicationServer server;
  GeneralClientConnectionAgencyMock* agency;
  arangodb::consensus::Store agencyStore;
  StorageEngineMock engine;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;
  ViewFactory viewFactory;

  ClusterInfoTest()
      : server(nullptr, nullptr), agencyStore(server, nullptr, "arango"), engine(server) {
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(agencyStore);
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(
        agencyStore);  // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // setup required application features
    features.emplace_back(server.addFeature<arangodb::MetricsFeature>());  
    features.emplace_back(server.addFeature<arangodb::AuthenticationFeature>(), false);  // required for ClusterFeature::prepare()
    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::application_features::CommunicationFeaturePhase>(),
                          false);
    features.emplace_back(server.addFeature<arangodb::ClusterFeature>(),
                          false);  // required for ClusterInfo::instance()
    features.emplace_back(server.addFeature<arangodb::QueryRegistryFeature>(), false);  // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(server.addFeature<arangodb::V8DealerFeature>(), false);  // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(server.addFeature<arangodb::ViewTypesFeature>(), false);  // required for LogicalView::instantiate(...)

#if USE_ENTERPRISE
    features.emplace_back(server.addFeature<arangodb::LdapFeature>(),
                          false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    for (auto& f : features) {
      f.first.prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first.start();
      }
    }

    // register view factory
    server.getFeature<arangodb::ViewTypesFeature>().emplace(
        arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef("testViewType")),
        viewFactory);

    agencyCommManager->start();  // initialize agency
  }

  ~ClusterInfoTest() {
    arangodb::ClusterInfo::cleanup();  // reset ClusterInfo::instance() before DatabaseFeature::unprepare()

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first.stop();
      }
    }

    for (auto& f : features) {
      f.first.unprepare();
    }

    ClusterCommControl::reset();
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(ClusterInfoTest, test_drop_database) {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  ASSERT_NE(nullptr, database);
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_NE(nullptr, ci);

  // test LogicalView dropped when database dropped
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
    // create database
    ASSERT_TRUE((TRI_ERROR_NO_ERROR ==
                 database->createDatabase(1, "testDatabase", arangodb::velocypack::Slice::emptyObjectSlice(), vocbase)));
    ASSERT_NE(nullptr, vocbase);

    // simulate heartbeat thread
    ASSERT_TRUE(arangodb::AgencyComm().setValue("Current/Databases/testDatabase", arangodb::velocypack::Slice::emptyObjectSlice(), 0.0).successful());
    ASSERT_TRUE(ci->createDatabaseCoordinator(
                  vocbase->name(),
                  arangodb::velocypack::Slice::emptyObjectSlice(), 0.0).ok());

    // initial view creation
    {
      arangodb::LogicalView::ptr logicalView;
      ASSERT_TRUE(
          (viewFactory.create(logicalView, *vocbase, viewCreateJson->slice()).ok()));
      ASSERT_FALSE(!logicalView);
    }
    EXPECT_TRUE(ci->dropDatabaseCoordinator(vocbase->name(), 0.0).ok());
    ASSERT_TRUE(arangodb::AgencyComm().setValue("Current/Databases/testDatabase", arangodb::velocypack::Slice::emptyObjectSlice(), 0.0).successful());
    EXPECT_TRUE((ci->createDatabaseCoordinator(
                   vocbase->name(),
                   arangodb::velocypack::Slice::emptyObjectSlice(), 0.0).ok()));
    arangodb::LogicalView::ptr logicalView;
    EXPECT_TRUE(
        (viewFactory.create(logicalView, *vocbase, viewCreateJson->slice()).ok()));
    EXPECT_FALSE(!logicalView);
  }
}
