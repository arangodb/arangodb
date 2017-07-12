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

#ifndef ARANGOD_AQL_VARIABLE_H
#define ARANGOD_AQL_VARIABLE_H 1

#include "Basics/Common.h"
#include "Aql/types.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}

namespace aql {
class Ast;

struct Variable {
  /// @brief create the variable
  Variable(std::string const&, VariableId);

  explicit Variable(arangodb::velocypack::Slice const&);

  Variable* clone() const { return new Variable(name, id); }

  /// @brief destroy the variable
  ~Variable();

  /// @brief registers a constant value for the variable
  /// this constant value is used for constant propagation in optimizations
  void constValue(void* node) { value = node; }

  /// @brief returns a constant value registered for this variable
  inline void* constValue() const { return value; }

  /// @brief whether or not the variable is user-defined
  inline bool isUserDefined() const {
    char const c = name[0];
    // variables starting with a number are not user-defined
    return (c < '0' || c > '9');
  }

  /// @brief whether or not the variable needs a register assigned
  inline bool needsRegister() const {
    // variables starting with a number are not user-defined
    return isUserDefined() || name.back() != '_';
  }

  /// @brief return a VelocyPack representation of the variable
  void toVelocyPack(arangodb::velocypack::Builder&) const;

  /// @brief replace a variable by another
  static Variable const* replace(
      Variable const*, std::unordered_map<VariableId, Variable const*> const&);

  /// @brief factory for (optional) variables from VPack
  static Variable* varFromVPack(Ast* ast, arangodb::velocypack::Slice const& base,
                                char const* variableName, bool optional = false);

  /// @brief variable name
  std::string name;

  /// @brief constant variable value (points to another AstNode)
  void* value;

  /// @brief variable id
  VariableId const id;

  /// @brief name of $OLD variable
  static char const* const NAME_OLD;

  /// @brief name of $NEW variable
  static char const* const NAME_NEW;

  /// @brief name of $CURRENT variable
  static char const* const NAME_CURRENT;
};
}
}

#endif
