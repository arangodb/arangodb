/// @brief Implementation of Traversal Execution Node
///
/// @file arangod/Aql/TraversalNode.cpp
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Ast.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::aql;

TraversalNode::TraversalNode (ExecutionPlan* plan,
               size_t id,
               TRI_vocbase_t* vocbase, 
               AstNode const* direction,
               AstNode const* start,
               AstNode const* graph)
  : ExecutionNode(plan, id), 
    _vocbase(vocbase),
    _vertexOutVariable(nullptr),
    _edgeOutVariable(nullptr),
    _pathOutVariable(nullptr),
    _graphObj(nullptr),
    _condition(nullptr)
{
  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(direction != nullptr);
  TRI_ASSERT(start != nullptr);
  TRI_ASSERT(graph != nullptr);
  std::unique_ptr<arango::CollectionNameResolver> resolver(new arango::CollectionNameResolver(vocbase));
  if (graph->type == NODE_TYPE_COLLECTION_LIST) {
    // List of edge collection names
    _graphName = '[';
    for (size_t i = 0; i <  graph->numMembers(); ++i) {
      auto eColName = graph->getMember(i)->getStringValue();
      auto eColType = resolver->getCollectionTypeCluster(eColName);
      if (_graphName.length() > 1) {
        _graphName += "'";
      }
      _graphName += "'";
      _graphName += eColName;
      _graphName += "'";
      if (eColType != TRI_COL_TYPE_EDGE) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
      }
      _edgeColls.push_back(eColName);
    }
  } else {
    if (_edgeColls.size() == 0) {
      if (graph->isStringValue()) {
        _graphName = graph->getStringValue();
        _graphObj = plan->getAst()->query()->lookupGraphByName(_graphName);


        auto eColls = _graphObj->edgeCollections();
        for (const auto& n: eColls) {
          _edgeColls.push_back(n);
        }
      }
    }
  }

  // Parse start node
  if (start->type == NODE_TYPE_REFERENCE) {
    _inVariable = static_cast<Variable*>(start->getData());
    _vertexId = "";
  } else {
    _inVariable = nullptr;
    _vertexId = start->getStringValue();
  }

  // Parse Steps and direction
  TRI_ASSERT(direction->type == NODE_TYPE_DIRECTION);
  TRI_ASSERT(direction->numMembers() == 2);
  // Member 0 is the direction. Already the correct Integer.
  // Is not inserted by user but by enum.
  auto dir = direction->getMember(0);
  auto steps = direction->getMember(1);
  TRI_ASSERT(dir->isIntValue());
  auto dirNum = dir->getIntValue();

  switch (dirNum) {
    case 0:
      _direction = TRI_EDGE_ANY;
      break;
    case 1:
      _direction = TRI_EDGE_IN;
      break;
    case 2:
      _direction = TRI_EDGE_OUT;
      break;
    default:
      TRI_ASSERT(false);
      break;
  }

  if (steps->isNumericValue()) {
    // Check if a double value is integer
    double v = steps->getDoubleValue();
    double intpart;
    if (modf(v, &intpart) != 0.0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "expecting integer number or range for number of steps.");
    }
    _minDepth = static_cast<uint64_t>(v);
    _maxDepth = static_cast<uint64_t>(v);
  } else if (steps->type == NODE_TYPE_RANGE) {
    // Range depth
    auto lhs = steps->getMember(0);
    auto rhs = steps->getMember(1);
    if (lhs->isNumericValue()) {
      // Range is left-closed
      // Check if a double value is integer
      double v = lhs->getDoubleValue();
      double intpart;
      if (modf(v, &intpart) != 0.0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "expecting integer number or range for number of steps.");
      }
      _minDepth = static_cast<uint64_t>(v);
    }
    if (rhs->isNumericValue()) {
      // Range is right-closed
      // Check if a double value is integer
      double v = rhs->getDoubleValue();
      double intpart;
      if (modf(v, &intpart) != 0.0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "expecting integer number or range for number of steps.");
      }
      _maxDepth = static_cast<uint64_t>(v);
    }
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "expecting integer number or range for number of steps.");
  }

}


