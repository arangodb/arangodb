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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterTtlMethods.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Futures/Utilities.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "RestServer/TtlFeature.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <vector>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::futures;
using namespace arangodb::rest;

namespace arangodb {

/// @brief get TTL statistics from all DBservers and aggregate them
Result getTtlStatisticsFromAllDBServers(ClusterFeature& feature, TtlStatistics& out) {
  ClusterInfo& ci = feature.clusterInfo();

  std::vector<ServerID> DBservers = ci.getCurrentDBServers();
  std::string const url("/_api/ttl/statistics");

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();
  std::vector<Future<network::Response>> futures;
  futures.reserve(DBservers.size());
  for (std::string const& server : DBservers) {
    futures.emplace_back(
        network::sendRequestRetry(pool, "server:" + server, fuerte::RestVerb::Get,
                                  url, VPackBufferUInt8()));
  }

  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.get();
    
    if (r.fail()) {
      return network::fuerteToArangoErrorCode(r);
    }
    if (r.response->statusCode() == fuerte::StatusOK) {
      out += r.slice().get("result");
    } else {
      int code = network::errorCodeFromBody(r.slice());
      if (code != TRI_ERROR_NO_ERROR) {
        return code;
      }
    }
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief get TTL properties from all DBservers
Result getTtlPropertiesFromAllDBServers(ClusterFeature& feature, VPackBuilder& out) {
  ClusterInfo& ci = feature.clusterInfo();

  std::vector<ServerID> DBservers = ci.getCurrentDBServers();
  std::string const url("/_api/ttl/properties");

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();
  std::vector<Future<network::Response>> futures;
  futures.reserve(DBservers.size());
  for (std::string const& server : DBservers) {
    futures.emplace_back(
        network::sendRequestRetry(pool, "server:" + server, fuerte::RestVerb::Get,
                                  url, VPackBufferUInt8()));
  }

  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.get();
    
    if (r.fail()) {
      return network::fuerteToArangoErrorCode(r);
    }
    if (r.response->statusCode() == fuerte::StatusOK) {
      out.add(r.slice().get("result"));
      break;
    } else {
      int code = network::errorCodeFromBody(r.slice());
      if (code != TRI_ERROR_NO_ERROR) {
        return code;
      }
    }
  }
  return TRI_ERROR_NO_ERROR;
}

/// @brief set TTL properties on all DBservers
Result setTtlPropertiesOnAllDBServers(ClusterFeature& feature,
                                      VPackSlice properties, VPackBuilder& out) {
  ClusterInfo& ci = feature.clusterInfo();

  std::vector<ServerID> DBservers = ci.getCurrentDBServers();
  std::string const url("/_api/ttl/properties");

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();
  std::vector<Future<network::Response>> futures;
  futures.reserve(DBservers.size());

  VPackBufferUInt8 buffer;
  buffer.append(properties.begin(), properties.byteSize());
  for (std::string const& server : DBservers) {
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + server,
                                                   fuerte::RestVerb::Put, url,
                                                   buffer));
  }

  for (Future<network::Response>& f : futures) {
    network::Response const& r = f.get();
    
    if (r.fail()) {
      return network::fuerteToArangoErrorCode(r);
    }

    if (r.response->statusCode() == fuerte::StatusOK) {
      out.add(r.slice().get("result"));
      break;
    } else {
      int code = network::errorCodeFromBody(r.slice());
      if (code != TRI_ERROR_NO_ERROR) {
        return code;
      }
    }
  }
  return TRI_ERROR_NO_ERROR;
}

}  // namespace arangodb
