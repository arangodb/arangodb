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
#include "Aql/WalkerWorker.h"
#include "Basics/StringUtils.h"
#include "Cluster/ServerState.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Aql/LocalGraphNode.h"
#endif

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace {
DistributeConsumerNode* createConsumerNode(ExecutionPlan* plan, ScatterNode* internalScatter,
                                           std::string_view const distributeId) {
  auto uniq_consumer =
      std::make_unique<DistributeConsumerNode>(plan, plan->nextId(),
                                               std::string(distributeId));
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

}  // namespace

using NodeAliasMap = std::map<ExecutionNodeId, ExecutionNodeId>;

/*
 * The CloneWorker "clones" a snippet
 *
 * rootNode -> N1 -> N2 -> ... -> Nk [-> DistributeConsumer -> internalScatter -> remoteNode]
 *
 * (the back part is optional, because we could be the last snippet and hence do not need to
 * talk to a coordinator)
 *
 * to
 *
 * internalGather -> rootNode -> CN1 -> CN2 -> ... -> CNk [ -> DistributeConsumerNode -> internalScatter ]
 *
 * where CN1 ... CNk are clones of N1 ... Nk, taking into account subquery nodes,
 * and the DistributeConsumer are only inserted (and *newly created*) if the original snippet
 * has the back part.
 *
 * Note that there is only *one* internal scatter/internal gather involved, and these are neither
 * created nor cloned in this code.
 *
 * This is used to create a plan of the form
 *
 *                      INTERNAL_SCATTER
 *            /               |                         \
 *           /                |                          \
 *  DistributeConsumer   DistributeConsumer  ...  DistributeConsumer
 *          |                 |                           |
 *         CNk               CNk                         CNk
 *          |                 |                           |
 *         ...               ...                         ...
 *          |                 |                           |
 *         CN0               CN0                         CN0
 *          \                 |                           /
 *           \                |                          /
 *                      INTERNAL_GATHER
 *
 */
class CloneWorker final : public WalkerWorker<ExecutionNode, WalkerUniqueness::NonUnique> {
 public:
  explicit CloneWorker(ExecutionNode* rootNode, GatherNode* internalGather,
                       ScatterNode* internalScatter,
                       MapNodeToColNameToShards const& localExpansions, size_t shardId,
                       std::string_view const distId, NodeAliasMap& nodeAliases);
  void process();

 private:
  bool before(ExecutionNode* node) override final;
  bool enterSubquery(ExecutionNode* subq, ExecutionNode* root) override final;

  void processAfter(ExecutionNode* node);

  void setUsedShardsOnClone(ExecutionNode* node, ExecutionNode* clone);

 private:
  ExecutionNode* _root{nullptr};
  ScatterNode* _internalScatter;
  GatherNode* _internalGather;
  MapNodeToColNameToShards const& _localExpansions;
  size_t _shardId{0};
  std::string_view const _distId;
  std::map<ExecutionNode*, ExecutionNode*> _originalToClone;
  NodeAliasMap& _nodeAliases;

  std::vector<ExecutionNode*> _stack{};
};

CloneWorker::CloneWorker(ExecutionNode* root, GatherNode* internalGather,
                         ScatterNode* internalScatter,
                         MapNodeToColNameToShards const& localExpansions, size_t shardId,
                         std::string_view const distId, NodeAliasMap& nodeAliases)
    : _root{root},
      _internalScatter{internalScatter},
      _internalGather{internalGather},
      _localExpansions(localExpansions),
      _shardId(shardId),
      _distId(distId),
      _nodeAliases{nodeAliases} {}

void CloneWorker::process() {
  _root->walk(*this); 

  // Home-brew early cancel: We collect the processed nodes on a stack
  // and process them in reverse order in processAfter
  for (auto&& n = _stack.rbegin(); n != _stack.rend(); n++) {
    processAfter(*n);
  }
}

void CloneWorker::setUsedShardsOnClone(ExecutionNode* node, ExecutionNode* clone) {
  auto permuter = _localExpansions.find(node);
  if (permuter != _localExpansions.end()) {
    if (clone->getType() == ExecutionNode::TRAVERSAL ||
        clone->getType() == ExecutionNode::SHORTEST_PATH ||
        clone->getType() == ExecutionNode::K_SHORTEST_PATHS) {
      // GraphNodes handle multiple collections
      auto graphNode = dynamic_cast<GraphNode*>(clone);
      if (graphNode != nullptr) {
        if (graphNode->isDisjoint()) {
          TRI_ASSERT(graphNode != nullptr);
          TRI_ASSERT(graphNode->isDisjoint());
          // we've found a Disjoint SmartGraph node, now add the `i` th shard for used collections
          for (auto const& myExp : permuter->second) {
            std::string const& cName = myExp.first;
            graphNode->addCollectionToShard(cName, *std::next(myExp.second.begin(), _shardId));
          }
        }
      } else {
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
      }
    } else {
      auto collectionAccessingNode = dynamic_cast<CollectionAccessingNode*>(clone);
      if (collectionAccessingNode != nullptr) {
        // we guarantee that we only have one collection here
        TRI_ASSERT(permuter->second.size() == 1);
        std::string const& cName = collectionAccessingNode->collection()->name();
        // Get the `i` th shard
        collectionAccessingNode->setUsedShard(
            *std::next(permuter->second.at(cName).begin(), _shardId));
      } else {
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
      }
    }
  }
}

bool CloneWorker::before(ExecutionNode* node) {
  auto plan = node->plan();

  // We don't clone the DistributeConsumerNode, but create a new one instead
  // This will get `internalScatter` as its sole dependency
  if (node->getType() == ExecutionNode::DISTRIBUTE_CONSUMER) {
    auto consumer = createConsumerNode(plan, _internalScatter, _distId);
    consumer->isResponsibleForInitializeCursor(false);
    _nodeAliases.try_emplace(consumer->id(), ExecutionNodeId::InternalNode);
    _originalToClone.try_emplace(node, consumer);

    // Stop here. Note that we do things special here and don't really
    // use the WalkerWorker!
    return true;
  } else if (node == _internalGather || node == _internalScatter) {
    // Never clone these nodes. We should never run into this case.
    TRI_ASSERT(false);
  } else {
    auto clone = node->clone(plan, false, false);

    // set the used shards on the clone just created. We have
    // to handle graph nodes specially as they have multiple
    // collections associated with them
    setUsedShardsOnClone(node, clone);

    TRI_ASSERT(clone->id() != node->id());
    _originalToClone.try_emplace(node, clone);
    _nodeAliases.try_emplace(clone->id(), node->id());
    _stack.push_back(node);
  }
  return false;
}

// This hooks up dependencies; we're doing this in after to make sure
// that all nodes (including those contained in subqueries!) are cloned
void CloneWorker::processAfter(ExecutionNode* node) {
  TRI_ASSERT(_originalToClone.count(node) == 1);
  auto clone = _originalToClone.at(node);

  auto deps = node->getDependencies();
  for (auto d : deps) {
    TRI_ASSERT(_originalToClone.count(d) == 1);
    auto depClone = _originalToClone.at(d);
    clone->addDependency(depClone);
  }

  if (node == _root) {
    _internalGather->addDependency(clone);
  }

  TRI_ASSERT(node->getType() != ExecutionNode::SUBQUERY);
}

bool CloneWorker::enterSubquery(ExecutionNode* subq, ExecutionNode* root) {
  // Enter all subqueries
  return true;
}

void QuerySnippet::addNode(ExecutionNode* node) {
  TRI_ASSERT(node != nullptr);
  _nodes.push_back(node);

  switch (node->getType()) {
    case ExecutionNode::REMOTE: {
      TRI_ASSERT(_remoteNode == nullptr);
      _remoteNode = ExecutionNode::castTo<RemoteNode*>(node);
      break;
    }
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

/*
 * WARNING --- WARNING
 *
 * This function changes the plan, and then (tries to) restore the original
 * state after serialisation.
 *
 * If the restoration is not complete this will lead to ugly bugs.
 *
 */
void QuerySnippet::serializeIntoBuilder(
    ServerID const& server,
    std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById,
    ShardLocking& shardLocking, std::map<ExecutionNodeId, ExecutionNodeId>& nodeAliases,
    VPackBuilder& infoBuilder) {

  ExecutionNode* remoteParent{nullptr};

  TRI_ASSERT(!_expansions.empty());
  auto firstBranchRes = prepareFirstBranch(server, nodesById, shardLocking);
  if (!firstBranchRes.ok()) {
    // We have at least one expansion that has no shard on this server.
    // we do not need to write this snippet here.
    TRI_ASSERT(firstBranchRes.is(TRI_ERROR_CLUSTER_NOT_LEADER));
    return;
  }
  MapNodeToColNameToShards& localExpansions =
      firstBranchRes.get();
  // We clone every Node* and maintain a list of ReportingGroups for profiler
  if (_remoteNode != nullptr) {
    if (!_madeResponsibleForShutdown) {
      // Enough to do this step once
      // We need to connect this Node to the sink
      // update the remote node with the information about the query
      _remoteNode->server("server:" + arangodb::ServerState::instance()->getId());
      _remoteNode->queryId(_inputSnippet);

      // A Remote can only contact a global SCATTER or GATHER node.
      TRI_ASSERT(_remoteNode->getFirstDependency() != nullptr);
      TRI_ASSERT(_remoteNode->getFirstDependency()->getType() == ExecutionNode::SCATTER ||
                 _remoteNode->getFirstDependency()->getType() == ExecutionNode::DISTRIBUTE);
      TRI_ASSERT(_remoteNode->hasDependency());

      // Need to wire up the internal scatter and distribute consumer in between the last two nodes
      // Note that we need to clean up this after we produced the snippets.
      _globalScatter =
          ExecutionNode::castTo<ScatterNode*>(_remoteNode->getFirstDependency());
      // Let the globalScatter node distribute data by server
      _globalScatter->setScatterType(ScatterNode::ScatterType::SERVER);

      // All of the above could be repeated as it is constant information.
      // this one line is the important one
      _remoteNode->isResponsibleForInitializeCursor(true);
      _madeResponsibleForShutdown = true;
    } else {
      _remoteNode->isResponsibleForInitializeCursor(false);
    }
    // We have it all serverbased now
    _remoteNode->setDistributeId(server);
    // Wire up this server to the global scatter
    TRI_ASSERT(_globalScatter != nullptr);
    _globalScatter->addClient(_remoteNode);

    // For serialization remove the dependency of Remote

    auto remoteDependencies = _remoteNode->getDependencies();
    TRI_ASSERT(remoteDependencies.size() == 1);
    TRI_ASSERT(remoteDependencies[0] == _globalScatter);

    _remoteNode->removeDependencies();
  }

  // The Key is required to build up the queryId mapping later
  infoBuilder.add(VPackValue(
      arangodb::basics::StringUtils::itoa(_idOfSinkRemoteNode.id()) + ":" + server));
  if (!localExpansions.empty()) {  // one expansion
    // We have Expansions to permutate, guaranteed they have
    // all identical lengths.
    auto const& colToShardMap = localExpansions.begin()->second;
    // We will never add an empty map
    TRI_ASSERT(!colToShardMap.empty());
    // For all collections within this map we will have the same amount of shards.
    size_t numberOfShardsToPermutate = colToShardMap.begin()->second.size();
    TRI_ASSERT(numberOfShardsToPermutate > 1);

    std::vector<std::string> distIds{};
    // Reserve the amount of localExpansions,
    distIds.reserve(numberOfShardsToPermutate);
    // Create an internal GatherNode, that will connect to all execution
    // steams of the query
    auto plan = _nodes.front()->plan();

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
    internalGather->setVarsUsedLater(_nodes.front()->getVarsUsedLaterStack());
    internalGather->setRegsToClear({});
    auto const reservedId = ExecutionNodeId::InternalNode;
    nodeAliases.try_emplace(internalGather->id(), reservedId);

    DistributeConsumerNode* prototypeConsumer{nullptr};
    ScatterNode* internalScatter{nullptr};
    if (_remoteNode != nullptr) {  // RemoteBlock talking to coordinator snippet
      TRI_ASSERT(_globalScatter != nullptr);
      TRI_ASSERT(plan == _globalScatter->plan());
      internalScatter =
          ExecutionNode::castTo<ScatterNode*>(_globalScatter->clone(plan, false, false));
      internalScatter->clearClients();

      TRI_ASSERT(_remoteNode->getDependencies().size() == 0);
      TRI_ASSERT(_remoteNode->getParents().size() == 1);

      // we store the remote parent to be able to
      // restore the plan later. Please, please don't ask why.
      remoteParent = _remoteNode->getFirstParent();
      plan->unlinkNode(_remoteNode);

      internalScatter->addDependency(_remoteNode);

      // Let the local Scatter node distribute data by SHARD
      internalScatter->setScatterType(ScatterNode::ScatterType::SHARD);
      nodeAliases.try_emplace(internalScatter->id(), reservedId);

      if (_globalScatter->getType() == ExecutionNode::DISTRIBUTE) {
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
            auto const collectionMap = exp.second.find(distCollection->name());
            TRI_ASSERT(collectionMap != exp.second.end());
            TRI_ASSERT(!collectionMap->second.empty());
            for (auto const& shardIds : collectionMap->second) {
              distIds.emplace_back(shardIds);
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

      // hook distribute node into stream '0', since that does not happen below
      prototypeConsumer = createConsumerNode(plan, internalScatter, distIds[0]);
      nodeAliases.try_emplace(prototypeConsumer->id(), ExecutionNodeId::InternalNode);
      // now wire up the temporary nodes

      TRI_ASSERT(_nodes.size() > 1);

      TRI_ASSERT(!remoteParent->hasDependency());
      remoteParent->addDependency(prototypeConsumer);
    } else {
      // TODO: Refactor the code above so that we don't copy and paste

      // Remote is nullptr, so we assume that an optimizer rule
      // removed the REMOTE/SCATTER bit of our snippet.
      for (size_t i = 0; i < numberOfShardsToPermutate; i++) {
        distIds.emplace_back(StringUtils::itoa(i));
      }
    }

    // We do not need to copy the first stream, we can use the one we have.
    // We only need copies for the other streams.
    internalGather->addDependency(_nodes.front());

    // NOTE: We will copy over the entire snippet stream here.
    // We will inject the permuted shards on the way.
    // Also note: the local plan will take memory responsibility
    // of the ExecutionNodes created during this procedure.
    TRI_ASSERT(!_nodes.empty());
    auto snippetRoot = _nodes.at(0);

    // make sure we don't explode accessing distIds
    TRI_ASSERT(numberOfShardsToPermutate == distIds.size());
    for (size_t i = 1; i < numberOfShardsToPermutate; ++i) {
      auto cloneWorker = CloneWorker(snippetRoot, internalGather, internalScatter,
                                     localExpansions, i, distIds.at(i), nodeAliases);
      // Warning, the walkerworker is abused.
      cloneWorker.process();
    }

    const unsigned flags = ExecutionNode::SERIALIZE_DETAILS;
    internalGather->toVelocyPack(infoBuilder, flags, false);

    // Clean up plan for next run
    //
    // TODO: This is so ragingly upsetting and needs to be fixed.
    //
    //       with a big hammer.
    //
    //       please.
    if (prototypeConsumer != nullptr) {
      plan->unlinkNode(prototypeConsumer);
    }
    if (internalScatter != nullptr) {
      plan->unlinkNode(internalScatter);
    }

    if (_remoteNode != nullptr) {
      TRI_ASSERT(remoteParent != nullptr);
      // Yes, we want setParent, to make sure all the
      // DistributeConsumer Junk we added upstairs is pruned.
      _remoteNode->setParent(remoteParent);
      TRI_ASSERT(remoteParent->getDependencies().size() == 1);
      TRI_ASSERT(remoteParent->getDependencies()[0] == _remoteNode);
    }
  } else {
    const unsigned flags = ExecutionNode::SERIALIZE_DETAILS;
    _nodes.front()->toVelocyPack(infoBuilder, flags, false);
  }

  if (_remoteNode != nullptr) {
    // For the local copy read the dependency of Remote
    TRI_ASSERT(_remoteNode->getDependencies().size() == 0);
    _remoteNode->addDependency(_globalScatter);
  }
}

auto QuerySnippet::prepareFirstBranch(
    ServerID const& server,
    std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById,
    ShardLocking& shardLocking) -> ResultT<MapNodeToColNameToShards> {
  MapNodeToColNameToShards localExpansions;
  std::unordered_map<ShardID, ServerID> const& shardMapping =
      shardLocking.getShardMapping();

  // It is of utmost importance that this is an ordered set of Shards.
  // We can only join identical indexes of shards for each collection
  // locally.
  std::unordered_map<std::string, std::set<ShardID>> myExpFinal;

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
        auto const* const protoCol =
            localGraphNode->isUsedAsSatellite()
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

      // This is either one shard or a single SatelliteGraph which is not used
      // as SatelliteGraph or a Disjoint SmartGraph.
      uint64_t numShards = 0;
      for (auto* aqlCollection : localGraphNode->collections()) {
        // It is of utmost importance that this is an ordered set of Shards.
        // We can only join identical indexes of shards for each collection
        // locally.
        std::set<ShardID> myExp;

        auto const& shards = shardLocking.shardsForSnippet(id(), aqlCollection);
        TRI_ASSERT(!shards.empty());
        for (auto const& shard : shards) {
          auto found = shardMapping.find(shard);
          TRI_ASSERT(found != shardMapping.end());
          // We should never have shards on other servers, except for
          // SatelliteGraphs which are used that way, or SatelliteCollections
          // (in a OneShard case) because local graphs (on DB-Servers) only ever
          // occur in either OneShard, SatelliteGraphs or Disjoint SmartGraphs.
          TRI_ASSERT(found->second == server || localGraphNode->isUsedAsSatellite() ||
                     aqlCollection->isSatellite() || localGraphNode->isDisjoint());
          // provide a correct translation from collection to shard
          // to be used in toVelocyPack methods of classes derived
          // from GraphNode
          if (localGraphNode->isDisjoint()) {
            if (found->second == server) {
              myExp.emplace(shard);
            } else {
              // the target server does not have anything to do with the particular
              // collection (e.g. because the collection's shards are all on other
              // servers), but it may be asked for this collection, because vertex
              // collections are registered _globally_ with the TraversalNode and
              // not on a per-target server basis.
              // so in order to serve later lookups for this collection, we insert
              // an empty string into the collection->shard map.
              // on lookup, we will react to this.
              localGraphNode->addCollectionToShard(aqlCollection->name(), "");
            }
          } else {
            localGraphNode->addCollectionToShard(aqlCollection->name(), shard);
          }

          numShards++;
        }
        if (myExp.size() > 0) {
          localGraphNode->addCollectionToShard(aqlCollection->name(), *myExp.begin());
          if (myExp.size() > 1) {
            myExpFinal.insert({aqlCollection->name(), std::move(myExp)});
          }
        }
      }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      // additional verification checks for Disjoint SmartGraphs
      if (localGraphNode->isDisjoint()) {
        if (!myExpFinal.empty()) {
          size_t numberOfShards = myExpFinal.begin()->second.size();
          // We need one expansion for every collection in the Graph
          TRI_ASSERT(myExpFinal.size() == localGraphNode->collections().size());
          for (auto const& expDefinition : myExpFinal) {
            TRI_ASSERT(expDefinition.second.size() == numberOfShards);
          }
        }
      }
#endif

      TRI_ASSERT(numShards > 0);
      if (numShards == 0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL,
                                       "Could not find a shard to instantiate "
                                       "for graph node when expected to");
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
      // We later use a clone mechanism to inject other shards of permutation
      auto collectionAccessingNode = dynamic_cast<CollectionAccessingNode*>(exp.node);
      TRI_ASSERT(collectionAccessingNode != nullptr);
      collectionAccessingNode->setUsedShard(*myExp.begin());

      if (!exp.doExpand) {
        TRI_ASSERT(myExp.size() == 1);
      } else {
        if (myExp.size() > 1) {
          // Only in this case we really need to do an expansion
          // Otherwise we get away with only using the main stream for
          // this server
          // NOTE: This might differ between servers.
          // One server might require an expansion (many shards) while another does not (only one shard).
          myExpFinal.insert({col->name(), std::move(myExp)});
        }
      }
    }

    if (exp.doExpand) {
      auto collectionAccessingNode = dynamic_cast<CollectionAccessingNode*>(exp.node);
      TRI_ASSERT(collectionAccessingNode != nullptr);
      TRI_ASSERT(!collectionAccessingNode->isUsedAsSatellite());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      size_t numberOfShardsToPermutate = 0;
      // set the max loop index (note this will essentially be done only once)
      // we can set first found map to overall size as they all must be the same (asserted above)
      if (myExpFinal.size() > 0) {
        numberOfShardsToPermutate = myExpFinal.begin()->second.size();
      }

      // All parts need to have exact same size, they need to be permutated pairwise!
      for (auto const& myExp : myExpFinal) {
        TRI_ASSERT(numberOfShardsToPermutate == 0 || myExp.second.size() == numberOfShardsToPermutate);
      }
#endif

      if (myExpFinal.size() > 0) {
        // we've found at least one entry to be expanded
        localExpansions.emplace(exp.node, std::move(myExpFinal));
      }
    }
  }  // for _expansions - end;
  return {localExpansions};
}
