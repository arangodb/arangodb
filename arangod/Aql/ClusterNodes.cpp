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

using namespace arangodb::basics;
using namespace arangodb::aql;

/// @brief constructor for RemoteNode from Json
RemoteNode::RemoteNode(ExecutionPlan* plan, arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          JsonHelper::checkAndGetStringValue(base.json(), "collection"))),
      _server(JsonHelper::checkAndGetStringValue(base.json(), "server")),
      _ownName(JsonHelper::checkAndGetStringValue(base.json(), "ownName")),
      _queryId(JsonHelper::checkAndGetStringValue(base.json(), "queryId")),
      _isResponsibleForInitializeCursor(JsonHelper::checkAndGetBooleanValue(
          base.json(), "isResponsibleForInitializeCursor")) {}

/// @brief toVelocyPack, for RemoteNode
void RemoteNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method

  nodes.add("database", VPackValue(_vocbase->_name));
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

/// @brief construct a scatter node from JSON
ScatterNode::ScatterNode(ExecutionPlan* plan,
                         arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          JsonHelper::checkAndGetStringValue(base.json(), "collection"))) {}

/// @brief toVelocyPack, for ScatterNode
void ScatterNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes, verbose);  // call base class method

  nodes.add("database", VPackValue(_vocbase->_name));
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

/// @brief construct a distribute node from JSON
DistributeNode::DistributeNode(ExecutionPlan* plan,
                               arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          JsonHelper::checkAndGetStringValue(base.json(), "collection"))),
      _varId(JsonHelper::checkAndGetNumericValue<VariableId>(base.json(),
                                                             "varId")),
      _alternativeVarId(JsonHelper::checkAndGetNumericValue<VariableId>(
          base.json(), "alternativeVarId")),
      _createKeys(
          JsonHelper::checkAndGetBooleanValue(base.json(), "createKeys")),
      _allowKeyConversionToObject(JsonHelper::checkAndGetBooleanValue(
          base.json(), "allowKeyConversionToObject")) {}

/// @brief toVelocyPack, for DistributedNode
void DistributeNode::toVelocyPackHelper(VPackBuilder& nodes,
                                        bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method

  nodes.add("database", VPackValue(_vocbase->_name));
  nodes.add("collection", VPackValue(_collection->getName()));
  nodes.add("varId", VPackValue(static_cast<int>(_varId)));
  nodes.add("alternativeVarId",
            VPackValue(static_cast<int>(_alternativeVarId)));
  nodes.add("createKeys", VPackValue(_createKeys));
  nodes.add("allowKeyConversionToObject",
            VPackValue(_allowKeyConversionToObject));

  // And close it:
  nodes.close();
}

/// @brief estimateCost
double DistributeNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies[0]->getCost(nrItems);
  return depCost + nrItems;
}

/// @brief construct a gather node from JSON
GatherNode::GatherNode(ExecutionPlan* plan, arangodb::basics::Json const& base,
                       SortElementVector const& elements)
    : ExecutionNode(plan, base),
      _elements(elements),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          JsonHelper::checkAndGetStringValue(base.json(), "collection"))) {}

/// @brief toVelocyPack, for GatherNode
void GatherNode::toVelocyPackHelper(VPackBuilder& nodes, bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method

  nodes.add("database", VPackValue(_vocbase->_name));
  nodes.add("collection", VPackValue(_collection->getName()));

  nodes.add(VPackValue("elements"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& it : _elements) {
      VPackObjectBuilder obj(&nodes);
      nodes.add(VPackValue("inVariable"));
      it.first->toVelocyPack(nodes);
      nodes.add("ascending", VPackValue(it.second));
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
