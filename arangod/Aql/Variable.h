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

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "Aql/AqlValue.h"
#include "Aql/types.h"

namespace arangodb {
struct ResourceMonitor;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
class Ast;
struct AstNode;

struct Variable {
  struct WithConstantValue {};

  /// @brief name of $OLD variable
  static constexpr std::string_view NAME_OLD = "$OLD";

  /// @brief name of $NEW variable
  static constexpr std::string_view NAME_NEW = "$NEW";

  /// @brief name of $CURRENT variable
  static constexpr std::string_view NAME_CURRENT = "$CURRENT";

  /// @brief indicates the type of the variable
  enum class Type {
    /// @brief a regular variable with a value determined while executing the
    /// query
    Regular,
    /// @brief a variable with a constant value
    Const,
    /// @brief variable is a replacement for a bind parameter
    BindParameter,
  };

  /// @brief create the variable
  Variable(std::string name, VariableId id, bool isFullDocumentFromCollection,
           arangodb::ResourceMonitor& resourceMonitor);

  explicit Variable(velocypack::Slice slice,
                    arangodb::ResourceMonitor& resourceMonitor);

  /// @brief destroy the variable
  ~Variable();

  Variable* clone() const;

  /// @brief whether or not the variable is user-defined
  bool isUserDefined() const noexcept;

  /// @brief whether or not the variable needs a register assigned
  bool needsRegister() const noexcept;

  /// @brief return a VelocyPack representation of the variable, not including
  /// the variable's constant value (if set)
  void toVelocyPack(velocypack::Builder& builder) const;

  /// @brief return a VelocyPack representation of the variable, including
  /// the variable's constant value (if set)
  void toVelocyPack(velocypack::Builder& builder, WithConstantValue) const;

  /// @brief replace a variable by another
  static Variable const* replace(
      Variable const*, std::unordered_map<VariableId, Variable const*> const&);

  /// @brief factory for (optional) variables from VPack
  static Variable* varFromVPack(Ast* ast, velocypack::Slice base,
                                std::string_view variableName,
                                bool optional = false);

  bool isEqualTo(Variable const& other) const noexcept;

  /// @brief returns the type of the variable. The type is determined based
  // on the constantValue. If constantValue.isNone, the type is Type::Regular,
  // otherwise it is Type::Const
  Type type() const noexcept;

  /// @brief returns the constant value of the variable.
  AqlValue constantValue() const noexcept { return _constantValue; }

  /// @brief set the constant value of the variable.
  /// This implicitly changes the type -> see type()
  void setConstantValue(AqlValue value);

  void setBindParameterReplacement(std::string name);

  std::string_view bindParameterName() const noexcept;

  /// @brief variable id
  VariableId const id;

  /// @brief variable name
  /// note: this cannot be const as variables can be renamed by the optimizer
  std::string name;

  /// @brief whether or not the source data for this variable is from a
  /// collection AND is a full document. this is only used for optimizations
  bool isFullDocumentFromCollection;

 private:
  arangodb::ResourceMonitor& _resourceMonitor;

  /// @brief serialize common parts
  void toVelocyPackCommon(velocypack::Builder& builder) const;

  // for const variables, this stores the constant value determined
  // while initializing the plan. Note: the variable takes ownership of this
  // value and destroys it
  AqlValue _constantValue;

  std::string _bindParameterName;
};
}  // namespace aql
}  // namespace arangodb
