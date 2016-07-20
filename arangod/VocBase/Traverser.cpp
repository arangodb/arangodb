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
#include "VocBase/SingleServerTraverser.h"

#include <velocypack/Iterator.h> 
#include <velocypack/velocypack-aliases.h>

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

  int res = trx->documentFastPath(collection, searchBuilder->slice(), builder);
  if (res != TRI_ERROR_NO_ERROR) {
    builder.clear(); // Just in case...
    builder.add(basics::VelocyPackHelper::NullValue());
  }
}

bool arangodb::traverser::TraverserOptions::evaluateEdgeExpression(arangodb::velocypack::Slice edge, size_t depth) const {
#warning This has to be implemented.
  return true;
}

bool arangodb::traverser::TraverserOptions::evaluateVertexExpression(arangodb::velocypack::Slice vertex, size_t depth) const {
#warning This has to be implemented.
  return true;
}

arangodb::traverser::EdgeCursor* arangodb::traverser::TraverserOptions::nextCursor(VPackSlice vertex, size_t depth) const {
  auto specific = _depthLookupInfo.find(depth);
  std::vector<LookupInfo> list;
  if (specific != _depthLookupInfo.end()) {
    list = specific->second;
  } else {
    list = _baseLookupInfos;
  }

#warning NOTE SINGLE SERVER CASE ONLY!
  auto allCursor = std::make_unique<SingleServerEdgeCursor>(list.size());
  auto& opCursors = allCursor->getCursors();
  VPackValueLength vidLength;
  char const* vid = vertex.getString(vidLength);
  for (auto& info : list) {
    auto& node = info.indexCondition;
    TRI_ASSERT(node->numMembers() > 0);
    auto dirCmp = node->getMemberUnchecked(node->numMembers() - 1);
    TRI_ASSERT(dirCmp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ); 
    TRI_ASSERT(dirCmp->numMembers() == 2);

    auto idNode = dirCmp->getMemberUnchecked(1);
    TRI_ASSERT(idNode->type == aql::NODE_TYPE_VALUE);
    TRI_ASSERT(idNode->isValueType(aql::VALUE_TYPE_STRING));
    idNode->setStringValue(vid, vidLength);
    opCursors.emplace_back(_trx->indexScanForCondition(
        info.idxHandle, node, _tmpVar, UINT64_MAX, 1000, false));
  }
  return allCursor.release();
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

  VPackSlice value = element.resolveExternal(); 
  
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

