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

#include "QuerySnippet.h"

#include "Aql/ClusterNodes.h"
#include "Aql/CollectionAccessingNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/IResearchViewNode.h"
#include "Cluster/ServerState.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

void QuerySnippet::addNode(ExecutionNode* node) {
  TRI_ASSERT(node != nullptr);
  _nodes.push_back(node);

  switch (node->getType()) {
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::INDEX:
    case ExecutionNode::INSERT:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REMOVE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPSERT: {
      // We do not actually need to know the details here.
      // We just wanna know the shards!
      auto collectionAccessingNode =
          ExecutionNode::castTo<CollectionAccessingNode*>(node);
      TRI_ASSERT(collectionAccessingNode != nullptr);
      auto col = collectionAccessingNode->collection();
      auto shards = col->shardIds();
      // Satellites can only be used on ReadNodes
      bool isSatellite = col->isSatellite() &&
                         (node->getType() == ExecutionNode::ENUMERATE_COLLECTION ||
                          ExecutionNode::INDEX);
      if (collectionAccessingNode->isRestricted()) {
        std::string const& onlyShard = collectionAccessingNode->restrictedShard();
        bool found =
            std::find(shards->begin(), shards->end(), onlyShard) != shards->end();
        if (!found) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_CLUSTER_SHARD_GONE,
              "Collection could not be restricted to the given shard: " + onlyShard +
                  " it is not part of collection " + col->name());
        }
        auto restrictedShards = std::make_shared<std::vector<ShardID>>();
        restrictedShards->emplace_back(onlyShard);
        _expansions.emplace_back(node, false, restrictedShards, isSatellite);
      } else {
        // Satellite can only have a single shard, and we have a modification node here.
        TRI_ASSERT(!isSatellite || shards->size() == 1);
        _expansions.emplace_back(node, shards->size() > 1, shards, isSatellite);
      }
      break;
    }
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
      auto viewNode = ExecutionNode::castTo<iresearch::IResearchViewNode*>(node);

      // evaluate node volatility before the distribution
      // can't do it on DB servers since only parts of the plan will be sent
      viewNode->volatility(true);
      auto collections = viewNode->collections();
      auto shardList = std::make_shared<std::vector<ShardID>>();
      for (aql::Collection const& c : collections) {
        auto shards = c.shardIds();
        shardList->insert(shardList->end(), shards->begin(), shards->end());
      }
      _expansions.emplace_back(node, false, shardList, false);
      break;
    }
    default:
      // do nothing
      break;
  }
}

