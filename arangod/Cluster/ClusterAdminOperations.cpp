////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ClusterAdminOperations.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Async/async.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterTypes.h"
#include "VocBase/LogicalCollection.h"
#include "Futures/Utilities.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::futures;

namespace arangodb {

/// @brief flush WAL on all DBservers
Result flushWalOnAllDBServers(ClusterFeature& feature, bool waitForSync,
                              bool flushColumnFamilies) {
  ClusterInfo& ci = feature.clusterInfo();

  std::vector<ServerID> DBservers = ci.getCurrentDBServers();

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue
  reqOpts
      .param(StaticStrings::WaitForSyncString, (waitForSync ? "true" : "false"))
      .param("waitForCollector", (flushColumnFamilies ? "true" : "false"));

  std::vector<Future<network::Response>> futures;
  futures.reserve(DBservers.size());

  VPackBufferUInt8 buffer;
  buffer.append(VPackSlice::noneSlice().begin(),
                1);  // necessary for some reason
  for (std::string const& server : DBservers) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + server, fuerte::RestVerb::Put, "/_admin/wal/flush",
        buffer, reqOpts));
  }

  for (Future<network::Response>& f : futures) {
    Result res = f.waitAndGet().combinedResult();
    if (res.fail()) {
      return res;
    }
  }
  return {};
}

// recalculate counts on all DB servers
Result recalculateCountsOnAllDBServers(ClusterFeature& feature,
                                       std::string_view dbname,
                                       std::string_view collname) {
  // Set a few variables needed for our work:
  NetworkFeature const& nf = feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }
  ClusterInfo& ci = feature.clusterInfo();

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
      std::string uri = baseUrl + shard.first + "/recalculateCount";
      auto f = network::sendRequestRetry(pool, "server:" + serverId,
                                         fuerte::RestVerb::Put, std::move(uri),
                                         body, options, headers);
      futures.emplace_back(std::move(f));
    }
  }

  auto responses = futures::collectAll(futures).waitAndGet();
  for (auto const& r : responses) {
    Result res = r.get().combinedResult();
    if (res.fail()) {
      return res;
    }
  }

  return {};
}

// Per-shard autotune draws a fresh sample and sweeps nprobe via FAISS, which
// can take minutes on large shards. Allow a generous window before the
// coordinator gives up on a shard.
constexpr double kAutoTuneRequestTimeoutSecs = 600.0;

// autotune a vector index on all shards (every replica), collecting per-shard
// outcomes without failing fast.
Result autoTuneVectorIndexOnAllDBServers(ClusterFeature& feature,
                                         std::string const& dbname,
                                         std::string const& collname,
                                         std::string const& indexId,
                                         velocypack::Slice params,
                                         VPackBuilder& result) {
  NetworkFeature const& nf = feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }
  ClusterInfo& ci = feature.clusterInfo();

  auto collinfo = ci.getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  // Forward the tuning parameters verbatim to every shard; default to an empty
  // object when the caller sent no usable body.
  VPackBuffer<uint8_t> body;
  if (params.isObject()) {
    body.append(params.start(), params.byteSize());
  } else {
    VPackSlice const empty = VPackSlice::emptyObjectSlice();
    body.append(empty.start(), empty.byteSize());
  }

  network::Headers headers;
  network::RequestOptions options;
  options.database = dbname;
  options.timeout = network::Timeout(kAutoTuneRequestTimeoutSecs);

  struct Target {
    std::string shard;
    std::string server;
  };
  std::vector<Target> targets;
  std::shared_ptr<ShardMap> shardList = collinfo->shardIds();
  std::vector<network::FutureRes> futures;
  for (auto const& shard : *shardList) {
    for (ServerID const& serverId : shard.second) {
      std::string uri = absl::StrCat("/_api/index/", std::string{shard.first},
                                     "/", indexId, "/autotune");
      futures.emplace_back(network::sendRequestRetry(
          pool, "server:" + serverId, fuerte::RestVerb::Post, std::move(uri),
          body, options, headers));
      targets.push_back({std::string{shard.first}, serverId});
    }
  }

  // collectAll never rejects: every shard outcome is reported, none aborts.
  auto responses = futures::collectAll(futures).waitAndGet();

  result.openArray();
  for (std::size_t i = 0; i < responses.size(); ++i) {
    network::Response const& r = responses[i].get();
    VPackObjectBuilder o(&result);
    result.add("shard", VPackValue(targets[i].shard));
    result.add("server", VPackValue(targets[i].server));
    if (Result const res = r.combinedResult(); res.fail()) {
      result.add(StaticStrings::Error, VPackValue(true));
      result.add(StaticStrings::ErrorNum, VPackValue(res.errorNumber()));
      result.add(StaticStrings::ErrorMessage, VPackValue(res.errorMessage()));
    } else {
      result.add(StaticStrings::Error, VPackValue(false));
      if (VPackSlice const slice = r.slice(); slice.isObject()) {
        for (auto const* field :
             {"topK", "minRecall", "operatingPointCount", "reachedMinRecall"}) {
          if (auto s = slice.get(field); !s.isNone()) {
            result.add(field, s);
          }
        }
      }
    }
  }
  result.close();

  return {};
}

