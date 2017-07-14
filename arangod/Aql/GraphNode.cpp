////////////////////////////////////////////////////////////////////////////////
/// @brief Generic Node for Graph operations in AQL
///
/// @file arangod/Aql/GraphNode.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "GraphNode.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Cluster/ServerState.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "Graph/BaseOptions.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::traverser;

static TRI_edge_direction_e parseDirection(AstNode const* node) {
  TRI_ASSERT(node->isIntValue());
  auto dirNum = node->getIntValue();

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

GraphNode::GraphNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
                     AstNode const* direction, AstNode const* graph,
                     std::unique_ptr<BaseOptions>& options)
    : ExecutionNode(plan, id),
      _vocbase(vocbase),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _graphObj(nullptr),
      _tmpObjVariable(_plan->getAst()->variables()->createTemporaryVariable()),
      _tmpObjVarNode(_plan->getAst()->createNodeReference(_tmpObjVariable)),
      _tmpIdNode(_plan->getAst()->createNodeValueString("", 0)),
      _options(std::move(options)),
      _optionsBuilt(false),
      _isSmart(false) {
  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_options != nullptr);

  TRI_ASSERT(direction != nullptr);
  TRI_ASSERT(graph != nullptr);

  // Direction is already the correct Integer.
  // Is not inserted by user but by enum.
  TRI_edge_direction_e baseDirection = parseDirection(direction);

  std::unordered_map<std::string, TRI_edge_direction_e> seenCollections;

  if (graph->type == NODE_TYPE_COLLECTION_LIST) {
    size_t edgeCollectionCount = graph->numMembers();

    _graphInfo.openArray();
    _edgeColls.reserve(edgeCollectionCount);
    _directions.reserve(edgeCollectionCount);

    // First determine whether all edge collections are smart and sharded
    // like a common collection:
    auto ci = ClusterInfo::instance();
    if (ServerState::instance()->isRunningInCluster()) {
      _isSmart = true;
      std::string distributeShardsLike;
      for (size_t i = 0; i < edgeCollectionCount; ++i) {
        auto col = graph->getMember(i);
        if (col->type == NODE_TYPE_DIRECTION) {
          col = col->getMember(1);  // The first member always is the collection
        }
        std::string n = col->getString();
        auto c = ci->getCollection(_vocbase->name(), n);
        if (!c->isSmart() || c->distributeShardsLike().empty()) {
          _isSmart = false;
          break;
        }
        if (distributeShardsLike.empty()) {
          distributeShardsLike = c->distributeShardsLike();
        } else if (distributeShardsLike != c->distributeShardsLike()) {
          _isSmart = false;
          break;
        }
      }
    }

    auto resolver = std::make_unique<CollectionNameResolver>(vocbase);

    // List of edge collection names
    for (size_t i = 0; i < edgeCollectionCount; ++i) {
      auto col = graph->getMember(i);
      TRI_edge_direction_e dir = TRI_EDGE_ANY;

      if (col->type == NODE_TYPE_DIRECTION) {
        // We have a collection with special direction.
        dir = parseDirection(col->getMember(0));
        col = col->getMember(1);
      } else {
        dir = baseDirection;
      }

      std::string eColName = col->getString();

      // now do some uniqueness checks for the specified collections
      auto it = seenCollections.find(eColName);
      if (it != seenCollections.end()) {
        if ((*it).second != dir) {
          std::string msg("conflicting directions specified for collection '" +
                          std::string(eColName));
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID, msg);
        }
        // do not re-add the same collection!
        continue;
      }
      seenCollections.emplace(eColName, dir);

      if (resolver->getCollectionTypeCluster(eColName) != TRI_COL_TYPE_EDGE) {
        std::string msg("collection type invalid for collection '" +
                        std::string(eColName) +
                        ": expecting collection type 'edge'");
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID,
                                       msg);
      }

      _graphInfo.add(VPackValue(eColName));
      if (ServerState::instance()->isRunningInCluster()) {
        auto c = ci->getCollection(_vocbase->name(), eColName);
        if (!c->isSmart()) {
          addEdgeCollection(eColName, dir);
        } else {
          std::vector<std::string> names;
          if (_isSmart) {
            names = c->realNames();
          } else {
            names = c->realNamesForRead();
          }
          for (auto const& name : names) {
            addEdgeCollection(name, dir);
          }
        }
      } else {
        addEdgeCollection(eColName, dir);
      }
    }
    _graphInfo.close();
  } else {
    if (_edgeColls.empty()) {
      if (graph->isStringValue()) {
        std::string graphName = graph->getString();
        _graphInfo.add(VPackValue(graphName));
        _graphObj = plan->getAst()->query()->lookupGraphByName(graphName);

        if (_graphObj == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
        }

        auto eColls = _graphObj->edgeCollections();
        size_t length = eColls.size();
        if (length == 0) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_EMPTY);
        }

        // First determine whether all edge collections are smart and sharded
        // like a common collection:
        auto ci = ClusterInfo::instance();
        if (ServerState::instance()->isRunningInCluster()) {
          _isSmart = true;
          std::string distributeShardsLike;
          for (auto const& n : eColls) {
            auto c = ci->getCollection(_vocbase->name(), n);
            if (!c->isSmart() || c->distributeShardsLike().empty()) {
              _isSmart = false;
              break;
            }
            if (distributeShardsLike.empty()) {
              distributeShardsLike = c->distributeShardsLike();
            } else if (distributeShardsLike != c->distributeShardsLike()) {
              _isSmart = false;
              break;
            }
          }
        }

        for (const auto& n : eColls) {
          if (ServerState::instance()->isRunningInCluster()) {
            auto c = ci->getCollection(_vocbase->name(), n);
            if (!c->isSmart()) {
              addEdgeCollection(n, baseDirection);
            } else {
              std::vector<std::string> names;
              if (_isSmart) {
                names = c->realNames();
              } else {
                names = c->realNamesForRead();
              }
              for (auto const& name : names) {
                addEdgeCollection(name, baseDirection);
              }
            }
          } else {
            addEdgeCollection(n, baseDirection);
          }
        }

        auto vColls = _graphObj->vertexCollections();
        length = vColls.size();
        if (length == 0) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_EMPTY);
        }
        _vertexColls.reserve(length);
        for (auto const& v : vColls) {
          _vertexColls.emplace_back(std::make_unique<aql::Collection>(
              v, _vocbase, AccessMode::Type::READ));
        }
      }
    }
  }
}

