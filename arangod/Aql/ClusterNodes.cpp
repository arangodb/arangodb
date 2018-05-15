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
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"

using namespace arangodb::basics;
using namespace arangodb::aql;

/// @brief constructor for RemoteNode 
RemoteNode::RemoteNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          base.get("collection").copyString())),
      _server(base.get("server").copyString()),
      _ownName(base.get("ownName").copyString()),
      _queryId(base.get("queryId").copyString()),
      _isResponsibleForInitializeCursor(base.get("isResponsibleForInitializeCursor").getBoolean()) {}

/// @brief toVelocyPack, for RemoteNode
void RemoteNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method

  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("collection", VPackValue(_collection->getName()));
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
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          base.get("collection").copyString())) {}

/// @brief toVelocyPack, for ScatterNode
void ScatterNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, verbose);  // call base class method

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
      _vocbase(plan->getAst()->query()->vocbase()),
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

/// @brief toVelocyPack, for DistributedNode
void DistributeNode::toVelocyPackHelper(VPackBuilder& nodes,
                                        bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method

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

/// @brief construct a gather node
GatherNode::GatherNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base,
                       SortElementVector const& elements, std::size_t shardsRequiredForHeapMerge)
    : ExecutionNode(plan, base),
      _elements(elements),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          base.get("collection").copyString())),
      _sortmode( _collection ? ( _collection->numberOfShards() >= shardsRequiredForHeapMerge ? 'h' : 'm') : 'u')
      {}
  
GatherNode::GatherNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
             Collection const* collection, std::size_t shardsRequiredForHeapMerge)
      : ExecutionNode(plan, id), _vocbase(vocbase), _collection(collection),
        _auxiliaryCollections(),
        _sortmode( _collection ? ( _collection->numberOfShards() >= shardsRequiredForHeapMerge ? 'h' : 'm') : 'u')
        {}

/// @brief toVelocyPack, for GatherNode
void GatherNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method

  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("collection", VPackValue(_collection->getName()));

  if(_sortmode == 'h'){
    nodes.add("sortmode", VPackValue("heap"));
  } else if (_sortmode == 'm') {
    nodes.add("sortmode", VPackValue("minelement"));
  } else {
    nodes.add("sortmode", VPackValue("unset"));
  }

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

/// @brief estimateCost
double GatherNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies[0]->getCost(nrItems);
  return depCost + nrItems;
}
