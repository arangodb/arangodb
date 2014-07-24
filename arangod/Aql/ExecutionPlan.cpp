////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionPlans
///
/// @file arangod/Aql/ExecutionPlan.h
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

#include <Basics/JsonHelper.h>

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                          methods of ExecutionPlan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, export an ExecutionPlan to JSON
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* ExecutionPlan::toJson (TRI_memory_zone_t* zone) {
  TRI_json_t* json = TRI_CreateArray2Json(zone, 2);
  if (nullptr == json) {
    return nullptr;
  }

  std::string type = getTypeString();
  TRI_json_t* sub = TRI_CreateString2CopyJson(zone, type.c_str(), type.size());
  if (nullptr == sub) {
    TRI_FreeJson(zone, json);
    return nullptr;
  }
  TRI_Insert3ArrayJson(zone, json, "type", sub);

  if (_dependencies.size() != 0) {
    sub = TRI_CreateList2Json(zone, _dependencies.size());
    if (nullptr == sub) {
      TRI_FreeJson(zone, json);
      return nullptr;
    }
    for (size_t i = 0; i < _dependencies.size(); i++) {
      TRI_json_t* subsub = _dependencies[i]->toJson(zone);
      if (subsub == nullptr) {
        TRI_FreeJson(zone, json);
        return nullptr;
      }
      TRI_PushBack3ListJson(zone, sub, subsub);
    }
    TRI_Insert3ArrayJson(zone, json, "dependencies", sub);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for EnumerateCollectionPlan
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* EnumerateCollectionPlan::toJson (TRI_memory_zone_t* zone) {
  auto ep = static_cast<ExecutionPlan*>(this);
  TRI_json_t* json = ep->toJson(zone);
  if (nullptr == json) {
    return nullptr;
  }
  // Now put info about vocbase and cid in there
  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test function
////////////////////////////////////////////////////////////////////////////////

using namespace triagens::basics;

void testExecutionPlans () {
  ExecutionPlan* e = new ExecutionPlan();
  ExecutionPlan* f = new ExecutionPlan(e);
  std::string st;
  e->appendAsString(st, 0);
  std::cout << "e as string:\n" << st << std::endl;
  st.clear();
  f->appendAsString(st, 0);
  std::cout << "f as string:\n" << st << std::endl;
  TRI_json_t* json = e->toJson(TRI_UNKNOWN_MEM_ZONE);
  if (json != nullptr) {
    std::cout << "e as JSON:\n" << 
              JsonHelper::toString(json) << std::endl;
  }
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  json = f->toJson(TRI_UNKNOWN_MEM_ZONE);
  if (json != nullptr) {
    std::cout << "f as JSON:\n" << 
              JsonHelper::toString(json) << std::endl;
  }
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  delete f;  // should not leave a leak

  json = Json(12);
  std::cout << JsonHelper::toString(json) << std::endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
  json = Json(true);
  std::cout << JsonHelper::toString(json) << std::endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  json = Json(Json::Null);
  std::cout << JsonHelper::toString(json) << std::endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  json = Json(Json::String);
  std::cout << JsonHelper::toString(json) << std::endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  json = Json(Json::List);
  std::cout << JsonHelper::toString(json) << std::endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  json = Json(Json::Array);
  std::cout << JsonHelper::toString(json) << std::endl;
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
  std::cout << JsonHelper::toString(json) << std::endl;
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  Json j(Json::Array);
       j("a", Json(12))
        ("b", Json(true))
        ("c", Json(Json::List) 
                (Json(1))(Json(2))(Json(3)))
        ("d", Json(Json::Array)
                ("x", Json(12))
                ("y", Json(true)));
  std::cout << j.toString() << std::endl;

  std::cout << j.get("a").toString() << std::endl;

  Json k = j.get("c");
  Json l = k.at(2);

  std::cout << l.toString() << std::endl;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


