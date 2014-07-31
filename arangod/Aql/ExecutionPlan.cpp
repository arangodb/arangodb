////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionPlans
///
/// @file arangod/Aql/ExecutionPlan.cpp
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

#include "Aql/ExecutionPlan.h"

using namespace triagens::basics;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                          methods of ExecutionPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, export an ExecutionPlan to JSON
////////////////////////////////////////////////////////////////////////////////

Json ExecutionPlan::toJson (TRI_memory_zone_t* zone) {
  std::map<ExecutionPlan*, int> indexTab;
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

Json ExecutionPlan::toJsonHelperGeneric (std::map<ExecutionPlan*, int>& indexTab,
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

void ExecutionPlan::appendAsString (std::string& st, int indent) {
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

void ExecutionPlan::walk (WalkerWorker& worker) {
  worker.before(this);
  for (auto it = _dependencies.begin();
            it != _dependencies.end(); 
            ++it) {
    (*it)->walk(worker);
  }
  if (getType() == SUBQUERY) {
    auto p = static_cast<SubqueryPlan*>(this);
    worker.enterSubquery(this, p->getSubquery());
    p->getSubquery()->walk(worker);
    worker.leaveSubquery(this, p->getSubquery());
  }
  worker.after(this);
}

// -----------------------------------------------------------------------------
// --SECTION--                                methods of SingletonPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SingletonPlan
////////////////////////////////////////////////////////////////////////////////

void SingletonPlan::toJsonHelper (
                std::map<ExecutionPlan*, int>& indexTab,
                triagens::basics::Json& nodes,
                TRI_memory_zone_t* zone) {
  Json json(ExecutionPlan::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                methods of EnumerateCollectionPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for EnumerateCollectionPlan
////////////////////////////////////////////////////////////////////////////////

void EnumerateCollectionPlan::toJsonHelper (std::map<ExecutionPlan*, int>& indexTab,
                                            triagens::basics::Json& nodes,
                                            TRI_memory_zone_t* zone) {
  Json json(ExecutionPlan::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }

  // Now put info about vocbase and cid in there
  if (_vocbase == nullptr) {
    json("vocbase", Json("<nullptr>"));
  }
  else {
    json("vocbase", Json(_vocbase->_name));
  }
  json("collection", Json(_collname))
      ("outVariable", _outVariable->toJson());

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                      methods of EnumerateListPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for EnumerateListPlan
////////////////////////////////////////////////////////////////////////////////

void EnumerateListPlan::toJsonHelper (std::map<ExecutionPlan*, int>& indexTab,
                                      triagens::basics::Json& nodes,
                                      TRI_memory_zone_t* zone) {
  Json json(ExecutionPlan::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  json("varNumber",    Json(static_cast<double>(_varNumber)))
      ("varName",      Json(_varName))
      ("outVarNumber", Json(static_cast<double>(_outVarNumber)))
      ("outVarName",   Json(_outVarName));


  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                              methods of LimitPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for LimitPlan
////////////////////////////////////////////////////////////////////////////////

void LimitPlan::toJsonHelper (std::map<ExecutionPlan*, int>& indexTab,
                              triagens::basics::Json& nodes,
                              TRI_memory_zone_t* zone) {
  Json json(ExecutionPlan::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
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
// --SECTION--                                        methods of CalculationPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for CalculationPlan
////////////////////////////////////////////////////////////////////////////////

void CalculationPlan::toJsonHelper (std::map<ExecutionPlan*, int>& indexTab,
                                    triagens::basics::Json& nodes,
                                    TRI_memory_zone_t* zone) {
  Json json(ExecutionPlan::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  json("varNumber",  Json(static_cast<double>(_varNumber)))
      ("varName",    Json(_varName))
      ("expression", _expression->toJson(TRI_UNKNOWN_MEM_ZONE));

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                           methods of SubqueryPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SubqueryPlan
////////////////////////////////////////////////////////////////////////////////

void SubqueryPlan::toJsonHelper (std::map<ExecutionPlan*, int>& indexTab,
                                 triagens::basics::Json& nodes,
                                 TRI_memory_zone_t* zone) {
  Json json(ExecutionPlan::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  json("varNumber", Json(static_cast<double>(_varNumber)))
      ("varName",   Json(_varName))
      ("subquery",  _subquery->toJson(TRI_UNKNOWN_MEM_ZONE));

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                         methods of ProjectionPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for ProjectionPlan
////////////////////////////////////////////////////////////////////////////////

void ProjectionPlan::toJsonHelper (std::map<ExecutionPlan*, int>& indexTab,
                                   triagens::basics::Json& nodes,
                                   TRI_memory_zone_t* zone) {
  Json json(ExecutionPlan::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  Json vec(Json::List,_keepAttributes.size());
  for (auto it = _keepAttributes.begin(); it != _keepAttributes.end(); ++it) {
    vec(Json(*it));
  }
  json("inVarNumber",    Json(static_cast<double>(_inVar)))
      ("inVarName",      Json(_inVarName))
      ("outVarNumber",   Json(static_cast<double>(_outVar)))
      ("outVarName",     Json(_outVarName))
      ("keepAttributes", vec);

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of FilterPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for FilterPlan
////////////////////////////////////////////////////////////////////////////////

void FilterPlan::toJsonHelper (std::map<ExecutionPlan*, int>& indexTab,
                               triagens::basics::Json& nodes,
                               TRI_memory_zone_t* zone) {
  Json json(ExecutionPlan::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  // Now put info about offset and limit in
  json("varName",   Json(_varName))
      ("varNumber", Json(static_cast<double>(_varNumber)));

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                               methods of SortPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SortPlan
////////////////////////////////////////////////////////////////////////////////

void SortPlan::toJsonHelper (std::map<ExecutionPlan*, int>& indexTab,
                             triagens::basics::Json& nodes,
                             TRI_memory_zone_t* zone) {
  Json json(ExecutionPlan::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  Json values(Json::List, _elements.size());
  for (auto it = _elements.begin(); it != _elements.end(); ++it) {
    Json element(Json::Array);
    element("varName",   Json(std::get<1>(*it)))
           ("varNumber", Json(static_cast<double>(std::get<0>(*it))))
           ("ascending", Json(std::get<2>(*it)));
    values(element);
  }
  json("elements", values);

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                methods of AggregateOnUnSortedPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for AggregateOnUnSortedPlan
////////////////////////////////////////////////////////////////////////////////

void AggregateOnUnsortedPlan::toJsonHelper (std::map<ExecutionPlan*, int>& indexTab,
                                            triagens::basics::Json& nodes,
                                            TRI_memory_zone_t* zone) {
  Json json(ExecutionPlan::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  Json numbers(Json::List, _varNumbers.size());
  for (auto it = _varNumbers.begin(); it != _varNumbers.end(); ++it) {
    numbers(Json(static_cast<double>(*it)));
  }
  Json names(Json::List, _varNames.size());
  for (auto it = _varNames.begin(); it != _varNames.end(); ++it) {
    names(Json(*it));
  }
  json("varNumbers",   numbers)
      ("varNames",     names)
      ("outVarNumber", Json(static_cast<double>(_outVarNumber)))
      ("outVarName",   Json(_outVarName));

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// -----------------------------------------------------------------------------
// --SECTION--                                               methods of RootPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for RootPlan
////////////////////////////////////////////////////////////////////////////////

void RootPlan::toJsonHelper (std::map<ExecutionPlan*, int>& indexTab,
                             triagens::basics::Json& nodes,
                             TRI_memory_zone_t* zone) {
  Json json(ExecutionPlan::toJsonHelperGeneric(indexTab, nodes, zone));  // call base class method
  if (json.isEmpty()) {
    return;
  }
  json("varNumber", Json(static_cast<double>(_varNumber)))
      ("varName"  , Json(_varName));

  // And add it:
  int len = static_cast<int>(nodes.size());
  nodes(json);
  indexTab.insert(make_pair(this, len));
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


