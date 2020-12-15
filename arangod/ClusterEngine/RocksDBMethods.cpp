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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBMethods.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Futures/Utilities.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
namespace rocksdb {

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the active journals for the collection on all DBServers
////////////////////////////////////////////////////////////////////////////////

Result recalculateCountsOnAllDBServers(std::string const& dbname, std::string const& collname) {
  ClusterEngine* ce = static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);
  if (!ce->isRocksDB()) {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }

  auto& server = ce->server();
  // Set a few variables needed for our work:
  NetworkFeature const& nf = server.getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }
  ClusterInfo& ci = server.getFeature<ClusterFeature>().clusterInfo();

  // First determine the collection ID from the name:
  auto collinfo = ci.getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  std::string const baseUrl = "/_api/collection/";

  VPackBuffer<uint8_t> body;
  VPackBuilder builder(body);
  builder.add(VPackSlice::emptyObjectSlice());

  network::Headers headers;
  network::RequestOptions options;
  options.database = dbname;
  options.timeout = network::Timeout(600.0);

  // now we notify all leader and follower shards
  std::shared_ptr<ShardMap> shardList = collinfo->shardIds();
  std::vector<network::FutureRes> futures;
  for (auto const& shard : *shardList) {
    for (ServerID const& serverId : shard.second) {
      std::string uri = baseUrl + basics::StringUtils::urlEncode(shard.first) +
                        "/recalculateCount";
      auto f = network::sendRequest(pool, "server:" + serverId, fuerte::RestVerb::Put,
                                    std::move(uri), body, options, headers);
      futures.emplace_back(std::move(f));
    }
  }

  auto responses = futures::collectAll(futures).get();
  for (auto const& r : responses) {
    Result res = r.get().combinedResult();
    if (res.fail()) {
      return res;
    }
  }

  return {};
}

}  // namespace rocksdb
}  // namespace arangodb
