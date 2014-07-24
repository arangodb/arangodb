////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, scopes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Scopes.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the scope
////////////////////////////////////////////////////////////////////////////////

Scope::Scope (ScopeType type,
              size_t level) 
  : _type(type),
    _level(level) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the scope
////////////////////////////////////////////////////////////////////////////////

Scope::~Scope () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a variable to the scope
////////////////////////////////////////////////////////////////////////////////

void Scope::addVariable (char const* name) {
  _variables.insert(std::make_pair(std::string(name), std::string(name)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end a scope
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a variable exists in the scope
////////////////////////////////////////////////////////////////////////////////

bool Scope::existsVariable (char const* name) const {
  auto it = _variables.find(std::string(name));

  return (it != _variables.end());
}

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the scopes
////////////////////////////////////////////////////////////////////////////////

Scopes::Scopes () 
  : _level(0),
    _activeScopes(),
    _allScopes() {

  _allScopes.reserve(8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the scopes
////////////////////////////////////////////////////////////////////////////////

Scopes::~Scopes () {
  for (auto it = _allScopes.begin(); it != _allScopes.end(); ++it) {
    delete (*it);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief start a new scope
////////////////////////////////////////////////////////////////////////////////

void Scopes::start (ScopeType type) {
  if (type == AQL_SCOPE_FOR && ! _activeScopes.empty()) {
    // check if a FOR scope must be converted in a FOR_NESTED scope
    auto last = _activeScopes.back();

    if (last->type() == AQL_SCOPE_FOR ||
        last->type() == AQL_SCOPE_FOR_NESTED) {
      type = AQL_SCOPE_FOR_NESTED;
    }
  }

  auto scope = new Scope(type, _level);
  _allScopes.push_back(scope);

  _activeScopes.push_back(scope);
  ++_level;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end the current scope
////////////////////////////////////////////////////////////////////////////////

Scope* Scopes::endCurrent () {
  TRI_ASSERT(! _activeScopes.empty());

  Scope* result = _activeScopes.back();
  _activeScopes.pop_back();
  --_level;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end the current scope plus any FOR scopes it is nested in
////////////////////////////////////////////////////////////////////////////////

void Scopes::endNested () {
  TRI_ASSERT(! _activeScopes.empty());

  auto scope = _activeScopes.back();
  if (scope->type() == AQL_SCOPE_MAIN ||
      scope->type() == AQL_SCOPE_SUBQUERY) {
    // nothing to do
    return;
  }

  while (! _activeScopes.empty()) {
    scope = endCurrent();

    if (scope->type() != AQL_SCOPE_FOR_NESTED) {
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a variable to the current scope
////////////////////////////////////////////////////////////////////////////////

bool Scopes::addVariable (char const* name) {
  TRI_ASSERT(! _activeScopes.empty());

  for (auto it = _activeScopes.rend(); it != _activeScopes.rbegin(); ++it) {
    auto scope = (*it);

    if (scope->existsVariable(name)) {
      // duplicate variable name
      return false;
    }
  }

  _activeScopes.back()->addVariable(name);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a variable exists in any scope
////////////////////////////////////////////////////////////////////////////////

bool Scopes::existsVariable (char const* name) const {
  TRI_ASSERT(! _activeScopes.empty());

  for (auto it = _activeScopes.rend(); it != _activeScopes.rbegin(); ++it) {
    auto scope = (*it);

    if (scope->existsVariable(name)) {
      return true;
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
