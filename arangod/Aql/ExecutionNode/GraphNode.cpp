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
/// @author Michael Hackstein
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "GraphNode.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Collections.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Executor/TraversalExecutor.h"
#include "Aql/Expression.h"
#include "Aql/OptimizerRule.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Graph/BaseOptions.h"
#include "Graph/Graph.h"
#include "Graph/TraverserOptions.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/Aql/LocalEnumeratePathsNode.h"
#include "Enterprise/Aql/LocalShortestPathNode.h"
#include "Enterprise/Aql/LocalTraversalNode.h"
#endif

#include <absl/strings/str_cat.h>
#include <velocypack/Iterator.h>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::traverser;

namespace {

struct DisjointSmartToSatelliteTester {
  Result isCollectionAllowed(
      std::shared_ptr<LogicalCollection> const& collection,
      TRI_edge_direction_e dir) {
    // We only need to check Sat -> Smart or Smart -> Sat collection, nothing
    // else
    if (collection->isSmartToSatEdgeCollection() ||
        collection->isSatToSmartEdgeCollection()) {
      if (dir == TRI_EDGE_ANY) {
        // ANY is always forbidden
        return {
            TRI_ERROR_UNSUPPORTED_CHANGE_IN_SMART_TO_SATELLITE_DISJOINT_EDGE_DIRECTION,
            absl::StrCat(
                "Using direction 'ANY' on collection: '", collection->name(),
                "' could switch from Smart to Satellite and back. This "
                "violates the isDisjoint feature and is forbidden.")};
      }
      // Unify Edge storage, and edge read direction.
      // The smartToSatDir defines the direction in which we attempt to
      // walk from Smart to Satellite collections. (OUT: Smart -> Sat, IN: Sat
      // -> Smart)
      bool isOut = dir == TRI_EDGE_OUT;
      auto smartToSatDir = isOut == collection->isSmartToSatEdgeCollection()
                               ? TRI_EDGE_OUT
                               : TRI_EDGE_IN;
      if (_disjointSmartToSatDirection == TRI_EDGE_ANY) {
        // We have not defined the direction yet, store it, this now defines the
        // only allowed switch
        _disjointSmartToSatDirection = smartToSatDir;
        TRI_ASSERT(_conflictingCollection == nullptr);
        _conflictingCollection = collection;
        _conflictingDirection = dir;
      } else if (_disjointSmartToSatDirection != smartToSatDir) {
        // We try to switch again! This is disallowed. Let us report.
        std::stringstream errorMessage;
        errorMessage << "Using direction ";
        if (dir == TRI_EDGE_OUT) {
          errorMessage << "OUTBOUND";
        } else {
          errorMessage << "INBOUND";
        }
        auto printCollection = [&errorMessage](LogicalCollection const& col,
                                               bool isOut) {
          errorMessage << "'" << col.name() << "' switching from ";
          if (isOut == col.isSmartToSatEdgeCollection()) {
            // Hits OUTBOUND on SmartToSat and INBOUND on SatToSmart
            errorMessage << "Smart to Satellite";
          } else {
            // Hits INBOUND on SmartToSat and OUTBOUND on SatToSmart
            errorMessage << "Satellite to Smart";
          }
        };
        errorMessage << " on collection: ";
        printCollection(*collection, isOut);

        errorMessage << ". Conflicting with: ";
        if (_conflictingDirection == TRI_EDGE_OUT) {
          errorMessage << "OUTBOUND";
        } else {
          errorMessage << "INBOUND";
        }
        errorMessage << " ";
        bool conflictingIsOut = _conflictingDirection == TRI_EDGE_OUT;
        TRI_ASSERT(_conflictingCollection != nullptr);
        printCollection(*_conflictingCollection, conflictingIsOut);
        errorMessage
            << ". This violates the isDisjoint feature and is forbidden.";
        return {
            TRI_ERROR_UNSUPPORTED_CHANGE_IN_SMART_TO_SATELLITE_DISJOINT_EDGE_DIRECTION,
            errorMessage.str()};
      }
    }
    return TRI_ERROR_NO_ERROR;
  }

 private:
  TRI_edge_direction_e _disjointSmartToSatDirection{TRI_EDGE_ANY};
  std::shared_ptr<LogicalCollection> _conflictingCollection{nullptr};
  TRI_edge_direction_e _conflictingDirection{TRI_EDGE_ANY};
};

TRI_edge_direction_e uint64ToDirection(uint64_t dirNum) {
  switch (dirNum) {
    case 0:
      return TRI_EDGE_ANY;
    case 1:
      return TRI_EDGE_IN;
    case 2:
      return TRI_EDGE_OUT;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_PARSE,
          "direction can only be INBOUND, OUTBOUND or ANY");
  }
}

