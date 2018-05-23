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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterNodes.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ClusterBlocks.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/IndexNode.h"
#include "Aql/GraphNode.h"

#include <type_traits>

using namespace arangodb::basics;
using namespace arangodb::aql;

namespace {

arangodb::velocypack::StringRef const SortModeUnset("unset");
arangodb::velocypack::StringRef const SortModeMinElement("minelement");
arangodb::velocypack::StringRef const SortModeHeap("heap");

bool toSortMode(
    arangodb::velocypack::StringRef const& str,
    GatherNode::SortMode& mode
) noexcept {
  static std::map<arangodb::velocypack::StringRef, GatherNode::SortMode> const NameToValue {
    { SortModeUnset, GatherNode::SortMode::Unset },
    { SortModeMinElement, GatherNode::SortMode::MinElement},
    { SortModeHeap, GatherNode::SortMode::Heap}
  };

  auto const it = NameToValue.find(str);

  if (it == NameToValue.end()) {
    TRI_ASSERT(false);
    return false;
  }

  mode = it->second;
  return true;
}

arangodb::velocypack::StringRef toString(GatherNode::SortMode mode) noexcept {
  switch (mode) {
    case GatherNode::SortMode::Unset:
      return SortModeUnset;
    case GatherNode::SortMode::MinElement:
      return SortModeMinElement;
    case GatherNode::SortMode::Heap:
      return SortModeHeap;
    default:
      TRI_ASSERT(false);
      return {};
  }
}

}

/// @brief constructor for RemoteNode 
RemoteNode::RemoteNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _vocbase(&(plan->getAst()->query()->vocbase())),
      _server(base.get("server").copyString()),
      _ownName(base.get("ownName").copyString()),
      _queryId(base.get("queryId").copyString()),
      _isResponsibleForInitializeCursor(base.get("isResponsibleForInitializeCursor").getBoolean()) {}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> RemoteNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&,
    std::unordered_set<std::string> const&
) const {
  return std::make_unique<RemoteBlock>(
    &engine, this, server(), ownName(), queryId()
  );
}

/// @brief toVelocyPack, for RemoteNode
void RemoteNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("server", VPackValue(_server));
  nodes.add("ownName", VPackValue(_ownName));
  nodes.add("queryId", VPackValue(_queryId));
  nodes.add("isResponsibleForInitializeCursor",
            VPackValue(_isResponsibleForInitializeCursor));

  // And close it:
  nodes.close();
}

/// @brief estimateCost
double RemoteNode::estimateCost(size_t& nrItems) const {
  if (_dependencies.size() == 1) {
    // This will usually be the case, however, in the context of the
    // instantiation it is possible that there is no dependency...
    double depCost = _dependencies[0]->estimateCost(nrItems);
    return depCost + nrItems;  // we need to process them all
  }
  // We really should not get here, but if so, do something bordering on
  // sensible:
  nrItems = 1;
  return 1.0;
}

/// @brief construct a scatter node
ScatterNode::ScatterNode(ExecutionPlan* plan,
                         arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _vocbase(&(plan->getAst()->query()->vocbase())),
      _collection(plan->getAst()->query()->collections()->get(
          base.get("collection").copyString())) {}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> ScatterNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&,
    std::unordered_set<std::string> const& includedShards
) const {
  auto const shardIds = collection()->shardIds(includedShards);

  return std::make_unique<ScatterBlock>(
    &engine, this, *shardIds
  );
}

/// @brief toVelocyPack, for ScatterNode
void ScatterNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("collection", VPackValue(_collection->getName()));

  // And close it:
  nodes.close();
}

/// @brief estimateCost
double ScatterNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies[0]->getCost(nrItems);
  auto shardIds = _collection->shardIds();
  size_t nrShards = shardIds->size();
  return depCost + nrItems * nrShards;
}

