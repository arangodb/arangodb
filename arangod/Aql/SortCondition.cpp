////////////////////////////////////////////////////////////////////////////////
/// @brief index sort condition
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/SortCondition.h"
#include "Aql/AstNode.h"

using namespace triagens::aql;
 
// -----------------------------------------------------------------------------
// --SECTION--                                               class SortCondition
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

SortCondition::SortCondition (std::vector<std::pair<AstNode const*, bool>> const& expressions) 
  : _expressions(expressions),
    _unidirectional(true),
    _onlyAttributeAccess(true) {

  size_t const n = _expressions.size();

  for (size_t i = 0; i < n; ++i) {
    if (_unidirectional && i > 0 && _expressions[i].second != _expressions[i - 1].second) {
      _unidirectional = false;
    }

    bool handled = false;
    auto node = _expressions[i].first;

    if (node != nullptr && 
        node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
      std::vector<triagens::basics::AttributeName> fieldNames;
      while (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        fieldNames.emplace_back(triagens::basics::AttributeName(node->getStringValue()));
        node = node->getMember(0);
      }
      if (node->type == NODE_TYPE_REFERENCE) {
        handled = true;

        _fields.emplace_back(std::make_pair(static_cast<Variable const*>(node->getData()), fieldNames));
      }
    }
    
    if (! handled) {
      _fields.emplace_back(std::pair<Variable const*, std::vector<triagens::basics::AttributeName>>());
      _onlyAttributeAccess = false;
    }
  }
  
  if (n == 0) {
    _onlyAttributeAccess = false;
  }
}

SortCondition::SortCondition (std::vector<std::pair<VariableId, bool>> const& sorts, 
                              std::unordered_map<VariableId, AstNode const*> const& variableDefinitions) 
  : _expressions(),
    _unidirectional(true),
    _onlyAttributeAccess(true) {
 
  size_t const n = sorts.size();

  for (size_t i = 0; i < n; ++i) {
    if (_unidirectional && i > 0 && sorts[i].second != sorts[i - 1].second) {
      _unidirectional = false;
    }

    bool handled = false;
    auto variableId = sorts[i].first;
    
    auto it = variableDefinitions.find(variableId);

    if (it != variableDefinitions.end()) {
      auto node = (*it).second;

      if (node != nullptr && 
          node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
        std::vector<triagens::basics::AttributeName> fieldNames;
        while (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          fieldNames.emplace_back(triagens::basics::AttributeName(node->getStringValue()));
          node = node->getMember(0);
        }

        if (node->type == NODE_TYPE_REFERENCE) {
          handled = true;

          _fields.emplace_back(std::make_pair(static_cast<Variable const*>(node->getData()), fieldNames));
        }
      }
    }
    
    if (! handled) {
      _fields.emplace_back(std::pair<Variable const*, std::vector<triagens::basics::AttributeName>>());
      _onlyAttributeAccess = false;
    }
  }

  if (n == 0) {
    _onlyAttributeAccess = false;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of attributes in the sort condition covered
/// by the specified index fields
////////////////////////////////////////////////////////////////////////////////

size_t SortCondition::isCoveredBy (Variable const* reference,
                                   std::vector<std::vector<triagens::basics::AttributeName>> const& indexAttributes) const {
  size_t coveredAttributes = 0;

  for (size_t i = 0; i < indexAttributes.size(); ++i) {
    if (i >= _fields.size()) {
      break;
    }

    if (reference != _fields[i].first) {
      break;
    }

    auto const& fieldNames = _fields[i].second;
    if (fieldNames.size() != indexAttributes[i].size()) {
      // different attribute path
      break;
    }

    bool found = true;
    for (size_t j = 0; j < indexAttributes[i].size(); ++j) {
      if (indexAttributes[i][j].shouldExpand ||
          fieldNames[j] != indexAttributes[i][j].name) {
        // expanded attribute or different attribute
        found = false;
        break;
      }
    }

    if (! found) {
      break;
    }

    // same attribute
    ++coveredAttributes;
  }

  return coveredAttributes;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
