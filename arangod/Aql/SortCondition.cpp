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

#include "SortCondition.h"
#include "Aql/AstNode.h"
#include "Logger/Logger.h"

using namespace arangodb::aql;

namespace {

/// @brief whether or not an attribute is contained in a vector
static bool isContained(std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
                        std::vector<arangodb::basics::AttributeName> const& attribute) {
  for (auto const& it : attributes) {
    if (arangodb::basics::AttributeName::isIdentical(it, attribute, false)) {
      return true;
    }
  }

  return false;
}

}

/// @brief create an empty condition
SortCondition::SortCondition()
    : _fields(),
      _constAttributes(),
      _unidirectional(false),
      _onlyAttributeAccess(false),
      _ascending(true) {}

/// @brief create the sort condition
SortCondition::SortCondition(
    std::vector<std::pair<VariableId, bool>> const& sorts,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& constAttributes,
    std::unordered_map<VariableId, AstNode const*> const& variableDefinitions)
    : _fields(),
      _constAttributes(constAttributes),
      _unidirectional(true),
      _onlyAttributeAccess(true),
      _ascending(true) {

  bool foundDirection = false;

  std::vector<arangodb::basics::AttributeName> fieldNames;
  size_t const n = sorts.size();

  for (size_t i = 0; i < n; ++i) {
    bool isConst = false; // const attribute?
    bool handled = false;
    auto variableId = sorts[i].first;

    auto it = variableDefinitions.find(variableId);

    if (it != variableDefinitions.end()) {
      auto node = (*it).second;

      if (node != nullptr && node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        fieldNames.clear();
        
        while (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          fieldNames.emplace_back(
              arangodb::basics::AttributeName(node->getString(), false));
          node = node->getMember(0);
        }

        if (node->type == NODE_TYPE_REFERENCE) {
          handled = true;

          if (fieldNames.size() > 1) {
            std::reverse(fieldNames.begin(), fieldNames.end());
          }

          _fields.emplace_back(std::make_pair(
              static_cast<Variable const*>(node->getData()), fieldNames));

          for (auto const& it2 : constAttributes) {
            if (it2 == fieldNames) {
              // const attribute
              isConst = true;
              break;
            }
          } 
        }
      }
    }

    if (!isConst) {
      // const attributes can be ignored for sorting
      if (!foundDirection) {
        // first attribute that we found
        foundDirection = true;
        _ascending = sorts[i].second;
      }
      else if (_unidirectional && sorts[i].second != _ascending) {
        _unidirectional = false;
      }
    }

    if (!handled) {
      _fields.emplace_back(
          std::pair<Variable const*,
                    std::vector<arangodb::basics::AttributeName>>());
      _onlyAttributeAccess = false;
    }
  }

  if (n == 0) {
    _onlyAttributeAccess = false;
  }
}

/// @brief destroy the sort condition
SortCondition::~SortCondition() {}

/// @brief returns the number of attributes in the sort condition covered
/// by the specified index fields
size_t SortCondition::coveredAttributes(
    Variable const* reference,
    std::vector<std::vector<arangodb::basics::AttributeName>> const&
        indexAttributes) const {
  size_t numCovered = 0;
  size_t fieldsPosition = 0;
  
  // iterate over all fields of the index definition
  size_t const n = indexAttributes.size();

  for (size_t i = 0; i < n; /* no hoisting */) {
    if (fieldsPosition >= _fields.size()) { 
      // done
      break;
    }
    
    auto const& field = _fields[fieldsPosition];
      
    // ...and check if the field is present in the index definition too
    if (reference == field.first &&
        arangodb::basics::AttributeName::isIdentical(field.second, indexAttributes[i], false)) {
      // field match
      ++fieldsPosition;
      ++numCovered;
      ++i; // next index field
      continue;
    }

    // no match
    bool isConstant = false;

    if (isContained(indexAttributes, field.second) &&
        isContained(_constAttributes, field.second)) {
      // no field match, but a constant attribute
      isConstant = true;
      ++fieldsPosition;
      ++numCovered;
    }
    
    if (!isConstant &&
        isContained(_constAttributes, indexAttributes[i])) {
      // no field match, but a constant attribute
      isConstant = true;
      ++i; // next index field
    }

          
    if (!isConstant) {
      break;
    }
  }

  TRI_ASSERT(numCovered <= _fields.size());
  return numCovered;
}
