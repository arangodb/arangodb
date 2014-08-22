////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, AST variable
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Variable.h"
#include "Basics/JsonHelper.h"

using namespace triagens::aql;
using namespace triagens::basics;
using Json = triagens::basics::Json;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the variable
////////////////////////////////////////////////////////////////////////////////

Variable::Variable (std::string const& name,
                    VariableId id) 
  : name(name),
    value(nullptr),
    id(id) {
}

Variable::Variable (Json const& json)
  : Variable(JsonHelper::checkAndGetStringValue(json.json(), "name"), JsonHelper::checkAndGetNumericValue<VariableId>(json.json(), "id")) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the variable
////////////////////////////////////////////////////////////////////////////////

Variable::~Variable () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the variable
////////////////////////////////////////////////////////////////////////////////

Json Variable::toJson () const {
  Json json(triagens::basics::Json::Array, 2);
  json("id", Json(static_cast<double>(id)))
      ("name", Json(name));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares two variables, using their ids
////////////////////////////////////////////////////////////////////////////////
    
bool Variable::Comparator (Variable const* l, 
                           Variable const* r) {
  return l->id < r->id;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
