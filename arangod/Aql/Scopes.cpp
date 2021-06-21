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

#include "Aql/Scopes.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"

using namespace arangodb::aql;

/// @brief create the scope
Scope::Scope(ScopeType type) : _type(type), _variables() {}

/// @brief destroy the scope
Scope::~Scope() = default;

/// @brief return the name of a scope type
std::string Scope::typeName() const { return typeName(_type); }

/// @brief return the name of a scope type
std::string Scope::typeName(ScopeType type) {
  switch (type) {
    case AQL_SCOPE_MAIN:
      return "main";
    case AQL_SCOPE_SUBQUERY:
      return "subquery";
    case AQL_SCOPE_FOR:
      return "for";
    case AQL_SCOPE_COLLECT:
      return "collection";
  }
  TRI_ASSERT(false);
  return "unknown";
}

/// @brief adds a variable to the scope
void Scope::addVariable(Variable* variable) {
  // intentionally like this... must always overwrite the value
  // if the key already exists
  _variables[variable->name] = variable;
}

/// @brief checks if a variable exists in the scope
bool Scope::existsVariable(char const* name, size_t nameLength) const {
  return (getVariable(name, nameLength) != nullptr);
}

/// @brief checks if a variable exists in the scope
bool Scope::existsVariable(std::string const& name) const {
  return (getVariable(name) != nullptr);
}

/// @brief returns a variable
Variable const* Scope::getVariable(char const* name, size_t nameLength) const {
  std::string const varname(name, nameLength);

  auto it = _variables.find(varname);

  if (it == _variables.end()) {
    return nullptr;
  }

  return (*it).second;
}

/// @brief returns a variable
Variable const* Scope::getVariable(std::string const& name) const {
  auto it = _variables.find(name);

  if (it == _variables.end()) {
    return nullptr;
  }

  return (*it).second;
}

/// @brief return a variable, allowing usage of special pseudo vars such
/// as OLD and NEW
Variable const* Scope::getVariable(char const* name, size_t nameLength, bool allowSpecial) const {
  auto variable = getVariable(name, nameLength);

  if (variable == nullptr && allowSpecial) {
    // variable does not exist
    // now try variable aliases OLD (= $OLD) and NEW (= $NEW)
    if (strcmp(name, "OLD") == 0) {
      variable = getVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_OLD));
    } else if (strcmp(name, "NEW") == 0) {
      variable = getVariable(TRI_CHAR_LENGTH_PAIR(Variable::NAME_NEW));
    }
  }

  return variable;
}

/// @brief create the scopes
Scopes::Scopes() : _activeScopes(), _currentVariables() {
  _activeScopes.reserve(4);
}

/// @brief destroy the scopes
Scopes::~Scopes() = default;

/// @brief start a new scope
void Scopes::start(ScopeType type) {
  auto scope = std::make_unique<Scope>(type);

  _activeScopes.emplace_back(std::move(scope));
}

/// @brief end the current scope
void Scopes::endCurrent() {
  TRI_ASSERT(!_activeScopes.empty());
  _activeScopes.pop_back();
}

/// @brief end the current scope plus any FOR scopes it is nested in
void Scopes::endNested() {
  TRI_ASSERT(!_activeScopes.empty());

  while (!_activeScopes.empty()) {
    auto const& scope = _activeScopes.back();
    ScopeType type = scope->type();

    if (type == AQL_SCOPE_MAIN || type == AQL_SCOPE_SUBQUERY) {
      // the main scope and subquery scopes cannot be closed here
      break;
    }

    TRI_ASSERT(type == AQL_SCOPE_FOR || type == AQL_SCOPE_COLLECT);
    endCurrent();
  }
}

/// @brief adds a variable to the current scope
void Scopes::addVariable(Variable* variable) {
  TRI_ASSERT(!_activeScopes.empty());
  TRI_ASSERT(variable != nullptr);

  for (auto it = _activeScopes.rbegin(); it != _activeScopes.rend(); ++it) {
    auto const& scope = (*it);

    if (scope->existsVariable(variable->name)) {
      // duplicate variable name
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_VARIABLE_REDECLARED,
                                    variable->name.c_str());
    }
  }

  // if this fails, there won't be a memleak
  _activeScopes.back()->addVariable(variable);
}

/// @brief replaces an existing variable in the current scope
void Scopes::replaceVariable(Variable* variable) {
  TRI_ASSERT(!_activeScopes.empty());
  TRI_ASSERT(variable != nullptr);

  _activeScopes.back()->addVariable(variable);
}

/// @brief checks whether a variable exists in any scope
bool Scopes::existsVariable(char const* name, size_t nameLength) const {
  return (getVariable(name, nameLength) != nullptr);
}

/// @brief return a variable by name - this respects the current scopes
Variable const* Scopes::getVariable(char const* name, size_t nameLength) const {
  TRI_ASSERT(!_activeScopes.empty());

  for (auto it = _activeScopes.rbegin(); it != _activeScopes.rend(); ++it) {
    auto variable = (*it)->getVariable(name, nameLength);

    if (variable != nullptr) {
      return variable;
    }
  }

  return nullptr;
}

/// @brief return a variable by name - this respects the current scopes
Variable const* Scopes::getVariable(std::string const& name) const {
  TRI_ASSERT(!_activeScopes.empty());

  for (auto it = _activeScopes.rbegin(); it != _activeScopes.rend(); ++it) {
    auto variable = (*it)->getVariable(name);

    if (variable != nullptr) {
      return variable;
    }
  }

  return nullptr;
}

/// @brief return a variable by name - this respects the current scopes
Variable const* Scopes::getVariable(char const* name, size_t nameLength,
                                    bool allowSpecial) const {
  TRI_ASSERT(!_activeScopes.empty());

  for (auto it = _activeScopes.rbegin(); it != _activeScopes.rend(); ++it) {
    auto variable = (*it)->getVariable(name, nameLength, allowSpecial);

    if (variable != nullptr) {
      return variable;
    }
  }

  return nullptr;
}

/// @brief get the $CURRENT variable
Variable const* Scopes::getCurrentVariable() const {
  if (_currentVariables.empty()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN, Variable::NAME_CURRENT);
  }
  auto result = _currentVariables.back();
  TRI_ASSERT(result != nullptr);
  return result;
}

/// @brief stack a $CURRENT variable from the stack
void Scopes::stackCurrentVariable(Variable const* variable) {
  _currentVariables.emplace_back(variable);
}

/// @brief unregister the $CURRENT variable from the stack
void Scopes::unstackCurrentVariable() {
  TRI_ASSERT(!_currentVariables.empty());

  _currentVariables.pop_back();
}
