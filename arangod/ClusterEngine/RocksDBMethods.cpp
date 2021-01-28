////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

namespace arangodb::rocksdb {

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the active journals for the collection on all DBServers
////////////////////////////////////////////////////////////////////////////////

Result recalculateCountsOnAllDBServers(application_features::ApplicationServer& server,
                                       std::string const& dbname,
                                       std::string const& collname) {
  ClusterEngine& ce =
      (server.getFeature<EngineSelectorFeature>().engine<ClusterEngine>());
  if (!ce.isRocksDB()) {
    return TRI_ERROR_NOT_IMPLEMENTED;
  }

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

  struct Collector {
    explicit Collector(futures::Promise<Result> result) : result(std::move(result)) {}
    futures::Promise<Result> result;
    std::mutex mutex;

    ~Collector() {
      if (!result.empty()) {
        std::move(result).fulfill(std::in_place);
      }
    }

    void onResult(futures::Try<network::Response> tryRes) noexcept {
      std::unique_lock guard(mutex);
      try {
        if (!result.empty()) {
          if (tryRes.has_error()) {
            return std::move(result).fulfill(tryRes.error());
          }
          auto res = tryRes.unwrap().combinedResult();
          if (res.fail()) {
            std::move(result).fulfill(std::in_place, std::move(res));
          }
        }
      } catch (...) {
        std::move(result).fulfill(std::current_exception());
      }
    }
  };

  auto&& [f, p] = futures::makePromise<Result>();
  auto collect = std::make_shared<Collector>(std::move(p));

  try {
    // now we notify all leader and follower shards
    std::shared_ptr<ShardMap> shardList = collinfo->shardIds();
    for (auto const& shard : *shardList) {
      for (ServerID const& serverId : shard.second) {
        std::string uri = baseUrl + basics::StringUtils::urlEncode(shard.first) +
                          "/recalculateCount";
        auto f = network::sendRequest(pool, "server:" + serverId, fuerte::RestVerb::Put,
                                      std::move(uri), body, options, headers);
        std::move(f).finally([collect](futures::Try<network::Response>&& result) noexcept {
          collect->onResult(std::move(result));
        });
      }
    }
  } catch (...) {
    // ignore all responses
    std::move(f).finally([](auto&&) noexcept {});
    throw;
  }

  return std::move(f).await_unwrap();
}

}  // namespace arangodb
