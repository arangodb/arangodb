////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, variable generator
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

#include "Aql/VariableGenerator.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the generator
////////////////////////////////////////////////////////////////////////////////

VariableGenerator::VariableGenerator () 
  : _variables(),
    _id(0) {
  _variables.reserve(8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the generator
////////////////////////////////////////////////////////////////////////////////

VariableGenerator::~VariableGenerator () {
  // free all variables
  for (auto it = _variables.begin(); it != _variables.end(); ++it) {
    delete (*it).second;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return a map of all variable ids with their names
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<VariableId, std::string const> VariableGenerator::variables (bool includeTemporaries) const {
  std::unordered_map<VariableId, std::string const> result;

  for (auto it = _variables.begin(); it != _variables.end(); ++it) {
    // check if we should include this variable...
    if (! includeTemporaries && ! (*it).second->isUserDefined()) {
      continue;
    }

    result.emplace(std::make_pair((*it).first, (*it).second->name));
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a variable
////////////////////////////////////////////////////////////////////////////////

Variable* VariableGenerator::createVariable (char const* name,
                                             bool isUserDefined) {
  TRI_ASSERT(name != nullptr);

  auto variable = new Variable(std::string(name), nextId());
  
  if (isUserDefined) {
    TRI_ASSERT(variable->isUserDefined());
  }

  try {
    _variables.emplace(std::make_pair(variable->id, variable));
  }
  catch (...) {
    // prevent memleak
    delete variable;
    throw;
  }

  return variable;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a variable
////////////////////////////////////////////////////////////////////////////////

Variable* VariableGenerator::createVariable (std::string const& name,
                                             bool isUserDefined) {
  auto variable = new Variable(name, nextId());

  if (isUserDefined) {
    TRI_ASSERT(variable->isUserDefined());
  }

  try {
    _variables.emplace(std::make_pair(variable->id, variable));
  }
  catch (...) {
    // prevent memleak
    delete variable;
    throw;
  }

  return variable;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a variable from JSON
////////////////////////////////////////////////////////////////////////////////

Variable* VariableGenerator::createVariable (triagens::basics::Json const& json) {
  auto variable = new Variable(json);

  auto existing = getVariable(variable->id);
  if (existing != nullptr) {
    // variable already existed. 
    delete variable;
    return existing;
  }
  
  try {
    _variables.emplace(std::make_pair(variable->id, variable));
  }
  catch (...) {
    // prevent memleak
    delete variable;
    throw;
  }

  return variable;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a temporary variable
////////////////////////////////////////////////////////////////////////////////
  
Variable* VariableGenerator::createTemporaryVariable () {
  return createVariable(nextName(), false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a variable by id - this does not respect the scopes!
////////////////////////////////////////////////////////////////////////////////

Variable* VariableGenerator::getVariable (VariableId id) const {
  auto it = _variables.find(id);

  if (it == _variables.end()) {
    return nullptr;
  }

  return (*it).second;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next temporary variable name
////////////////////////////////////////////////////////////////////////////////
  
std::string VariableGenerator::nextName () const {
  // note: if the naming scheme is adjusted, it may be necessary to adjust
  // Variable::isUserDefined, too!
  return std::to_string(_id); // to_string: c++11
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
