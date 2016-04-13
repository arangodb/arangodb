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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Traverser.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/json-utilities.h"
#include "Indexes/EdgeIndex.h"
#include "Utils/Transaction.h"
#include "Utils/TransactionContext.h"
#include "VocBase/KeyGenerator.h"

#include <velocypack/Iterator.h> 
#include <velocypack/velocypack-aliases.h>

using TraverserExpression = arangodb::traverser::TraverserExpression;

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper to transform a vertex _id string to VertexId struct.
/// NOTE:  Make sure the given string is not freed as long as the resulting
///        VertexId is in use
////////////////////////////////////////////////////////////////////////////////

arangodb::traverser::VertexId arangodb::traverser::IdStringToVertexId(
    CollectionNameResolver const* resolver, std::string const& vertex) {
  size_t split;
  char const* str = vertex.c_str();

  if (!TRI_ValidateDocumentIdKeyGenerator(str, vertex.size(), &split)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  std::string const collectionName = vertex.substr(0, split);
  auto cid = resolver->getCollectionIdCluster(collectionName);

  if (cid == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  return VertexId(cid, const_cast<char*>(str + split + 1));
}

void arangodb::traverser::TraverserOptions::setCollections(
    std::vector<std::string> const& colls, TRI_edge_direction_e dir) {
  // We do not allow to reset the collections.
  TRI_ASSERT(_collections.empty());
  TRI_ASSERT(_directions.empty());
  TRI_ASSERT(!colls.empty());
  _collections = colls;
  _directions.emplace_back(dir);
  for (auto const& it : colls) {
    _indexHandles.emplace_back(_trx->edgeIndexHandle(it));
  }
}

void arangodb::traverser::TraverserOptions::setCollections(
    std::vector<std::string> const& colls,
    std::vector<TRI_edge_direction_e> const& dirs) {
  // We do not allow to reset the collections.
  TRI_ASSERT(_collections.empty());
  TRI_ASSERT(_directions.empty());
  TRI_ASSERT(!colls.empty());
  TRI_ASSERT(colls.size() == dirs.size());
  _collections = colls;
  _directions = dirs;
  for (auto const& it : colls) {
    _indexHandles.emplace_back(_trx->edgeIndexHandle(it));
  }
}

size_t arangodb::traverser::TraverserOptions::collectionCount () const {
  return _collections.size();
}

bool arangodb::traverser::TraverserOptions::getCollection(
    size_t const index, std::string& name, TRI_edge_direction_e& dir) const {
  if (index >= _collections.size()) {
    // No more collections stop now
    return false;
  }
  if (_directions.size() == 1) {
    dir = _directions.at(0);
  } else {
    dir = _directions.at(index);
  }
  name = _collections.at(index);

  // arangodb::EdgeIndex::buildSearchValue(direction, eColName, _builder);
  return true;
}

bool arangodb::traverser::TraverserOptions::getCollectionAndSearchValue(
    size_t index, std::string const& vertexId, std::string& name,
    Transaction::IndexHandle& indexHandle, VPackBuilder& builder) {
  if (index >= _collections.size()) {
    // No more collections stop now
    return false;
  }
  TRI_edge_direction_e dir;
  if (_directions.size() == 1) {
    dir = _directions.at(0);
  } else {
    dir = _directions.at(index);
  }
  name = _collections.at(index);
  indexHandle = _indexHandles.at(index);

  builder.clear();
  arangodb::EdgeIndex::buildSearchValue(dir, vertexId, builder);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates an expression from a VelocyPackSlice
////////////////////////////////////////////////////////////////////////////////

TraverserExpression::TraverserExpression(VPackSlice const& slice) {
  isEdgeAccess = slice.get("isEdgeAccess").getBool();
  comparisonType = static_cast<aql::AstNodeType>(
      slice.get("comparisonType").getNumber<uint32_t>());
  auto registerNode = [&](aql::AstNode const* node)
                          -> void { _nodeRegister.emplace_back(node); };

  auto registerString = [&](std::string const& str) -> char const* {
    auto copy = std::make_unique<std::string>(str.c_str(), str.size());

    _stringRegister.emplace_back(copy.get());
    auto p = copy.release();
    TRI_ASSERT(p != nullptr);
    TRI_ASSERT(p->c_str() != nullptr);
    return p->c_str();  // should never change its position, even if vector
                        // grows/shrinks
  };

  arangodb::basics::Json varNode(
      TRI_UNKNOWN_MEM_ZONE,
      basics::VelocyPackHelper::velocyPackToJson(slice.get("varAccess")),
      arangodb::basics::Json::AUTOFREE);

  VPackSlice compareToSlice = slice.get("compareTo");
  VPackBuilder* builder = new VPackBuilder;
  try {
    builder->add(compareToSlice);
  } catch (...) {
    delete builder;
    throw;
  }
  compareTo.reset(builder);
  // If this fails everything before does not leak
  varAccess = new aql::AstNode(registerNode, registerString, varNode);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transforms the expression into json
////////////////////////////////////////////////////////////////////////////////

void TraverserExpression::toJson(arangodb::basics::Json& json,
                                 TRI_memory_zone_t* zone) const {
  json("isEdgeAccess", arangodb::basics::Json(isEdgeAccess))(
       "comparisonType",
       arangodb::basics::Json(static_cast<int32_t>(comparisonType)))(
       "varAccess", varAccess->toJson(zone, true));

  if (compareTo != nullptr) {
    // We have to copy compareTo. The json is greedy and steals it...
    TRI_json_t* extracted = arangodb::basics::VelocyPackHelper::velocyPackToJson(compareTo->slice());
    if (extracted == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    json("compareTo", arangodb::basics::Json(TRI_UNKNOWN_MEM_ZONE, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, extracted)));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively iterates through the access ast
///        Returns false whenever the document does not have the required format
////////////////////////////////////////////////////////////////////////////////

bool TraverserExpression::recursiveCheck(arangodb::aql::AstNode const* node,
                                         arangodb::velocypack::Slice& element) const {
  switch (node->type) {
    case arangodb::aql::NODE_TYPE_REFERENCE:
      // We are on the variable access
      return true;
    case arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS: {
      std::string name(node->getString());
      if (!recursiveCheck(node->getMember(0), element)) {
        return false;
      }
      if (!element.isObject() || !element.hasKey(name)) {
        return false;
      }
      element = element.get(name);
      break;
    }
    case arangodb::aql::NODE_TYPE_INDEXED_ACCESS: {
      auto index = node->getMember(1);
      if (!index->isIntValue()) {
        return false;
      }
      if (!recursiveCheck(node->getMember(0), element)) {
        return false;
      }
      auto idx = index->getIntValue();
      if (!element.isArray()) {
        return false;
      }
      element = element.at(idx);
      break;
    }
    default:
      return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief evaluates if an element matches the given expression
////////////////////////////////////////////////////////////////////////////////

bool TraverserExpression::matchesCheck(arangodb::Transaction* trx,
                                       VPackSlice const& element) const {
  TRI_ASSERT(trx != nullptr);

  VPackSlice value = element; 
  
  // initialize compare value to Null
  VPackSlice result = arangodb::basics::VelocyPackHelper::NullValue();
  // perform recursive check. this may modify value
  if (recursiveCheck(varAccess, value)) {
    result = value;
  }

  TRI_ASSERT(compareTo != nullptr);
  VPackOptions* options = trx->transactionContext()->getVPackOptions();

  switch (comparisonType) {
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ:
      return arangodb::basics::VelocyPackHelper::compare(result, compareTo->slice(), false, options) == 0; 
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE:
      return arangodb::basics::VelocyPackHelper::compare(result, compareTo->slice(), false, options) != 0; 
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT:
      return arangodb::basics::VelocyPackHelper::compare(result, compareTo->slice(), true, options) < 0; 
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE:
      return arangodb::basics::VelocyPackHelper::compare(result, compareTo->slice(), true, options) <= 0; 
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE:
      return arangodb::basics::VelocyPackHelper::compare(result, compareTo->slice(), true, options) >= 0; 
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT:
      return arangodb::basics::VelocyPackHelper::compare(result, compareTo->slice(), true, options) > 0;
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN: {
      // In means any of the elements in compareTo is identical
      VPackSlice compareArray = compareTo->slice();
      for (auto const& cmp : VPackArrayIterator(compareArray)) {
        if (arangodb::basics::VelocyPackHelper::compare(result, cmp, false, options) == 0) {
          // One is identical
          return true;
        }
      }
      // If we get here non is identical
      return false;
    }
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN: {
      // NIN means none of the elements in compareTo is identical
      VPackSlice compareArray = compareTo->slice();
      for (auto const& cmp : VPackArrayIterator(compareArray)) {
        if (arangodb::basics::VelocyPackHelper::compare(result, cmp, false, options) == 0) {
          // One is identical
          return false;
        }
      }
      // If we get here non is identical
      return true;
    }
    default:
      TRI_ASSERT(false);
  }
  return false;
}

