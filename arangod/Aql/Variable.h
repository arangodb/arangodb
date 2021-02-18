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

#ifndef ARANGOD_AQL_VARIABLE_H
#define ARANGOD_AQL_VARIABLE_H 1

#include <string>

#include "Aql/AqlValue.h"
#include "Aql/types.h"
#include "Basics/Common.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
class Ast;
struct AstNode;

struct Variable {
  /// @brief indicates the type of the variable
  enum class Type {
    /// @brief a regular variable with a value determined while executing the query
    Regular,
    /// @brief a variable with a constant value
    Const
  };

  /// @brief create the variable
  Variable(std::string name, VariableId id, bool isDataFromCollection);

  explicit Variable(arangodb::velocypack::Slice const&);

  /// @brief destroy the variable
  ~Variable();
  
  Variable* clone() const;

  /// @brief registers a constant value for the variable
  /// this constant value is used for constant propagation while creating the AST
  void setConstAstNode(AstNode* node) { _constAstNode = node; }

  /// @brief returns a constant value registered for this variable
  AstNode* getConstAstNode() const { return _constAstNode; }

  /// @brief whether or not the variable is user-defined
  bool isUserDefined() const;

  /// @brief whether or not the variable needs a register assigned
  bool needsRegister() const;

  /// @brief return a VelocyPack representation of the variable
  void toVelocyPack(arangodb::velocypack::Builder&) const;

  /// @brief replace a variable by another
  static Variable const* replace(Variable const*,
                                 std::unordered_map<VariableId, Variable const*> const&);

  /// @brief factory for (optional) variables from VPack
  static Variable* varFromVPack(Ast* ast, arangodb::velocypack::Slice const& base,
                                char const* variableName, bool optional = false);


  bool isEqualTo(Variable const& other) const;
  
  /// @brief returns the type of the variable. The type is determined based
  // on the constantValue. If constantValue.isNone, the type is Type::Regular,
  // otherwise it is Type::Const
  Type type() const noexcept;

  /// @brief returns the constant value of the variable.
  AqlValue constantValue() const noexcept { return _constantValue; }

  /// @brief set the constant value of the variable.
  /// This implicitly changes the type -> see type()
  void setConstantValue(AqlValue value) noexcept;

  /// @brief variable id
  VariableId const id;

  /// @brief variable name
  /// note: this cannot be const as variables can be renamed by the optimizer
  std::string name;

  /// @brief whether or not the source data for this variable is from a collection 
  /// (i.e. is a document). this is only used for optimizations
  bool isDataFromCollection;
 
  /// @brief name of $OLD variable
  static char const* const NAME_OLD;

  /// @brief name of $NEW variable
  static char const* const NAME_NEW;

  /// @brief name of $CURRENT variable
  static char const* const NAME_CURRENT;

private:
  /// @brief constant variable value (points to another AstNode)
  /// Used for constant propagation while creating the AST.
  AstNode* _constAstNode{nullptr};
  
  // TODO - we have two kinds of const values here; this should be cleaned up!
  /// @brief for const variables, this stores the constant value determined while
  /// initializing the plan.
  /// Note: the variable takes ownership of this value and destroys it
  AqlValue _constantValue;
};
}  // namespace aql
}  // namespace arangodb

#endif
