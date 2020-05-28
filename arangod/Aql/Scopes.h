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

#ifndef ARANGOD_AQL_SCOPES_H
#define ARANGOD_AQL_SCOPES_H 1

#include <memory>
#include <vector>

#include "Aql/Variable.h"
#include "Basics/Common.h"
#include "Basics/debugging.h"

namespace arangodb {
namespace aql {

enum ScopeType {
  AQL_SCOPE_MAIN,
  AQL_SCOPE_SUBQUERY,
  AQL_SCOPE_FOR,
  AQL_SCOPE_COLLECT
};

class Scope {
 public:
  /// @brief create a scope
  explicit Scope(ScopeType);

  /// @brief destroy the scope
  ~Scope();

 public:
  /// @brief return the name of a scope type
  std::string typeName() const;

  /// @brief return the name of a scope type
  static std::string typeName(ScopeType);

  /// @brief return the scope type
  inline ScopeType type() const { return _type; }

  /// @brief adds a variable to the scope
  void addVariable(Variable*);

  /// @brief checks if a variable exists in the scope
  bool existsVariable(char const*, size_t) const;

  /// @brief checks if a variable exists in the scope
  bool existsVariable(std::string const&) const;

  /// @brief return a variable
  Variable const* getVariable(char const*, size_t) const;

  /// @brief return a variable
  Variable const* getVariable(std::string const&) const;

  /// @brief return a variable, allowing usage of special pseudo vars such
  /// as OLD and NEW
  Variable const* getVariable(char const*, size_t, bool) const;

 private:
  /// @brief scope type
  ScopeType const _type;

  /// @brief variables introduced by the scope
  std::unordered_map<std::string, Variable*> _variables;
};

/// @brief scope management
class Scopes {
 public:
  /// @brief create the scopes
  Scopes();

  /// @brief destroy the scopes
  ~Scopes();

 public:
  /// @brief number of currently active scopes
  inline size_t numActive() const { return _activeScopes.size(); }

  /// @brief return the type of the currently active scope
  ScopeType type() const {
    TRI_ASSERT(numActive() > 0);
    return _activeScopes.back()->type();
  }

  /// @brief whether or not the $CURRENT variable can be used at the caller's
  /// current position
  inline bool canUseCurrentVariable() const {
    return (!_currentVariables.empty());
  }

  /// @brief start a new scope
  void start(ScopeType);

  /// @brief end the current scope
  void endCurrent();

  /// @brief end the current scope plus any FOR scopes it is nested in
  void endNested();

  /// @brief adds a variable to the current scope
  void addVariable(Variable*);

  /// @brief replaces an existing variable in the current scope
  void replaceVariable(Variable*);

  /// @brief checks whether a variable exists in any scope
  bool existsVariable(char const*, size_t) const;

  /// @brief return a variable by name - this respects the current scopes
  Variable const* getVariable(char const*, size_t) const;

  /// @brief return a variable by name - this respects the current scopes
  Variable const* getVariable(std::string const&) const;

  /// @brief return a variable by name - this respects the current scopes
  /// this also allows using special pseudo vars such as OLD and NEW
  Variable const* getVariable(char const*, size_t, bool) const;

  /// @brief get the $CURRENT variable
  Variable const* getCurrentVariable() const;

  /// @brief stack a $CURRENT variable from the stack
  void stackCurrentVariable(Variable const*);

  /// @brief unregister the $CURRENT variable from the stack
  void unstackCurrentVariable();

 private:
  /// @brief currently active scopes
  std::vector<std::unique_ptr<Scope>> _activeScopes;

  /// @brief a stack with aliases for the $CURRENT variable
  std::vector<Variable const*> _currentVariables;
};
}  // namespace aql
}  // namespace arangodb

#endif