TRI_edge_direction_e parseDirection(AstNode const* node) {
  TRI_ASSERT(node->isIntValue() || node->type == NODE_TYPE_DIRECTION);
  AstNode const* dirNode = node;
  if (node->type == NODE_TYPE_DIRECTION) {
    TRI_ASSERT(node->numMembers() == 2);
    dirNode = node->getMember(0);
  }
  TRI_ASSERT(dirNode->isIntValue());
  return uint64ToDirection(dirNode->getIntValue());
}

}  // namespace

GraphNode::GraphNode(ExecutionPlan* plan, ExecutionNodeId id,
                     TRI_vocbase_t* vocbase, AstNode const* direction,
                     AstNode const* graph, std::unique_ptr<BaseOptions> options)
    : ExecutionNode(plan, id),
      _vocbase(vocbase),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _optimizedOutVariables({}),
      _graphObj(nullptr),
      _tmpObjVariable(_plan->getAst()->variables()->createTemporaryVariable()),
      _tmpObjVarNode(_plan->getAst()->createNodeReference(_tmpObjVariable)),
      _tmpIdNode(_plan->getAst()->createNodeValueString("", 0)),
      _defaultDirection(parseDirection(direction)),
      _optionsBuilt(false),
      _isSmart(false),
      _isDisjoint(false),
      _enabledClusterOneShardRule(false),
      _options(std::move(options)) {
  // Direction is already the correct Integer.
  // Is not inserted by user but by enum.

  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_options != nullptr);

  TRI_ASSERT(direction != nullptr);
  TRI_ASSERT(graph != nullptr);

  DisjointSmartToSatelliteTester disjointTest{};

  if (graph->type == NODE_TYPE_COLLECTION_LIST) {
    size_t edgeCollectionCount = graph->numMembers();

    _graphInfo.openArray();
    _edgeColls.reserve(edgeCollectionCount);
    _directions.reserve(edgeCollectionCount);

    determineEnterpriseFlags(graph, plan->getAst()->query().operationOrigin());

    std::unordered_map<std::string, TRI_edge_direction_e> seenCollections;
    CollectionNameResolver const& resolver = plan->getAst()->query().resolver();

    // List of edge collection names
    for (size_t i = 0; i < edgeCollectionCount; ++i) {
      auto col = graph->getMember(i);
      TRI_edge_direction_e dir = TRI_EDGE_ANY;

      if (col->type == NODE_TYPE_DIRECTION) {
        // We have a collection with special direction.
        dir = parseDirection(col->getMember(0));
        col = col->getMember(1);
      } else {
        dir = _defaultDirection;
      }

      std::string eColName = col->getString();

      if (_options->shouldExcludeEdgeCollection(eColName)) {
        // excluded edge collection
        continue;
      }

      // now do some uniqueness checks for the specified collections
      auto [it, inserted] = seenCollections.try_emplace(eColName, dir);
      if (!inserted) {
        if ((*it).second != dir) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID,
              absl::StrCat("conflicting directions specified for collection '",
                           eColName));
        }
        // do not re-add the same collection!
        continue;
      }

      auto collection = resolver.getCollection(eColName);

      if (!collection || collection->type() != TRI_COL_TYPE_EDGE) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID,
            absl::StrCat("collection type invalid for collection '", eColName,
                         ": expecting collection type 'edge'"));
      }
      if (_isDisjoint) {
        // TODO: Alternative to "THROW" we could run a community based Query
        // here, instead of a Disjoint one.
        auto res = disjointTest.isCollectionAllowed(collection, dir);
        if (res.fail()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
        }
      }

      auto& collections = plan->getAst()->query().collections();

      _graphInfo.add(VPackValue(eColName));
      if (ServerState::instance()->isRunningInCluster()) {
        if (!collection->isSmart()) {
          addEdgeCollection(collections, eColName, dir);
        } else {
          addEdgeAlias(eColName);
          std::vector<std::string> names;
          if (_isSmart) {
            names = collection->realNames();
          } else {
            names = collection->realNamesForRead();
          }
          for (auto const& name : names) {
            addEdgeCollection(collections, name, dir);
          }
        }
      } else {
        addEdgeCollection(collections, eColName, dir);
      }
    }
    _graphInfo.close();
  } else if (graph->isStringValue()) {
    std::string graphName = graph->getString();
    _graphInfo.add(VPackValue(graphName));
    auto graphLookupResult =
        plan->getAst()->query().lookupGraphByName(graphName);

    if (graphLookupResult.fail()) {
      THROW_ARANGO_EXCEPTION(graphLookupResult.result());
    }

    TRI_ASSERT(graphLookupResult.ok());
    _graphObj = graphLookupResult.get();

    auto eColls = _graphObj->edgeCollections();
    size_t length = eColls.size();
    if (length == 0) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_EMPTY);
    }

    // Just use the Graph Object information
    if (ServerState::instance()->isRunningInCluster()) {
      _isSmart = _graphObj->isSmart();
      _isDisjoint = _graphObj->isDisjoint();
    }
    auto& ci = _vocbase->server().getFeature<ClusterFeature>().clusterInfo();
    auto& collections = plan->getAst()->query().collections();
    for (auto const& n : eColls) {
      if (_options->shouldExcludeEdgeCollection(n)) {
        // excluded edge collection
        continue;
      }

      if (ServerState::instance()->isRunningInCluster()) {
        auto c = ci.getCollection(_vocbase->name(), n);
        if (_isDisjoint) {
          // TODO: Alternative to "THROW" we could run a community based Query
          // here, instead of a Disjoint one.
          auto res = disjointTest.isCollectionAllowed(c, _defaultDirection);
          if (res.fail()) {
            THROW_ARANGO_EXCEPTION(res);
          }
        }
        if (!c->isSmart()) {
          addEdgeCollection(collections, n, _defaultDirection);
        } else {
          addEdgeAlias(n);
          std::vector<std::string> names;
          if (_isSmart) {
            names = c->realNames();
          } else {
            names = c->realNamesForRead();
          }
          for (auto const& name : names) {
            addEdgeCollection(collections, name, _defaultDirection);
          }
        }
      } else {
        addEdgeCollection(collections, n, _defaultDirection);
      }
    }

    auto vColls = _graphObj->vertexCollections();
    length = vColls.size();
    if (length == 0) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_EMPTY);
    }
    _vertexColls.reserve(length);
    for (auto const& v : vColls) {
      addVertexCollection(collections, v);
    }
  }
}

