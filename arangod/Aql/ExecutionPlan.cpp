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
    json("attribute", Json(_attribute))
        ("value", Json(_value.copy()));
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

// -----------------------------------------------------------------------------
// --SECTION--                                        methods of RootPlan
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
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