void QuerySnippet::serializeIntoBuilder(ServerID const& server,
                                        std::unordered_map<ShardID, ServerID> const& shardMapping,
                                        VPackBuilder& infoBuilder) {
  TRI_ASSERT(!_nodes.empty());
  TRI_ASSERT(!_expansions.empty());
  size_t numberOfShardsToPermutate = 0;
  // The Key is required to build up the queryId mapping later
  infoBuilder.add(VPackValue(
      arangodb::basics::StringUtils::itoa(_idOfSinkRemoteNode) + ":" + server));

  std::unordered_map<ExecutionNode*, std::set<ShardID>> localExpansions;
  for (auto const& exp : _expansions) {
    std::set<ShardID> myExp;
    if (exp.node->getType() == ExecutionNode::ENUMERATE_IRESEARCH_VIEW) {
      // Special case, VIEWs can serve more than 1 shard per Node.
      // We need to inject them all at once.
      auto* viewNode = ExecutionNode::castTo<iresearch::IResearchViewNode*>(exp.node);
      auto& viewShardList = viewNode->shards();
      viewShardList.clear();
      for (auto const& s : *exp.shards) {
        auto check = shardMapping.find(s);
        // If we find a shard here that is not in this mapping,
        // we have 1) a problem with locking before that should have thrown
        // 2) a problem with shardMapping lookup that should have thrown before
        TRI_ASSERT(check != shardMapping.end());
        if (check->second == server) {
          viewShardList.emplace_back(s);
        }
      }
      continue;
    }
    for (auto const& s : *exp.shards) {
      auto check = shardMapping.find(s);
      // If we find a shard here that is not in this mapping,
      // we have 1) a problem with locking before that should have thrown
      // 2) a problem with shardMapping lookup that should have thrown before
      TRI_ASSERT(check != shardMapping.end());
      if (check->second == server) {
        myExp.emplace(s);
      } else if (exp.isSatellite && _expansions.size() > 1) {
        // Satellite collection is used for local join.
        // If we only have one expansion we have a snippet only
        // based on the satellite, that needs to be only executed once
        myExp.emplace(s);
      }
    }
    if (myExp.empty()) {
      // There are no shards in this snippet for this server.
      // By definition all nodes need to have at LEAST one Shard
      // on this server for this snippet to work.
      // Skip it.
      // We have not modified infoBuilder up until now.
      return;
    }
    // For all other Nodes we can inject a single shard at a time.
    // Always use the list of nodes we maintain to hold the first
    // of all shards.
    // We later use a cone mechanism to inject other shards of permutation
    auto collectionAccessingNode =
        ExecutionNode::castTo<CollectionAccessingNode*>(exp.node);
    collectionAccessingNode->setUsedShard(*myExp.begin());
    if (exp.doExpand) {
      // All parts need to have exact same size, they need to be permutated pairwise!
      TRI_ASSERT(numberOfShardsToPermutate == 0 || myExp.size() == numberOfShardsToPermutate);
      // set the max loop index (note this will essentially be done only once)
      numberOfShardsToPermutate = myExp.size();
      if (numberOfShardsToPermutate > 1) {
        // Only in this case we really need to do an expansion
        // Otherwise we get away with only using the main stream for
        // this server
        // NOTE: This might differ between servers.
        // One server might require an expansion (many shards) while another does not (only one shard).
        localExpansions.emplace(exp.node, std::move(myExp));
      }
    } else {
      TRI_ASSERT(myExp.size() == 1);
    }
  }
  // TODO toVPack all nodes for this specific server
  // We clone every Node* and maintain a list of ReportingGroups for profiler
  ExecutionNode* lastNode = _nodes.back();
  bool lastIsRemote = lastNode->getType() == ExecutionNode::REMOTE;
  // Query can only ed with a REMOTE or SINGLETON
  TRI_ASSERT(lastIsRemote || lastNode->getType() == ExecutionNode::SINGLETON);
  // Singleton => noDependency
  TRI_ASSERT(lastNode->getType() != ExecutionNode::SINGLETON || !lastNode->hasDependency());
  if (lastIsRemote) {
    auto rem = ExecutionNode::castTo<RemoteNode*>(lastNode);
    if (!_madeResponsibleForShutdown) {
      // Enough to do this step once
      // We need to connect this Node to the sink
      // update the remote node with the information about the query
      rem->server("server:" + arangodb::ServerState::instance()->getId());
      rem->queryId(_inputSnippet);
      rem->isResponsibleForInitializeCursor(true);
      // Cut off all dependencies here, they are done implicitly
      rem->removeDependencies();
      _madeResponsibleForShutdown = true;
    } else {
      rem->isResponsibleForInitializeCursor(false);
    }
    // We have it all serverbased now
    rem->ownName(server);
  }

  if (!localExpansions.empty()) {
    // We have Expansions to permutate

    // Create an internal GatherNode, that will connect to all execution
    // steams of the query
    auto plan = lastNode->plan();
    // Clone the sink node, we do not need dependencies (second bool)
    // And we do not need variables
    GatherNode* internalGather =
        ExecutionNode::castTo<GatherNode*>(_sinkNode->clone(plan, false, false));
    ScatterNode* internalScatter = nullptr;
    if (lastIsRemote) {
      // Not supported yet
      // TODO Add a scatter node between the top 2 nodes.
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    // We do not need to copy the first stream, we can use the one we have.
    // We only need copies for the other streams.
    internalGather->addDependency(_nodes.front());

    // NOTE: We will copy over the entire snippet stream here.
    // We will inject the permuted shards on the way.
    // Also note: the local plan will take memory responsibility
    // of the ExecutionNodes created during this procedure.
    for (size_t i = 1; i < numberOfShardsToPermutate; ++i) {
      ExecutionNode* previous = nullptr;
      for (auto enIt = _nodes.rbegin(), end = _nodes.rend(); enIt != end; ++enIt) {
        ExecutionNode* current = *enIt;
        if (lastIsRemote && current == lastNode) {
          // Do never clone the remote, link following node
          // to Scatter instead
          TRI_ASSERT(internalScatter != nullptr);
          previous = internalScatter;
          continue;
        }
        ExecutionNode* clone = current->clone(plan, false, false);
        auto permuter = localExpansions.find(current);
        if (permuter != localExpansions.end()) {
          auto collectionAccessingNode =
              ExecutionNode::castTo<CollectionAccessingNode*>(clone);
          // Get the `i` th shard
          collectionAccessingNode->setUsedShard(*std::next(permuter->second.begin(), i));
        }
        if (previous != nullptr) {
          clone->addDependency(previous);
        }
        previous = clone;
      }
      TRI_ASSERT(previous != nullptr);
      // Previous is now the last node, where our internal GATHER needs to be connected to
      internalGather->addDependency(previous);
    }

    const unsigned flags = ExecutionNode::SERIALIZE_DETAILS;
    internalGather->toVelocyPack(infoBuilder, flags, false);
    // No need to clean up
  } else {
    const unsigned flags = ExecutionNode::SERIALIZE_DETAILS;
    _nodes.front()->toVelocyPack(infoBuilder, flags, false);
  }
}