GraphNode::GraphNode(ExecutionPlan* plan, velocypack::Slice base)
    : ExecutionNode(plan, base),
      _vocbase(&(plan->getAst()->query().vocbase())),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _optimizedOutVariables({}),
      _graphObj(nullptr),
      _tmpObjVariable(nullptr),
      _tmpObjVarNode(nullptr),
      _tmpIdNode(nullptr),
      _defaultDirection(
          uint64ToDirection(arangodb::basics::VelocyPackHelper::stringUInt64(
              base.get("defaultDirection")))),
      _optionsBuilt(false),
      _isSmart(arangodb::basics::VelocyPackHelper::getBooleanValue(
          base, StaticStrings::IsSmart, false)),
      _isDisjoint(arangodb::basics::VelocyPackHelper::getBooleanValue(
          base, StaticStrings::IsDisjoint, false)),
      _enabledClusterOneShardRule(
          arangodb::basics::VelocyPackHelper::getBooleanValue(
              base, StaticStrings::ForceOneShardAttributeValue, false)) {
  if (!ServerState::instance()->isDBServer()) {
    // Graph Information. Do we need to reload the graph here?
    std::string graphName;
    if (base.get("graph").isString()) {
      graphName = base.get("graph").copyString();
      if (base.hasKey("graphDefinition")) {
        // load graph and store pointer
        auto graphLookupResult =
            plan->getAst()->query().lookupGraphByName(graphName);

        if (graphLookupResult.fail()) {
          THROW_ARANGO_EXCEPTION(graphLookupResult.result());
        }

        _graphObj = graphLookupResult.get();
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                       "missing graphDefinition.");
      }
    } else {
      _graphInfo.add(base.get("graph"));
      if (!_graphInfo.slice().isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                       "graph has to be an array.");
      }
    }
  }

  // Collection information
  VPackSlice edgeCollections = base.get("edgeCollections");
  if (!edgeCollections.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                   "graph needs an array of edge collections.");
  }
  // Directions
  VPackSlice dirList = base.get("directions");
  if (!dirList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                   "graph needs an array of directions.");
  }

  if (edgeCollections.length() != dirList.length()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_BAD_JSON_PLAN,
        "graph needs the same number of edge collections and directions.");
  }

  QueryContext& query = plan->getAst()->query();
  // MSVC did throw an internal compiler error with auto instead of std::string
  // here at the time of this writing (some MSVC 2019 14.25.28610). Could
  // reproduce it only in our CI, my local MSVC (same version) ran fine...
  auto getAqlCollectionFromName =
      [&](std::string const& name) -> aql::Collection& {
    // if the collection was already existent in the query, addCollection will
    // just return it.
    return *query.collections().add(name, AccessMode::Type::READ,
                                    aql::Collection::Hint::Collection);
  };

  auto vPackDirListIter = VPackArrayIterator(dirList);
  auto vPackEdgeColIter = VPackArrayIterator(edgeCollections);
  for (auto dirIt = vPackDirListIter.begin(), edgeIt = vPackEdgeColIter.begin();
       dirIt != vPackDirListIter.end() && edgeIt != vPackEdgeColIter.end();
       ++dirIt, ++edgeIt) {
    uint64_t dir = arangodb::basics::VelocyPackHelper::stringUInt64(*dirIt);
    TRI_edge_direction_e d = uint64ToDirection(dir);
    // Only TRI_EDGE_IN and TRI_EDGE_OUT allowed here
    TRI_ASSERT(d == TRI_EDGE_IN || d == TRI_EDGE_OUT);
    std::string e =
        arangodb::basics::VelocyPackHelper::getStringValue(*edgeIt, "");
    auto& aqlCollection = getAqlCollectionFromName(e);
    addEdgeCollection(aqlCollection, d);
    addEdgeAlias(e);
  }

  VPackSlice vertexCollections = base.get("vertexCollections");

  if (!vertexCollections.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_BAD_JSON_PLAN,
        "graph needs an array of vertex collections.");
  }

  for (VPackSlice it : VPackArrayIterator(vertexCollections)) {
    std::string v = arangodb::basics::VelocyPackHelper::getStringValue(it, "");
    auto& aqlCollection = getAqlCollectionFromName(v);
    addVertexCollection(aqlCollection);
  }

  // translations for one-shard-databases
  VPackSlice collectionToShard = base.get("collectionToShard");
  if (!collectionToShard.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_BAD_JSON_PLAN,
        "graph needs a translation from collection to shard names");
  }
  for (auto const& item : VPackObjectIterator(collectionToShard)) {
    auto maybeShardID = ShardID::shardIdFromString(item.value.stringView());
    if (ADB_UNLIKELY(maybeShardID.fail())) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     maybeShardID.errorMessage());
    }
    _collectionToShard.insert({item.key.copyString(), maybeShardID.get()});
  }

  // Out variables
  if (base.hasKey("vertexOutVariable")) {
    _vertexOutVariable =
        Variable::varFromVPack(plan->getAst(), base, "vertexOutVariable");
  }
  if (base.hasKey("edgeOutVariable")) {
    _edgeOutVariable =
        Variable::varFromVPack(plan->getAst(), base, "edgeOutVariable");
  }

  VPackSlice optimizedOutVariables = base.get("optimizedOutVariables");
  if (optimizedOutVariables.isArray()) {
    for (auto const& var : VPackArrayIterator(optimizedOutVariables)) {
      if (var.isNumber()) {
        _optimizedOutVariables.emplace(var.getNumber<VariableId>());
      }
    }
  }

  // Temporary Filter Objects
  TRI_ASSERT(base.hasKey("tmpObjVariable"));
  _tmpObjVariable =
      Variable::varFromVPack(plan->getAst(), base, "tmpObjVariable");

  TRI_ASSERT(base.hasKey("tmpObjVarNode"));
  // the plan's AST takes ownership of the newly created AstNode, so this is
  // safe cppcheck-suppress *
  _tmpObjVarNode = plan->getAst()->createNode(base.get("tmpObjVarNode"));

  TRI_ASSERT(base.hasKey("tmpIdNode"));
  // the plan's AST takes ownership of the newly created AstNode, so this is
  // safe cppcheck-suppress *
  _tmpIdNode = plan->getAst()->createNode(base.get("tmpIdNode"));

  VPackSlice opts = base.get("options");
  if (!opts.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                   "graph options have to be a json-object.");
  }

  _options =
      BaseOptions::createOptionsFromSlice(_plan->getAst()->query(), opts);
  // set traversal-translations
  _options->setCollectionToShard(
      _collectionToShard);  // could be moved as it will only be used here
  if (_options->parallelism() > 1) {
    _plan->getAst()->setContainsParallelNode();
  }
}