GraphNode::GraphNode(ExecutionPlan* plan,
                     arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _graphObj(nullptr),
      _tmpObjVariable(nullptr),
      _tmpObjVarNode(nullptr),
      _tmpIdNode(nullptr),
      _options(nullptr),
      _optionsBuilt(false),
      _isSmart(false) {
  // Directions
  VPackSlice dirList = base.get("directions");
  for (auto const& it : VPackArrayIterator(dirList)) {
    uint64_t dir = arangodb::basics::VelocyPackHelper::stringUInt64(it);
    TRI_edge_direction_e d;
    switch (dir) {
      case 0:
        TRI_ASSERT(false);
        break;
      case 1:
        d = TRI_EDGE_IN;
        break;
      case 2:
        d = TRI_EDGE_OUT;
        break;
      default:
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "Invalid direction value");
        break;
    }
    _directions.emplace_back(d);
  }

  // Graph Information. Do we need to reload the graph here?
  std::string graphName;
  if (base.hasKey("graph") && (base.get("graph").isString())) {
    graphName = base.get("graph").copyString();
    if (base.hasKey("graphDefinition")) {
      _graphObj = plan->getAst()->query()->lookupGraphByName(graphName);

      if (_graphObj == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
      }
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

  // Collection information
  VPackSlice list = base.get("edgeCollections");
  if (!list.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                   "graph needs an array of edge collections.");
  }

  for (auto const& it : VPackArrayIterator(list)) {
    std::string e = arangodb::basics::VelocyPackHelper::getStringValue(it, "");
    _edgeColls.emplace_back(
        std::make_unique<aql::Collection>(e, _vocbase, AccessMode::Type::READ));
  }

  list = base.get("vertexCollections");

  if (!list.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_BAD_JSON_PLAN,
        "graph needs an array of vertex collections.");
  }

  for (auto const& it : VPackArrayIterator(list)) {
    std::string v = arangodb::basics::VelocyPackHelper::getStringValue(it, "");
    _vertexColls.emplace_back(
        std::make_unique<aql::Collection>(v, _vocbase, AccessMode::Type::READ));
  }

  // Out variables
  if (base.hasKey("vertexOutVariable")) {
    _vertexOutVariable =
        Variable::varFromVPack(plan->getAst(), base, "vertexOutVariable");
  }
  if (base.hasKey("edgeOutVariable")) {
    _edgeOutVariable = Variable::varFromVPack(plan->getAst(), base, "edgeOutVariable");
  }

  // Temporary Filter Objects
  TRI_ASSERT(base.hasKey("tmpObjVariable"));
  _tmpObjVariable = Variable::varFromVPack(plan->getAst(), base, "tmpObjVariable");

  TRI_ASSERT(base.hasKey("tmpObjVarNode"));
  _tmpObjVarNode = new AstNode(plan->getAst(), base.get("tmpObjVarNode"));

  TRI_ASSERT(base.hasKey("tmpIdNode"));
  _tmpIdNode = new AstNode(plan->getAst(), base.get("tmpIdNode"));

  VPackSlice opts = base.get("options");
  if (!opts.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                   "graph options have to be a json-object.");
  }
  _options = BaseOptions::createOptionsFromSlice(
      _plan->getAst()->query()->trx(), opts);
}

