////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019-2019 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/DistributeConsumerNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IResearchViewNode.h"
#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

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
                          node->getType() == ExecutionNode::INDEX);
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
                                        std::unordered_map<size_t, size_t>& nodeAliases,
                                        VPackBuilder& infoBuilder) {
  TRI_ASSERT(!_nodes.empty());
  TRI_ASSERT(!_expansions.empty());
  size_t numberOfShardsToPermutate = 0;
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

      // A Remote can only contact a global SCATTER or GATHER node.
      TRI_ASSERT(rem->getFirstDependency() != nullptr);
      TRI_ASSERT(rem->getFirstDependency()->getType() == ExecutionNode::SCATTER ||
                 rem->getFirstDependency()->getType() == ExecutionNode::DISTRIBUTE);
      TRI_ASSERT(rem->hasDependency());

      // Need to wire up the internal scatter and distribute consumer in between the last two nodes
      // Note that we need to clean up this after we produced the snippets.
      _globalScatter = ExecutionNode::castTo<ScatterNode*>(rem->getFirstDependency());
      // Let the globalScatter node distribute data by server
      _globalScatter->setScatterType(ScatterNode::ScatterType::SERVER);

      _madeResponsibleForShutdown = true;
    } else {
      rem->isResponsibleForInitializeCursor(false);
    }
    // We have it all serverbased now
    rem->setDistributeId(server);
    // Wire up this server to the global scatter
    TRI_ASSERT(_globalScatter != nullptr);
    _globalScatter->addClient(rem);
  }

  if (lastIsRemote) {
    // For serialization remove the dependency of Remote
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    std::vector<ExecutionNode*> deps;
    lastNode->dependencies(deps);
    TRI_ASSERT(deps.size() == 1);
    TRI_ASSERT(deps[0] == _globalScatter);
#endif
    lastNode->removeDependencies();
  }
  // The Key is required to build up the queryId mapping later
  infoBuilder.add(VPackValue(
      arangodb::basics::StringUtils::itoa(_idOfSinkRemoteNode) + ":" + server));

  if (!localExpansions.empty()) {
    // We have Expansions to permutate
    std::vector<std::string> distIds{};
    distIds.reserve(numberOfShardsToPermutate);

    // Create an internal GatherNode, that will connect to all execution
    // steams of the query
    auto plan = lastNode->plan();
    // Clone the sink node, we do not need dependencies (second bool)
    // And we do not need variables
    GatherNode* internalGather =
        ExecutionNode::castTo<GatherNode*>(_sinkNode->clone(plan, false, false));
    // Use the same elements for sorting
    internalGather->elements(_sinkNode->elements());

    ScatterNode* internalScatter = nullptr;
    if (lastIsRemote) {
      TRI_ASSERT(_globalScatter != nullptr);
      internalScatter =
          ExecutionNode::castTo<ScatterNode*>(_globalScatter->clone(plan, false, false));
      internalScatter->clearClients();
      internalScatter->addDependency(lastNode);
      // Let the local Scatter node distribute data by SHARD
      internalScatter->setScatterType(ScatterNode::ScatterType::SHARD);
      if (_globalScatter->getType() == ExecutionNode::DISTRIBUTE) {
        DistributeNode const* dist =
            ExecutionNode::castTo<DistributeNode const*>(_globalScatter);
        TRI_ASSERT(dist != nullptr);
        auto distCollection = dist->collection();
        TRI_ASSERT(distCollection != nullptr);
        // Now find the node that provides the distribute information
        for (auto const& exp : localExpansions) {
          auto colAcc = ExecutionNode::castTo<CollectionAccessingNode*>(exp.first);
          if (colAcc->collection() == distCollection) {
            // Found one, use all shards of it
            for (auto const& s : exp.second) {
              distIds.emplace_back(s);
            }
          }
        }
      } else {
        // In this case we actually do not care for the real value, we just need
        // to ensure that every client get's exactly one copy.
        for (size_t i = 0; i < numberOfShardsToPermutate; i++) {
          distIds.emplace_back(StringUtils::itoa(i));
        }
      }

      auto uniq_consumer =
          std::make_unique<DistributeConsumerNode>(plan, plan->nextId(), distIds[0]);
      auto consumer = uniq_consumer.get();
      TRI_ASSERT(consumer != nullptr);
      // Hand over responsibilty to plan, s.t. it can clean up if one of the below fails
      plan->registerNode(uniq_consumer.release());
      consumer->addDependency(internalScatter);
      consumer->cloneRegisterPlan(internalScatter);
      internalScatter->addClient(consumer);

      // now wire up the temporary nodes
      TRI_ASSERT(_nodes.size() > 1);
      ExecutionNode* secondToLast = _nodes[_nodes.size() - 2];
      TRI_ASSERT(secondToLast->hasDependency());
      secondToLast->swapFirstDependency(consumer);
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
          // to Consumer and Scatter instead
          TRI_ASSERT(internalScatter != nullptr);
          auto uniq_consumer =
              std::make_unique<DistributeConsumerNode>(plan, plan->nextId(), distIds[i]);
          auto consumer = uniq_consumer.get();
          TRI_ASSERT(consumer != nullptr);
          // Hand over responsibilty to plan, s.t. it can clean up if one of the below fails
          plan->registerNode(uniq_consumer.release());
          consumer->isResponsibleForInitializeCursor(false);
          consumer->addDependency(internalScatter);
          consumer->cloneRegisterPlan(internalScatter);
          internalScatter->addClient(consumer);
          previous = consumer;

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
        TRI_ASSERT(clone->id() != current->id());
        nodeAliases.emplace(clone->id(), current->id());
        previous = clone;
      }
      TRI_ASSERT(previous != nullptr);
      // Previous is now the last node, where our internal GATHER needs to be connected to
      internalGather->addDependency(previous);
    }

    const unsigned flags = ExecutionNode::SERIALIZE_DETAILS;
    internalGather->toVelocyPack(infoBuilder, flags, false);

    // We need to clean up ONLY if we have injected the local scatter
    if (lastIsRemote) {
      TRI_ASSERT(internalScatter != nullptr);
      TRI_ASSERT(_nodes.size() > 1);
      ExecutionNode* secondToLast = _nodes[_nodes.size() - 2];
      TRI_ASSERT(secondToLast->hasDependency());
      secondToLast->swapFirstDependency(lastNode);
    }
  } else {
    const unsigned flags = ExecutionNode::SERIALIZE_DETAILS;
    _nodes.front()->toVelocyPack(infoBuilder, flags, false);
  }
  if (lastIsRemote) {
    // For the local copy readd the dependency of Remote
    lastNode->addDependency(_globalScatter);
  }
}