async<Result> getVectorIndexTunedTablesOnAllDBServers(
    ClusterFeature& feature, std::string const& dbname,
    std::string const& collname, std::string const& indexId,
    VPackBuilder& result) {
  NetworkFeature const& nf = feature.server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    // nullptr happens only during controlled shutdown
    co_return Result{TRI_ERROR_SHUTTING_DOWN};
  }
  ClusterInfo& ci = feature.clusterInfo();

  auto collinfo = ci.getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    co_return Result{TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  network::RequestOptions options;
  options.database = dbname;

  struct Target {
    std::string shard;
    std::string server;
  };
  std::vector<Target> targets;
  std::shared_ptr<ShardMap> shardList = collinfo->shardIds();
  std::vector<network::FutureRes> futures;
  for (auto const& shard : *shardList) {
    for (ServerID const& serverId : shard.second) {
      std::string uri = absl::StrCat("/_api/index/", std::string{shard.first},
                                     "/", indexId, "/autotune");
      futures.emplace_back(network::sendRequestRetry(
          pool, "server:" + serverId, fuerte::RestVerb::Get, std::move(uri),
          VPackBuffer<uint8_t>{}, options));
      targets.push_back({std::string{shard.first}, serverId});
    }
  }

  // Await every shard without blocking a thread; collectAll never rejects, so
  // each outcome is reported individually.
  auto responses = co_await futures::collectAll(futures);

  result.openArray();
  for (std::size_t i = 0; i < responses.size(); ++i) {
    network::Response const& r = responses[i].get();
    VPackObjectBuilder o(&result);
    result.add("shard", VPackValue(targets[i].shard));
    result.add("server", VPackValue(targets[i].server));
    if (Result const res = r.combinedResult(); res.fail()) {
      result.add(StaticStrings::Error, VPackValue(true));
      result.add(StaticStrings::ErrorNum, VPackValue(res.errorNumber()));
      result.add(StaticStrings::ErrorMessage, VPackValue(res.errorMessage()));
    } else {
      result.add(StaticStrings::Error, VPackValue(false));
      if (VPackSlice const slice = r.slice(); slice.isObject()) {
        if (auto s = slice.get("tunedTables"); !s.isNone()) {
          result.add("tunedTables", s);
        }
      }
    }
  }
  result.close();

  co_return Result{};
}

/// @brief compact the entire dataset on all DB servers
Result compactOnAllDBServers(ClusterFeature& feature, bool changeLevel,
                             bool compactBottomMostLevel) {
  ClusterInfo& ci = feature.clusterInfo();

  std::vector<ServerID> DBservers = ci.getCurrentDBServers();

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(3600);
  reqOpts.skipScheduler = true;  // hack to avoid scheduler queue
  reqOpts.param("changeLevel", (changeLevel ? "true" : "false"))
      .param("compactBottomMostLevel",
             (compactBottomMostLevel ? "true" : "false"));

  std::vector<Future<network::Response>> futures;
  futures.reserve(DBservers.size());

  VPackBufferUInt8 buffer;
  VPackSlice s = VPackSlice::emptyObjectSlice();
  buffer.append(s.start(), s.byteSize());
  for (std::string const& server : DBservers) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + server, fuerte::RestVerb::Put, "/_admin/compact",
        buffer, reqOpts));
  }

  for (Future<network::Response>& f : futures) {
    Result res = f.waitAndGet().combinedResult();
    if (res.fail()) {
      return res;
    }
  }
  return {};
}

/// @brief compact the data of a single collection on all DB servers
Result compactOnAllDBServers(ClusterFeature& feature, std::string const& dbname,
                             std::string const& collname) {
  ClusterInfo& ci = feature.clusterInfo();

  // First determine the collection ID from the name:
  auto collinfo = ci.getCollectionNT(dbname, collname);
  if (collinfo == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();

  std::string const baseUrl = "/_api/collection/";

  VPackBuffer<uint8_t> body;
  VPackBuilder builder(body);
  builder.add(VPackSlice::emptyObjectSlice());

  network::Headers headers;
  network::RequestOptions options;
  options.database = dbname;
  options.timeout = network::Timeout(3600.0);

  // now we notify all leader and follower shards
  std::shared_ptr<ShardMap> shardList = collinfo->shardIds();
  std::vector<network::FutureRes> futures;
  for (auto const& shard : *shardList) {
    for (ServerID const& serverId : shard.second) {
      std::string uri =
          absl::StrCat(baseUrl, std::string{shard.first}, "/compact");
      auto f = network::sendRequestRetry(pool, "server:" + serverId,
                                         fuerte::RestVerb::Put, std::move(uri),
                                         body, options, headers);
      futures.emplace_back(std::move(f));
    }
  }

  for (Future<network::Response>& f : futures) {
    Result res = f.waitAndGet().combinedResult();
    if (res.fail()) {
      return res;
    }
  }
  return {};
}

arangodb::Result getEngineStatsFromDBServers(ClusterFeature& feature,
                                             VPackBuilder& report) {
  ClusterInfo& ci = feature.clusterInfo();

  std::vector<ServerID> DBservers = ci.getCurrentDBServers();

  auto* pool = feature.server().getFeature<NetworkFeature>().pool();

  network::RequestOptions reqOpts;
  reqOpts.skipScheduler = true;
  std::vector<Future<network::Response>> futures;
  futures.reserve(DBservers.size());

  for (std::string const& server : DBservers) {
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + server, fuerte::RestVerb::Get, "/_api/engine/stats",
        VPackBuffer<uint8_t>(), reqOpts));
  }

  auto responses = futures::collectAll(std::move(futures)).waitAndGet();

  report.openObject();
  for (auto const& tryRes : responses) {
    network::Response const& r = tryRes.get();

    if (r.fail()) {
      return {network::fuerteToArangoErrorCode(r),
              network::fuerteToArangoErrorMessage(r)};
    }

    // cut off "server:" from the destination
    report.add(r.destination.substr(7), r.slice());
  }
  report.close();

  return {};
}

}  // namespace arangodb
