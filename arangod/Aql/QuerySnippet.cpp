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
#include "Aql/Collection.h"
#include "Aql/CollectionAccessingNode.h"
#include "Aql/DistributeConsumerNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/GraphNode.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/ShardLocking.h"
#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Aql/LocalGraphNode.h"
#endif

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
      auto collectionAccessingNode = dynamic_cast<CollectionAccessingNode*>(node);
      TRI_ASSERT(collectionAccessingNode != nullptr);
      _expansions.emplace_back(node, !collectionAccessingNode->isUsedAsSatellite(),
                               collectionAccessingNode->isUsedAsSatellite());
      break;
    }
    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::K_SHORTEST_PATHS: {
      auto* graphNode = ExecutionNode::castTo<GraphNode*>(node);
      auto const isSatellite = graphNode->isUsedAsSatellite();
      _expansions.emplace_back(node, !isSatellite, isSatellite);
      break;
    }
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
      auto viewNode = ExecutionNode::castTo<iresearch::IResearchViewNode*>(node);

      // evaluate node volatility before the distribution
      // can't do it on DB servers since only parts of the plan will be sent
      viewNode->volatility(true);
      _expansions.emplace_back(node, false, false);
      break;
    }
    case ExecutionNode::MATERIALIZE: {
      auto collectionAccessingNode = dynamic_cast<CollectionAccessingNode*>(node);
      // Materialize index node - true
      // Materialize view node - false
      if (collectionAccessingNode != nullptr) {
        _expansions.emplace_back(node, true, false);
      }
      break;
    }
    default:
      // do nothing
      break;
  }
}

