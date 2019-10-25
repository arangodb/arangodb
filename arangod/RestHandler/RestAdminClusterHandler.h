////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_ADMIN_CLUSTER_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_ADMIN_CLUSTER_HANDLER_H 1

#include "RestHandler/RestVocbaseBaseHandler.h"

namespace arangodb {

class RestAdminClusterHandler : public RestVocbaseBaseHandler {
 public:
  RestAdminClusterHandler(application_features::ApplicationServer&, GeneralRequest*, GeneralResponse*);
  ~RestAdminClusterHandler();

 public:
  RestStatus execute() override;
  char const* name() const override final { return "RestAdminClusterHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }

 private:
  static std::string const Health;
  static std::string const NumberOfServers;
  static std::string const Maintenance;
  static std::string const NodeVersion;
  static std::string const NodeStatistics;
  static std::string const NodeEngine;
  static std::string const Statistics;
  static std::string const ShardDistribution;
  static std::string const CollectionShardDistribution;
  static std::string const CleanoutServer;
  static std::string const ResignLeadership;
  static std::string const MoveShard;
  static std::string const QueryJobStatus;
  static std::string const RemoveServer;

  RestStatus handleHealth();
  RestStatus handleNumberOfServers();
  RestStatus handleMaintenance();

  RestStatus handlePutMaintenance(bool state);
  RestStatus handleGetMaintenance();

  RestStatus handleGetNumberOfServers();
  RestStatus handlePutNumberOfServers();

  RestStatus handleNodeVersion();
  RestStatus handleNodeStatistics();
  RestStatus handleNodeEngine();
  RestStatus handleStatistics();

  RestStatus handleShardDistribution();
  RestStatus handleCollectionShardDistribution();

  RestStatus handleCleanoutServer();
  RestStatus handleResignLeadership();
  RestStatus handleMoveShard();
  RestStatus handleQueryJobStatus();

  RestStatus handleRemoveServer();

private:

  struct MoveShardContext {
    std::string database;
    std::string collection;
    std::string shard;
    std::string fromServer;
    std::string toServer;
    std::string collectionID;

    static std::unique_ptr<MoveShardContext> fromVelocyPack(VPackSlice slice);
  };

  RestStatus handlePostMoveShard(std::unique_ptr<MoveShardContext>&& ctx);

  RestStatus handleSingleServerJob(std::string const& job);
  RestStatus handleCreateSingleServerJob(std::string const& job, std::string const& server);


  typedef std::chrono::steady_clock clock;
  typedef futures::Future<futures::Unit> futureVoid;

  futureVoid waitForSupervisionState(bool state, clock::time_point startTime = clock::time_point());


  struct RemoveServerContext {
    size_t tries;
    std::string server;

    RemoveServerContext(std::string const& s) : tries(0), server(s) {}
  };

  futureVoid tryDeleteServer(std::unique_ptr<RemoveServerContext>&& ctx);
  futureVoid retryTryDeleteServer(std::unique_ptr<RemoveServerContext>&& ctx);
  futureVoid createMoveShard(std::unique_ptr<MoveShardContext>&& ctx, VPackSlice plan);

  RestStatus handleProxyGetRequest(std::string const& url, std::string const& serverFromParameter);
  RestStatus handleGetCollectionShardDistribution(std::string const& collection);

  RestStatus handlePostRemoveServer(std::string const& server);

};
}  // namespace arangodb

#endif