/// @brief Internal constructor to clone the node.
GraphNode::GraphNode(ExecutionPlan* plan, ExecutionNodeId id,
                     TRI_vocbase_t* vocbase,
                     std::vector<Collection*> const& edgeColls,
                     std::vector<Collection*> const& vertexColls,
                     TRI_edge_direction_e defaultDirection,
                     std::vector<TRI_edge_direction_e> directions,
                     std::unique_ptr<graph::BaseOptions> options,
                     Graph const* graph)
    : ExecutionNode(plan, id),
      _vocbase(vocbase),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _optimizedOutVariables({}),
      _graphObj(graph),
      _tmpObjVariable(_plan->getAst()->variables()->createTemporaryVariable()),
      _tmpObjVarNode(_plan->getAst()->createNodeReference(_tmpObjVariable)),
      _tmpIdNode(_plan->getAst()->createNodeValueString("", 0)),
      _defaultDirection(defaultDirection),
      _optionsBuilt(false),
      _isSmart(false),
      _isDisjoint(false),
      _enabledClusterOneShardRule(false),
      _directions(std::move(directions)),
      _options(std::move(options)) {
  setGraphInfoAndCopyColls(edgeColls, vertexColls);
}

#ifndef USE_ENTERPRISE
void GraphNode::determineEnterpriseFlags(
    AstNode const*, transaction::OperationOrigin /*operationOrigin*/) {
  _isSmart = false;
  _isDisjoint = false;
  _enabledClusterOneShardRule = false;
}
#endif

