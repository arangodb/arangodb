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
#include "Utils/Exception.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the scope
////////////////////////////////////////////////////////////////////////////////

Scope::Scope (ScopeType type) 
  : _type(type),
    _variables() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the scope
////////////////////////////////////////////////////////////////////////////////

Scope::~Scope () {
  for (auto it = _variables.begin(); it != _variables.end(); ++it) {
    delete (*it).second;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of a scope type
////////////////////////////////////////////////////////////////////////////////

std::string Scope::typeName () const {
  switch (_type) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a variable to the scope
////////////////////////////////////////////////////////////////////////////////

Variable* Scope::addVariable (std::string const& name,
                              size_t id,
                              bool isUserDefined) {
  auto variable = new Variable(name, id, isUserDefined);

  try {
    _variables.insert(std::make_pair(name, variable));
  }
  catch (...) {
    // prevent memleak
    delete variable;
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }  
  
  return variable; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a variable exists in the scope
////////////////////////////////////////////////////////////////////////////////

bool Scope::existsVariable (char const* name) const {
  return (getVariable(name) != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a variable
////////////////////////////////////////////////////////////////////////////////

Variable* Scope::getVariable (char const* name) const {
  std::string const varname(name);

  auto it = _variables.find(varname);

  if (it == _variables.end()) {
    return nullptr;
  }

  return (*it).second;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the scopes
////////////////////////////////////////////////////////////////////////////////

Scopes::Scopes () 
  : _activeScopes(),
    _allScopes(),
    _variableId(0) {

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
  auto scope = new Scope(type);
  _allScopes.push_back(scope);
  _activeScopes.push_back(scope);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end the current scope
////////////////////////////////////////////////////////////////////////////////

Scope* Scopes::endCurrent () {
  TRI_ASSERT(! _activeScopes.empty());

  Scope* result = _activeScopes.back();
  TRI_ASSERT(result != nullptr);

  _activeScopes.pop_back();

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end the current scope plus any FOR scopes it is nested in
////////////////////////////////////////////////////////////////////////////////

void Scopes::endNested () {
  TRI_ASSERT(! _activeScopes.empty());
  int iterations = 0;

  while (! _activeScopes.empty()) {
    auto scope = _activeScopes.back();
    TRI_ASSERT(scope != nullptr);
    ScopeType type = scope->type();

    if (type == AQL_SCOPE_MAIN) {
      // main scope cannot be closed here
      return;
    }
    
    if (type != AQL_SCOPE_FOR && ++iterations < 2) {
      // do not close anything but for scopes
      return;
    }

    endCurrent();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a variable to the current scope
////////////////////////////////////////////////////////////////////////////////

Variable* Scopes::addVariable (char const* name,
                               bool isUserDefined) {
  TRI_ASSERT(! _activeScopes.empty());
  TRI_ASSERT(name != nullptr);

  for (auto it = _activeScopes.rbegin(); it != _activeScopes.rend(); ++it) {
    auto scope = (*it);

    if (scope->existsVariable(name)) {
      // duplicate variable name
      return 0;
    }
  }

  size_t id = ++_variableId;

  return _activeScopes.back()->addVariable(std::string(name), id, isUserDefined);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a variable exists in any scope
////////////////////////////////////////////////////////////////////////////////

bool Scopes::existsVariable (char const* name) const {
  return (getVariable(name) != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a variable
////////////////////////////////////////////////////////////////////////////////
        
Variable* Scopes::getVariable (char const* name) const {
  TRI_ASSERT(! _activeScopes.empty());

  for (auto it = _activeScopes.rbegin(); it != _activeScopes.rend(); ++it) {
    auto variable = (*it)->getVariable(name);

    if (variable != nullptr) {
      return variable;
    }
  }

  return nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