TraversalNode::TraversalNode (ExecutionPlan* plan,
                       size_t id,
                       TRI_vocbase_t* vocbase, 
                       std::vector<std::string> const& edgeColls,
                       Variable const* inVariable,
                       std::string const& vertexId,
                       TRI_edge_direction_e direction,
                       uint64_t minDepth,
                       uint64_t maxDepth)
  : ExecutionNode(plan, id),
    _vocbase(vocbase),
    _vertexOutVariable(nullptr),
    _edgeOutVariable(nullptr),
    _pathOutVariable(nullptr),
    _inVariable(inVariable),
    _vertexId(vertexId),
    _minDepth(minDepth),
    _maxDepth(maxDepth),
    _direction(direction),
    _condition(nullptr)
{
  for (auto& it : edgeColls) {
    _edgeColls.push_back(it);
  }
}

int TraversalNode::checkIsOutVariable(size_t variableId) {
  if (_vertexOutVariable != nullptr && _vertexOutVariable->id == variableId) {
    return 0;
  }
  if (_edgeOutVariable != nullptr && _edgeOutVariable->id == variableId) {
    return 1;
  }
  if (_pathOutVariable != nullptr && _pathOutVariable->id == variableId) {
    return 2;
  }
  return -1;
}


TraversalNode::TraversalNode (ExecutionPlan* plan,
                              triagens::basics::Json const& base)
  : ExecutionNode(plan, base),
    _vocbase(plan->getAst()->query()->vocbase()),
    _vertexOutVariable(nullptr),
    _edgeOutVariable(nullptr),
    _pathOutVariable(nullptr),
    _inVariable(nullptr),
    _condition(nullptr)
    {

  _minDepth = triagens::basics::JsonHelper::stringUInt64(base.json(), "minDepth");
  _maxDepth = triagens::basics::JsonHelper::stringUInt64(base.json(), "maxDepth");
  uint64_t dir = triagens::basics::JsonHelper::stringUInt64(base.json(), "direction");
  switch (dir) {
    case 0:
      _direction = TRI_EDGE_ANY;
      break;
    case 1:
      _direction = TRI_EDGE_OUT;
      break;
    case 2:
      _direction = TRI_EDGE_IN;
      break;
    default:
      TRI_ASSERT(false);
      break;
  }

  // In Vertex
  if (base.has("inVariable")) {
    _inVariable = varFromJson(plan->getAst(), base, "inVariable");
  }
  else {
    triagens::basics::JsonHelper::getStringValue(base.json(), "vertexId", _vertexId);  
  }

  TRI_json_t const* condition = JsonHelper::checkAndGetObjectValue(base.json(), "condition");

  if (condition != nullptr) {
    triagens::basics::Json conditionJson(TRI_UNKNOWN_MEM_ZONE, condition, triagens::basics::Json::NOFREE);
    _condition = Condition::fromJson(plan, conditionJson);
  }
  else {
    _condition = nullptr;
  }

  // Out variables
  if (base.has("vertexOutVariable")) {
    _vertexOutVariable = varFromJson(plan->getAst(), base, "vertexOutVariable");
  }
  if (base.has("edgeOutVariable")) {
    _edgeOutVariable = varFromJson(plan->getAst(), base, "edgeOutVariable");
  }
  if (base.has("pathOutVariable")) {
    _pathOutVariable = varFromJson(plan->getAst(), base, "pathOutVariable");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for TraversalNode
////////////////////////////////////////////////////////////////////////////////

void TraversalNode::toJsonHelper (triagens::basics::Json& nodes,
                                  TRI_memory_zone_t* zone,
                                  bool verbose) const {
  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  json("database", triagens::basics::Json(_vocbase->_name))
    ("minDepth", triagens::basics::Json(static_cast<int32_t>(_minDepth)))
    ("maxDepth", triagens::basics::Json(static_cast<int32_t>(_maxDepth)))
    ("direction", triagens::basics::Json(static_cast<int32_t>(_direction)));
  json("graph" , triagens::basics::Json(_graphName));

  // In variable
  if (usesInVariable()) {
    json("inVariable", inVariable()->toJson());
  }
  else {
    json("vertexId", triagens::basics::Json(_vertexId));
  }

  if (_condition != nullptr) {
    json("condition", _condition->toJson(TRI_UNKNOWN_MEM_ZONE, verbose)); 
  }
  
  if (_graphObj != nullptr) {
    json("graphDefinition", _graphObj->toJson(TRI_UNKNOWN_MEM_ZONE, verbose)); 
  }

  // Out variables
  if (usesVertexOutVariable()) {
    json("vertexOutVariable", vertexOutVariable()->toJson());
  }
  if (usesEdgeOutVariable()) {
    json("edgeOutVariable", edgeOutVariable()->toJson());
  }
  if (usesPathOutVariable()) {
    json("pathOutVariable", pathOutVariable()->toJson());
  }

  if (_expressions.size() > 0) {
    triagens::basics::Json expressionObject = triagens::basics::Json(triagens::basics::Json::Object,
                                                                     _expressions.size());
    for (auto const & map : _expressions) {
      triagens::basics::Json expressionArray = triagens::basics::Json(triagens::basics::Json::Array,
                                                                      map.second.size());
      for (auto const & x : map.second) {
        triagens::basics::Json exp(zone, triagens::basics::Json::Object);
        x->toJson(exp, zone);
        expressionArray(exp);
      }
      expressionObject.set(std::to_string(map.first), expressionArray);
    }
    json("simpleExpressions", expressionObject);
  }
  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* TraversalNode::clone (ExecutionPlan* plan,
                                     bool withDependencies,
                                     bool withProperties) const {
  auto c = new TraversalNode(plan, _id, _vocbase, _edgeColls, _inVariable,
                                      _vertexId, _direction, _minDepth, _maxDepth);

  if (usesVertexOutVariable()) {
    auto vertexOutVariable = _vertexOutVariable;
    if (withProperties) {
      vertexOutVariable = plan->getAst()->variables()->createVariable(vertexOutVariable);
    }
    TRI_ASSERT(vertexOutVariable != nullptr);
    c->setVertexOutput(vertexOutVariable);
  }

  if (usesEdgeOutVariable()) {
    auto edgeOutVariable = _edgeOutVariable;
    if (withProperties) {
      edgeOutVariable = plan->getAst()->variables()->createVariable(edgeOutVariable);
    }
    TRI_ASSERT(edgeOutVariable != nullptr);
    c->setEdgeOutput(edgeOutVariable);
  }

  if (usesPathOutVariable()) {
    auto pathOutVariable = _pathOutVariable;
    if (withProperties) {
      pathOutVariable = plan->getAst()->variables()->createVariable(pathOutVariable);
    }
    TRI_ASSERT(pathOutVariable != nullptr);
    c->setPathOutput(pathOutVariable);
  }

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a traversal node
////////////////////////////////////////////////////////////////////////////////
        
double TraversalNode::estimateCost (size_t& nrItems) const { 
  size_t incoming;
  double depCost = _dependencies.at(0)->getCost(incoming);
  size_t count = 1000; // TODO: FIXME
  nrItems = incoming * count;
  return depCost + nrItems;
}

void TraversalNode::fillTraversalOptions (triagens::arango::traverser::TraverserOptions& opts) const {
  opts.direction = _direction;
  opts.minDepth = _minDepth;
  opts.maxDepth = _maxDepth;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief remember the condition to execute for early traversal abortion.
////////////////////////////////////////////////////////////////////////////////

void TraversalNode::setCondition(triagens::aql::Condition* condition){

  std::unordered_set<Variable const*> varsUsedByCondition;

  Ast::getReferencedVariables(condition->root(), varsUsedByCondition);

  for (auto oneVar : varsUsedByCondition) {
    if ((_vertexOutVariable != nullptr && oneVar->id !=  _vertexOutVariable->id) &&
        (_edgeOutVariable   != nullptr && oneVar->id !=  _edgeOutVariable->id) &&
        (_pathOutVariable   != nullptr && oneVar->id !=  _pathOutVariable->id) &&
        (_inVariable        != nullptr && oneVar->id !=  _inVariable->id)) {

      _conditionVariables.push_back(oneVar);
    }
  }

  _condition = condition;
}

void TraversalNode::storeSimpleExpression(bool isEdgeAccess,
                                          size_t indexAccess,
                                          AstNodeType comparisonType,
                                          AstNode const* varAccess,
                                          AstNode const* compareTo) {
  auto it = _expressions.find(indexAccess);
  if (it == _expressions.end()) {
    std::vector<triagens::arango::traverser::TraverserExpression* > sec;
    _expressions.emplace(indexAccess, sec);
    it = _expressions.find(indexAccess);
  }
  std::unique_ptr<SimpleTraverserExpression> e(new SimpleTraverserExpression(isEdgeAccess,
                                                                             comparisonType,
                                                                             varAccess,
                                                                             compareTo));
  it->second.push_back(e.get());
  e.release();
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