void GraphNode::setGraphInfoAndCopyColls(
    std::vector<Collection*> const& edgeColls,
    std::vector<Collection*> const& vertexColls) {
  if (_graphObj == nullptr) {
    _graphInfo.openArray();
    for (auto const& it : edgeColls) {
      if (it == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "access to non-existing edge collection in AQL graph traversal");
      }
      _edgeColls.emplace_back(it);
      _graphInfo.add(VPackValue(it->name()));
    }
    _graphInfo.close();
  } else {
    _graphInfo.add(VPackValue(_graphObj->name()));
    for (auto const& it : edgeColls) {
      if (it == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "access to non-existing edge collection in AQL graph traversal");
      }
      _edgeColls.emplace_back(it);
    }
  }

  for (auto& it : vertexColls) {
    if (it == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "access to non-existing vertex collection in AQL graph traversal");
    }
    addVertexCollection(*it);
  }
}

GraphNode::GraphNode(ExecutionPlan& plan, GraphNode const& other,
                     std::unique_ptr<graph::BaseOptions> options)
    : ExecutionNode(plan, other),
      _vocbase(other._vocbase),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _optimizedOutVariables(other._optimizedOutVariables),
      _graphObj(other.graph()),
      _tmpObjVariable(_plan->getAst()->variables()->createTemporaryVariable()),
      _tmpObjVarNode(_plan->getAst()->createNodeReference(_tmpObjVariable)),
      _tmpIdNode(_plan->getAst()->createNodeValueString("", 0)),
      _defaultDirection(other._defaultDirection),
      _optionsBuilt(false),
      _isSmart(other.isSmart()),
      _isDisjoint(other.isDisjoint()),
      _enabledClusterOneShardRule(other.isClusterOneShardRuleEnabled()),
      _directions(other._directions),
      _options(std::move(options)),
      _collectionToShard(other._collectionToShard) {
  setGraphInfoAndCopyColls(other.edgeColls(), other.vertexColls());
}

GraphNode::GraphNode(THIS_THROWS_WHEN_CALLED)
    : ExecutionNode(nullptr, ExecutionNodeId{0}), _defaultDirection() {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

void GraphNode::doToVelocyPack(velocypack::Builder& nodes,
                               unsigned flags) const {
  // TODO We need Both?!
  // Graph definition
  nodes.add("graph", _graphInfo.slice());
  nodes.add("isLocalGraphNode", VPackValue(isLocalGraphNode()));
  nodes.add("isUsedAsSatellite", VPackValue(isUsedAsSatellite()));

  // Graph Definition
  if (_graphObj != nullptr) {
    nodes.add(VPackValue("graphDefinition"));
    _graphObj->toVelocyPack(nodes);
  }

  // Default Direction
  nodes.add("defaultDirection", VPackValue(_defaultDirection));

  // Directions
  nodes.add(VPackValue("directions"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& d : _directions) {
      nodes.add(VPackValue(d));
    }
  }

  auto appendCollectionOrShard = [&](Collection const* col,
                                     VPackBuilder& builder) {
    TRI_ASSERT(builder.isOpenArray());
    // if the mapped shard for a collection is empty, it means that
    // we have a collection that is only relevant on some of the
    // target servers
    TRI_ASSERT(col != nullptr);

    if (_collectionToShard.empty()) {
      builder.add(VPackValue(col->name()));
    } else {
      auto found = _collectionToShard.find(col->name());
      if (found == _collectionToShard.end()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL_AQL,
            absl::StrCat("unable to find shard '", col->name(),
                         "' in query shard map"));
      }
      if (found->second.isValid()) {
        builder.add(VPackValue(found->second));
      }
    }
  };
  // Collections
  nodes.add(VPackValue("edgeCollections"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& e : _edgeColls) {
      appendCollectionOrShard(e, nodes);
    }
  }

  nodes.add(VPackValue("vertexCollections"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& v : _vertexColls) {
      appendCollectionOrShard(v, nodes);
    }
  }

  // translations for one-shard-databases
  nodes.add(VPackValue("collectionToShard"));
  {
    VPackObjectBuilder guard(&nodes);
    for (auto const& item : _collectionToShard) {
      nodes.add(item.first, VPackValue(item.second));
    }
  }

  nodes.add(VPackValue("optimizedOutVariables"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& var : _optimizedOutVariables) {
      nodes.add(VPackValue(var));
    }
  }

  // Flags
  nodes.add(StaticStrings::IsSmart, VPackValue(_isSmart));
  nodes.add(StaticStrings::IsDisjoint, VPackValue(_isDisjoint));
  nodes.add(StaticStrings::ForceOneShardAttributeValue,
            VPackValue(_enabledClusterOneShardRule));

  // Temporary AST Nodes for conditions
  TRI_ASSERT(_tmpObjVariable != nullptr);
  nodes.add(VPackValue("tmpObjVariable"));
  _tmpObjVariable->toVelocyPack(nodes);

  TRI_ASSERT(_tmpObjVarNode != nullptr);
  nodes.add(VPackValue("tmpObjVarNode"));
  _tmpObjVarNode->toVelocyPack(nodes, flags != 0);

  TRI_ASSERT(_tmpIdNode != nullptr);
  nodes.add(VPackValue("tmpIdNode"));
  _tmpIdNode->toVelocyPack(nodes, flags != 0);

  nodes.add(VPackValue("options"));
  _options->toVelocyPack(nodes);

  nodes.add(VPackValue("indexes"));
  _options->toVelocyPackIndexes(nodes);

  // serialize index hint
  hint().toVelocyPack(nodes);
}