void QuerySnippet::serializeIntoBuilder(
    ServerID const& server,
    std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById,
    ShardLocking& shardLocking,
    std::unordered_map<ExecutionNodeId, ExecutionNodeId>& nodeAliases,
    VPackBuilder& infoBuilder) {
  TRI_ASSERT(!_nodes.empty());
  TRI_ASSERT(!_expansions.empty());
  auto firstBranchRes = prepareFirstBranch(server, nodesById, shardLocking);
  if (!firstBranchRes.ok()) {
    // We have at least one expansion that has no shard on this server.
    // we do not need to write this snippet here.
    TRI_ASSERT(firstBranchRes.is(TRI_ERROR_CLUSTER_NOT_LEADER));
    return;
  }
  std::unordered_map<ExecutionNode*, std::set<ShardID>>& localExpansions =
      firstBranchRes.get();

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

      // All of the above could be repeated as it is constant information.
      // this one line is the important one
      rem->isResponsibleForInitializeCursor(true);
      _madeResponsibleForShutdown = true;
    } else {
      rem->isResponsibleForInitializeCursor(false);
    }
    // We have it all serverbased now
    rem->setDistributeId(server);
    // Wire up this server to the global scatter
    TRI_ASSERT(_globalScatter != nullptr);
    _globalScatter->addClient(rem);

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
      arangodb::basics::StringUtils::itoa(_idOfSinkRemoteNode.id()) + ":" + server));
  if (!localExpansions.empty()) {
    // We have Expansions to permutate, guaranteed they have
    // all identical lengths.
    size_t numberOfShardsToPermutate = localExpansions.begin()->second.size();
    std::vector<std::string> distIds{};
    // Reserve the amount of localExpansions,
    distIds.reserve(numberOfShardsToPermutate);
    // Create an internal GatherNode, that will connect to all execution
    // steams of the query
    auto plan = lastNode->plan();
    TRI_ASSERT(plan == _sinkNode->plan());
    // Clone the sink node, we do not need dependencies (second bool)
    // And we do not need variables
    auto* internalGather =
        ExecutionNode::castTo<GatherNode*>(_sinkNode->clone(plan, false, false));
    // Use the same elements for sorting
    internalGather->elements(_sinkNode->elements());
    // We need to modify the registerPlanning.
    // The internalGather is NOT allowed to reduce the number of registers,
    // it needs to expose it's input register by all means
    internalGather->setVarsUsedLater(_nodes.front()->getVarsUsedLater());
    internalGather->setRegsToClear({});
    auto const reservedId = ExecutionNodeId{std::numeric_limits<ExecutionNodeId::BaseType>::max()};
    nodeAliases.try_emplace(internalGather->id(), reservedId);

    ScatterNode* internalScatter = nullptr;
    if (lastIsRemote) {
      TRI_ASSERT(_globalScatter != nullptr);
      TRI_ASSERT(plan == _globalScatter->plan());
      internalScatter =
          ExecutionNode::castTo<ScatterNode*>(_globalScatter->clone(plan, false, false));
      internalScatter->clearClients();
      internalScatter->addDependency(lastNode);
      // Let the local Scatter node distribute data by SHARD
      internalScatter->setScatterType(ScatterNode::ScatterType::SHARD);
      nodeAliases.try_emplace(internalScatter->id(), reservedId);

      if (_globalScatter->getType() == ExecutionNode::DISTRIBUTE) {
        {
          // We are not allowed to generate new keys on the DBServer.
          // We need to take the ones generated by the node above.
          DistributeNode* internalDist =
              ExecutionNode::castTo<DistributeNode*>(internalScatter);
          internalDist->setCreateKeys(false);
        }
        DistributeNode const* dist =
            ExecutionNode::castTo<DistributeNode const*>(_globalScatter);
        TRI_ASSERT(dist != nullptr);
        auto distCollection = dist->collection();
        TRI_ASSERT(distCollection != nullptr);

        // Now find the node that provides the distribute information
        for (auto const& exp : localExpansions) {
          auto colAcc = dynamic_cast<CollectionAccessingNode*>(exp.first);
          TRI_ASSERT(colAcc != nullptr);
          if (colAcc->collection() == distCollection) {
            // Found one, use all shards of it
            for (auto const& s : exp.second) {
              distIds.emplace_back(s);
            }
            break;
          }
        }
      } else {
        // In this case we actually do not care for the real value, we just need
        // to ensure that every client get's exactly one copy.
        for (size_t i = 0; i < numberOfShardsToPermutate; i++) {
          distIds.emplace_back(StringUtils::itoa(i));
        }
      }

      DistributeConsumerNode* consumer =
          createConsumerNode(plan, internalScatter, distIds[0]);
      nodeAliases.try_emplace(consumer->id(), std::numeric_limits<size_t>::max());
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
          DistributeConsumerNode* consumer =
              createConsumerNode(plan, internalScatter, distIds[i]);
          consumer->isResponsibleForInitializeCursor(false);
          nodeAliases.try_emplace(consumer->id(), std::numeric_limits<size_t>::max());
          previous = consumer;
          continue;
        }
        ExecutionNode* clone = current->clone(plan, false, false);
        auto permuter = localExpansions.find(current);
        if (permuter != localExpansions.end()) {
          auto collectionAccessingNode = dynamic_cast<CollectionAccessingNode*>(clone);
          TRI_ASSERT(collectionAccessingNode != nullptr);
          // Get the `i` th shard
          collectionAccessingNode->setUsedShard(*std::next(permuter->second.begin(), i));
        }
        if (previous != nullptr) {
          clone->addDependency(previous);
        }
        TRI_ASSERT(clone->id() != current->id());
        nodeAliases.try_emplace(clone->id(), current->id());
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
    // For the local copy read the dependency of Remote
    lastNode->addDependency(_globalScatter);
  }
}

