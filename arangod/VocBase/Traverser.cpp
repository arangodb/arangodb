////////////////////////////////////////////////////////////////////////////////
/// @brief Traverser
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Traverser.h"
#include "Basics/json-utilities.h"

using TraverserExpression = triagens::arango::traverser::TraverserExpression;

////////////////////////////////////////////////////////////////////////////////
/// @brief transforms the expression into json
////////////////////////////////////////////////////////////////////////////////

void TraverserExpression::toJson (triagens::basics::Json& json,
                                  TRI_memory_zone_t* zone) const {
  auto op = triagens::aql::AstNode::Operators.find(comparisonType);
  
  if (op == triagens::aql::AstNode::Operators.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "invalid operator for TraverserExpression");
  }
  std::string const operatorStr = op->second;

  json("isEdgeAccess", triagens::basics::Json(isEdgeAccess))
      ("comparisonType", triagens::basics::Json(operatorStr))
      ("varAccess", varAccess->toJson(zone, true));
  if (compareTo != nullptr) {
    json("compareTo", *compareTo);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively iterates through the access ast
///        Returns false whenever the document does not have the required format
////////////////////////////////////////////////////////////////////////////////

bool TraverserExpression::recursiveCheck (triagens::aql::AstNode const* node,
                                          DocumentAccessor& accessor) const {
  switch (node->type) {
    case triagens::aql::NODE_TYPE_REFERENCE:
      // We are on the variable access
      return true;
      break;
    case triagens::aql::NODE_TYPE_ATTRIBUTE_ACCESS: {
      char const* attributeName = node->getStringValue();
      TRI_ASSERT(attributeName != nullptr);
      std::string name(attributeName, node->getStringLength());
      if (! recursiveCheck(node->getMember(0), accessor)) {
        return false;
      }
      if (! accessor.isObject() || ! accessor.hasKey(name)) {
        return false;
      }
      accessor.get(name);
      break;
    }
    case triagens::aql::NODE_TYPE_INDEXED_ACCESS: {
      auto index = node->getMember(1);
      if (! index->isIntValue()) {
        return false;
      }
      if (! recursiveCheck(node->getMember(0), accessor)) {
        return false;
      }
      auto idx = index->getIntValue();
        // TODO check for Number
      if (! accessor.isArray()) {
        return false;
      }
      accessor.at(idx);
      break;
    }
    default:
      return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief evalutes if an element matches the given expression
////////////////////////////////////////////////////////////////////////////////

bool TraverserExpression::matchesCheck (TRI_doc_mptr_t& element,
                                        TRI_document_collection_t* collection,
                                        CollectionNameResolver* resolver) const {
  DocumentAccessor accessor(resolver, collection, &element);
  if (recursiveCheck(varAccess, accessor)) {
    triagens::basics::Json result = accessor.toJson();
    return TRI_CompareValuesJson(result.json(), compareTo->json(), false) == 0;
  }
  return false;
}
