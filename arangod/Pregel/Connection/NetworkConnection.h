////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include "Cluster/ClusterTypes.h"
#include "Network/ConnectionPool.h"
#include "Network/Methods.h"
#include "Pregel/Connection/Connection.h"
#include "VocBase/vocbase.h"

namespace arangodb::pregel {

struct NetworkConnection : Connection {
 public:
  NetworkConnection(std::string baseUrl, network::RequestOptions requestOptions,
                    TRI_vocbase_t& vocbase);
  auto send(Destination const& destination, ModernMessage&& message)
      -> futures::Future<ResultT<ModernMessage>> override;
  auto sendWithoutRetry(Destination const& destination, ModernMessage&& message)
      -> futures::Future<Result>;
  ~NetworkConnection() = default;

 private:
  std::string _baseUrl;
  network::RequestOptions _requestOptions;
  network::ConnectionPool* _connectionPool;
};

}  // namespace arangodb::pregel