void GraphNode::graphCloneHelper(ExecutionPlan&, GraphNode& clone) const {
  clone._isSmart = _isSmart;
  clone._isDisjoint = _isDisjoint;
  clone._enabledClusterOneShardRule = _enabledClusterOneShardRule;

  // Optimized Out Variables
  clone._optimizedOutVariables = _optimizedOutVariables;
}

CostEstimate GraphNode::estimateCost() const {
  CostEstimate estimate = _dependencies.at(0)->getCost();
  size_t incoming = estimate.estimatedNrItems;
  if (_optionsBuilt) {
    // We know which indexes are in use.
    estimate.estimatedCost +=
        incoming * _options->estimateCost(estimate.estimatedNrItems);
  } else {
    // Some hard-coded value, this is identical to build lookupInfos
    // if no index estimate is availble (and it is not as long as the options
    // are not built)
    double baseCost = 1;
    size_t baseNumItems = 0;
    for (auto& e : _edgeColls) {
      TRI_ASSERT(e != nullptr);
      auto count = e->count(_options->trx(), transaction::CountType::kTryCache);
      // Assume an estimate if 10% hit rate
      baseCost *= count / 10;
      baseNumItems += static_cast<size_t>(std::ceil(count / 10));
    }

    size_t estDepth = _options->estimateDepth();
    double tmpNrItems = incoming * std::pow(baseNumItems, estDepth);
    // Protect against size_t overflow, just to be sure.
    if (tmpNrItems > static_cast<double>(std::numeric_limits<size_t>::max())) {
      // This will be an expensive query...
      estimate.estimatedNrItems = std::numeric_limits<size_t>::max();
    } else {
      estimate.estimatedNrItems += static_cast<size_t>(tmpNrItems);
    }
    estimate.estimatedCost += incoming * std::pow(baseCost, estDepth);
  }

  return estimate;
}

AsyncPrefetchEligibility GraphNode::canUseAsyncPrefetching() const noexcept {
  if (_options->parallelism() == 1) {
    if (ServerState::instance()->isSingleServer()) {
      // in single server mode, we allow async prefetching if the
      // traversal itself does not use parallelism
      return AsyncPrefetchEligibility::kEnableForNode;
    } else if (ServerState::instance()->isCoordinator()) {
#ifdef USE_ENTERPRISE
      if (plan()->hasAppliedRule(OptimizerRule::clusterOneShardRule)) {
        // we can also allow async prefetching when we already applied
        // the "cluster-one-shard" rule. when this rule triggers, all
        // traversals are on the DB server and we do not use any
        // TraverserEngines.
        return AsyncPrefetchEligibility::kEnableForNode;
      }
#endif
      // fallthrough intentional
    }
  }
  // in cluster mode, we have to disallow prefetching because we may
  // have TraverserEngines.
  return AsyncPrefetchEligibility::kDisableGlobally;
}

void GraphNode::addEngine(aql::EngineId engineId, ServerID const& server) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  _engines.try_emplace(server, engineId);
}

/// @brief Returns a reference to the engines. (CLUSTER ONLY)
std::unordered_map<ServerID, aql::EngineId> const* GraphNode::engines() const {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  return &_engines;
}

