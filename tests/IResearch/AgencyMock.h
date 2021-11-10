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

#ifndef ARANGODB_IRESEARCH__IRESEARCH_AGENCY_MOCK_H
#define ARANGODB_IRESEARCH__IRESEARCH_AGENCY_MOCK_H 1

#include <fuerte/connection.h>

#include "Basics/debugging.h"
#include "Cluster/AgencyCache.h"
#include "Network/ConnectionPool.h"
#include "RestServer/MetricsFeature.h"

namespace arangodb::fuerte {
inline namespace v1 {
class ConnectionBuilder;
}
}

struct AsyncAgencyStorePoolConnection final : public arangodb::fuerte::Connection {
  AsyncAgencyStorePoolConnection(arangodb::AgencyCache& cache, std::string endpoint);

  std::size_t requestsLeft() const override { return 1; }
  State state() const override;

  void cancel() override;

  auto handleRead(VPackSlice body) -> std::unique_ptr<arangodb::fuerte::Response>;
  auto handleWrite(VPackSlice body) -> std::unique_ptr<arangodb::fuerte::Response>;
  void sendRequest(std::unique_ptr<arangodb::fuerte::Request> req,
                   arangodb::fuerte::RequestCallback cb) override;

  arangodb::AgencyCache& _cache;
  std::string _endpoint;
};

struct AsyncAgencyStorePoolMock final : public arangodb::network::ConnectionPool {

  explicit AsyncAgencyStorePoolMock(
    arangodb::application_features::ApplicationServer& server, ConnectionPool::Config const& config)
    : ConnectionPool(config), _server(server), _index(0) {}

  explicit AsyncAgencyStorePoolMock(arangodb::application_features::ApplicationServer& server)
    : ConnectionPool(server.getFeature<arangodb::MetricsFeature>()), _server(server), _index(0) {}

  std::shared_ptr<arangodb::fuerte::Connection> createConnection(
    arangodb::fuerte::ConnectionBuilder&) override;

  arangodb::application_features::ApplicationServer& _server;
  arangodb::consensus::index_t _index;

};

#endif
