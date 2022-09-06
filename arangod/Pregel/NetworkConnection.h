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
#include <string_view>
#include "Basics/ResultT.h"
#include "Cluster/ClusterTypes.h"
#include "Futures/Future.h"
#include "Network/ConnectionPool.h"
#include "velocypack/Builder.h"
#include "Network/Methods.h"

namespace arangodb::pregel {

template<typename T>
concept RestMessage = requires(T a) {
  { a.path } -> std::convertible_to<std::string>;
};

struct Connection {
 public:
  static auto create(ServerID const& destinationId, std::string const& baseUrl,
                     TRI_vocbase_t& vocbase) -> Connection;
  Connection(ServerID destinationId, std::string baseUrl,
             network::RequestOptions requestOptions,
             network::ConnectionPool* connectionPool);
  template<typename OutType, RestMessage InType>
  auto post(InType const& message) -> futures::Future<ResultT<OutType>>;

 private:
  ServerID _destinationId;
  std::string _baseUrl;
  network::RequestOptions _requestOptions;
  network::ConnectionPool* _connectionPool;
};

}  // namespace arangodb::pregel
