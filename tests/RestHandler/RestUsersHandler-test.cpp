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
#include "../IResearch/RestHandlerMock.h"
#include "../IResearch/StorageEngineMock.h"
#include "Aql/QueryRegistry.h"
#include "Basics/StaticStrings.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "RestHandler/RestUsersHandler.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "V8Server/V8DealerFeature.h"
#include "velocypack/Parser.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/vocbase.h"

namespace {

struct TestView: public arangodb::LogicalView {
  arangodb::Result _appendVelocyPackResult;
  arangodb::velocypack::Builder _properties;

  TestView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition, uint64_t planVersion)
    : arangodb::LogicalView(vocbase, definition, planVersion) {
  }
  virtual arangodb::Result appendVelocyPack(arangodb::velocypack::Builder& builder, bool /*detailed*/, bool /*forPersistence*/) const override { 
    builder.add("properties", _properties.slice());
    return _appendVelocyPackResult;
  }
  virtual arangodb::Result drop() override { return arangodb::Result(); }
  virtual void open() override {}
  virtual arangodb::Result rename(std::string&& newName) override { name(std::move(newName)); return arangodb::Result(); }
  virtual arangodb::Result properties(arangodb::velocypack::Slice const& properties, bool partialUpdate) override { _properties = arangodb::velocypack::Builder(properties); return arangodb::Result(); }
  virtual bool visitCollections(CollectionVisitor const& visitor) const override { return true; }
};

