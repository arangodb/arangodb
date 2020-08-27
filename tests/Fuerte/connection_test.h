////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once 

#include <algorithm>
#include <stdlib.h>

#include <fuerte/fuerte.h>
#include <fuerte/loop.h>
#include <fuerte/helper.h>

#include "gtest/gtest.h"

#include "common.h"

namespace fu = ::arangodb::fuerte;

struct ConnectionTestParams {
  const fu::ProtocolType _protocol;
  const size_t _threads;  // #Threads to use for the EventLoopService 
  const size_t _repeat;   // Number of times to repeat repeatable tests.
};

/*::std::ostream& operator<<(::std::ostream& os, const ConnectionTestParams& p) {
  return os << "url=" << p._url << " threads=" << p._threads;
}*/

// ConnectionTestF is a test fixture that can be used for all kinds of connection 
// tests. You can configure it using the ConnectionTestParams struct.
class ConnectionTestF : public ::testing::TestWithParam<ConnectionTestParams> {
 public:
  const char _major_arango_version = '3';
 protected:
  ConnectionTestF() {}
  virtual ~ConnectionTestF() noexcept {}

  virtual void SetUp() override {
    try {
      // make connection
      _connection = createConnection();
    } catch(std::exception const& ex) {
      std::cout << "SETUP OF FIXTURE FAILED" << std::endl;
      throw ex;
    }
  }

  std::shared_ptr<fu::Connection> createConnection() {
    // Set connection parameters
    fu::ConnectionBuilder cbuilder;
    setupEndpointFromEnv(cbuilder);
    cbuilder.protocolType(GetParam()._protocol);
    setupAuthenticationFromEnv(cbuilder);
    return cbuilder.connect(_eventLoopService);
  }
  
  virtual void TearDown() override {
    _connection.reset();
  }

  inline size_t threads() const {
    return std::max(GetParam()._threads, size_t(1));
  }
  // Number of times to repeat certain tests.
  inline size_t repeat() const {
    return std::max(GetParam()._repeat, size_t(1));
  }
  
  fu::StatusCode createCollection(std::string const& name) {
    // create the collection
    VPackBuilder builder;
    builder.openObject();
    builder.add("name", VPackValue(name));
    builder.close();
    auto request = fu::createRequest(fu::RestVerb::Post, "/_api/collection");
    request->addVPack(builder.slice());
    auto response = _connection->sendRequest(std::move(request));
    return response->statusCode();
  }
  
  fu::StatusCode dropCollection(std::string const& name) {
    auto request = fu::createRequest(fu::RestVerb::Delete, "/_api/collection/" + name);
    auto response = _connection->sendRequest(std::move(request));
    return response->statusCode();
  }

  std::shared_ptr<fu::Connection> _connection;

 private:
  fu::EventLoopService _eventLoopService;
};

