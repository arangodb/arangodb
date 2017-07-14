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
#include "Aql/Ast.h"
#include "Aql/VariableGenerator.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

/// @brief name of $OLD variable
char const* const Variable::NAME_OLD = "$OLD";

/// @brief name of $NEW variable
char const* const Variable::NAME_NEW = "$NEW";

/// @brief name of $CURRENT variable
char const* const Variable::NAME_CURRENT = "$CURRENT";

/// @brief create the variable
Variable::Variable(std::string const& name, VariableId id)
    : name(name), value(nullptr), id(id) {}

Variable::Variable(arangodb::velocypack::Slice const& slice)
    : Variable(arangodb::basics::VelocyPackHelper::checkAndGetStringValue(
                   slice, "name"),
               arangodb::basics::VelocyPackHelper::checkAndGetNumericValue<
                   VariableId>(slice, "id")) {}

/// @brief destroy the variable
Variable::~Variable() {}

/// @brief return a VelocyPack representation of the variable
void Variable::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder b(&builder);
  builder.add("id", VPackValue(static_cast<double>(id)));
  builder.add("name", VPackValue(name));
}

/// @brief replace a variable by another
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

/// @brief factory for (optional) variables from VPack
Variable* Variable::varFromVPack(Ast* ast,
                                 arangodb::velocypack::Slice const& base,
                                 char const* variableName, bool optional) {
  VPackSlice variable = base.get(variableName);

  if (variable.isNone()) {
    if (optional) {
      return nullptr;
    }

    std::string msg;
    msg +=
        "mandatory variable \"" + std::string(variableName) + "\" not found.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }
  return ast->variables()->createVariable(variable);
}