/// @brief construct a distribute node
DistributeNode::DistributeNode(ExecutionPlan* plan,
                               arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _vocbase(&(plan->getAst()->query()->vocbase())),
      _collection(plan->getAst()->query()->collections()->get(
          base.get("collection").copyString())),
      _variable(nullptr),
      _alternativeVariable(nullptr),
      _createKeys(base.get("createKeys").getBoolean()),
      _allowKeyConversionToObject(base.get("allowKeyConversionToObject").getBoolean()),
      _allowSpecifiedKeys(false) {
  if (base.hasKey("variable") && base.hasKey("alternativeVariable")) {     
    _variable = Variable::varFromVPack(plan->getAst(), base, "variable");
    _alternativeVariable = Variable::varFromVPack(plan->getAst(), base, "alternativeVariable");
  } else {
    _variable = plan->getAst()->variables()->getVariable(base.get("varId").getNumericValue<VariableId>());
    _alternativeVariable = plan->getAst()->variables()->getVariable(base.get("alternativeVarId").getNumericValue<VariableId>());
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> DistributeNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&,
    std::unordered_set<std::string> const& includedShards
) const {
  auto const shardIds = collection()->shardIds(includedShards);

  return std::make_unique<DistributeBlock>(
    &engine, this, *shardIds, collection()
  );
}

/// @brief toVelocyPack, for DistributedNode
void DistributeNode::toVelocyPackHelper(VPackBuilder& nodes,
                                        unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("collection", VPackValue(_collection->getName()));
  nodes.add("createKeys", VPackValue(_createKeys));
  nodes.add("allowKeyConversionToObject",
            VPackValue(_allowKeyConversionToObject));
  nodes.add(VPackValue("variable"));
  _variable->toVelocyPack(nodes);
  nodes.add(VPackValue("alternativeVariable"));
  _alternativeVariable->toVelocyPack(nodes);
  
  // legacy format, remove in 3.4
  nodes.add("varId", VPackValue(static_cast<int>(_variable->id)));
  nodes.add("alternativeVarId",
            VPackValue(static_cast<int>(_alternativeVariable->id)));

  // And close it:
  nodes.close();
}
  
/// @brief getVariablesUsedHere, returning a vector
std::vector<Variable const*> DistributeNode::getVariablesUsedHere() const {
  std::vector<Variable const*> vars;
  vars.emplace_back(_variable);
  if (_variable != _alternativeVariable) {
    vars.emplace_back(_alternativeVariable);
  }
  return vars;
}
  
/// @brief getVariablesUsedHere, modifying the set in-place
void DistributeNode::getVariablesUsedHere(std::unordered_set<Variable const*>& vars) const {
  vars.emplace(_variable);
  vars.emplace(_alternativeVariable);
}

/// @brief estimateCost
double DistributeNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies[0]->getCost(nrItems);
  return depCost + nrItems;
}

/*static*/ GatherNode::SortMode GatherNode::getSortMode(
    Collection const* collection,
    std::size_t shardsRequiredForHeapMerge /*= 5*/
) {
  return collection
    ? (collection->numberOfShards() >= shardsRequiredForHeapMerge
       ? SortMode::Heap
       : SortMode::MinElement)
    : SortMode::Unset;
}

/// @brief construct a gather node
GatherNode::GatherNode(
    ExecutionPlan* plan,
    arangodb::velocypack::Slice const& base,
    SortElementVector const& elements)
  : ExecutionNode(plan, base),
    _elements(elements),
    _sortmode(SortMode::Unset) {
  auto const sortModeSlice = base.get("sortmode");

  if (!toSortMode(VelocyPackHelper::getStringRef(sortModeSlice, ""), _sortmode)) {
    LOG_TOPIC(ERR, Logger::AQL)
      << "invalid sort mode detected while creating 'GatherNode' from vpack";
  }
}

GatherNode::GatherNode(
    ExecutionPlan* plan,
    size_t id,
    SortMode sortMode) noexcept
  : ExecutionNode(plan, id),
    _sortmode(sortMode) {
}

Collection const* GatherNode::collection() const {
  ExecutionNode const* node = getFirstDependency();

  while (node) {
    switch (node->getType()) {
      case ENUMERATE_COLLECTION:
        return castTo<EnumerateCollectionNode const*>(node)->collection();
      case INDEX:
        return castTo<IndexNode const*>(node)->collection();
      case TRAVERSAL:
      case SHORTEST_PATH:
        return castTo<GraphNode const*>(node)->collection();
      default:
        node = node->getFirstDependency();
        break;
    }
  }

  return nullptr;
}

/// @brief toVelocyPack, for GatherNode
void GatherNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags);

  nodes.add("sortmode", VPackValue(toString(_sortmode).data()));

  nodes.add(VPackValue("elements"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& it : _elements) {
      VPackObjectBuilder obj(&nodes);
      nodes.add(VPackValue("inVariable"));
      it.var->toVelocyPack(nodes);
      nodes.add("ascending", VPackValue(it.ascending));
      if (!it.attributePath.empty()) {
        nodes.add(VPackValue("path"));
        VPackArrayBuilder arr(&nodes);
        for (auto const& a : it.attributePath) {
          nodes.add(VPackValue(a));
        }
      }
    }
  }

  // And close it:
  nodes.close();
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> GatherNode::createBlock(
    ExecutionEngine& engine,
    std::unordered_map<ExecutionNode*, ExecutionBlock*> const&,
    std::unordered_set<std::string> const&
) const {
  return std::make_unique<GatherBlock>(&engine, this);
}

/// @brief estimateCost
double GatherNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies[0]->getCost(nrItems);
  return depCost + nrItems;
}
