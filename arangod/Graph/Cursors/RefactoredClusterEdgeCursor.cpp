////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "./RefactoredClusterEdgeCursor.h"
#include <Aql/AstNode.h>

#include "Cluster/ClusterMethods.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

// TODO: Needed for the IndexAccessor, should be modified
#include "Graph/Providers/ClusterProvider.h"

using namespace arangodb;
using namespace arangodb::graph;

namespace {
IndexIteratorOptions defaultIndexIteratorOptions;
}

RefactoredClusterEdgeCursor::LookupInfo::LookupInfo(transaction::Methods::IndexHandle idx,
                                                    aql::AstNode* condition,
                                                    std::optional<size_t> memberToUpdate)
    : _idxHandle(std::move(idx)),
      _indexCondition(condition),
      _cursor(nullptr),
      _conditionMemberToUpdate(memberToUpdate) {}

RefactoredClusterEdgeCursor::LookupInfo::LookupInfo(LookupInfo&& other) noexcept
    : _idxHandle(std::move(other._idxHandle)),
      _expression(std::move(other._expression)),
      _indexCondition(other._indexCondition),
      _cursor(std::move(other._cursor)){};

RefactoredClusterEdgeCursor::LookupInfo::~LookupInfo() = default;

void RefactoredClusterEdgeCursor::LookupInfo::rearmVertex(VertexType vertex,
                                                          transaction::Methods* trx,
                                                          arangodb::aql::Variable const* tmpVar) {
  auto& node = _indexCondition;
  // We need to rewire the search condition for the new vertex
  TRI_ASSERT(node->numMembers() > 0);
  if (_conditionMemberToUpdate.has_value()) {
    // We have to inject _from/_to iff the condition needs it
    auto dirCmp = node->getMemberUnchecked(_conditionMemberToUpdate.value());
    TRI_ASSERT(dirCmp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ);
    TRI_ASSERT(dirCmp->numMembers() == 2);

    auto idNode = dirCmp->getMemberUnchecked(1);
    TRI_ASSERT(idNode->type == aql::NODE_TYPE_VALUE);
    TRI_ASSERT(idNode->isValueType(aql::VALUE_TYPE_STRING));
    // must edit node in place; TODO replace node?
    // TODO i think there is now a mutable String node available
    TEMPORARILY_UNLOCK_NODE(idNode);
    idNode->setStringValue(vertex.data(), vertex.length());
  }

  // We need to reset the cursor

  // check if the underlying index iterator supports rearming
  if (_cursor != nullptr && _cursor->canRearm()) {
    // rearming supported
    if (!_cursor->rearm(node, tmpVar, ::defaultIndexIteratorOptions)) {
      _cursor = std::make_unique<EmptyIndexIterator>(_cursor->collection(), trx);
    }
  } else {
    // rearming not supported - we need to throw away the index iterator
    // and create a new one
    _cursor = trx->indexScanForCondition(_idxHandle, node, tmpVar, ::defaultIndexIteratorOptions);
  }
}

IndexIterator& RefactoredClusterEdgeCursor::LookupInfo::cursor() {
  // If this kicks in, you forgot to call rearm with a specific vertex
  TRI_ASSERT(_cursor != nullptr);
  return *_cursor;
}

RefactoredClusterEdgeCursor::RefactoredClusterEdgeCursor(
    arangodb::transaction::Methods* trx,
    arangodb::aql::FixedVarExpressionContext const& expressionContext,
    ClusterTraverserCache* cache, bool backward)
    : _currentCursor(0), _trx(trx), _expressionContext(expressionContext), _cache(cache), _backward(backward) {}

RefactoredClusterEdgeCursor::~RefactoredClusterEdgeCursor() {}

#ifdef USE_ENTERPRISE
static bool CheckInaccessible(transaction::Methods* trx, VPackSlice const& edge) {
  // for skipInaccessibleCollections we need to check the edge
  // document, in that case nextWithExtra has no benefit
  TRI_ASSERT(edge.isString());
  arangodb::velocypack::StringRef str(edge);
  size_t pos = str.find('/');
  TRI_ASSERT(pos != std::string::npos);
  return trx->isInaccessibleCollection(str.substr(0, pos).toString());
}
#endif

void RefactoredClusterEdgeCursor::rearm(arangodb::velocypack::StringRef vertexId,
                                        uint64_t depth) {
  _edgeList.clear();
  _cursorPosition = 0;

  TRI_ASSERT(trx() != nullptr);
  TRI_ASSERT(_cache != nullptr);


  // Default trav. variant
  //Result res = fetchEdgesFromEngines(*trx(), *_cache, _expressionContext,
  // vertexId, depth, _edgeList); // TODO: check vertexId copy

  // shortest path variant?
  transaction::BuilderLeaser b(trx());
  b->add(VPackValuePair(vertexId.data(), vertexId.length(), VPackValueType::String));
  Result res = fetchEdgesFromEngines(*trx(), *_cache, b->slice(), _backward, _edgeList, _cache->insertedDocuments());

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  // _httpRequests += _cache->engines()->size(); TODO: increase http requests in stats
}

void RefactoredClusterEdgeCursor::readAll(aql::TraversalStats& stats, Callback const& callback) {
  for (VPackSlice const& edge : _edgeList) {
    callback(EdgeDocumentToken(edge), edge, _cursorPosition);
  }
}

arangodb::transaction::Methods* RefactoredClusterEdgeCursor::trx() const {
  return _trx;
}

arangodb::aql::FixedVarExpressionContext const& RefactoredClusterEdgeCursor::getExpressionContext() {
  return _expressionContext;
}