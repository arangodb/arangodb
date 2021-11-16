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

#include "RefactoredSingleServerEdgeCursor.h"

#include "Aql/AstNode.h"
#include "Aql/Ast.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/NonConstExpression.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

// TODO: Needed for the IndexAccessor, should be modified
#include "Graph/Providers/SingleServerProvider.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace {
IndexIteratorOptions defaultIndexIteratorOptions;
}

RefactoredSingleServerEdgeCursor::LookupInfo::LookupInfo(IndexAccessor* accessor)
    : _accessor(accessor),
      _cursor(nullptr) {}

RefactoredSingleServerEdgeCursor::LookupInfo::LookupInfo(LookupInfo&& other) noexcept
      : _accessor(other._accessor),
      _cursor(std::move(other._cursor)){};

RefactoredSingleServerEdgeCursor::LookupInfo::~LookupInfo() = default;

aql::Expression* RefactoredSingleServerEdgeCursor::LookupInfo::getExpression() {
  return _accessor->getExpression();
}

size_t RefactoredSingleServerEdgeCursor::LookupInfo::getCursorID() const {
  return _accessor->cursorId();
}

void RefactoredSingleServerEdgeCursor::LookupInfo::rearmVertex(
    VertexType vertex, transaction::Methods* trx, arangodb::aql::Variable const* tmpVar) {
  auto* node = _accessor->getCondition();
  // We need to rewire the search condition for the new vertex
  TRI_ASSERT(node->numMembers() > 0);
  if (_accessor->getMemberToUpdate().has_value()) {
    // We have to inject _from/_to iff the condition needs it
    auto dirCmp = node->getMemberUnchecked(_accessor->getMemberToUpdate().value());
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
    auto expressionNode = _accessor->getExpression()->nodeForModification();

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
    _cursor = trx->indexScanForCondition(_accessor->indexHandle(), node, tmpVar, ::defaultIndexIteratorOptions, ReadOwnWrites::no);
  }
}

IndexIterator& RefactoredSingleServerEdgeCursor::LookupInfo::cursor() {
  // If this kicks in, you forgot to call rearm with a specific vertex
  TRI_ASSERT(_cursor != nullptr);
  return *_cursor;
}

RefactoredSingleServerEdgeCursor::RefactoredSingleServerEdgeCursor(
    arangodb::transaction::Methods* trx,
    arangodb::aql::Variable const* tmpVar, std::vector<IndexAccessor>& indexConditions,
    arangodb::aql::FixedVarExpressionContext& expressionContext)
    : _tmpVar(tmpVar), _currentCursor(0), _trx(trx),
      _expressionCtx(expressionContext) {
  // We need at least one indexCondition, otherwise nothing to serve
  TRI_ASSERT(!indexConditions.empty());
  _lookupInfo.reserve(indexConditions.size());
  for (auto& idxCond : indexConditions) {
    _lookupInfo.emplace_back(&idxCond);
  }
}

RefactoredSingleServerEdgeCursor::~RefactoredSingleServerEdgeCursor() {}

void RefactoredSingleServerEdgeCursor::LookupInfo::calculateIndexExpressions(Ast* ast, ExpressionContext& ctx) {
  if (!_accessor->hasNonConstParts()) {
    return;
  }

  // The following are needed to evaluate expressions with local data from
  // the current incoming item:
  auto& nonConstPart = _accessor->nonConstPart();
  for (auto& toReplace : nonConstPart._expressions) {
    auto exp = toReplace->expression.get();
    bool mustDestroy;
    AqlValue a = exp->execute(&ctx, mustDestroy);
    AqlValueGuard guard(a, mustDestroy);

    AqlValueMaterializer materializer(&(ctx.trx().vpackOptions()));
    VPackSlice slice = materializer.slice(a, false);
    AstNode* evaluatedNode = ast->nodeFromVPack(slice, true);

    AstNode* tmp = _accessor->getCondition();
    for (size_t x = 0; x < toReplace->indexPath.size(); x++) {
      size_t idx = toReplace->indexPath[x];
      AstNode* old = tmp->getMember(idx);
      // modify the node in place
      TEMPORARILY_UNLOCK_NODE(tmp);
      if (x + 1 < toReplace->indexPath.size()) {
        AstNode* cpy = old;
        tmp->changeMember(idx, cpy);
        tmp = cpy;
      } else {
        // insert the actual expression value
        tmp->changeMember(idx, evaluatedNode);
      }
    }
  }
}


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
    info.rearmVertex(vertex, _trx, _tmpVar);
  }
}

void RefactoredSingleServerEdgeCursor::readAll(aql::TraversalStats& stats,
                                               Callback const& callback) {
  TRI_ASSERT(!_lookupInfo.empty());
  for (_currentCursor = 0; _currentCursor < _lookupInfo.size(); ++_currentCursor) {
    auto& cursor = _lookupInfo[_currentCursor].cursor();
    LogicalCollection* collection = cursor.collection();
    auto cid = collection->id();
    if (cursor.hasExtra()) {
      cursor.allExtra([&](LocalDocumentId const& token, VPackSlice edge) {
        stats.addScannedIndex(1);
#ifdef USE_ENTERPRISE
        if (_trx->skipInaccessible() && CheckInaccessible(_trx, edge)) {
          return false;
        }
#endif
        callback(EdgeDocumentToken(cid, token), edge, _currentCursor);
        return true;
      });
    } else {
      cursor.all([&](LocalDocumentId const& token) {
        return collection->getPhysical()->read(_trx, token, [&](LocalDocumentId const&, VPackSlice edgeDoc) {
          stats.addScannedIndex(1);
#ifdef USE_ENTERPRISE
          if (_trx->skipInaccessible()) {
            // TODO: we only need to check one of these
            VPackSlice from = transaction::helpers::extractFromFromDocument(edgeDoc);
            VPackSlice to = transaction::helpers::extractToFromDocument(edgeDoc);
            if (CheckInaccessible(_trx, from) || CheckInaccessible(_trx, to)) {
              return false;
            }
          }
#endif
          callback(EdgeDocumentToken(cid, token), edgeDoc, _currentCursor);
          return true;
        }, ReadOwnWrites::no).ok();
      });
    }
  }
}

arangodb::transaction::Methods* RefactoredSingleServerEdgeCursor::trx() const {
  return _trx;
}

void RefactoredSingleServerEdgeCursor::prepareIndexExpressions(aql::Ast* ast) {
  for (auto& it : _lookupInfo) {
    it.calculateIndexExpressions(ast, _expressionCtx);
  }
}
