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

Variable::Variable (triagens::basics::Json const& json)
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

triagens::basics::Json Variable::toJson () const {
  triagens::basics::Json json(triagens::basics::Json::Array, 2);
  json("id", triagens::basics::Json(static_cast<double>(id)))
      ("name", triagens::basics::Json(name));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a variable by another
////////////////////////////////////////////////////////////////////////////////

Variable const* Variable::replace (Variable const* variable,
                                   std::unordered_map<VariableId, Variable const*> const& replacements) {
  while (variable != nullptr) {
    auto it = replacements.find(variable->id);
    if (it != replacements.end()) {
      variable = (*it).second;
    }
    else {
      break;
    }
  }

  return variable;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
