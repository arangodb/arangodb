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

#include "EngineInfoContainerDBServerServerBased.h"

#include "Aql/ExecutionNode.h"
#include "Aql/GraphNode.h"
#include "Aql/Query.h"
#include "Cluster/ClusterComm.h"

#include <set>

using namespace arangodb;
using namespace arangodb::aql;

class EngineInfoContainerDBServerServerBased {
 public:
  explicit EngineInfoContainerDBServerServerBased::EngineInfoContainerDBServerServerBased(
      Query* query) noexcept {}

  // Insert a new node into the last engine on the stack
  // If this Node contains Collections, they will be added into the map
  // for ShardLocking
  void EngineInfoContainerDBServerServerBased::addNode(ExecutionNode* node) {
    TRI_ASSERT(node);
    TRI_ASSERT(!_snippetStack.empty());
    // Add the node to the open Snippet
    _snippetStack.top()->addNode(node);
    // Upgrade CollectionLocks if necessary
    handleCollectionLocking(node);
  }

  // Open a new snippet, which is connected to the given remoteNode id
  void EngineInfoContainerDBServerServerBased::openSnippet(size_t idOfRemoteNode) {
    // TODO add a new snippet on the stack
  }

  // Closes the given snippet and connects it
  // to the given queryid of the coordinator.
  void EngineInfoContainerDBServerServerBased::closeSnippet(QueryId id) {
    // TODO pop snippet from stack, close it and memorize
  }

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
  Result EngineInfoContainerDBServerServerBased::buildEngines(MapRemoteToSnippet& queryIds) const {
    // This needs to be a set with a defined order, it is important, that we contact
    // the database servers only in this specific order to avoid cluster-wide deadlock situations.
    std::set<ServerID> dbServers;

    auto cc = ClusterComm::instance();
    if (cc == nullptr) {
      // nullptr only happens on controlled shutdown
      return {TRI_ERROR_SHUTTING_DOWN};
    }

    double ttl = _query->queryOptions().ttl;

    std::string const url(
        "/_db/" + arangodb::basics::StringUtils::urlEncode(_query->vocbase().name()) +
        "/_api/aql/setup?ttl=" + std::to_string(ttl));

    auto cleanupGuard = scopeGuard([this, &cc, &queryIds]() {
      cleanupEngines(cc, TRI_ERROR_INTERNAL, _query->vocbase().name(), queryIds);
    });

    // TODO Figure out which servers participate

    // Build Lookup Infos
    VPackBuilder infoBuilder;
    transaction::Methods* trx = _query->trx();
    for (auto const& server : dbServers) {
      std::string const serverDest = "server:" + server;

      LOG_TOPIC("4bbe6", DEBUG, arangodb::Logger::AQL)
          << "Building Engine Info for " << server;
      infoBuilder.clear();
      infoBuilder.openObject();
      addLockingPart(infoBuilder);
      TRI_ASSERT(infoBuilder.isOpenObject());

      addOptionsPart(infoBuilder);
      TRI_ASSERT(infoBuilder.isOpenObject());

      addVariablesPart(infoBuilder);
      TRI_ASSERT(infoBuilder.isOpenObject());

      addSnippetPart(infoBuilder);
      TRI_ASSERT(infoBuilder.isOpenObject());

      addTraversalEnginesPart(infoBuilder);
      TRI_ASSERT(infoBuilder.isOpenObject());

      infoBuilder.close();  // Base object
      TRI_ASSERT(infoBuilder.isClosed());
      // Partial assertions to check if all required keys are present
      TRI_ASSERT(infoBuilder.slice().hasKey("lockInfo"));
      TRI_ASSERT(infoBuilder.slice().hasKey("options"));
      TRI_ASSERT(infoBuilder.slice().hasKey("variables"));
      // We need to have at least one: snippets or traverserEngines
      TRI_ASSERT(infoBuilder.slice().hasKey("snippets") ||
                 infoBuilder.slice().hasKey("traverserEngines"));
      // TODO send messages out per server
    }
  }

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
  void EngineInfoContainerDBServerServerBased::cleanupEngines(
      std::shared_ptr<ClusterComm> cc, int errorCode, std::string const& dbname,
      MapRemoteToSnippet& queryIds) const;

  // Insert a GraphNode that needs to generate TraverserEngines on
  // the DBServers. The GraphNode itself will retain on the coordinator.
  void EngineInfoContainerDBServerServerBased::addGraphNode(GraphNode* node);

  void EngineInfoContainerDBServerServerBased::handleCollectionLocking(ExecutionNode* node) {
    TRI_ASSERT(node != nullptr);
    switch (node->getType()) {
        // TODO handle all nodes which directly access collections
      default:
        // Nothing todo
    }
  }
};