/// @brief Clears the graph Engines. (CLUSTER ONLY)
/// NOTE: Use with care, if you do not refill
/// the engines this graph node cannot communicate.
/// and will yield no results.
void GraphNode::clearEngines() {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  _engines.clear();
}

BaseOptions* GraphNode::options() const { return _options.get(); }

AstNode* GraphNode::getTemporaryRefNode() const { return _tmpObjVarNode; }

Variable const* GraphNode::getTemporaryVariable() const {
  return _tmpObjVariable;
}

void GraphNode::getConditionVariables(std::vector<Variable const*>& res) const {
  // No variables used, nothing todo
}

Collection const* GraphNode::getShardingPrototype() const {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  if (_edgeColls.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "AQL graph traversal without edge collections");
  }

  TRI_ASSERT(!_edgeColls.empty());
  for (auto const* c : _edgeColls) {
    if (c == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "access to non-existing collection in AQL graph traversal");
    }
    // We are required to valuate non-satellites above
    // satellites, as the collection is used as the prototype
    // for this graphs sharding.
    // The Satellite collection does not have sharding.
    TRI_ASSERT(c != nullptr);
    if (!c->isSatellite()) {
      return c;
    }
  }
  // We have not found any non-satellite Collection
  // just return the first satellite then.
  TRI_ASSERT(_edgeColls.front() != nullptr);
  return _edgeColls.front();
}

Collection const* GraphNode::collection() const {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  TRI_ASSERT(!_vertexColls.empty());
  if (_vertexColls.empty()) {
    return getShardingPrototype();
  }

  for (auto const* vC : _vertexColls) {
    // We are required to valuate non-satellites above
    // satellites, as the collection is used as the prototype
    // for these graphs sharding.
    // The Satellite collection does not have sharding.
    TRI_ASSERT(vC != nullptr);
    if (!vC->isSatellite() && vC->type() == TRI_COL_TYPE_DOCUMENT) {
      auto const& shardID = vC->shardKeys(false);
      if (shardID.size() == 1 &&
          shardID.at(0) == StaticStrings::PrefixOfKeyString) {
        return vC;
      }
    }
  }

  for (auto const* eC : _edgeColls) {
    // We are required to valuate non-satellites above
    // satellites, as the collection is used as the prototype
    // for these graphs sharding.
    // The Satellite collection does not have sharding.
    TRI_ASSERT(eC != nullptr);
    if (!eC->isSatellite() && eC->type() == TRI_COL_TYPE_EDGE) {
      auto const& shardID = eC->shardKeys(false);
      if (shardID.size() == 1 &&
          shardID.at(0) == StaticStrings::PrefixOfKeyString) {
        return eC;
      }
    }
  }

  // We have not found any non-satellite Collection
  // just return the first satellite then.
  TRI_ASSERT(_vertexColls.front() != nullptr);
  return _vertexColls.front();
}

void GraphNode::injectVertexCollection(aql::Collection& other) {
  TRI_ASSERT(ServerState::instance()->isCoordinator() ||
             ServerState::instance()->isSingleServer());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // This is a workaround to inject all unknown aql collections into
  // this node, that should be list of unique values!
  for (auto const& v : _vertexColls) {
    TRI_ASSERT(v->name() != other.name());
  }
#endif
  addVertexCollection(other);
}

#ifndef USE_ENTERPRISE
void GraphNode::enhanceEngineInfo(VPackBuilder& builder) const {
  if (_graphObj != nullptr) {
    _graphObj->enhanceEngineInfo(builder);
  } else {
    // TODO enhance the Info based on EdgeCollections.
  }
}
#endif

void GraphNode::addEdgeCollection(Collections const& collections,
                                  std::string const& name,
                                  TRI_edge_direction_e dir) {
  auto aqlCollection = collections.get(name);
  if (aqlCollection != nullptr) {
    addEdgeCollection(*aqlCollection, dir);
  } else {
    std::string msg =
        absl::StrCat("Edge collection `", name, "` could not be found");
    if (_graphObj != nullptr) {
      msg += absl::StrCat(", but is part of graph `", _graphObj->name(), "`");
    }
    msg += ".";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   std::move(msg));
  }
}

void GraphNode::addEdgeCollection(aql::Collection& collection,
                                  TRI_edge_direction_e dir) {
  auto add = [this](aql::Collection& collection, TRI_edge_direction_e dir) {
    _directions.emplace_back(dir);
    _edgeColls.emplace_back(&collection);
  };

  if (_isSmart) {
    auto const& n = collection.name();
    if (n.starts_with(StaticStrings::FullFromPrefix)) {
      if (dir != TRI_EDGE_IN) {
        add(collection, TRI_EDGE_OUT);
      }
      return;
    } else if (n.starts_with(StaticStrings::FullToPrefix)) {
      if (dir != TRI_EDGE_OUT) {
        add(collection, TRI_EDGE_IN);
      }
      return;
    }
    // fallthrough intentional
  }

  if (dir == TRI_EDGE_ANY) {
    // ANY = OUT + IN
    add(collection, TRI_EDGE_OUT);
    add(collection, TRI_EDGE_IN);
  } else {
    add(collection, dir);
  }

  TRI_ASSERT(_directions.size() == _edgeColls.size());
}

