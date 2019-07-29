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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_ENGINE_INFO_CONTAINER_DBSERVER_SERVER_BASED_H
#define ARANGOD_AQL_ENGINE_INFO_CONTAINER_DBSERVER_SERVER_BASED_H 1

#include "Basics/Common.h"

#include "Aql/types.h"

#include <stack>

namespace arangodb {
class ClusterComm;

namespace velocypack {
class Builder;
}

namespace aql {

class ExecutionNode;
class GraphNode;
class Query;
class QuerySnippet;

class EngineInfoContainerDBServerServerBased {
 public:
  explicit EngineInfoContainerDBServerServerBased(Query* query) noexcept;

  // Insert a new node into the last engine on the stack
  // If this Node contains Collections, they will be added into the map
  // for ShardLocking
  void addNode(ExecutionNode* node);

  // Open a new snippet, which is connected to the given remoteNode id
  void openSnippet(size_t idOfRemoteNode);

  // Closes the given snippet and connects it
  // to the given queryid of the coordinator.
  void closeSnippet(QueryId id);

  // Build the Engines for the DBServer
  //   * Creates one Query-Entry for each Snippet per Shard (multiple on the
  //   same DB)
  //   * All snippets know all locking information for the query.
  //   * Only the first snippet is responsible to lock.
  //   * After this step DBServer-Collections are locked!
  //
  //   Error Case: It is guaranteed that for all snippets created during
  //   this methods a shutdown request is send to all DBServers.
  //   In case the network is broken and this shutdown request is lost
  //   the DBServers will clean up their snippets after a TTL.
  Result buildEngines(MapRemoteToSnippet& queryIds) const;

  /**
   * @brief Will send a shutdown to all engines registered in the list of
   * queryIds.
   * NOTE: This function will ignore all queryids where the key is not of
   * the expected format
   * they may be leftovers from Coordinator.
   * Will also clear the list of queryIds after return.
   *
   * @param cc The ClusterComm
   * @param errorCode error Code to be send to DBServers for logging.
   * @param dbname Name of the database this query is executed in.
   * @param queryIds A map of QueryIds of the format: (remoteNodeId:shardId) ->
   * queryid.
   */
  void cleanupEngines(std::shared_ptr<ClusterComm> cc, int errorCode,
                      std::string const& dbname, MapRemoteToSnippet& queryIds) const;

  // Insert a GraphNode that needs to generate TraverserEngines on
  // the DBServers. The GraphNode itself will retain on the coordinator.
  void addGraphNode(GraphNode* node);

 private:
  // Adjust locking level if a node uses a collections
  void handleCollectionLocking(ExecutionNode* node);

  // Insert the Locking information into the message to be send to DBServers
  void addLockingPart(arangodb::velocypack::Builder& builder) const;

  // Insert the Options information into the message to be send to DBServers
  void addOptionsPart(arangodb::velocypack::Builder& builder) const;

  // Insert the Variables information into the message to be send to DBServers
  void addVariablesPart(arangodb::velocypack::Builder& builder) const;

  // Insert the Snippets information into the message to be send to DBServers
  void addSnippetPart(arangodb::velocypack::Builder& builder) const;

  // Insert the TraversalEngine information into the message to be send to DBServers
  void addTraversalEnginesPart(arangodb::velocypack::Builder& builder) const;

 private:
  std::stack<std::shared_ptr<QuerySnippet>, std::vector<std::shared_ptr<QuerySnippet>>> _snippetStack;

  Query* _query;
};

}  // namespace aql
}  // namespace arangodb
#endif
