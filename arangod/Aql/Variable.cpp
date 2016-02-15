////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Variable.h"
#include "Basics/JsonHelper.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;
using JsonHelper = arangodb::basics::JsonHelper;

////////////////////////////////////////////////////////////////////////////////
/// @brief name of $OLD variable
////////////////////////////////////////////////////////////////////////////////

char const* const Variable::NAME_OLD = "$OLD";

////////////////////////////////////////////////////////////////////////////////
/// @brief name of $NEW variable
////////////////////////////////////////////////////////////////////////////////

char const* const Variable::NAME_NEW = "$NEW";

////////////////////////////////////////////////////////////////////////////////
/// @brief name of $CURRENT variable
////////////////////////////////////////////////////////////////////////////////

char const* const Variable::NAME_CURRENT = "$CURRENT";

////////////////////////////////////////////////////////////////////////////////
/// @brief create the variable
////////////////////////////////////////////////////////////////////////////////

Variable::Variable(std::string const& name, VariableId id)
    : name(name), value(nullptr), id(id) {}

Variable::Variable(arangodb::basics::Json const& json)
    : Variable(
          JsonHelper::checkAndGetStringValue(json.json(), "name"),
          JsonHelper::checkAndGetNumericValue<VariableId>(json.json(), "id")) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the variable
////////////////////////////////////////////////////////////////////////////////

Variable::~Variable() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the variable
////////////////////////////////////////////////////////////////////////////////

arangodb::basics::Json Variable::toJson() const {
  arangodb::basics::Json json(arangodb::basics::Json::Object, 2);
  json("id", arangodb::basics::Json(static_cast<double>(id)))(
      "name", arangodb::basics::Json(name));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a VelocyPack representation of the variable
////////////////////////////////////////////////////////////////////////////////

void Variable::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder b(&builder);
  builder.add("id", VPackValue(static_cast<double>(id)));
  builder.add("name", VPackValue(name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a variable by another
////////////////////////////////////////////////////////////////////////////////

Variable const* Variable::replace(
    Variable const* variable,
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  while (variable != nullptr) {
    auto it = replacements.find(variable->id);
    if (it != replacements.end()) {
      variable = (*it).second;
    } else {
      break;
    }
  }

  return variable;
}