void GraphNode::addEdgeAlias(std::string const& name) {
  _edgeAliases.emplace(name);
}

void GraphNode::addVertexCollection(Collections const& collections,
                                    std::string const& name) {
  auto aqlCollection = collections.get(name);
  if (aqlCollection != nullptr) {
    addVertexCollection(*aqlCollection);
  } else {
    std::string msg =
        absl::StrCat("Vertex collection `", name, "` could not be found");
    // If we have vertex collections, _graphObj should never be nullptr.
    // But let's stay on the side that's obviously safe without any context.
    if (_graphObj != nullptr) {
      msg += absl::StrCat(", but is part of graph `", _graphObj->name(), "`");
    }
    msg += ".";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   std::move(msg));
  }
}

#ifndef USE_ENTERPRISE
void GraphNode::addVertexCollection(aql::Collection& collection) {
  _vertexColls.emplace_back(&collection);
}
#endif

std::vector<aql::Collection const*> GraphNode::collections() const {
  std::unordered_set<aql::Collection const*> set;
  set.reserve(_edgeColls.size() + _vertexColls.size());

  for (auto const& collPointer : _edgeColls) {
    TRI_ASSERT(collPointer != nullptr);
    set.emplace(collPointer);
  }
  for (auto const& collPointer : _vertexColls) {
    TRI_ASSERT(collPointer != nullptr);
    set.emplace(collPointer);
  }

  std::vector<aql::Collection const*> vector;
  vector.reserve(set.size());
  std::move(set.begin(), set.end(), std::back_inserter(vector));

  return vector;
}

bool GraphNode::isSmart() const { return _isSmart; }

bool GraphNode::isDisjoint() const { return _isDisjoint; }

TRI_vocbase_t* GraphNode::vocbase() const { return _vocbase; }

Variable const* GraphNode::vertexOutVariable() const {
  return _vertexOutVariable;
}

bool GraphNode::isVertexOutVariableUsedLater() const {
  return _vertexOutVariable != nullptr && _options->produceVertices();
}

void GraphNode::setVertexOutput(Variable const* outVar) {
  if (outVar == nullptr) {
    markUnusedConditionVariable(_vertexOutVariable);
  }
  _vertexOutVariable = outVar;
}

void GraphNode::markUnusedConditionVariable(Variable const* var) {
  _optimizedOutVariables.emplace(var->id);
}

Variable const* GraphNode::edgeOutVariable() const { return _edgeOutVariable; }

bool GraphNode::isEdgeOutVariableUsedLater() const {
  return _edgeOutVariable != nullptr && _options->produceEdges();
}

void GraphNode::setEdgeOutput(Variable const* outVar) {
  if (outVar == nullptr) {
    markUnusedConditionVariable(_edgeOutVariable);
  }
  _edgeOutVariable = outVar;
}

std::vector<aql::Collection*> const& GraphNode::edgeColls() const {
  return _edgeColls;
}

std::vector<aql::Collection*> const& GraphNode::vertexColls() const {
  return _vertexColls;
}

graph::Graph const* GraphNode::graph() const noexcept { return _graphObj; }

void GraphNode::initializeIndexConditions() const {
  // We need to prepare the variable accesses before we ask the index nodes.
  // Those are located on the options, and need to be partially executed.
  options()->initializeIndexConditions(
      plan()->getAst(), getRegisterPlan()->varInfo, getTemporaryVariable());
}

void GraphNode::setVertexProjections(Projections projections) {
  options()->setVertexProjections(std::move(projections));
}

void GraphNode::setEdgeProjections(Projections projections) {
  options()->setEdgeProjections(std::move(projections));
}

bool GraphNode::isEligibleAsSatelliteTraversal() const {
  return graph() != nullptr && graph()->isSatellite();
}

IndexHint const& GraphNode::hint() const { return options()->hint(); }

/* Enterprise features */

#ifndef USE_ENTERPRISE

bool GraphNode::isUsedAsSatellite() const { return false; }

bool GraphNode::isLocalGraphNode() const { return false; }

bool GraphNode::isHybridDisjoint() const { return false; }

void GraphNode::enableClusterOneShardRule(bool enable) { TRI_ASSERT(false); }

bool GraphNode::isClusterOneShardRuleEnabled() const { return false; }

#endif
