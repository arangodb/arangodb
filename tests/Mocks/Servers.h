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

#include "ApplicationFeatures/ApplicationServer.h"
#include "StorageEngineMock.h"

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
 public:
  MockServer();
  virtual ~MockServer();

  void init();

  TRI_vocbase_t& getSystemDatabase() const;

 protected:
  // Implementation knows the place when all features are included
  void startFeatures();

 private:
  // Will be called by destructor
  void stopFeatures();

 protected:
  arangodb::application_features::ApplicationServer _server;
  StorageEngineMock _engine;
  std::unique_ptr<TRI_vocbase_t> _system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> _features;
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

}  // namespace mocks
}  // namespace tests
}  // namespace arangodb

#endif
