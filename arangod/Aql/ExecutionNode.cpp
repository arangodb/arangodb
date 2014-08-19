////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionPlans
///
/// @file arangod/Aql/ExecutionNode.cpp
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ExecutionNode.h"
#include "Aql/Collection.h"
#include "Aql/WalkerWorker.h"
#include "Aql/Ast.h"

using namespace triagens::basics;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                             static initialization
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief type names
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<int, std::string const> const ExecutionNode::TypeNames{ 
  { static_cast<int>(ILLEGAL),                      "ExecutionNode (abstract)" },
  { static_cast<int>(SINGLETON),                    "SingletonNode" },
  { static_cast<int>(ENUMERATE_COLLECTION),         "EnumerateCollectionNode" },
  { static_cast<int>(ENUMERATE_LIST),               "EnumerateListNode" },
  { static_cast<int>(INDEX_RANGE),                  "IndexRangeNode" },
  { static_cast<int>(LIMIT),                        "LimitNode" },
  { static_cast<int>(CALCULATION),                  "CalculationNode" },
  { static_cast<int>(SUBQUERY),                     "SubqueryNode" },
  { static_cast<int>(FILTER),                       "FilterNode" },
  { static_cast<int>(SORT),                         "SortNode" },
  { static_cast<int>(AGGREGATE),                    "AggregateNode" },
  { static_cast<int>(RETURN),                       "ReturnNode" },
  { static_cast<int>(REMOVE),                       "RemoveNode" },
  { static_cast<int>(INSERT),                       "InsertNode" },
  { static_cast<int>(UPDATE),                       "UpdateNode" },
  { static_cast<int>(REPLACE),                      "ReplaceNode" }
};
          
// -----------------------------------------------------------------------------
// --SECTION--                                          methods of ExecutionNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the type name of the node
////////////////////////////////////////////////////////////////////////////////

const std::string& ExecutionNode::getTypeString () const {
  auto it = TypeNames.find(static_cast<int>(getType()));
  if (it != TypeNames.end()) {
    return (*it).second;
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "missing type in TypeNames");
}

void ExecutionNode::validateType (int type) {
  auto it = TypeNames.find(static_cast<int>(type));
  if (it == TypeNames.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "unknown TypeID");
  }
}

