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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_TESTS_MOCKS_SERVERS_H
#define ARANGODB_TESTS_MOCKS_SERVERS_H 1

#include "StorageEngineMock.h"

#include "Agency/Store.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"

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

class MockServer {
 protected:
  // You can only create specialized types
  MockServer();
  virtual ~MockServer();

 public:
  void init();

  TRI_vocbase_t& getSystemDatabase() const;
  std::string const testFilesystemPath() const {
    return _testFilesystemPath;
  }

 protected:
  // Implementation knows the place when all features are included
  virtual void startFeatures();

 private:
  // Will be called by destructor
  void stopFeatures();

 protected:

  arangodb::application_features::ApplicationServer::State
    _oldApplicationServerState;
  arangodb::application_features::ApplicationServer _server;
  StorageEngineMock _engine;
  std::unordered_map<arangodb::application_features::ApplicationFeature*, bool> _features;
  std::string _testFilesystemPath;

};

class MockAqlServer : public MockServer {
 public:
  MockAqlServer();
  ~MockAqlServer();

  std::shared_ptr<arangodb::transaction::Methods> createFakeTransaction() const;
  std::unique_ptr<arangodb::aql::Query> createFakeQuery() const;
};

class MockRestServer : public MockServer {
 public:
  MockRestServer();
  ~MockRestServer();
};

class MockClusterServer : public MockServer {
 public:
  virtual TRI_vocbase_t* createDatabase(std::string const& name) = 0;
  virtual void dropDatabase(std::string const& name) = 0;
  arangodb::consensus::Store& getAgencyStore() { return _agencyStore; };

  // You can only create specialized types
 protected:
  MockClusterServer();
  ~MockClusterServer();

 protected:
  // Implementation knows the place when all features are included
  void startFeatures() override;
  void agencyTrx(std::string const& key, std::string const& value);
  void agencyCreateDatabase(std::string const& name);
  void agencyDropDatabase(std::string const& name);

 private:
  arangodb::consensus::Store _agencyStore;
  arangodb::ServerState::RoleEnum _oldRole;
};

class MockDBServer : public MockClusterServer {
 public:
  MockDBServer();
  ~MockDBServer();

  TRI_vocbase_t* createDatabase(std::string const& name) override;
  void dropDatabase(std::string const& name) override;
};

class MockCoordinator : public MockClusterServer {
 public:
  MockCoordinator();
  ~MockCoordinator();

  TRI_vocbase_t* createDatabase(std::string const& name) override;
  void dropDatabase(std::string const& name) override;
};

}  // namespace mocks
}  // namespace tests
}  // namespace arangodb

#endif
