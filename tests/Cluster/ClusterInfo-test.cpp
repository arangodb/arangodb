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
#include "../IResearch/AgencyMock.h"
#include "../IResearch/StorageEngineMock.h"
#include "Agency/Store.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationPhase.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "VocBase/LogicalView.h"
#include "VocBase/vocbase.h"

namespace {

struct TestView: public arangodb::LogicalView {
  arangodb::velocypack::Builder _definition;
  TestView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition, uint64_t planVersion)
    : arangodb::LogicalView(vocbase, definition, planVersion), _definition(definition) {
  }
  virtual arangodb::Result appendVelocyPack(arangodb::velocypack::Builder& builder, bool , bool) const override {
    return arangodb::iresearch::mergeSlice(builder, _definition.slice()) ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL;
  }
  virtual arangodb::Result drop() override {
    auto* ci = arangodb::ClusterInfo::instance();

    if (!ci) {
      return TRI_ERROR_INTERNAL;
    }

    deleted(true);

    std::string error;
    auto res = ci->dropViewCoordinator(vocbase().name(), std::to_string(id()), error);

    return arangodb::Result(res, error);
  }
  virtual void open() override {}
  virtual arangodb::Result properties(arangodb::velocypack::Slice const&, bool) override { return arangodb::Result(); }
  virtual arangodb::Result rename(std::string&& newName) override { name(std::move(newName)); return arangodb::Result(); }
  virtual bool visitCollections(CollectionVisitor const& visitor) const override { return true; }
};

struct ViewFactory: public arangodb::ViewFactory {
  virtual arangodb::Result create(
      arangodb::LogicalView::ptr& view,
      TRI_vocbase_t& vocbase,
      arangodb::velocypack::Slice const& definition
  ) const override {
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
    res = view->properties(builder, true, true);

    if (!res.ok()) {
      return res;
    }

    builder.close();

    std::string error;
    return ci->createViewCoordinator(
      vocbase.name(), std::to_string(view->id()), builder.slice(), error
    );
  }

  virtual arangodb::Result instantiate(
      arangodb::LogicalView::ptr& view,
      TRI_vocbase_t& vocbase,
      arangodb::velocypack::Slice const& definition,
      uint64_t planVersion
  ) const override {
    view = std::make_shared<TestView>(vocbase, definition, planVersion);

    return arangodb::Result();
  }
};

}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct ClusterInfoSetup {
  struct ClusterCommControl: arangodb::ClusterComm {
    static void reset() { arangodb::ClusterComm::_theInstanceInit.store(0); }
  };

  GeneralClientConnectionAgencyMock* agency;
  arangodb::consensus::Store agencyStore{nullptr, "arango"};
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  ViewFactory viewFactory;

  ClusterInfoSetup(): engine(server), server(nullptr, nullptr) {
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(agencyStore);
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(agencyStore); // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress INFO {cluster} Starting up with role SINGLE
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::FATAL);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(), arangodb::LogLevel::FATAL);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), false); // required for ClusterFeature::prepare()
    features.emplace_back(arangodb::DatabaseFeature::DATABASE = new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::application_features::CommunicationFeaturePhase(server), false);
    features.emplace_back(new arangodb::ClusterFeature(server), false); // required for ClusterInfo::instance()
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::V8DealerFeature(server), false); // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::ViewTypesFeature(server), false); // required for LogicalView::instantiate(...)

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    for (auto& f: features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f: features) {
      f.first->prepare();
    }

    for (auto& f: features) {
      if (f.second) {
        f.first->start();
      }
    }

    // register view factory
    arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::ViewTypesFeature
    >()->emplace(
      arangodb::LogicalDataSource::Type::emplace(
        arangodb::velocypack::StringRef("testViewType")
      ),
      viewFactory
    );

    agencyCommManager->start(); // initialize agency
  }

  ~ClusterInfoSetup() {
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(), arangodb::LogLevel::DEFAULT);
    arangodb::ClusterInfo::cleanup(); // reset ClusterInfo::instance() before DatabaseFeature::unprepare()
    arangodb::application_features::ApplicationServer::server = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }

    ClusterCommControl::reset();
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("ClusterInfoTest", "[cluster]") {
  ClusterInfoSetup s;
  (void)(s);

SECTION("test_drop_databse") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE((nullptr != ci));

  // test LogicalView dropped when database dropped
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    std::string error;
    TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
    REQUIRE((TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase)));
    REQUIRE((nullptr != vocbase));
    REQUIRE((TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(vocbase->name(), arangodb::velocypack::Slice::emptyObjectSlice(), error, 0.0)));

    // initial view creation
    {
      arangodb::LogicalView::ptr logicalView;
      REQUIRE((s.viewFactory.create(logicalView, *vocbase, viewCreateJson->slice()).ok()));
      REQUIRE((false == !logicalView));
    }

    CHECK((TRI_ERROR_NO_ERROR == ci->dropDatabaseCoordinator(vocbase->name(), error, 0.0)));
    CHECK((TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(vocbase->name(), arangodb::velocypack::Slice::emptyObjectSlice(), error, 0.0)));

    arangodb::LogicalView::ptr logicalView;
    CHECK((s.viewFactory.create(logicalView, *vocbase, viewCreateJson->slice()).ok()));
    CHECK((false == !logicalView));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
