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

std::string ExecutionNode::getTypeString () const {
  auto it = TypeNames.find(static_cast<int>(getType()));
  if (it != TypeNames.end()) {
    return std::string((*it).second);
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "missing type in TypeNames");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, export an ExecutionNode to JSON
////////////////////////////////////////////////////////////////////////////////

Json ExecutionNode::toJson (TRI_memory_zone_t* zone) {
  std::map<ExecutionNode*, int> indexTab;
  Json json;
  Json nodes;
  try {
    nodes = Json(Json::List, 10);
    toJsonHelper(indexTab, nodes, zone);
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

Json ExecutionNode::toJsonHelperGeneric (std::map<ExecutionNode*, int>& indexTab,
                                         triagens::basics::Json& nodes,
                                         TRI_memory_zone_t* zone) {
  auto iter = indexTab.find(this);
  if (iter != indexTab.end()) {
    return Json();
  }

  for (size_t i = 0; i < _dependencies.size(); i++) {
    _dependencies[i]->toJsonHelper(indexTab, nodes, zone);
  }
  Json json;
  json = Json(Json::Array, 2)
           ("type", Json(getTypeString()));
  Json deps(Json::List, _dependencies.size());
  for (size_t i = 0; i < _dependencies.size(); i++) {
    auto it = indexTab.find(_dependencies[i]);
    if (it != indexTab.end()) {
      deps(Json(static_cast<double>(it->second)));
    }
    else {
      deps(Json("unknown"));
    }
  }
  json("dependencies", deps);
  json("index", Json(static_cast<double>(nodes.size())));
  if(this->_estimatedCost != 0){
    json("estimated cost", Json(this->_estimatedCost));
  }
  if (_varUsageValid) {
    Json varsValid(Json::List, _varsValid.size());
    for (auto v : _varsValid) {
      varsValid(Json(v->name));
    }
    json("varsValid", varsValid);
    Json varsUsedLater(Json::List, _varsUsedLater.size());
    for (auto v : _varsUsedLater) {
      varsUsedLater(Json(v->name));
    }
    json("varsUsedLater", varsUsedLater);
  }

  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                          methods of SingletonNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SingletonNode
////////////////////////////////////////////////////////////////////////////////

void SingletonNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                                  triagens::basics::Json& nodes,
                                  TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                methods of EnumerateCollectionNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for EnumerateCollectionNode
////////////////////////////////////////////////////////////////////////////////

void EnumerateCollectionNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                                            triagens::basics::Json& nodes,
                                            TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  json("database", Json(_vocbase->_name))
      ("collection", Json(_collection->name))
      ("outVariable", _outVariable->toJson());

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                      methods of EnumerateListNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for EnumerateListNode
////////////////////////////////////////////////////////////////////////////////

void EnumerateListNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                                      triagens::basics::Json& nodes,
                                      TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  json("inVariable",  _inVariable->toJson())
      ("outVariable", _outVariable->toJson());

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for IndexRangeNode
////////////////////////////////////////////////////////////////////////////////

void IndexRangeNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                                   triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone) {

  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  
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
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                              methods of LimitNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for LimitNode
////////////////////////////////////////////////////////////////////////////////

void LimitNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                              triagens::basics::Json& nodes,
                              TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  // Now put info about offset and limit in
  json("offset", Json(static_cast<double>(_offset)))
      ("limit",  Json(static_cast<double>(_limit)));

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                        methods of CalculationNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for CalculationNode
////////////////////////////////////////////////////////////////////////////////

void CalculationNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                                    triagens::basics::Json& nodes,
                                    TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  
  json("expression", _expression->toJson(TRI_UNKNOWN_MEM_ZONE))
      ("outVariable", _outVariable->toJson())
      ("canThrow", Json(_expression->canThrow()));


  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                           methods of SubqueryNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SubqueryNode
////////////////////////////////////////////////////////////////////////////////

void SubqueryNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                                 triagens::basics::Json& nodes,
                                 TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  json("subquery",  _subquery->toJson(TRI_UNKNOWN_MEM_ZONE))
      ("outVariable", _outVariable->toJson());

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of FilterNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for FilterNode
////////////////////////////////////////////////////////////////////////////////

void FilterNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                               triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  
  json("inVariable", _inVariable->toJson());

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                               methods of SortNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SortNode
////////////////////////////////////////////////////////////////////////////////

void SortNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                             triagens::basics::Json& nodes,
                             TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
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
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                          methods of AggregateNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for AggregateNode
////////////////////////////////////////////////////////////////////////////////

void AggregateNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                                  triagens::basics::Json& nodes,
                                  TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
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
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of ReturnNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for ReturnNode
////////////////////////////////////////////////////////////////////////////////

void ReturnNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                               triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method

  if (json.isEmpty()) {
    return;
  }
      
  json("inVariable", _inVariable->toJson());

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of RemoveNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void RemoveNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                               triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method

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
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of InsertNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void InsertNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                               triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method

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
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of UpdateNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void UpdateNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                               triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method

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
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                            methods of ReplaceNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson
////////////////////////////////////////////////////////////////////////////////

void ReplaceNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                                triagens::basics::Json& nodes,
                                TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method

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
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


