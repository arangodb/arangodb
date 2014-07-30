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

Json ExecutionPlan::toJson (TRI_memory_zone_t* zone) const {
  Json json;
  try {
    json = Json(Json::Array,2)
             ("type", Json(getTypeString()));
  }
  catch (std::exception& e) {
    return json;
  }
  if (_dependencies.size() != 0) {
    try {
      Json deps(Json::List, _dependencies.size());
      for (size_t i = 0; i < _dependencies.size(); i++) {
        deps(_dependencies[i]->toJson(zone));
      }
      json("dependencies", deps);
    }
    catch (std::exception& e) {
      return Json();  // returns an empty one
    }
  }
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

// -----------------------------------------------------------------------------
// --SECTION--                                methods of SingletonPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SingletonPlan
////////////////////////////////////////////////////////////////////////////////

Json SingletonPlan::toJson (TRI_memory_zone_t* zone) const {
  Json json(ExecutionPlan::toJson(zone));  // call base class method
  if (json.isEmpty()) {
    return json;
  }
  // Now put info about number of vars:
  try {
    json("nrVariables", Json(_nrVars));
  }
  catch (std::exception& e) {
    return Json();
  }

  // And return it:
  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                methods of EnumerateCollectionPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for EnumerateCollectionPlan
////////////////////////////////////////////////////////////////////////////////

Json EnumerateCollectionPlan::toJson (TRI_memory_zone_t* zone) const {
  Json json(ExecutionPlan::toJson(zone));  // call base class method
  if (json.isEmpty()) {
    return json;
  }
  // Now put info about vocbase and cid in there
  try {
    if (_vocbase == nullptr) {
      json("vocbase", Json("<nullptr>"));
    }
    else {
      json("vocbase", Json(_vocbase->_name));
    }
    json("collection", Json(_collname))
        ("nrVariables", Json(_nrVars));
  }
  catch (std::exception& e) {
    return Json();
  }

  // And return it:
  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      methods of EnumerateListPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for EnumerateListPlan
////////////////////////////////////////////////////////////////////////////////

Json EnumerateListPlan::toJson (TRI_memory_zone_t* zone) const {
  Json json(ExecutionPlan::toJson(zone));  // call base class method
  if (json.isEmpty()) {
    return json;
  }
  try {
    json("varNumber", Json(static_cast<double>(_varNumber)))
        ("varName",   Json(_varName));
  }
  catch (std::exception& e) {
    return Json();
  }

  // And return it:
  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                              methods of LimitPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for LimitPlan
////////////////////////////////////////////////////////////////////////////////

Json LimitPlan::toJson (TRI_memory_zone_t* zone) const {
  Json json(ExecutionPlan::toJson(zone));  // call base class method
  if (json.isEmpty()) {
    return json;
  }
  // Now put info about offset and limit in
  try {
    json("offset", Json(static_cast<double>(_offset)))
        ("limit",  Json(static_cast<double>(_limit)));
  }
  catch (std::exception& e) {
    return Json();
  }

  // And return it:
  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        methods of CalculationPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for CalculationPlan
////////////////////////////////////////////////////////////////////////////////

Json CalculationPlan::toJson (TRI_memory_zone_t* zone) const {
  Json json(ExecutionPlan::toJson(zone));  // call base class method
  if (json.isEmpty()) {
    return json;
  }
  try {
    json("varNumber",  Json(static_cast<double>(_varNumber)))
        ("varName",    Json(_varName))
        ("expression", _expression->toJson(TRI_UNKNOWN_MEM_ZONE));
  }
  catch (std::exception& e) {
    return Json();
  }

  // And return it:
  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                         methods of ProjectionPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for ProjectionPlan
////////////////////////////////////////////////////////////////////////////////

Json ProjectionPlan::toJson (TRI_memory_zone_t* zone) const {
  Json json(ExecutionPlan::toJson(zone));  // call base class method
  if (json.isEmpty()) {
    return json;
  }
  try {
    Json vec(Json::List,_keepAttributes.size());
    for (auto it = _keepAttributes.begin(); it != _keepAttributes.end(); ++it) {
      vec(Json(*it));
    }
    json("inVarNumber",    Json(static_cast<double>(_inVar)))
        ("inVarName",      Json(_inVarName))
        ("outVarNumber",   Json(static_cast<double>(_outVar)))
        ("outVarName",     Json(_outVarName))
        ("keepAttributes", vec);
  }
  catch (std::exception& e) {
    return Json();
  }

  // And return it:
  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                             methods of FilterPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for FilterPlan
////////////////////////////////////////////////////////////////////////////////

Json FilterPlan::toJson (TRI_memory_zone_t* zone) const {
  Json json(ExecutionPlan::toJson(zone));  // call base class method
  if (json.isEmpty()) {
    return json;
  }
  // Now put info about offset and limit in
  try {
    json("varName",   Json(_varName))
        ("varNumber", Json(static_cast<double>(_varNumber)));
  }
  catch (std::exception& e) {
    return Json();
  }

  // And return it:
  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                               methods of SortPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for SortPlan
////////////////////////////////////////////////////////////////////////////////

Json SortPlan::toJson (TRI_memory_zone_t* zone) const {
  Json json(ExecutionPlan::toJson(zone));  // call base class method
  if (json.isEmpty()) {
    return json;
  }
  try {
    Json numbers(Json::List, _varNumbers.size());
    for (auto it = _varNumbers.begin(); it != _varNumbers.end(); ++it) {
      numbers(Json(static_cast<double>(*it)));
    }
    Json names(Json::List, _varNames.size());
    for (auto it = _varNames.begin(); it != _varNames.end(); ++it) {
      names(Json(*it));
    }
    Json vec(Json::List, _sortAscending.size());
    for (auto it = _sortAscending.begin(); it != _sortAscending.end(); ++it) {
      vec(Json(*it));
    }
    json("varNumbers",    numbers)
        ("varNames",      names)
        ("sortAscending", vec);
  }
  catch (std::exception& e) {
    return Json();
  }

  // And return it:
  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                methods of AggregateOnUnSortedPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for AggregateOnUnSortedPlan
////////////////////////////////////////////////////////////////////////////////

Json AggregateOnUnsortedPlan::toJson (TRI_memory_zone_t* zone) const {
  Json json(ExecutionPlan::toJson(zone));  // call base class method
  if (json.isEmpty()) {
    return json;
  }
  try {
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
        ("outVarName",   Json(_outVarName))
        ("nrVars",       Json(static_cast<double>(_nrVars)));
  }
  catch (std::exception& e) {
    return Json();
  }

  // And return it:
  return json;
}

// -----------------------------------------------------------------------------
// --SECTION--                                               methods of RootPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for RootPlan
////////////////////////////////////////////////////////////////////////////////

Json RootPlan::toJson (TRI_memory_zone_t* zone) const {
  Json json(ExecutionPlan::toJson(zone));  // call base class method
  if (json.isEmpty()) {
    return json;
  }
  // Now put info about ...
  try {
    // TODO: add specific stuff
  }
  catch (std::exception& e) {
    return Json();
  }

  // And return it:
  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test function
////////////////////////////////////////////////////////////////////////////////

using namespace triagens::basics;

using namespace std;

void testExecutionPlans () {
  Json a(12);
  Json b(Json::Array);
  b("a",a);
  std::cout << b.toString() << std::endl;
  std::cout << a.toString() << std::endl;
  std::cout << "Got here" << std::endl;

  auto ec = new EnumerateCollectionPlan(nullptr, "guck", 1);
  Json jjj(ec->toJson());
  cout << jjj.toString() << endl;
  auto li = new LimitPlan(12, 17);
  li->addDependency(ec);
  jjj = li->toJson();
  cout << jjj.toString() << endl;

  TRI_json_t* json = Json(12);
  cout << JsonHelper::toString(json) << endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
  json = Json(true);
  cout << JsonHelper::toString(json) << endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  json = Json(Json::Null);
  cout << JsonHelper::toString(json) << endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  json = Json(Json::String);
  cout << JsonHelper::toString(json) << endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  json = Json(Json::List);
  cout << JsonHelper::toString(json) << endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  json = Json(Json::Array);
  cout << JsonHelper::toString(json) << endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  json = Json(Json::Array, 10)
           ("myinteger", Json(12))
           ("mystring", Json("hallo"))
           ("mybool", Json(false))
           ("mynull", Json(Json::Null))
           ("mylist", Json(Json::List, 3)
                            (Json(1))
                            (Json(2))
                            (Json(3)))
           ("myarray", Json(Json::Array, 2)
                            ("a",Json("hallo"))
                            ("b",Json(13)));
  cout << JsonHelper::toString(json) << endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  Json j(Json::Array);
       j("a", Json(12))
        ("b", Json(true))
        ("c", Json(Json::List) 
                (Json(1))(Json(2))(Json(3)))
        ("d", Json(Json::Array)
                ("x", Json(12))
                ("y", Json(true)));
  cout << j.toString() << endl;

  // We expect to see exactly two copies here:
  Json jjjj = j.copy();  // create an explicit copy
  Json jj(12);
  
  cout << "Before assignment" << jj.toString() << endl;
  jj = j;  // this steals the pointer from j

  cout << jjjj.toString();
  cout << jj.toString();

  Json k = jj.get("c");
  Json l = k.at(2);

  cout << l.toString() << endl;

  cout << "and now a complete test" << endl << endl;

  // Here we want to build the unoptimised plan for:
  //
  // LET A = [1,2,3]
  // FOR g IN guck
  //   FILTER g.name == "Wurst"
  //   SORT g.vorname
  //   LIMIT 2, 10
  //   FOR i IN A
  //     LET X = { "vorname": g.vorname, "addresse": g.addresse,
  //               "nr": i*2 }
  //     RETURN X
  //
  //     
  {
  AstNode* a;
  AstNode* b;
  AstNode* c;
  ExecutionPlan* e;
  ExecutionPlan* n;

  // Singleton
  e = new SingletonPlan(1);

  // LET A = [1,2,3]
  a = new AstNode(NODE_TYPE_LIST);
  b = new AstNode(NODE_TYPE_VALUE);
  b->setValueType(VALUE_TYPE_INT);
  b->setIntValue(1);
  a->addMember(b);
  b = new AstNode(NODE_TYPE_VALUE);
  b->setValueType(VALUE_TYPE_INT);
  b->setIntValue(2);
  a->addMember(b);
  b = new AstNode(NODE_TYPE_VALUE);
  b->setValueType(VALUE_TYPE_INT);
  b->setIntValue(3);
  a->addMember(b);
  n = new CalculationPlan(new AqlExpression(a), 1, "A");
  n->addDependency(e);
  e = n;

  // FOR g IN guck
  n = new EnumerateCollectionPlan(nullptr, "guck", 3);
  n->addDependency(e);
  e = n;

  // FILTER g.name == "Wurst"
  a = new AstNode(NODE_TYPE_OPERATOR_BINARY_EQ);
  b = new AstNode(NODE_TYPE_VALUE);
  b->setValueType(VALUE_TYPE_STRING);
  b->setStringValue("g.name");
  a->addMember(b);
  b = new AstNode(NODE_TYPE_VALUE);
  b->setValueType(VALUE_TYPE_STRING);
  b->setStringValue("Wurst");
  a->addMember(b);
  n = new CalculationPlan(new AqlExpression(a), 2, "_1");
  n->addDependency(e);
  e = n;
  n = new FilterPlan(2, "_1");
  n->addDependency(e);
  e = n;

  // SORT g.vorname
  a = new AstNode(NODE_TYPE_VALUE);
  a->setValueType(VALUE_TYPE_STRING);
  a->setStringValue("g.vorname");
  n = new CalculationPlan(new AqlExpression(a), 3, "_2");
  n->addDependency(e);
  e = n;
  std::vector<VariableId> vars;
  std::vector<std::string> names;
  std::vector<bool> asc;
  vars.push_back(3);
  names.push_back(string("_2"));
  asc.push_back(true);
  n = new SortPlan(vars, names, asc);
  n->addDependency(e);
  e = n;

  // LIMIT 2, 10
  n = new LimitPlan(2, 10);
  n->addDependency(e);
  e = n;

  // FOR i in A
  n = new EnumerateListPlan(1, "A");
  n->addDependency(e);
  e = n;

  // LET X = {"vorname": g.vorname, "addresse": g.addresse, "nr": i*2}
  a = new AstNode(NODE_TYPE_ARRAY);
  b = new AstNode(NODE_TYPE_ARRAY_ELEMENT);
  b->setValueType(VALUE_TYPE_STRING);
  b->setStringValue("vorname");
  c = new AstNode(NODE_TYPE_VALUE);
  c->setValueType(VALUE_TYPE_STRING);
  c->setStringValue("g.vorname");
  b->addMember(c);
  a->addMember(b);
  b = new AstNode(NODE_TYPE_ARRAY_ELEMENT);
  b->setValueType(VALUE_TYPE_STRING);
  b->setStringValue("adresse");
  c = new AstNode(NODE_TYPE_VALUE);
  c->setValueType(VALUE_TYPE_STRING);
  c->setStringValue("g.addresse");
  b->addMember(c);
  a->addMember(b);
  b = new AstNode(NODE_TYPE_ARRAY_ELEMENT);
  b->setValueType(VALUE_TYPE_STRING);
  b->setStringValue("nr");
  c = new AstNode(NODE_TYPE_VALUE);
  c->setValueType(VALUE_TYPE_STRING);
  c->setStringValue("i*2");
  b->addMember(c);
  a->addMember(b);
  n = new CalculationPlan(new AqlExpression(a), 4, "X");
  n->addDependency(e);
  e = n;

  // RETURN X
  n = new RootPlan();
  n->addDependency(e);
  e = n;

  cout << e->toJson().toString() << endl;
  }
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


