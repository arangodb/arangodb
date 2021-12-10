////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/debugging.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

/// @brief create the variable
Variable::Variable(std::string name, VariableId id, bool isDataFromCollection)
    : id(id), 
      name(std::move(name)), 
      isDataFromCollection(isDataFromCollection) {}

Variable::Variable(arangodb::velocypack::Slice const& slice)
    : id(arangodb::basics::VelocyPackHelper::checkAndGetNumericValue<VariableId>(slice, "id")),
      name(arangodb::basics::VelocyPackHelper::checkAndGetStringValue(slice, "name")),
      isDataFromCollection(arangodb::basics::VelocyPackHelper::getBooleanValue(slice, "isDataFromCollection", false)),
      _constantValue(slice.get("constantValue")) {}

/// @brief destroy the variable
Variable::~Variable() {
  _constantValue.destroy();
}
  
Variable* Variable::clone() const { 
  return new Variable(name, id, isDataFromCollection); 
}
  
bool Variable::isUserDefined() const {
  TRI_ASSERT(!name.empty());
  char const c = name[0];
  // variables starting with a number are not user-defined
  return (c < '0' || c > '9');
}
  
bool Variable::needsRegister() const {
  TRI_ASSERT(!name.empty());
  // variables starting with a number are not user-defined
  return isUserDefined() || name.back() != '_';
}

/// @brief return a VelocyPack representation of the variable
void Variable::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder b(&builder);
  builder.add("id", VPackValue(id));
  builder.add("name", VPackValue(name));
  builder.add("isDataFromCollection", VPackValue(isDataFromCollection));
  if (type() == Variable::Type::Const) {
    builder.add(VPackValue("constantValue"));
    _constantValue.toVelocyPack(nullptr, builder, /*resolveExternals*/ false,
                                /*allowUnindexed*/ true);
  }
}

/// @brief replace a variable by another
Variable const* Variable::replace(Variable const* variable,
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
Variable* Variable::varFromVPack(Ast* ast, arangodb::velocypack::Slice const& base,
                                 char const* variableName, bool optional) {
  VPackSlice variable = base.get(variableName);

  if (variable.isNone()) {
    if (optional) {
      return nullptr;
    }

    std::string msg;
    msg += "mandatory variable \"" + std::string(variableName) + "\" not found.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }
  return ast->variables()->createVariable(variable);
}

bool Variable::isEqualTo(Variable const& other) const {
  return (id == other.id) && (name == other.name);
}

Variable::Type Variable::type() const noexcept {
  if (_constantValue.isNone()) {
    return Variable::Type::Regular;
  }
  return Variable::Type::Const;
}

void Variable::setConstantValue(AqlValue value) noexcept {
  _constantValue.destroy();
  _constantValue = value;
}
