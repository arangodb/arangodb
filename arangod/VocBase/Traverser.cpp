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
#include "VocBase/KeyGenerator.h"

using TraverserExpression = triagens::arango::traverser::TraverserExpression;

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper to transform a vertex _id string to VertexId struct.
/// NOTE:  Make sure the given string is not freed as long as the resulting
///        VertexId is in use
////////////////////////////////////////////////////////////////////////////////

triagens::arango::traverser::VertexId triagens::arango::traverser::IdStringToVertexId (
    CollectionNameResolver const* resolver,
    std::string const& vertex
  ) {
  size_t split;
  char const* str = vertex.c_str();

  if (! TRI_ValidateDocumentIdKeyGenerator(str, &split)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  std::string const collectionName = vertex.substr(0, split);
  auto cid = resolver->getCollectionIdCluster(collectionName);

  if (cid == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  return VertexId(cid, const_cast<char *>(str + split + 1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates an expression from a TRI_json_t*
////////////////////////////////////////////////////////////////////////////////

TraverserExpression::TraverserExpression (TRI_json_t const* json) {
  isEdgeAccess = basics::JsonHelper::checkAndGetBooleanValue(json, "isEdgeAccess");

  comparisonType = static_cast<aql::AstNodeType>(basics::JsonHelper::checkAndGetNumericValue<uint32_t>(json, "comparisonType"));

  auto registerNode = [&](aql::AstNode const* node) -> void {
    _nodeRegister.emplace_back(node);
  };

  auto registerString = [&](std::string const& str) -> char const* {
    auto copy = std::make_unique<std::string>(str.c_str(), str.size());

    _stringRegister.emplace_back(copy.get());
    auto p = copy.release();
    TRI_ASSERT(p != nullptr);
    TRI_ASSERT(p->c_str() != nullptr);
    return p->c_str(); // should never change its position, even if vector grows/shrinks
  };

  triagens::basics::Json varNode(TRI_UNKNOWN_MEM_ZONE, basics::JsonHelper::checkAndGetObjectValue(json, "varAccess"), triagens::basics::Json::NOFREE);

  compareTo.reset(new triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, basics::JsonHelper::getObjectElement(json, "compareTo"), triagens::basics::Json::NOFREE));

  if (compareTo->json() == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid compareTo value");
  }
  // If this fails everything before does not leak
  varAccess = new aql::AstNode(registerNode, registerString, varNode);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transforms the expression into json
////////////////////////////////////////////////////////////////////////////////

void TraverserExpression::toJson (triagens::basics::Json& json,
                                  TRI_memory_zone_t* zone) const {
  json("isEdgeAccess", triagens::basics::Json(isEdgeAccess))
      ("comparisonType", triagens::basics::Json(static_cast<int32_t>(comparisonType)))
      ("varAccess", varAccess->toJson(zone, true));
  if (compareTo.get() != nullptr) {
    // We have to copy compareTo. The json is greedy and steals it...
    json("compareTo", compareTo->copy());
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

bool TraverserExpression::matchesCheck (DocumentAccessor& accessor) const {

  triagens::basics::Json result(triagens::basics::Json::Null);
  if (recursiveCheck(varAccess, accessor)) {
    result = accessor.toJson();
  }

  TRI_ASSERT(compareTo != nullptr);
  TRI_ASSERT(compareTo->json() != nullptr);

  switch (comparisonType) {
    case triagens::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      return TRI_CompareValuesJson(result.json(), compareTo->json(), false) == 0;
    case triagens::aql::NODE_TYPE_OPERATOR_BINARY_NE:
      return TRI_CompareValuesJson(result.json(), compareTo->json(), false) != 0;
    case triagens::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      return TRI_CompareValuesJson(result.json(), compareTo->json(), true) < 0;
    case triagens::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      return TRI_CompareValuesJson(result.json(), compareTo->json(), true) <= 0;
    case triagens::aql::NODE_TYPE_OPERATOR_BINARY_GE:
      return TRI_CompareValuesJson(result.json(), compareTo->json(), true) >= 0;
    case triagens::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      return TRI_CompareValuesJson(result.json(), compareTo->json(), true) > 0;
    default:
      TRI_ASSERT(false);
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief evalutes if an element matches the given expression
////////////////////////////////////////////////////////////////////////////////

bool TraverserExpression::matchesCheck (TRI_json_t const* element) const {
  DocumentAccessor accessor(element);
  return matchesCheck(accessor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief evalutes if an element matches the given expression
////////////////////////////////////////////////////////////////////////////////

bool TraverserExpression::matchesCheck (TRI_doc_mptr_t& element,
                                        TRI_document_collection_t* collection,
                                        triagens::arango::CollectionNameResolver const* resolver) const {
  DocumentAccessor accessor(resolver, collection, &element);
  return matchesCheck(accessor);
}