ExecutionNode* ExecutionNode::fromJsonFactory (Ast const* ast,
                                               const Json &oneNode) {
  auto JsonString = oneNode.toString();

  int nodeTypeID = JsonHelper::getNumericValue<int>(oneNode.json(), "typeID", 0);
  triagens::aql::Query* query = ast->query();
  validateType(nodeTypeID);

  NodeType nodeType = (NodeType) nodeTypeID;
  switch (nodeType) {
    case SINGLETON:
      return new SingletonNode(query, oneNode);
    case ENUMERATE_COLLECTION:
      return new EnumerateCollectionNode(query, oneNode);
    case ENUMERATE_LIST:
      return new EnumerateListNode(query, oneNode);
    case FILTER:
      return new FilterNode(query, oneNode);
    case LIMIT:
      return new LimitNode(query, oneNode);
    case CALCULATION:
      return new CalculationNode(query, oneNode);
    case SUBQUERY: 
      return new SubqueryNode(ast, query, oneNode);
    case SORT: {
      Json jsonElements = oneNode.get("elements");
      if (! jsonElements.isList()){
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected value for SortNode elements");
      }
      size_t len = jsonElements.size();
      std::vector<std::pair<Variable const*, bool>> elements;
      elements.reserve(len);
      for (size_t i = 0; i < len; i++) {
        Json oneJsonElement = jsonElements.at(i);
        bool ascending = JsonHelper::getBooleanValue(oneJsonElement.json(), "ascending", false);
        Variable *v = query->registerVar(new Variable(oneJsonElement.get("inVariable")));
        elements.push_back(std::make_pair(v, ascending));
      }

      return new SortNode(query, oneNode, elements);
    }
    case AGGREGATE: {

      Json outVariableJson = oneNode.get("outVariable");
      Variable *outVariable = nullptr;

      if (!outVariableJson.isEmpty()) /* Optional... */
        outVariable = query->registerVar(new Variable(outVariableJson));


      Json jsonAaggregates = oneNode.get("aggregates");
      if (!jsonAaggregates.isList()){
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "missing node type in valueTypeNames"); //// TODO
      }

      int len = jsonAaggregates.size();
      std::vector<std::pair<Variable const*, Variable const*>> aggregateVariables;

      aggregateVariables.reserve(len);
      for (int i = 0; i < len; i++) {
        Json oneJsonAggregate = jsonAaggregates.at(i);
        Variable* outVariable = query->registerVar(new Variable(oneJsonAggregate.get("outVariable")));
        Variable* inVariable =  query->registerVar(new Variable(oneJsonAggregate.get("inVariable")));

        aggregateVariables.push_back(std::make_pair(outVariable, inVariable));
      }

      return new AggregateNode(query,
                              oneNode,
                              outVariable,
                              ast->variables()->variables(false),
                              aggregateVariables);
    }
    case INSERT:
      return new InsertNode(query, oneNode);
    case REMOVE:
      return new RemoveNode(query, oneNode);
    case REPLACE:
      return new ReplaceNode(query, oneNode);
    case UPDATE:
      return new UpdateNode(query, oneNode);
    case RETURN:
      return new ReturnNode(query, oneNode);
    case INTERSECTION:
      //return new (query, oneNode);
    case PROJECTION:
      //return new (query, oneNode);
    case LOOKUP_JOIN:
      //return new (query, oneNode);
    case MERGE_JOIN:
      //return new (query, oneNode);
    case LOOKUP_INDEX_UNIQUE:
      //return new (query, oneNode);
    case LOOKUP_INDEX_RANGE:
      //return new (query, oneNode);
    case LOOKUP_FULL_COLLECTION:
      //return new (query, oneNode);
    case CONCATENATION:
      //return new (query, oneNode);
    case INDEX_RANGE:
      //return new (query, oneNode);
    case MERGE:
      //return new (query, oneNode);
    case REMOTE:
      //return new (query, oneNode);
    case ILLEGAL:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unhandled node type");
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, export an ExecutionNode to JSON
////////////////////////////////////////////////////////////////////////////////

Json ExecutionNode::toJson (TRI_memory_zone_t* zone) {
  Json json;
  Json nodes;
  try {
    nodes = Json(Json::List, 10);
    toJsonHelper(nodes, zone);
    json = Json(Json::Array, 1)
             ("nodes", nodes);
  }
  catch (std::exception& e) {
    return Json();
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to a string, basically for debugging purposes
////////////////////////////////////////////////////////////////////////////////

void ExecutionNode::appendAsString (std::string& st, int indent) {
  for (int i = 0; i < indent; i++) {
    st.push_back(' ');
  }
  
  st.push_back('<');
  st.append(getTypeString());
  if (_dependencies.size() != 0) {
    st.push_back('\n');
    for (size_t i = 0; i < _dependencies.size(); i++) {
      _dependencies[i]->appendAsString(st, indent + 2);
      if (i != _dependencies.size() - 1) {
        st.push_back(',');
      }
      else {
        st.push_back(' ');
      }
    }
  }
  st.push_back('>');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief functionality to walk an execution plan recursively
////////////////////////////////////////////////////////////////////////////////

void ExecutionNode::walk (WalkerWorker<ExecutionNode>* worker) {
  // Only do every node exactly once:
  if (worker->done(this)) {
    return;
  }

  worker->before(this);
  
  // Now the children in their natural order:
  for (auto it = _dependencies.begin();
            it != _dependencies.end(); 
            ++it) {
    (*it)->walk(worker);
  }
  
  // Now handle a subquery:
  if (getType() == SUBQUERY) {
    auto p = static_cast<SubqueryNode*>(this);
    if (worker->enterSubquery(this, p->getSubquery())) {
      p->getSubquery()->walk(worker);
      worker->leaveSubquery(this, p->getSubquery());
    }
  }

  worker->after(this);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJsonHelper, for a generic node
////////////////////////////////////////////////////////////////////////////////

Json ExecutionNode::toJsonHelperGeneric (triagens::basics::Json& nodes,
                                         TRI_memory_zone_t* zone) {
  size_t const n = _dependencies.size();
  for (size_t i = 0; i < n; i++) {
    _dependencies[i]->toJsonHelper(nodes, zone);
  }

  Json json;
  json = Json(Json::Array, 2)
           ("type", Json(getTypeString()));
  json ("typeID", Json(static_cast<int>(getType())));
  Json deps(Json::List, n);
  for (size_t i = 0; i < n; i++) {
    deps(Json(static_cast<double>(_dependencies[i]->id())));
  }
  json("dependencies", deps);
  json("id", Json(static_cast<double>(id())));

  if(this->_estimatedCost != 0){
    json("estimatedCost", Json(this->_estimatedCost));
  }
  /*
  if (_varUsageValid) {
    Json varsValid(Json::List, _varsValid.size());
    for (auto v : _varsValid) {
      varsValid(v->toJson());
    }
    json("varsValid", varsValid);
    Json varsUsedLater(Json::List, _varsUsedLater.size());
    for (auto v : _varsUsedLater) {
      varsUsedLater(v->toJson());
    }
    json("varsUsedLater", varsUsedLater);
  }
*/
  return json;
}

void ExecutionNode::fromJsonHelper (triagens::aql::Query* q, basics::Json const& base) {
  this->_estimatedCost = JsonHelper::getNumericValue<double>(base.json(), "estimatedCost", 0.0);
  /*  
  Json varsUsedLaterJson = base.get("varsUsedLater");
  if (!varsUsedLaterJson.isEmpty()) {
    int len = varsUsedLaterJson.size();
    _varsUsedLater.reserve(len);
    for (int i = 0; i < len; i++) {
      _varsUsedLater.insert(q->registerVar(new Variable(varsUsedLaterJson.at(i))));
    }
    _varUsageValid = true;
  }

  Json varsValidJson = base.get("varsValid");
  if (!varsValidJson.isEmpty()) {
    int len = varsValidJson.size();
    _varsValid.reserve(len);
    for (int i = 0; i < len; i++) {
      _varsValid.insert(q->registerVar(new Variable(varsUsedLaterJson.at(i))));
    }
    _varUsageValid = true;
  }
  */
}

// -----------------------------------------------------------------------------
// --SECTION--                                          methods of SingletonNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SingletonNode
////////////////////////////////////////////////////////////////////////////////

SingletonNode::SingletonNode (triagens::aql::Query* query, basics::Json const& base)
  : ExecutionNode(base) {
  fromJsonHelper(query, base);
}

void SingletonNode::toJsonHelper (triagens::basics::Json& nodes,
                                  TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }

  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                methods of EnumerateCollectionNode
// -----------------------------------------------------------------------------

EnumerateCollectionNode::EnumerateCollectionNode (triagens::aql::Query* q, basics::Json const& base)
  : ExecutionNode(base),
    _vocbase(q->vocbase()),
    _collection(q->collections()->get(JsonHelper::getStringValue(base.json(), "collection", ""))),
    _outVariable(q->registerVar(new Variable(base.get("outVariable")))) {
  fromJsonHelper(q, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for EnumerateCollectionNode
////////////////////////////////////////////////////////////////////////////////

void EnumerateCollectionNode::toJsonHelper (triagens::basics::Json& nodes,
                                            TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  json("database", Json(_vocbase->_name))
      ("collection", Json(_collection->name))
      ("outVariable", _outVariable->toJson());

  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                      methods of EnumerateListNode
// -----------------------------------------------------------------------------

EnumerateListNode::EnumerateListNode (triagens::aql::Query* q, basics::Json const& base)
  : ExecutionNode(base),
    _inVariable(q->registerVar(new Variable(base.get("inVariable")))),
    _outVariable(q->registerVar(new Variable(base.get("outVariable")))) {
  fromJsonHelper(q, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for EnumerateListNode
////////////////////////////////////////////////////////////////////////////////

void EnumerateListNode::toJsonHelper (triagens::basics::Json& nodes,
                                      TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  json("inVariable",  _inVariable->toJson())
      ("outVariable", _outVariable->toJson());

  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for IndexRangeNode
////////////////////////////////////////////////////////////////////////////////

void IndexRangeNode::toJsonHelper (triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone) {

  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  
  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // put together the range info . . .
  Json ranges(Json::List);

  for (auto x : *_ranges) {
    Json item(Json::Array);
    item("name", Json(x._name))
        ("low", x._low)
        ("lowOpen", Json(x._lowOpen))
        ("high", x._high)
        ("highOpen", Json(x._highOpen));
    ranges(item);
  }

  // Now put info about vocbase and cid in there
  json("database", Json(_vocbase->_name))
      ("collection", Json(_collection->name))
      ("outVariable", _outVariable->toJson())
      ("index", _index->index()->json(_index->index()))
      ("ranges", ranges);
  
  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                              methods of LimitNode
// -----------------------------------------------------------------------------

LimitNode::LimitNode (triagens::aql::Query* query, basics::Json const& base)
  : ExecutionNode(base) {
  _offset = JsonHelper::getNumericValue<double>(base.json(), "offset", 0.0);
  _limit = JsonHelper::getNumericValue<double>(base.json(), "limit", 0.0);
  fromJsonHelper(query, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for LimitNode
////////////////////////////////////////////////////////////////////////////////

void LimitNode::toJsonHelper (triagens::basics::Json& nodes,
                              TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  // Now put info about offset and limit in
  json("offset", Json(static_cast<double>(_offset)))
      ("limit",  Json(static_cast<double>(_limit)));

  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                        methods of CalculationNode
// -----------------------------------------------------------------------------

CalculationNode::CalculationNode (triagens::aql::Query* q, basics::Json const& base)
  : ExecutionNode(base),
    _expression(new Expression(q, base)),
    _outVariable(q->registerVar(new Variable(base.get("outVariable")))) {
   ////  _expression->fromJson(base, "expression")) -> list -> for schleife.
  fromJsonHelper(q, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for CalculationNode
////////////////////////////////////////////////////////////////////////////////

void CalculationNode::toJsonHelper (triagens::basics::Json& nodes,
                                    TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  
  json("expression", _expression->toJson(TRI_UNKNOWN_MEM_ZONE))
      ("outVariable", _outVariable->toJson())
      ("canThrow", Json(_expression->canThrow()));


  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                           methods of SubqueryNode
// -----------------------------------------------------------------------------

SubqueryNode::SubqueryNode (Ast const* ast,
                            triagens::aql::Query* q,
                            basics::Json const& base)
  : ExecutionNode(base),
    _subquery(nullptr),
    _outVariable(q->registerVar(new Variable(base.get("outVariable")))) {
  fromJsonHelper(q, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SubqueryNode
////////////////////////////////////////////////////////////////////////////////

void SubqueryNode::toJsonHelper (triagens::basics::Json& nodes,
                                 TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  json("subquery",  _subquery->toJson(TRI_UNKNOWN_MEM_ZONE))
      ("outVariable", _outVariable->toJson());

  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper struct to find all (outer) variables used in a SubqueryNode
////////////////////////////////////////////////////////////////////////////////

struct SubqueryVarUsageFinder : public WalkerWorker<ExecutionNode> {
  std::unordered_set<Variable const*> _usedLater;
  std::unordered_set<Variable const*> _valid;

  SubqueryVarUsageFinder () {
  }

  ~SubqueryVarUsageFinder () {
  }

  void before (ExecutionNode* en) {
    // Add variables used here to _usedLater:
    auto&& usedHere = en->getVariablesUsedHere();
    for (auto v : usedHere) {
      _usedLater.insert(v);
    }
  }

  void after (ExecutionNode* en) {
    // Add variables set here to _valid:
    auto&& setHere = en->getVariablesSetHere();
    for (auto v : setHere) {
      _valid.insert(v);
    }
  }

  bool enterSubquery (ExecutionNode* super, ExecutionNode* sub) {
    SubqueryVarUsageFinder subfinder;
    sub->walk(&subfinder);

    // keep track of all variables used by a (dependent) subquery
    // this is, all variables in the subqueries _usedLater that are not in _valid
     
    // create the set difference. note: cannot use std::set_difference as our sets are NOT sorted
    for (auto it = subfinder._usedLater.begin(); it != subfinder._usedLater.end(); ++it) {
      if (_valid.find(*it) != _valid.end()) {
        _usedLater.insert((*it));
      }
    }
    return false;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

std::vector<Variable const*> SubqueryNode::getVariablesUsedHere () {
  SubqueryVarUsageFinder finder;
  _subquery->walk(&finder);
      
  std::vector<Variable const*> v;
  for (auto it = finder._usedLater.begin(); it != finder._usedLater.end(); ++it) {
    if (finder._valid.find(*it) == finder._valid.end()) {
      v.push_back((*it));
    }
  }
  return v;
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of FilterNode
// -----------------------------------------------------------------------------

FilterNode::FilterNode (triagens::aql::Query* q, basics::Json const& base)
  : ExecutionNode(base),
    _inVariable(q->registerVar(new Variable(base.get("inVariable")))) {
   fromJsonHelper(q, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for FilterNode
////////////////////////////////////////////////////////////////////////////////

void FilterNode::toJsonHelper (triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  
  json("inVariable", _inVariable->toJson());

  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                               methods of SortNode
// -----------------------------------------------------------------------------

SortNode::SortNode (triagens::aql::Query* query,
                    basics::Json const& base,
                    std::vector<std::pair<Variable const*, bool>> elements)
  : ExecutionNode(base),
    _elements(elements) {
  fromJsonHelper(query, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SortNode
////////////////////////////////////////////////////////////////////////////////

void SortNode::toJsonHelper (triagens::basics::Json& nodes,
                             TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  Json values(Json::List, _elements.size());
  for (auto it = _elements.begin(); it != _elements.end(); ++it) {
    Json element(Json::Array);
    element("inVariable", (*it).first->toJson())
           ("ascending", Json((*it).second));
    values(element);
  }
  json("elements", values);

  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                          methods of AggregateNode
// -----------------------------------------------------------------------------

AggregateNode::AggregateNode (triagens::aql::Query* q,
                              basics::Json const& base,
                              Variable const* outVariable,
                              std::unordered_map<VariableId, std::string const> const& variableMap,
                              std::vector<std::pair<Variable const*, Variable const*>> aggregateVariables)
  : ExecutionNode(base),
    _aggregateVariables(aggregateVariables), 
    _outVariable(outVariable),
    _variableMap(variableMap) {
  fromJsonHelper(q, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for AggregateNode
////////////////////////////////////////////////////////////////////////////////

void AggregateNode::toJsonHelper (triagens::basics::Json& nodes,
                                  TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }

  Json values(Json::List, _aggregateVariables.size());
  for (auto it = _aggregateVariables.begin(); it != _aggregateVariables.end(); ++it) {
    Json variable(Json::Array);
    variable("outVariable", (*it).first->toJson())
            ("inVariable", (*it).second->toJson());
    values(variable);
  }
  json("aggregates", values);

  // output variable might be empty
  if (_outVariable != nullptr) {
    json("outVariable", _outVariable->toJson());
  }

  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of ReturnNode
// -----------------------------------------------------------------------------

ReturnNode::ReturnNode (triagens::aql::Query* q, basics::Json const& base)
  : ExecutionNode(base),
    _inVariable(q->registerVar(new Variable(base.get("inVariable")))) {
  fromJsonHelper(q, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for ReturnNode
////////////////////////////////////////////////////////////////////////////////

void ReturnNode::toJsonHelper (triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method

  if (json.isEmpty()) {
    return;
  }
      
  json("inVariable", _inVariable->toJson());

  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  ModificationNode
// -----------------------------------------------------------------------------

ModificationNode::ModificationNode (triagens::aql::Query* q,
                                    basics::Json const& base)
  : ExecutionNode(base), 
    _vocbase(q->vocbase()),
    _collection(q->collections()->get(JsonHelper::getStringValue(base.json(), "collection", ""))),
    _options(base) {
  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_collection != nullptr);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of RemoveNode
// -----------------------------------------------------------------------------

RemoveNode::RemoveNode (triagens::aql::Query* q, basics::Json const& base)
  : ModificationNode(q, base),
    _inVariable(q->registerVar(new Variable(base.get("inVariable")))),
    _outVariable(base.get("outVariable").isEmpty() ? nullptr : q->registerVar(new Variable(base.get("outVariable")))) {
  fromJsonHelper(q, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void RemoveNode::toJsonHelper (triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  json("database", Json(_vocbase->_name))
      ("collection", Json(_collection->name))
      ("inVariable", _inVariable->toJson());
  
  // output variable might be empty
  if (_outVariable != nullptr) {
    json("outVariable", _outVariable->toJson());
  }

  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of InsertNode
// -----------------------------------------------------------------------------

InsertNode::InsertNode (triagens::aql::Query* q, basics::Json const& base)
  : ModificationNode(q, base),
    _inVariable(q->registerVar(new Variable(base.get("inVariable")))),
    _outVariable(base.get("outVariable").isEmpty() ? nullptr : q->registerVar(new Variable(base.get("outVariable")))) {
  fromJsonHelper(q, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void InsertNode::toJsonHelper (triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  json("database", Json(_vocbase->_name))
      ("collection", Json(_collection->name))
      ("inVariable", _inVariable->toJson());
  
  // output variable might be empty
  if (_outVariable != nullptr) {
    json("outVariable", _outVariable->toJson());
  }

  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of UpdateNode
// -----------------------------------------------------------------------------

UpdateNode::UpdateNode (triagens::aql::Query* q, basics::Json const& base)
  : ModificationNode(q, base),
    _inDocVariable(q->registerVar(new Variable(base.get("inDocVariable")))),
    _inKeyVariable(base.get("inKeyVariable").isEmpty() ? nullptr : q->registerVar(new Variable(base.get("inKeyVariable")))),
    _outVariable(base.get("outVariable").isEmpty() ? nullptr : q->registerVar(new Variable(base.get("outVariable")))) {
  fromJsonHelper(q, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void UpdateNode::toJsonHelper (triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  json("database", Json(_vocbase->_name))
      ("collection", Json(_collection->name))
      ("inDocVariable", _inDocVariable->toJson());
  
  // inKeyVariable might be empty
  if (_inKeyVariable != nullptr) {
    json("inKeyVariable", _inKeyVariable->toJson());
  }
  
  // output variable might be empty
  if (_outVariable != nullptr) {
    json("outVariable", _outVariable->toJson());
  }

  // And add it:
  nodes(json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                            methods of ReplaceNode
// -----------------------------------------------------------------------------

ReplaceNode::ReplaceNode (triagens::aql::Query* q, basics::Json const& base)
  : ModificationNode(q, base),
    _inDocVariable(q->registerVar(new Variable(base.get("inDocVariable")))),
    _inKeyVariable(base.get("inKeyVariable").isEmpty() ? nullptr : q->registerVar(new Variable(base.get("inKeyVariable")))),
    _outVariable(base.get("outVariable").isEmpty() ? nullptr : q->registerVar(new Variable(base.get("outVariable")))) {
  fromJsonHelper(q, base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void ReplaceNode::toJsonHelper (triagens::basics::Json& nodes,
                                TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  json("database", Json(_vocbase->_name))
      ("collection", Json(_collection->name))
      ("inDocVariable", _inDocVariable->toJson());
  
  // inKeyVariable might be empty
  if (_inKeyVariable != nullptr) {
    json("inKeyVariable", _inKeyVariable->toJson());
  }
  
  // output variable might be empty
  if (_outVariable != nullptr) {
    json("outVariable", _outVariable->toJson());
  }

  // And add it:
  nodes(json);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


