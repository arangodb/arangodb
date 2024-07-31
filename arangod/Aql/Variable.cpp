////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/QueryContext.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;

/// @brief create the variable
Variable::Variable(std::string name, VariableId id,
                   bool isFullDocumentFromCollection,
                   arangodb::ResourceMonitor& resourceMonitor)
    : id(id),
      name(std::move(name)),
      isFullDocumentFromCollection(isFullDocumentFromCollection),
      _resourceMonitor(resourceMonitor) {}

Variable::Variable(velocypack::Slice slice,
                   arangodb::ResourceMonitor& resourceMonitor)
    : id(basics::VelocyPackHelper::checkAndGetNumericValue<VariableId>(slice,
                                                                       "id")),
      name(basics::VelocyPackHelper::checkAndGetStringValue(slice, "name")),
      isFullDocumentFromCollection(basics::VelocyPackHelper::getBooleanValue(
          slice, "isFullDocumentFromCollection", false)),
      _resourceMonitor(resourceMonitor) {
  setConstantValue(AqlValue{slice.get("constantValue")});
  if (auto s = slice.get("bindParameter"); s.isString()) {
    setBindParameterReplacement(s.copyString());
  }
}

/// @brief destroy the variable
Variable::~Variable() {
  _resourceMonitor.decreaseMemoryUsage(_constantValue.memoryUsage());
  _constantValue.destroy();
}

Variable* Variable::clone() const {
  TRI_ASSERT(type() == Type::Regular);
  return new Variable(name, id, isFullDocumentFromCollection, _resourceMonitor);
}

bool Variable::isUserDefined() const noexcept {
  TRI_ASSERT(!name.empty());
  char c = name[0];
  // variables starting with a number are not user-defined
  return (c < '0' || c > '9');
}

bool Variable::needsRegister() const noexcept {
  TRI_ASSERT(!name.empty());
  // variables starting with a number are not user-defined
  return isUserDefined() || name.back() != '_';
}

/// @brief return a VelocyPack representation of the variable
void Variable::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder b(&builder);
  toVelocyPackCommon(builder);
}

/// @brief return a VelocyPack representation of the variable
void Variable::toVelocyPack(velocypack::Builder& builder,
                            Variable::WithConstantValue /*tag*/) const {
  VPackObjectBuilder b(&builder);
  toVelocyPackCommon(builder);

  if (type() == Variable::Type::Const) {
    builder.add(VPackValue("constantValue"));
    _constantValue.toVelocyPack(nullptr, builder, /*allowUnindexed*/ true);
  }
}

void Variable::toVelocyPackCommon(velocypack::Builder& builder) const {
  builder.add("id", VPackValue(id));
  builder.add("name", VPackValue(name));
  builder.add("isFullDocumentFromCollection",
              VPackValue(isFullDocumentFromCollection));
  if (type() == Variable::Type::BindParameter) {
    builder.add("bindParameter", VPackValue(_bindParameterName));
  }
}

/// @brief replace a variable by another
Variable const* Variable::replace(
    Variable const* variable,
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  while (variable != nullptr) {
    auto it = replacements.find(variable->id);
    if (it == replacements.end()) {
      break;
    }
    variable = (*it).second;
  }

  return variable;
}

/// @brief factory for (optional) variables from VPack
Variable* Variable::varFromVPack(Ast* ast, velocypack::Slice base,
                                 std::string_view variableName, bool optional) {
  VPackSlice variable = base.get(variableName);

  if (variable.isNone()) {
    if (optional) {
      return nullptr;
    }

    std::string msg;
    msg +=
        "mandatory variable \"" + std::string(variableName) + "\" not found.";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::move(msg));
  }
  return ast->variables()->createVariable(variable);
}

bool Variable::isEqualTo(Variable const& other) const noexcept {
  return (id == other.id) && (name == other.name);
}

Variable::Type Variable::type() const noexcept {
  if (!_bindParameterName.empty()) {
    return Variable::Type::BindParameter;
  }
  if (_constantValue.isNone()) {
    return Variable::Type::Regular;
  }
  return Variable::Type::Const;
}

void Variable::setConstantValue(AqlValue value) {
  _resourceMonitor.decreaseMemoryUsage(_constantValue.memoryUsage());
  _constantValue.destroy();

  try {
    _resourceMonitor.increaseMemoryUsage(value.memoryUsage());
    _constantValue = value;
  } catch (...) {
    throw;
  }
}

std::string_view Variable::bindParameterName() const noexcept {
  TRI_ASSERT(type() == Type::BindParameter);
  return _bindParameterName;
}

void Variable::setBindParameterReplacement(std::string bindName) {
  TRI_ASSERT(type() == Type::Regular);
  _bindParameterName = std::move(bindName);
}
