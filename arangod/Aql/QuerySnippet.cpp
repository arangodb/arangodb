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
  infoBuilder.add(VPackValue(arangodb::basics::StringUtils::itoa(_idOfSinkNode) + ":" + server));

  std::unordered_map<ExecutionNode*, std::set<ShardID>> localExpansions;
  for (auto const& exp : _expansions) {
    std::set<ShardID> myExp;
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
    if (exp.doExpand) {
      // All parts need to have exact same size, they need to be permutated pairwise!
      TRI_ASSERT(numberOfShardsToPermutate == 0 || myExp.size() == numberOfShardsToPermutate);
      // set the max loop index (note this will essentially be done only once)
      numberOfShardsToPermutate = myExp.size();
      if (numberOfShardsToPermutate == 1) {
        // Special case, we do not need to expand.
        // However keep track that we found one and cannot have
        // a query with a collection requiring real permutation.
        // We only have these other types that can exactly one shard at a time
        auto collectionAccessingNode =
            ExecutionNode::castTo<CollectionAccessingNode*>(exp.node);
        // so let it inject this shard!
        TRI_ASSERT(myExp.size() == 1);
        collectionAccessingNode->setUsedShard(*myExp.begin());
      } else {
        localExpansions.emplace(exp.node, std::move(myExp));
      }
    } else {
      if (exp.node->getType() == ExecutionNode::ENUMERATE_IRESEARCH_VIEW) {
        // TODO implement shard injection on views
      } else {
        // We only have these other types that can exactly one shard at a time
        auto collectionAccessingNode =
            ExecutionNode::castTo<CollectionAccessingNode*>(exp.node);
        // so let it inject this shard!
        TRI_ASSERT(myExp.size() == 1);
        collectionAccessingNode->setUsedShard(*myExp.begin());
      }
    }
  }
  // TODO toVPack all nodes for this specific server
  // We clone every Node* and maintain a list of ReportingGroups for profiler
  ExecutionNode* lastNode = _nodes.back();
  // Query can only ed with a REMOTE or SINGLETON
  TRI_ASSERT(lastNode->getType() == ExecutionNode::REMOTE ||
             lastNode->getType() == ExecutionNode::SINGLETON);
  // Singleton => noDependency
  TRI_ASSERT(lastNode->getType() != ExecutionNode::SINGLETON || !lastNode->hasDependency());

  if (lastNode->getType() == ExecutionNode::REMOTE) {
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
    } else {
      rem->isResponsibleForInitializeCursor(false);
    }
    // We have it all serverbased now
    rem->ownName(server);
  }

  if (!localExpansions.empty()) {
    // We have Expansions to permutate
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    // TODO iterate over all localExpansions
    // Inject the shard for this server into the nodes.
    /*
    for (auto const& exp : localExpansions) {
    }
    */
  } else {
    const unsigned flags = ExecutionNode::SERIALIZE_DETAILS;
    _nodes.front()->toVelocyPack(infoBuilder, flags, false);
  }
  // If we end up here we have send one that is responsible
  // to Shutdown the query.
  _madeResponsibleForShutdown = true;
}
