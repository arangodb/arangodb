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

#ifndef ARANGOD_AQL_VARIABLE_GENERATOR_H
#define ARANGOD_AQL_VARIABLE_GENERATOR_H 1

#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Basics/Common.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}

namespace aql {

class VariableGenerator {
 public:
  /// @brief create the generator
  VariableGenerator();

  VariableGenerator(VariableGenerator const& other) = delete;
  VariableGenerator& operator=(VariableGenerator const& other) = delete;

  /// @brief destroy the generator
  ~VariableGenerator() = default;

 public:
  /// @brief visit all variables
  void visit(std::function<void(Variable*)> const&); 

  /// @brief return a map of all variable ids with their names
  std::unordered_map<VariableId, std::string const> variables(bool includeTemporaries) const;

  /// @brief generate a variable
  Variable* createVariable(std::string name, bool isUserDefined);

  /// @brief generate a variable from VelocyPack
  Variable* createVariable(arangodb::velocypack::Slice);

  /// @brief clones a variable from an existing one
  Variable* createVariable(Variable const*);

  /// @brief generate a temporary variable
  Variable* createTemporaryVariable();

  /// @brief renames a variable (assigns a temporary name)
  Variable* renameVariable(VariableId);

  /// @brief renames a variable (assigns the specified name)
  Variable* renameVariable(VariableId, std::string const&);

  /// @brief return a variable by id - this does not respect the scopes!
  Variable* getVariable(VariableId) const;

  /// @brief return the next temporary variable name
  std::string nextName();

  /// @brief export to VelocyPack
  void toVelocyPack(arangodb::velocypack::Builder& builder) const;

  /// @brief import from VelocyPack
  void fromVelocyPack(arangodb::velocypack::Slice const allVariablesList);

 private:
  /// @brief returns the next variable id
  VariableId nextId() noexcept;

 private:
  /// @brief all variables created
  std::unordered_map<VariableId, std::unique_ptr<Variable>> _variables;

  /// @brief the next assigned variable id
  VariableId _id;
};
}  // namespace aql
}  // namespace arangodb

#endif
