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
#include "Indexes/EdgeIndex.h"
#include "Utils/Transaction.h"
#include "Utils/TransactionContext.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/TraverserOptions.h"

#include <velocypack/Iterator.h> 
#include <velocypack/velocypack-aliases.h>

using Traverser = arangodb::traverser::Traverser;
using TraverserExpression = arangodb::traverser::TraverserExpression;

/// @brief Class Shortest Path

/// @brief Clears the path
void arangodb::traverser::ShortestPath::clear() {
  _vertices.clear();
  _edges.clear();
}

void arangodb::traverser::ShortestPath::edgeToVelocyPack(Transaction*, size_t position, VPackBuilder& builder) {
  TRI_ASSERT(position < length());
  if (position == 0) {
    builder.add(basics::VelocyPackHelper::NullValue());
  } else {
    TRI_ASSERT(position - 1 < _edges.size());
    builder.add(_edges[position - 1]);
  }
}

void arangodb::traverser::ShortestPath::vertexToVelocyPack(Transaction* trx, size_t position, VPackBuilder& builder) {
  TRI_ASSERT(position < length());
  VPackSlice v = _vertices[position];
  TRI_ASSERT(v.isString());
  std::string collection =  v.copyString();
  size_t p = collection.find("/");
  TRI_ASSERT(p != std::string::npos);

  TransactionBuilderLeaser searchBuilder(trx);
  searchBuilder->add(VPackValue(collection.substr(p + 1)));
  collection = collection.substr(0, p);

  int res =
      trx->documentFastPath(collection, searchBuilder->slice(), builder, true);
  if (res != TRI_ERROR_NO_ERROR) {
    builder.clear(); // Just in case...
    builder.add(basics::VelocyPackHelper::NullValue());
  }
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
  varAccess = new aql::AstNode(registerNode, registerString, slice.get("varAccess"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transforms the expression into VelocyPack
////////////////////////////////////////////////////////////////////////////////

void TraverserExpression::toVelocyPack(VPackBuilder& builder) const {
  builder.openObject();
  builder.add("isEdgeAccess", VPackValue(isEdgeAccess));
  builder.add("comparisonType",
              VPackValue(static_cast<int32_t>(comparisonType)));
  
  builder.add(VPackValue("varAccess"));
  varAccess->toVelocyPack(builder, true);
  if (compareTo != nullptr) {
    builder.add("compareTo", compareTo->slice());
  }
  builder.close();
}



////////////////////////////////////////////////////////////////////////////////
/// @brief recursively iterates through the access ast
///        Returns false whenever the document does not have the required format
////////////////////////////////////////////////////////////////////////////////

bool TraverserExpression::recursiveCheck(arangodb::aql::AstNode const* node,
                                         arangodb::velocypack::Slice& element,
                                         arangodb::velocypack::Slice& base) const {

  base = arangodb::basics::VelocyPackHelper::EmptyObjectValue();

  switch (node->type) {
    case arangodb::aql::NODE_TYPE_REFERENCE:
      // We are on the variable access
      return true;
    case arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS: {
      std::string name(node->getString());
      if (!recursiveCheck(node->getMember(0), element, base)) {
        return false;
      }
      if (!element.isObject() || !element.hasKey(name)) {
        return false;
      }
      base = element; // set base object
      element = element.get(name);
      break;
    }
    case arangodb::aql::NODE_TYPE_INDEXED_ACCESS: {
      auto index = node->getMember(1);
      if (!index->isIntValue()) {
        return false;
      }
      if (!recursiveCheck(node->getMember(0), element, base)) {
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
  VPackSlice base = arangodb::basics::VelocyPackHelper::EmptyObjectValue();

  VPackSlice value = element.resolveExternal(); 
  
  // initialize compare value to Null
  VPackSlice result = arangodb::basics::VelocyPackHelper::NullValue();
  // perform recursive check. this may modify value
  if (recursiveCheck(varAccess, value, base)) {
    result = value;
  }

  // hack for _id attribute
  TransactionBuilderLeaser builder(trx);
  if (result.isCustom() && base.isObject()) {
    builder->add(VPackValue(trx->extractIdString(base)));
    result = builder->slice();
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


bool Traverser::VertexGetter::getVertex(
    VPackSlice edge, std::vector<VPackSlice>& result) {
  VPackSlice cmp = result.back();
  VPackSlice res = Transaction::extractFromFromDocument(edge);
  if (arangodb::basics::VelocyPackHelper::compare(cmp, res, false) == 0) {
    res = Transaction::extractToFromDocument(edge);
  }

  if (!_traverser->vertexMatchesConditions(res, result.size())) {
    return false;
  }
  result.emplace_back(res);
  return true;
}

bool Traverser::VertexGetter::getSingleVertex(VPackSlice edge,
                                                          VPackSlice cmp,
                                                          size_t depth,
                                                          VPackSlice& result) {
  VPackSlice from = Transaction::extractFromFromDocument(edge);
  if (arangodb::basics::VelocyPackHelper::compare(cmp, from, false) != 0) {
    result = from;
  } else {
    result = Transaction::extractToFromDocument(edge);
  }
  return _traverser->vertexMatchesConditions(result, depth);
}



void Traverser::VertexGetter::reset(arangodb::velocypack::Slice) {
}

bool Traverser::UniqueVertexGetter::getVertex(
  VPackSlice edge, std::vector<VPackSlice>& result) {
  VPackSlice toAdd = Transaction::extractFromFromDocument(edge);
  VPackSlice cmp = result.back();

  if (arangodb::basics::VelocyPackHelper::compare(toAdd, cmp, false) == 0) {
    toAdd = Transaction::extractToFromDocument(edge);
  }
  

  // First check if we visited it. If not, than mark
  if (_returnedVertices.find(toAdd) != _returnedVertices.end()) {
    // This vertex is not unique.
    ++_traverser->_filteredPaths;
    return false;
  } else {
    _returnedVertices.emplace(toAdd);
  }

  if (!_traverser->vertexMatchesConditions(toAdd, result.size())) {
    return false;
  }

  result.emplace_back(toAdd);
  return true;
}

bool Traverser::UniqueVertexGetter::getSingleVertex(
  VPackSlice edge, VPackSlice cmp, size_t depth, VPackSlice& result) {
  result = Transaction::extractFromFromDocument(edge);

  if (arangodb::basics::VelocyPackHelper::compare(result, cmp, false) == 0) {
    result = Transaction::extractToFromDocument(edge);
  }
  
  // First check if we visited it. If not, than mark
  if (_returnedVertices.find(result) != _returnedVertices.end()) {
    // This vertex is not unique.
    ++_traverser->_filteredPaths;
    return false;
  } else {
    _returnedVertices.emplace(result);
  }

  return _traverser->vertexMatchesConditions(result, depth);
}

void Traverser::UniqueVertexGetter::reset(VPackSlice startVertex) {
  _returnedVertices.clear();
  // The startVertex always counts as visited!
  _returnedVertices.emplace(startVertex);
}

Traverser::Traverser(arangodb::traverser::TraverserOptions* opts, arangodb::Transaction* trx)
    : _trx(trx),
      _startIdBuilder(trx),
      _readDocuments(0),
      _filteredPaths(0),
      _done(true),
      _opts(opts) {
  if (opts->uniqueVertices == TraverserOptions::UniquenessLevel::GLOBAL) {
    _vertexGetter = std::make_unique<UniqueVertexGetter>(this);
  } else {
    _vertexGetter = std::make_unique<VertexGetter>(this);
  }
}

bool arangodb::traverser::Traverser::edgeMatchesConditions(VPackSlice e,
                                                           VPackSlice vid,
                                                           size_t depth,
                                                           size_t cursorId) {
  if (!_opts->evaluateEdgeExpression(e, vid, depth, cursorId)) {
    ++_filteredPaths;
    return false;
  }
  return true;
}

bool arangodb::traverser::Traverser::vertexMatchesConditions(VPackSlice v, size_t depth) {
  TRI_ASSERT(v.isString());
  if (_opts->vertexHasFilter(depth)) {
    aql::AqlValue vertex = fetchVertexData(v);
    if (!_opts->evaluateVertexExpression(vertex.slice(), depth)) {
      ++_filteredPaths;
      return false;
    }
  }
  return true;
}

bool arangodb::traverser::Traverser::next() {
  TRI_ASSERT(!_done);
  bool res = _enumerator->next();
  if (!res) {
    _done = true;
  }
  return res;
}

arangodb::aql::AqlValue arangodb::traverser::Traverser::lastVertexToAqlValue() {
  return _enumerator->lastVertexToAqlValue();
}

arangodb::aql::AqlValue arangodb::traverser::Traverser::lastEdgeToAqlValue() {
  return _enumerator->lastEdgeToAqlValue();
}

arangodb::aql::AqlValue arangodb::traverser::Traverser::pathToAqlValue(
    VPackBuilder& builder) {
  return _enumerator->pathToAqlValue(builder);
}
