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
// --SECTION--                                          methods of ExecutionNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, export an ExecutionNode to JSON
////////////////////////////////////////////////////////////////////////////////

Json ExecutionNode::toJson (TRI_memory_zone_t* zone) {
  std::map<ExecutionNode*, int> indexTab;
  Json json;
  Json nodes;
  try {
    nodes = Json(Json::List,10);
    toJsonHelper(indexTab, nodes, zone);
    json = Json(Json::Array,1)
             ("nodes", nodes);
  }
  catch (std::exception& e) {
    return Json();
  }
  return json;
}

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
  json = Json(Json::Array,2)
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

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert to a string, basically for debugging purposes
////////////////////////////////////////////////////////////////////////////////

static void someSpaces (std::string& st, int nr) {
  for (int i = 0; i < nr; i++) {
    st.push_back(' ');
  }
}

void ExecutionNode::appendAsString (std::string& st, int indent) {
  someSpaces(st, indent);
  st.push_back('<');
  st.append(getTypeString());
  if (_dependencies.size() != 0) {
    st.push_back('\n');
    for (size_t i = 0; i < _dependencies.size(); i++) {
      _dependencies[i]->appendAsString(st, indent+2);
      if (i != _dependencies.size()-1) {
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

void ExecutionNode::walk (WalkerWorker* worker) {
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
// --SECTION--                                methods of SingletonNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SingletonNode
////////////////////////////////////////////////////////////////////////////////

void SingletonNode::toJsonHelper (
                std::map<ExecutionNode*, int>& indexTab,
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
      ("collection", Json(_collname))
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
      ("outVariable", _outVariable->toJson());

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
// --SECTION--                                         methods of ProjectionNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for ProjectionNode
////////////////////////////////////////////////////////////////////////////////

void ProjectionNode::toJsonHelper (std::map<ExecutionNode*, int>& indexTab,
                                   triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone) {
  Json json(ExecutionNode::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  Json vec(Json::List,_keepAttributes.size());
  for (auto it = _keepAttributes.begin(); it != _keepAttributes.end(); ++it) {
    vec(Json(*it));
  }
  
  json("inVariable", _inVariable->toJson())
      ("outVariable", _outVariable->toJson())
      ("keepAttributes", vec);

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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