ResultT<std::unordered_map<ExecutionNode*, std::set<ShardID>>> QuerySnippet::prepareFirstBranch(
    ServerID const& server,
    std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById,
    ShardLocking& shardLocking) {
  size_t numberOfShardsToPermutate = 0;
  std::unordered_map<ExecutionNode*, std::set<ShardID>> localExpansions;
  std::unordered_map<ShardID, ServerID> const& shardMapping =
      shardLocking.getShardMapping();

  for (auto const& exp : _expansions) {
    if (exp.node->getType() == ExecutionNode::ENUMERATE_IRESEARCH_VIEW) {
      // Special case, VIEWs can serve more than 1 shard per Node.
      // We need to inject them all at once.
      auto* viewNode = ExecutionNode::castTo<iresearch::IResearchViewNode*>(exp.node);
      auto& viewShardList = viewNode->shards();
      viewShardList.clear();

      auto collections = viewNode->collections();
      for (aql::Collection const& c : collections) {
        auto const& shards = shardLocking.shardsForSnippet(id(), &c);
        for (auto const& s : shards) {
          auto check = shardMapping.find(s);
          // If we find a shard here that is not in this mapping,
          // we have 1) a problem with locking before that should have thrown
          // 2) a problem with shardMapping lookup that should have thrown before
          TRI_ASSERT(check != shardMapping.end());
          if (check->second == server) {
            viewShardList.emplace_back(s);
          }
        }
      }
    } else if (exp.node->getType() == ExecutionNode::TRAVERSAL ||
               exp.node->getType() == ExecutionNode::SHORTEST_PATH ||
               exp.node->getType() == ExecutionNode::K_SHORTEST_PATHS) {
#ifndef USE_ENTERPRISE
      // These can only ever be LocalGraphNodes, which are only available in
      // Enterprise. This should never happen without enterprise optimization,
      // either.
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
#else
      // the same translation is copied to all servers
      // there are no local expansions

      auto* localGraphNode = ExecutionNode::castTo<LocalGraphNode*>(exp.node);
      localGraphNode->setCollectionToShard({});  // clear previous information

      TRI_ASSERT(localGraphNode->isUsedAsSatellite() == exp.isSatellite);

      // Check whether `servers` is the leader for any of the shards of the
      // prototype collection.
      // We want to instantiate this snippet here exactly iff this is the case.
      auto needInstanceHere = std::invoke([&]() {
        auto const* const protoCol = localGraphNode->isUsedAsSatellite()
                ? ExecutionNode::castTo<CollectionAccessingNode*>(
                      localGraphNode->getSatelliteOf(nodesById))
                      ->collection()
                : localGraphNode->collection();

        auto const& shards = shardLocking.shardsForSnippet(id(), protoCol);

        return std::any_of(shards.begin(), shards.end(),
                           [&shardMapping, &server](auto const& shard) {
                             auto mappedServerIt = shardMapping.find(shard);
                             // If we find a shard here that is not in this mapping,
                             // we have 1) a problem with locking before that should have thrown
                             // 2) a problem with shardMapping lookup that should have thrown before
                             TRI_ASSERT(mappedServerIt != shardMapping.end());
                             return mappedServerIt->second == server;
                           });
      });

      if (!needInstanceHere) {
        return {TRI_ERROR_CLUSTER_NOT_LEADER};
      }

      // This is either one shard or a single satellite graph which is not used
      // as satellite graph.
      uint64_t numShards = 0;
      for (auto* aqlCollection : localGraphNode->collections()) {
        auto const& shards = shardLocking.shardsForSnippet(id(), aqlCollection);
        TRI_ASSERT(!shards.empty());
        for (auto const& shard : shards) {
          auto found = shardMapping.find(shard);
          TRI_ASSERT(found != shardMapping.end());
          // We should never have shards on other servers, except for satellite
          // graphs which are used that way, or satellite collections (in a
          // OneShard case) because local graphs (on DBServers) only ever occur
          // in either OneShard or SatelliteGraphs.
          TRI_ASSERT(found->second == server || localGraphNode->isUsedAsSatellite() ||
                     aqlCollection->isSatellite() || localGraphNode->isDisjoint());
          // provide a correct translation from collection to shard
          // to be used in toVelocyPack methods of classes derived
          // from GraphNode
          if (found->second == server || !localGraphNode->isDisjoint()) {
            // TODO: Optimize this, we're looking into to many collections here
            localGraphNode->addCollectionToShard(aqlCollection->name(), shard);
          }

          numShards++;
        }
      }

      TRI_ASSERT(numShards > 0);
      if (numShards == 0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL, "Could not find a shard to instantiate for graph node when expected to");
      }

      if (localGraphNode->isEligibleAsSatelliteTraversal()) {
        auto foundEnoughShards = numShards == localGraphNode->collections().size();
        TRI_ASSERT(foundEnoughShards);
        if (!foundEnoughShards) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
        }
      }
#endif
    } else {
      // exp.node is now either an enumerate collection, index, or modification.
      TRI_ASSERT(exp.node->getType() == ExecutionNode::ENUMERATE_COLLECTION ||
                 exp.node->getType() == ExecutionNode::INDEX ||
                 exp.node->getType() == ExecutionNode::INSERT ||
                 exp.node->getType() == ExecutionNode::UPDATE ||
                 exp.node->getType() == ExecutionNode::REMOVE ||
                 exp.node->getType() == ExecutionNode::REPLACE ||
                 exp.node->getType() == ExecutionNode::UPSERT ||
                 exp.node->getType() == ExecutionNode::MATERIALIZE);

      // It is of utmost importance that this is an ordered set of Shards.
      // We can only join identical indexes of shards for each collection
      // locally.
      std::set<ShardID> myExp;

      auto modNode = dynamic_cast<CollectionAccessingNode const*>(exp.node);
      // Only accessing nodes can end up here.
      TRI_ASSERT(modNode != nullptr);
      auto col = modNode->collection();
      // Should be hit earlier, a modification node here is required to have a collection
      TRI_ASSERT(col != nullptr);
      auto const& shards = shardLocking.shardsForSnippet(id(), col);
      for (auto const& s : shards) {
        auto check = shardMapping.find(s);
        // If we find a shard here that is not in this mapping,
        // we have 1) a problem with locking before that should have thrown
        // 2) a problem with shardMapping lookup that should have thrown before
        TRI_ASSERT(check != shardMapping.end());
        if (check->second == server || exp.isSatellite) {
          // add all shards on satellites.
          // and all shards where this server is the leader
          myExp.emplace(s);
        }
      }
      if (myExp.empty()) {
        return {TRI_ERROR_CLUSTER_NOT_LEADER};
      }
      // For all other Nodes we can inject a single shard at a time.
      // Always use the list of nodes we maintain to hold the first
      // of all shards.
      // We later use a cone mechanism to inject other shards of permutation
      auto collectionAccessingNode = dynamic_cast<CollectionAccessingNode*>(exp.node);
      TRI_ASSERT(collectionAccessingNode != nullptr);
      collectionAccessingNode->setUsedShard(*myExp.begin());
      if (exp.doExpand) {
        TRI_ASSERT(!collectionAccessingNode->isUsedAsSatellite());
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
  }  // for _expansions - end;
  return {localExpansions};
}

DistributeConsumerNode* QuerySnippet::createConsumerNode(ExecutionPlan* plan,
                                                         ScatterNode* internalScatter,
                                                         std::string const& distributeId) {
  auto uniq_consumer =
      std::make_unique<DistributeConsumerNode>(plan, plan->nextId(), distributeId);
  auto consumer = uniq_consumer.get();
  TRI_ASSERT(consumer != nullptr);
  // Hand over responsibility to plan, s.t. it can clean up if one of the below fails
  plan->registerNode(uniq_consumer.release());
  consumer->setIsInSplicedSubquery(internalScatter->isInSplicedSubquery());
  consumer->addDependency(internalScatter);
  consumer->cloneRegisterPlan(internalScatter);
  internalScatter->addClient(consumer);
  return consumer;
}