struct ViewFactory: public arangodb::ViewFactory {
  virtual arangodb::Result create(
      arangodb::LogicalView::ptr& view,
      TRI_vocbase_t& vocbase,
      arangodb::velocypack::Slice const& definition
  ) const override {
    view = vocbase.createView(definition);

    return arangodb::Result();
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

struct RestUsersHandlerSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  ViewFactory viewFactory;

  RestUsersHandlerSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    features.emplace_back(new arangodb::AuthenticationFeature(server), false); // required for VocbaseContext
    features.emplace_back(new arangodb::DatabaseFeature(server), false); // required for UserManager::updateUser(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required for TRI_vocbase_t
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::ShardingFeature(server), false); // required for LogicalCollection::LogicalCollection(...)
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::ViewTypesFeature(server), false); // required for LogicalView::create(...)

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    arangodb::application_features::ApplicationServer::server->addFeature(
      new arangodb::V8DealerFeature(server)
    ); // add without calling prepare(), required for DatabaseFeature::createDatabase(...)

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

    auto* viewTypesFeature =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::ViewTypesFeature>();

    viewTypesFeature->emplace(
      arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef("testViewType")),
      viewFactory
    );
  }

  ~RestUsersHandlerSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
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

    arangodb::EngineSelectorFeature::ENGINE = nullptr; // nullify only after DatabaseFeature::unprepare()
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("RestUsersHandlerTest", "[rest]") {
  RestUsersHandlerSetup s;
  (void)(s);

SECTION("test_collection_auth") {
  auto usersJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"_users\", \"isSystem\": true }");
  static const std::string userName("testUser");
  auto* databaseFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabaseFeature>("Database");
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
  REQUIRE((TRI_ERROR_NO_ERROR == databaseFeature->createDatabase(1, "testDatabase", vocbase)));
  auto grantRequestPtr = std::make_unique<GeneralRequestMock>(*vocbase);
  auto& grantRequest = *grantRequestPtr;
  auto grantResponcePtr = std::make_unique<GeneralResponseMock>();
  auto& grantResponce = *grantResponcePtr;
  auto grantWildcardRequestPtr = std::make_unique<GeneralRequestMock>(*vocbase);
  auto& grantWildcardRequest = *grantWildcardRequestPtr;
  auto grantWildcardResponcePtr = std::make_unique<GeneralResponseMock>();
  auto& grantWildcardResponce = *grantWildcardResponcePtr;
  auto revokeRequestPtr = std::make_unique<GeneralRequestMock>(*vocbase);
  auto& revokeRequest = *revokeRequestPtr;
  auto revokeResponcePtr = std::make_unique<GeneralResponseMock>();
  auto& revokeResponce = *revokeResponcePtr;
  auto revokeWildcardRequestPtr = std::make_unique<GeneralRequestMock>(*vocbase);
  auto& revokeWildcardRequest = *revokeWildcardRequestPtr;
  auto revokeWildcardResponcePtr = std::make_unique<GeneralResponseMock>();
  auto& revokeWildcardResponce = *revokeWildcardResponcePtr;
  arangodb::RestUsersHandler grantHandler(grantRequestPtr.release(), grantResponcePtr.release());
  arangodb::RestUsersHandler grantWildcardHandler(grantWildcardRequestPtr.release(), grantWildcardResponcePtr.release());
  arangodb::RestUsersHandler revokeHandler(revokeRequestPtr.release(), revokeResponcePtr.release());
  arangodb::RestUsersHandler revokeWildcardHandler(revokeWildcardRequestPtr.release(), revokeWildcardResponcePtr.release());

  grantRequest.addSuffix("testUser");
  grantRequest.addSuffix("database");
  grantRequest.addSuffix(vocbase->name());
  grantRequest.addSuffix("testDataSource");
  grantRequest.setRequestType(arangodb::rest::RequestType::PUT);
  grantRequest._payload.openObject();
  grantRequest._payload.add("grant", arangodb::velocypack::Value(arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW)));
  grantRequest._payload.close();

  grantWildcardRequest.addSuffix("testUser");
  grantWildcardRequest.addSuffix("database");
  grantWildcardRequest.addSuffix(vocbase->name());
  grantWildcardRequest.addSuffix("*");
  grantWildcardRequest.setRequestType(arangodb::rest::RequestType::PUT);
  grantWildcardRequest._payload.openObject();
  grantWildcardRequest._payload.add("grant", arangodb::velocypack::Value(arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW)));
  grantWildcardRequest._payload.close();

  revokeRequest.addSuffix("testUser");
  revokeRequest.addSuffix("database");
  revokeRequest.addSuffix(vocbase->name());
  revokeRequest.addSuffix("testDataSource");
  revokeRequest.setRequestType(arangodb::rest::RequestType::DELETE_REQ);

  revokeWildcardRequest.addSuffix("testUser");
  revokeWildcardRequest.addSuffix("database");
  revokeWildcardRequest.addSuffix(vocbase->name());
  revokeWildcardRequest.addSuffix("*");
  revokeWildcardRequest.setRequestType(arangodb::rest::RequestType::DELETE_REQ);

  struct ExecContext: public arangodb::ExecContext {
    ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, userName, "",
                                         arangodb::auth::Level::RW, arangodb::auth::Level::NONE) {} // ExecContext::isAdminUser() == true
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
  userManager->setGlobalVersion(0); // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);

  // test auth missing (grant)
  {
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(s.system->createCollection(usersJson->slice()).get(), [&s](arangodb::LogicalCollection* ptr)->void{ s.system->dropCollection(ptr->id(), true, 0.0); });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap); // insure an empy map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty, true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user)->arangodb::Result { userPtr = const_cast<arangodb::auth::User*>(&user); return arangodb::Result(); });
    REQUIRE((nullptr != userPtr));

    CHECK((arangodb::auth::Level::NONE == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = grantHandler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == grantResponce.responseCode()));
    auto slice = grantResponce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    CHECK((arangodb::auth::Level::NONE == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth missing (revoke)
  {
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(s.system->createCollection(usersJson->slice()).get(), [&s](arangodb::LogicalCollection* ptr)->void{ s.system->dropCollection(ptr->id(), true, 0.0); });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap); // insure an empy map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty, true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user)->arangodb::Result { userPtr = const_cast<arangodb::auth::User*>(&user); return arangodb::Result(); });
    REQUIRE((nullptr != userPtr));
    userPtr->grantCollection(vocbase->name(), "testDataSource", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level

    CHECK((arangodb::auth::Level::RO == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = revokeHandler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == revokeResponce.responseCode()));
    auto slice = revokeResponce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    CHECK((arangodb::auth::Level::RO == execContext.collectionAuthLevel(vocbase->name(), "testDataSource"))); // not modified from above
  }

  // test auth collection (grant)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(s.system->createCollection(usersJson->slice()).get(), [&s](arangodb::LogicalCollection* ptr)->void{ s.system->dropCollection(ptr->id(), true, 0.0); });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap); // insure an empy map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty, true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user)->arangodb::Result { userPtr = const_cast<arangodb::auth::User*>(&user); return arangodb::Result(); });
    REQUIRE((nullptr != userPtr));
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(vocbase->createCollection(collectionJson->slice()).get(), [vocbase](arangodb::LogicalCollection* ptr)->void{ vocbase->dropCollection(ptr->id(), false, 0); });
    REQUIRE((false == !logicalCollection));

    CHECK((arangodb::auth::Level::NONE == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = grantHandler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == grantResponce.responseCode()));
    auto slice = grantResponce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(vocbase->name() + "/testDataSource") && slice.get(vocbase->name() + "/testDataSource").isString() && arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW) == slice.get(vocbase->name() + "/testDataSource").copyString()));
    CHECK((arangodb::auth::Level::RW == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth collection (revoke)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(s.system->createCollection(usersJson->slice()).get(), [&s](arangodb::LogicalCollection* ptr)->void{ s.system->dropCollection(ptr->id(), true, 0.0); });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap); // insure an empy map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty, true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user)->arangodb::Result { userPtr = const_cast<arangodb::auth::User*>(&user); return arangodb::Result(); });
    REQUIRE((nullptr != userPtr));
    userPtr->grantCollection(vocbase->name(), "testDataSource", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(vocbase->createCollection(collectionJson->slice()).get(), [vocbase](arangodb::LogicalCollection* ptr)->void{ vocbase->dropCollection(ptr->id(), false, 0); });
    REQUIRE((false == !logicalCollection));

    CHECK((arangodb::auth::Level::RO == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = revokeHandler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::ACCEPTED == revokeResponce.responseCode()));
    auto slice = revokeResponce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::ACCEPTED) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((arangodb::auth::Level::NONE == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth view (grant)
  {
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testDataSource\", \"type\": \"testViewType\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(s.system->createCollection(usersJson->slice()).get(), [&s](arangodb::LogicalCollection* ptr)->void{ s.system->dropCollection(ptr->id(), true, 0.0); });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap); // insure an empy map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty, true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user)->arangodb::Result { userPtr = const_cast<arangodb::auth::User*>(&user); return arangodb::Result(); });
    REQUIRE((nullptr != userPtr));
    auto logicalView = std::shared_ptr<arangodb::LogicalView>(vocbase->createView(viewJson->slice()).get(), [vocbase](arangodb::LogicalView* ptr)->void{ vocbase->dropView(ptr->id(), false); });
    REQUIRE((false == !logicalView));

    CHECK((arangodb::auth::Level::NONE == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = grantHandler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == grantResponce.responseCode()));
    auto slice = grantResponce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    CHECK((arangodb::auth::Level::NONE == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth view (revoke)
  {
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testDataSource\", \"type\": \"testViewType\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(s.system->createCollection(usersJson->slice()).get(), [&s](arangodb::LogicalCollection* ptr)->void{ s.system->dropCollection(ptr->id(), true, 0.0); });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap); // insure an empy map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty, true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user)->arangodb::Result { userPtr = const_cast<arangodb::auth::User*>(&user); return arangodb::Result(); });
    REQUIRE((nullptr != userPtr));
    userPtr->grantCollection(vocbase->name(), "testDataSource", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
    auto logicalView = std::shared_ptr<arangodb::LogicalView>(vocbase->createView(viewJson->slice()).get(), [vocbase](arangodb::LogicalView* ptr)->void{ vocbase->dropView(ptr->id(), false); });
    REQUIRE((false == !logicalView));

    CHECK((arangodb::auth::Level::RO == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = revokeHandler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == revokeResponce.responseCode()));
    auto slice = revokeResponce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    CHECK((arangodb::auth::Level::RO == execContext.collectionAuthLevel(vocbase->name(), "testDataSource"))); // not modified from above
  }

  // test auth wildcard (grant)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(s.system->createCollection(usersJson->slice()).get(), [&s](arangodb::LogicalCollection* ptr)->void{ s.system->dropCollection(ptr->id(), true, 0.0); });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap); // insure an empy map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty, true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user)->arangodb::Result { userPtr = const_cast<arangodb::auth::User*>(&user); return arangodb::Result(); });
    REQUIRE((nullptr != userPtr));
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(vocbase->createCollection(collectionJson->slice()).get(), [vocbase](arangodb::LogicalCollection* ptr)->void{ vocbase->dropCollection(ptr->id(), false, 0); });
    REQUIRE((false == !logicalCollection));

    CHECK((arangodb::auth::Level::NONE == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = grantWildcardHandler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == grantWildcardResponce.responseCode()));
    auto slice = grantWildcardResponce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(vocbase->name() + "/*") && slice.get(vocbase->name() + "/*").isString() && arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW) == slice.get(vocbase->name() + "/*").copyString()));
    CHECK((arangodb::auth::Level::RW == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth wildcard (revoke)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(s.system->createCollection(usersJson->slice()).get(), [&s](arangodb::LogicalCollection* ptr)->void{ s.system->dropCollection(ptr->id(), true, 0.0); });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap); // insure an empy map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty, true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user)->arangodb::Result { userPtr = const_cast<arangodb::auth::User*>(&user); return arangodb::Result(); });
    REQUIRE((nullptr != userPtr));
    userPtr->grantCollection(vocbase->name(), "testDataSource", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(vocbase->createCollection(collectionJson->slice()).get(), [vocbase](arangodb::LogicalCollection* ptr)->void{ vocbase->dropCollection(ptr->id(), false, 0); });
    REQUIRE((false == !logicalCollection));

    CHECK((arangodb::auth::Level::RO == execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = revokeWildcardHandler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::ACCEPTED == revokeWildcardResponce.responseCode()));
    auto slice = revokeWildcardResponce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::ACCEPTED) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((arangodb::auth::Level::RO == execContext.collectionAuthLevel(vocbase->name(), "testDataSource"))); // unchanged since revocation is only for exactly matching collection names
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------