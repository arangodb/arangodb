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

#include "./RefactoredSingleServerEdgeCursor.h"
#include <Aql/AstNode.h>

#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

// TODO: Needed for the IndexAccessor, should be modified
#include "Graph/Providers/SingleServerProvider.h"

using namespace arangodb;
using namespace arangodb::graph;

namespace {
IndexIteratorOptions defaultIndexIteratorOptions;
}

RefactoredSingleServerEdgeCursor::LookupInfo::LookupInfo(
    transaction::Methods::IndexHandle idx, aql::AstNode* condition,
    std::optional<size_t> memberToUpdate, aql::Expression* expression, arangodb::transaction::Methods* trx)
    : _idxHandle(std::move(idx)),
      _expression(expression),
      _indexCondition(condition),
      _cursor(nullptr),
      _conditionMemberToUpdate(memberToUpdate) {}

RefactoredSingleServerEdgeCursor::LookupInfo::LookupInfo(LookupInfo&& other) noexcept
    : _idxHandle(std::move(other._idxHandle)),
      _expression(std::move(other._expression)),
      _indexCondition(other._indexCondition),
      _cursor(std::move(other._cursor)){};

RefactoredSingleServerEdgeCursor::LookupInfo::~LookupInfo() = default;

aql::Expression* RefactoredSingleServerEdgeCursor::LookupInfo::getExpression() {
  return _expression;
}

