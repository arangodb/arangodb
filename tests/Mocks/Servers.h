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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/AgencyMock.h"
#include "StorageEngineMock.h"

#include "Mocks/LogLevels.h"

#include "Agency/Store.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchCommon.h"
#include "Logger/LogMacros.h"

struct TRI_vocbase_t;

namespace arangodb {

namespace transaction {
class Methods;
}

namespace aql {
class Query;
}

namespace application_features {
class ApplicationFeature;
}

namespace tests {
namespace mocks {

/// @brief mock application server with no features added
class MockServer {
 public:
  // Note, setting injectClusterIndexes causes the "create" methods to fail.
  // this is all hardly worked around the Cluster engine and needs a proper
  // clean up. It is highly recommended to not set injectClusterIndexes unless
  // you want to specificly test something that selects an index, but cannot use
  // it. Use with care for now.
  MockServer(arangodb::ServerState::RoleEnum =
                 arangodb::ServerState::RoleEnum::ROLE_SINGLE,
             bool injectClusterIndexes = false);
  virtual ~MockServer();

  application_features::ApplicationServer& server();
  void init();

  TRI_vocbase_t& getSystemDatabase() const;
  std::string const testFilesystemPath() const { return _testFilesystemPath; }

  // add a feature to the underlying server, keep track of it;
  // all added features will be prepared in startFeatures(), and unprepared in
  // stopFeatures(); if start == true, then it will also be started and stopped
  // in those methods; after startFeatures() is called, this method can no
  // longer be called, and additional features must be added via
  // addFeatureUntracked(), and will not be managed by this class
  template<typename Type, typename As = Type, typename... Args>
  As& addFeature(bool start, Args&&... args) {
    TRI_ASSERT(!_started);
    As& feature = _server.addFeature<Type, As>(std::forward<Args>(args)...);
    _features.emplace(&feature, start);
    return feature;
  }

  // add a feature to the underlying server, but do not track it;
  // it will not be prepared, started, etc.
  template<typename Type, typename As = Type, typename... Args>
  As& addFeatureUntracked(Args&&... args) {
    return _server.addFeature<Type, As>(std::forward<Args>(args)...);
  }

  // make previously added feature untracked.
  // useful for successors of base mock servers
  // that want to exclude some standart features from
  // bootstrapping
  template<typename Type>
  void untrackFeature() {
    _features.erase(&getFeature<Type>());
  }

  // convenience method to fetch feature, equivalent to server().getFeature....
  template<typename T>
  T& getFeature() {
    return _server.getFeature<T>();
  }

  // Implementation knows the place when all features are included
  virtual void startFeatures();

 private:
  // Will be called by destructor
  void stopFeatures();

 protected:
  arangodb::application_features::ApplicationServer::State
      _oldApplicationServerState = arangodb::application_features::
          ApplicationServer::State::UNINITIALIZED;
  arangodb::application_features::ApplicationServer _server;
  StorageEngineMock _engine;
  std::unordered_map<arangodb::application_features::ApplicationFeature*, bool>
      _features;
  std::string _testFilesystemPath;
  arangodb::RebootId _oldRebootId;

