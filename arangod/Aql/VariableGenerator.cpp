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

#include "Aql/VariableGenerator.h"
#include "Basics/Exceptions.h"

using namespace arangodb::aql;
using Json = arangodb::basics::Json;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the generator
////////////////////////////////////////////////////////////////////////////////

VariableGenerator::VariableGenerator() : _variables(), _id(0) {
  _variables.reserve(8);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the generator
////////////////////////////////////////////////////////////////////////////////

VariableGenerator::~VariableGenerator() {
  // free all variables
  for (auto& it : _variables) {
    delete it.second;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a map of all variable ids with their names
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<VariableId, std::string const> VariableGenerator::variables(
    bool includeTemporaries) const {
  std::unordered_map<VariableId, std::string const> result;

  for (auto const& it : _variables) {
    // check if we should include this variable...
    if (!includeTemporaries && !it.second->isUserDefined()) {
      continue;
    }

    result.emplace(it.first, it.second->name);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a variable
////////////////////////////////////////////////////////////////////////////////

Variable* VariableGenerator::createVariable(char const* name, size_t length,
                                            bool isUserDefined) {
  TRI_ASSERT(name != nullptr);

  auto variable = new Variable(std::string(name, length), nextId());

  if (isUserDefined) {
    TRI_ASSERT(variable->isUserDefined());
  }

  try {
    _variables.emplace(variable->id, variable);
  } catch (...) {
    // prevent memleak
    delete variable;
    throw;
  }

  return variable;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a variable
////////////////////////////////////////////////////////////////////////////////

Variable* VariableGenerator::createVariable(std::string const& name,
                                            bool isUserDefined) {
  auto variable = new Variable(name, nextId());

  if (isUserDefined) {
    TRI_ASSERT(variable->isUserDefined());
  }

  try {
    _variables.emplace(variable->id, variable);
  } catch (...) {
    // prevent memleak
    delete variable;
    throw;
  }

  return variable;
}

Variable* VariableGenerator::createVariable(Variable const* original) {
  TRI_ASSERT(original != nullptr);
  auto variable = original->clone();

  try {
    _variables.emplace(variable->id, variable);
  } catch (...) {
    // prevent memleak
    delete variable;
    throw;
  }

  return variable;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a variable from JSON
////////////////////////////////////////////////////////////////////////////////

Variable* VariableGenerator::createVariable(
    arangodb::basics::Json const& json) {
  auto variable = new Variable(json);

  auto existing = getVariable(variable->id);
  if (existing != nullptr) {
    // variable already existed.
    delete variable;
    return existing;
  }

  try {
    _variables.emplace(variable->id, variable);
  } catch (...) {
    // prevent memleak
    delete variable;
    throw;
  }

  return variable;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a temporary variable
////////////////////////////////////////////////////////////////////////////////

Variable* VariableGenerator::createTemporaryVariable() {
  return createVariable(nextName(), false);
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief renames a variable (assigns a temporary name)
////////////////////////////////////////////////////////////////////////////////

Variable* VariableGenerator::renameVariable(VariableId id) {
  return renameVariable(id, nextName());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a variable (assigns the specified name
////////////////////////////////////////////////////////////////////////////////

Variable* VariableGenerator::renameVariable(VariableId id, std::string const& name) {
  auto it = _variables.find(id);

  if (it == _variables.end()) {
    return nullptr;
  }

  (*it).second->name = name;

  return (*it).second;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a variable by id - this does not respect the scopes!
////////////////////////////////////////////////////////////////////////////////

Variable* VariableGenerator::getVariable(VariableId id) const {
  auto it = _variables.find(id);

  if (it == _variables.end()) {
    return nullptr;
  }

  return (*it).second;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next temporary variable name
////////////////////////////////////////////////////////////////////////////////

std::string VariableGenerator::nextName() {
  // note: if the naming scheme is adjusted, it may be necessary to adjust
  // Variable::isUserDefined, too!
  return std::to_string(nextId());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON, returns an AUTOFREE Json object
////////////////////////////////////////////////////////////////////////////////

arangodb::basics::Json VariableGenerator::toJson(
    TRI_memory_zone_t* zone) const {
  Json jsonAllVariablesList(Json::Array, _variables.size());

  for (auto const& oneVariable : _variables) {
    jsonAllVariablesList(oneVariable.second->toJson());
  }

  return jsonAllVariablesList;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief import from JSON
////////////////////////////////////////////////////////////////////////////////

void VariableGenerator::fromJson(Json const& query) {
  Json jsonAllVariablesList = query.get("variables");

  if (!jsonAllVariablesList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "variables needs to be an array");
  }

  auto len = jsonAllVariablesList.size();
  _variables.reserve(len);

  for (size_t i = 0; i < len; i++) {
    createVariable(jsonAllVariablesList.at(i));
  }
}