/// @brief Internal constructor to clone the node.
GraphNode::GraphNode(
    ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
    std::vector<std::unique_ptr<Collection>> const& edgeColls,
    std::vector<std::unique_ptr<Collection>> const& vertexColls,
    std::vector<TRI_edge_direction_e> const& directions,
    std::unique_ptr<BaseOptions>& options)
    : ExecutionNode(plan, id),
      _vocbase(vocbase),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _graphObj(nullptr),
      _tmpObjVariable(_plan->getAst()->variables()->createTemporaryVariable()),
      _tmpObjVarNode(_plan->getAst()->createNodeReference(_tmpObjVariable)),
      _tmpIdNode(_plan->getAst()->createNodeValueString("", 0)),
      _directions(directions),
      _options(std::move(options)),
      _optionsBuilt(false),
      _isSmart(false) {
  _graphInfo.openArray();
  for (auto& it : edgeColls) {
    // Collections cannot be copied. So we need to create new ones to prevent
    // leaks
    _edgeColls.emplace_back(std::make_unique<aql::Collection>(
        it->getName(), _vocbase, AccessMode::Type::READ));
    _graphInfo.add(VPackValue(it->getName()));
  }
  _graphInfo.close();

  for (auto& it : vertexColls) {
    // Collections cannot be copied. So we need to create new ones to prevent
    // leaks
    _vertexColls.emplace_back(std::make_unique<aql::Collection>(
        it->getName(), _vocbase, AccessMode::Type::READ));
  }
}

GraphNode::~GraphNode() {}

void GraphNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method
  // Vocbase
  nodes.add("database", VPackValue(_vocbase->name()));

  // TODO We need Both?!
  // Graph definition
  nodes.add("graph", _graphInfo.slice());

  // Graph Definition
  if (_graphObj != nullptr) {
    nodes.add(VPackValue("graphDefinition"));
    _graphObj->toVelocyPack(nodes, verbose);
  }

  // Directions
  nodes.add(VPackValue("directions"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& d : _directions) {
      nodes.add(VPackValue(d));
    }
  }

  // Collections
  nodes.add(VPackValue("edgeCollections"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& e : _edgeColls) {
      nodes.add(VPackValue(e->getName()));
    }
  }

  nodes.add(VPackValue("vertexCollections"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& v : _vertexColls) {
      nodes.add(VPackValue(v->getName()));
    }
  }

  // Out variables
  if (usesVertexOutVariable()) {
    nodes.add(VPackValue("vertexOutVariable"));
    vertexOutVariable()->toVelocyPack(nodes);
  }
  if (usesEdgeOutVariable()) {
    nodes.add(VPackValue("edgeOutVariable"));
    edgeOutVariable()->toVelocyPack(nodes);
  }

  // Temporary AST Nodes for conditions
  TRI_ASSERT(_tmpObjVariable != nullptr);
  nodes.add(VPackValue("tmpObjVariable"));
  _tmpObjVariable->toVelocyPack(nodes);

  TRI_ASSERT(_tmpObjVarNode != nullptr);
  nodes.add(VPackValue("tmpObjVarNode"));
  _tmpObjVarNode->toVelocyPack(nodes, verbose);

  TRI_ASSERT(_tmpIdNode != nullptr);
  nodes.add(VPackValue("tmpIdNode"));
  _tmpIdNode->toVelocyPack(nodes, verbose);

  nodes.add(VPackValue("options"));
  _options->toVelocyPack(nodes);

  nodes.add(VPackValue("indexes"));
  _options->toVelocyPackIndexes(nodes);
}

void GraphNode::addEngine(TraverserEngineID const& engine,
                          ServerID const& server) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  _engines.emplace(server, engine);
}

/// @brief Returns a reference to the engines. (CLUSTER ONLY)
std::unordered_map<ServerID, traverser::TraverserEngineID> const*
GraphNode::engines() const {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  return &_engines;
}

BaseOptions* GraphNode::options() const { return _options.get(); }

AstNode* GraphNode::getTemporaryRefNode() const { return _tmpObjVarNode; }

Variable const* GraphNode::getTemporaryVariable() const {
  return _tmpObjVariable;
}

void GraphNode::getConditionVariables(std::vector<Variable const*>& res) const {
  // No variables used, nothing todo
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

void GraphNode::addEdgeCollection(std::string const& n,
                                  TRI_edge_direction_e dir) {
  if (_isSmart) {
    if (n.compare(0, 6, "_from_") == 0) {
      if (dir != TRI_EDGE_IN) {
        _directions.emplace_back(TRI_EDGE_OUT);
        _edgeColls.emplace_back(std::make_unique<aql::Collection>(
            n, _vocbase, AccessMode::Type::READ));
      }
      return;
    } else if (n.compare(0, 4, "_to_") == 0) {
      if (dir != TRI_EDGE_OUT) {
        _directions.emplace_back(TRI_EDGE_IN);
        _edgeColls.emplace_back(std::make_unique<aql::Collection>(
            n, _vocbase, AccessMode::Type::READ));
      }
      return;
    }
  }

  if (dir == TRI_EDGE_ANY) {
    _directions.emplace_back(TRI_EDGE_OUT);
    _edgeColls.emplace_back(
        std::make_unique<aql::Collection>(n, _vocbase, AccessMode::Type::READ));

    _directions.emplace_back(TRI_EDGE_IN);
    _edgeColls.emplace_back(
        std::make_unique<aql::Collection>(n, _vocbase, AccessMode::Type::READ));
  } else {
    _directions.emplace_back(dir);
    _edgeColls.emplace_back(
        std::make_unique<aql::Collection>(n, _vocbase, AccessMode::Type::READ));
  }
};