 private:
  bool _started;
  arangodb::ServerState::RoleEnum _oldRole;
  bool _originalMockingState;
};

/// @brief a server with almost no features added (Metrics are available
/// though)
class MockMetricsServer
    : public MockServer,
      public LogSuppressor<Logger::AUTHENTICATION, LogLevel::WARN>,
      public LogSuppressor<Logger::FIXME, LogLevel::ERR>,
      public LogSuppressor<iresearch::TOPIC, LogLevel::FATAL>,
      public IResearchLogSuppressor {
 public:
  MockMetricsServer(bool startFeatures = true);
};

/// @brief a server with features added that allow to execute V8 code
/// and bindings
class MockV8Server
    : public MockServer,
      public LogSuppressor<Logger::AUTHENTICATION, LogLevel::WARN>,
      public LogSuppressor<Logger::FIXME, LogLevel::ERR>,
      public LogSuppressor<iresearch::TOPIC, LogLevel::FATAL>,
      public IResearchLogSuppressor {
 public:
  MockV8Server(bool startFeatures = true);
  ~MockV8Server();
};

/// @brief a server with features added that allow to execute AQL queries
class MockAqlServer
    : public MockServer,
      public LogSuppressor<Logger::AUTHENTICATION, LogLevel::WARN>,
      public LogSuppressor<Logger::CLUSTER, LogLevel::ERR>,
      public LogSuppressor<Logger::FIXME, LogLevel::ERR>,
      public LogSuppressor<iresearch::TOPIC, LogLevel::FATAL>,
      public IResearchLogSuppressor {
 public:
  explicit MockAqlServer(bool startFeatures = true);
  ~MockAqlServer() override;

  std::shared_ptr<arangodb::transaction::Methods> createFakeTransaction() const;
  // runBeforePrepare gives an entry point to modify the list of collections one
  // want to use within the Query.
  std::shared_ptr<arangodb::aql::Query> createFakeQuery(
      bool activateTracing = false, std::string queryString = "",
      std::function<void(arangodb::aql::Query&)> runBeforePrepare =
          [](arangodb::aql::Query&) {}) const;
};

/// @brief a server with features added that allow to execute RestHandler
/// code
class MockRestServer
    : public MockServer,
      public LogSuppressor<Logger::AUTHENTICATION, LogLevel::WARN>,
      public LogSuppressor<Logger::FIXME, LogLevel::ERR>,
      public LogSuppressor<iresearch::TOPIC, LogLevel::FATAL>,
      public IResearchLogSuppressor {
 public:
  explicit MockRestServer(bool startFeatures = true);
};

class MockRestAqlServer
    : public MockServer,
      public LogSuppressor<Logger::AUTHENTICATION, LogLevel::WARN>,
      public LogSuppressor<Logger::CLUSTER, LogLevel::ERR>,
      public LogSuppressor<Logger::FIXME, LogLevel::ERR>,
      public LogSuppressor<iresearch::TOPIC, LogLevel::FATAL>,
      public IResearchLogSuppressor {
 public:
  explicit MockRestAqlServer();
};

class MockClusterServer
    : public MockServer,
      public LogSuppressor<Logger::AGENCY, LogLevel::FATAL>,
      public LogSuppressor<Logger::AUTHENTICATION, LogLevel::ERR>,
      public LogSuppressor<Logger::CLUSTER, LogLevel::WARN>,
      public LogSuppressor<iresearch::TOPIC, LogLevel::FATAL>,
      public IResearchLogSuppressor {
 public:
  virtual TRI_vocbase_t* createDatabase(std::string const& name) = 0;
  virtual void dropDatabase(std::string const& name) = 0;
  void startFeatures() override;

  // Create a clusterWide Collection.
  // This does NOT create Shards.
  std::shared_ptr<LogicalCollection> createCollection(
      std::string const& dbName, std::string collectionName,
      std::vector<std::pair<std::string, std::string>>
          shardNameToServerNamePairs,
      TRI_col_type_e type,
      VPackSlice additionalProperties = VPackSlice{VPackSlice::nullSlice()});

#ifdef USE_ENTERPRISE
  std::shared_ptr<LogicalCollection> createSmartCollection(
      std::string const& dbName, std::string collectionName,
      std::vector<std::pair<std::string, std::string>>
          shardNameToServerNamePairs,
      TRI_col_type_e type,
      VPackSlice additionalProperties = VPackSlice{VPackSlice::nullSlice()});
#endif

  void buildCollectionProperties(
      VPackBuilder& props, std::string const& collectionName,
      std::string const& cid, TRI_col_type_e type,
      std::vector<std::pair<std::string, std::string>> const&
          shardNameToServerNamePairs,
      VPackSlice additionalProperties);

  void injectCollectionToAgency(std::string const& dbName, VPackBuilder& velocy,
                                DataSourceId const& planId,
                                std::vector<std::pair<std::string, std::string>>
                                    shardNameToServerNamePairs);

  std::shared_ptr<arangodb::aql::Query> createFakeQuery(
      bool activateTracing = false, std::string queryString = "",
      std::function<void(arangodb::aql::Query&)> runBeforePrepare =
          [](arangodb::aql::Query&) {}) const;
  // You can only create specialized types
 protected:
  MockClusterServer(bool useAgencyMockConnection,
                    arangodb::ServerState::RoleEnum role,
                    bool injectClusterIndexes = false);
  ~MockClusterServer();

 protected:
  // Implementation knows the place when all features are included
  consensus::index_t agencyTrx(std::string const& key,
                               std::string const& value);
  void agencyCreateDatabase(std::string const& name);

  // creation of collection is separated
  // as for DBerver at first maintenance should
  // create database and only after collections
  // will be populated in plan.
  void agencyCreateCollections(std::string const& name);
  void agencyDropDatabase(std::string const& name);

 protected:
  std::unique_ptr<arangodb::network::ConnectionPool> _pool;

 private:
  bool _useAgencyMockPool;
  int _dummy;
};

class MockDBServer : public MockClusterServer {
 public:
  MockDBServer(bool startFeatures = true, bool useAgencyMockConnection = true);
  ~MockDBServer();

  TRI_vocbase_t* createDatabase(std::string const& name) override;
  void dropDatabase(std::string const& name) override;

  void createShard(std::string const& dbName, std::string shardName,
                   LogicalCollection& clusterCollection);
};

class MockCoordinator : public MockClusterServer {
 public:
  MockCoordinator(bool startFeatures = true,
                  bool useAgencyMockConnection = true,
                  bool injectClusterIndexes = false);
  ~MockCoordinator();

  TRI_vocbase_t* createDatabase(std::string const& name) override;
  void dropDatabase(std::string const& name) override;

  std::pair<std::string, std::string> registerFakedDBServer(
      std::string const& serverName);

  arangodb::network::ConnectionPool* getPool();
};

}  // namespace mocks
}  // namespace tests
}  // namespace arangodb
