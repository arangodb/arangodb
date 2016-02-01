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

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for RemoteNode from Json
////////////////////////////////////////////////////////////////////////////////

RemoteNode::RemoteNode(ExecutionPlan* plan, arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          JsonHelper::checkAndGetStringValue(base.json(), "collection"))),
      _server(JsonHelper::checkAndGetStringValue(base.json(), "server")),
      _ownName(JsonHelper::checkAndGetStringValue(base.json(), "ownName")),
      _queryId(JsonHelper::checkAndGetStringValue(base.json(), "queryId")),
      _isResponsibleForInitCursor(JsonHelper::checkAndGetBooleanValue(
          base.json(), "isResponsibleForInitCursor")) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for RemoteNode
////////////////////////////////////////////////////////////////////////////////

void RemoteNode::toJsonHelper(arangodb::basics::Json& nodes,
                              TRI_memory_zone_t* zone, bool verbose) const {
  arangodb::basics::Json json(ExecutionNode::toJsonHelperGeneric(
      nodes, zone, verbose));  // call base class method
  if (json.isEmpty()) {
    return;
  }

  json("database", arangodb::basics::Json(_vocbase->_name))(
      "collection", arangodb::basics::Json(_collection->getName()))(
      "server", arangodb::basics::Json(_server))(
      "ownName", arangodb::basics::Json(_ownName))(
      "queryId", arangodb::basics::Json(_queryId))(
      "isResponsibleForInitCursor",
      arangodb::basics::Json(_isResponsibleForInitCursor));

  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a scatter node from JSON
////////////////////////////////////////////////////////////////////////////////

ScatterNode::ScatterNode(ExecutionPlan* plan,
                         arangodb::basics::Json const& base)
    : ExecutionNode(plan, base),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          JsonHelper::checkAndGetStringValue(base.json(), "collection"))) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for ScatterNode
////////////////////////////////////////////////////////////////////////////////

void ScatterNode::toJsonHelper(arangodb::basics::Json& nodes,
                               TRI_memory_zone_t* zone, bool verbose) const {
  arangodb::basics::Json json(ExecutionNode::toJsonHelperGeneric(
      nodes, zone, verbose));  // call base class method
  if (json.isEmpty()) {
    return;
  }

  json("database", arangodb::basics::Json(_vocbase->_name))(
      "collection", arangodb::basics::Json(_collection->getName()));

  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
////////////////////////////////////////////////////////////////////////////////

double ScatterNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies[0]->getCost(nrItems);
  auto shardIds = _collection->shardIds();
  size_t nrShards = shardIds->size();
  return depCost + nrItems * nrShards;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a distribute node from JSON
////////////////////////////////////////////////////////////////////////////////

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

void DistributeNode::toJsonHelper(arangodb::basics::Json& nodes,
                                  TRI_memory_zone_t* zone, bool verbose) const {
  arangodb::basics::Json json(ExecutionNode::toJsonHelperGeneric(
      nodes, zone, verbose));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  json("database", arangodb::basics::Json(_vocbase->_name))(
      "collection", arangodb::basics::Json(_collection->getName()))(
      "varId", arangodb::basics::Json(static_cast<int>(_varId)))(
      "alternativeVarId",
      arangodb::basics::Json(static_cast<int>(_alternativeVarId)))(
      "createKeys", arangodb::basics::Json(_createKeys))(
      "allowKeyConversionToObject",
      arangodb::basics::Json(_allowKeyConversionToObject));

  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
////////////////////////////////////////////////////////////////////////////////

double DistributeNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies[0]->getCost(nrItems);
  return depCost + nrItems;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a gather node from JSON
////////////////////////////////////////////////////////////////////////////////

GatherNode::GatherNode(ExecutionPlan* plan, arangodb::basics::Json const& base,
                       SortElementVector const& elements)
    : ExecutionNode(plan, base),
      _elements(elements),
      _vocbase(plan->getAst()->query()->vocbase()),
      _collection(plan->getAst()->query()->collections()->get(
          JsonHelper::checkAndGetStringValue(base.json(), "collection"))) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for GatherNode
////////////////////////////////////////////////////////////////////////////////

void GatherNode::toJsonHelper(arangodb::basics::Json& nodes,
                              TRI_memory_zone_t* zone, bool verbose) const {
  arangodb::basics::Json json(ExecutionNode::toJsonHelperGeneric(
      nodes, zone, verbose));  // call base class method
  if (json.isEmpty()) {
    return;
  }

  json("database", arangodb::basics::Json(_vocbase->_name))(
      "collection", arangodb::basics::Json(_collection->getName()));

  arangodb::basics::Json values(arangodb::basics::Json::Array,
                                _elements.size());
  for (auto it = _elements.begin(); it != _elements.end(); ++it) {
    arangodb::basics::Json element(arangodb::basics::Json::Object);
    element("inVariable", (*it).first->toJson())(
        "ascending", arangodb::basics::Json((*it).second));
    values(element);
  }
  json("elements", values);

  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
////////////////////////////////////////////////////////////////////////////////

double GatherNode::estimateCost(size_t& nrItems) const {
  double depCost = _dependencies[0]->getCost(nrItems);
  return depCost + nrItems;
}