void RefactoredSingleServerEdgeCursor::LookupInfo::rearmVertex(
    VertexType vertex, transaction::Methods* trx, arangodb::aql::Variable const* tmpVar) {
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
  } else {
    // If we have to inject the vertex value it has to be within
    // the last member of the condition.
    // We only get into this case iff the index used does
    // not cover _from resp. _to.
    // inject _from/_to value
    auto expressionNode = _expression->nodeForModification();

    TRI_ASSERT(expressionNode->numMembers() > 0);
    auto dirCmp = expressionNode->getMemberUnchecked(expressionNode->numMembers() - 1);
    TRI_ASSERT(dirCmp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ);
    TRI_ASSERT(dirCmp->numMembers() == 2);

    auto idNode = dirCmp->getMemberUnchecked(1);
    TRI_ASSERT(idNode->type == aql::NODE_TYPE_VALUE);
    TRI_ASSERT(idNode->isValueType(aql::VALUE_TYPE_STRING));

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

IndexIterator& RefactoredSingleServerEdgeCursor::LookupInfo::cursor() {
  // If this kicks in, you forgot to call rearm with a specific vertex
  TRI_ASSERT(_cursor != nullptr);
  return *_cursor;
}

RefactoredSingleServerEdgeCursor::RefactoredSingleServerEdgeCursor(
    SingleServerProvider& provider, arangodb::aql::Variable const* tmpVar,
    std::vector<IndexAccessor> const& indexConditions, arangodb::aql::QueryContext& queryContext)
    : _tmpVar(tmpVar),
      _currentCursor(0),
      _provider(provider),
      _expressionCtx(*_provider.trx(), queryContext, _aqlFunctionsInternalCache) {
  // We need at least one indexCondition, otherwise nothing to serve
  TRI_ASSERT(!indexConditions.empty());
  _lookupInfo.reserve(indexConditions.size());
  for (auto const& idxCond : indexConditions) {
    _lookupInfo.emplace_back(idxCond.indexHandle(), idxCond.getCondition(),
                             idxCond.getMemberToUpdate(), idxCond.getExpression(), _provider.trx());
  }
}

RefactoredSingleServerEdgeCursor::~RefactoredSingleServerEdgeCursor() {}

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

void RefactoredSingleServerEdgeCursor::rearm(VertexType vertex, uint64_t /*depth*/) {
  _currentCursor = 0;
  for (auto& info : _lookupInfo) {
    info.rearmVertex(vertex, _provider.trx(), _tmpVar);
  }
}

void RefactoredSingleServerEdgeCursor::readAll(aql::TraversalStats& stats,
                                               Callback const& callback) {
  TRI_ASSERT(!_lookupInfo.empty());
  auto trx = _provider.trx();
  VPackBuilder tmpBuilder;

  for (_currentCursor = 0; _currentCursor < _lookupInfo.size(); ++_currentCursor) {
    auto& cursor = _lookupInfo[_currentCursor].cursor();
    LogicalCollection* collection = cursor.collection();
    auto cid = collection->id();
    if (cursor.hasExtra()) {
      cursor.allExtra([&](LocalDocumentId const& token, VPackSlice edge) {
        stats.addScannedIndex(1);
#ifdef USE_ENTERPRISE
        if (trx->skipInaccessible() && CheckInaccessible(trx, edge)) {
          return false;
        }
#endif
        // eval expression if available
        if (_lookupInfo[_currentCursor].getExpression() != nullptr) {
          VPackSlice e = edge;
          if (edge.isString()) {
            tmpBuilder.clear();
            _provider.insertEdgeIntoResult(EdgeDocumentToken(cid, token), tmpBuilder);
            e = tmpBuilder.slice();
          }
          bool result =
              evaluateExpression(_lookupInfo[_currentCursor].getExpression(), e);
          if (!result) {
            stats.incrFiltered();
            return false;
          }
        } else {
          LOG_DEVEL << "NOT FOUND 1";
        }
        callback(EdgeDocumentToken(cid, token), edge, _currentCursor);
        return true;
      });
    } else {
      cursor.all([&](LocalDocumentId const& token) {
        return collection->getPhysical()
            ->read(trx, token,
                   [&](LocalDocumentId const&, VPackSlice edgeDoc) {
                     stats.addScannedIndex(1);
#ifdef USE_ENTERPRISE
                     if (trx->skipInaccessible()) {
                       // TODO: we only need to check one of these
                       VPackSlice from =
                           transaction::helpers::extractFromFromDocument(edgeDoc);
                       VPackSlice to =
                           transaction::helpers::extractToFromDocument(edgeDoc);
                       if (CheckInaccessible(trx, from) || CheckInaccessible(trx, to)) {
                         return false;
                       }
                     }
#endif
                     // eval expression if available
                     if (_lookupInfo[_currentCursor].getExpression() != nullptr) {
                       bool result =
                           evaluateExpression(_lookupInfo[_currentCursor].getExpression(),
                                              edgeDoc);
                       if (!result) {
                         stats.incrFiltered();
                         return false;
                       }
                     } else {
                       LOG_DEVEL << "NOT FOUND 2";
                     }
                     callback(EdgeDocumentToken(cid, token), edgeDoc, _currentCursor);
                     return true;
                   })
            .ok();
      });
    }
  }
}

bool RefactoredSingleServerEdgeCursor::evaluateExpression(arangodb::aql::Expression* expression,
                                                          VPackSlice value) {
  if (expression == nullptr) {
    LOG_DEVEL << "Returned true";
    return true;
  }

  VPackBuilder builder;
  expression->toVelocyPack(builder, false);
  LOG_DEVEL << "Expression: " << builder.toJson();

  TRI_ASSERT(value.isObject() || value.isNull());
  builder.clear();
  _tmpVar->toVelocyPack(builder);
  LOG_DEVEL << "tmpVar: " << builder.toJson() << " with value: " << value.toJson();

  expression->setVariable(_tmpVar, value);
  bool mustDestroy = false;
  aql::AqlValue res = expression->execute(&_expressionCtx, mustDestroy);
  TRI_ASSERT(res.isBoolean());
  bool result = res.toBoolean();
  expression->clearVariable(_tmpVar);
  if (mustDestroy) {
    res.destroy();
  }

  return result;
}

arangodb::transaction::Methods* RefactoredSingleServerEdgeCursor::trx() const {
  return _provider.trx();
}
