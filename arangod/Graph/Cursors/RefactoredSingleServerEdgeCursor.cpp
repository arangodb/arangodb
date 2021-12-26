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

#include "Aql/AqlValueMaterializer.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/NonConstExpression.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

// TODO: Needed for the IndexAccessor, should be modified
#include "Graph/Providers/SingleServerProvider.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace {
IndexIteratorOptions defaultIndexIteratorOptions;

#ifdef USE_ENTERPRISE
static bool CheckInaccessible(transaction::Methods* trx,
                              VPackSlice const& edge) {
  // for skipInaccessibleCollections we need to check the edge
  // document, in that case nextWithExtra has no benefit
  TRI_ASSERT(edge.isString());
  arangodb::velocypack::StringRef str(edge);
  size_t pos = str.find('/');
  TRI_ASSERT(pos != std::string::npos);
  return trx->isInaccessibleCollection(str.substr(0, pos).toString());
}
#endif
}  // namespace

template<class Step>
RefactoredSingleServerEdgeCursor<Step>::LookupInfo::LookupInfo(
    IndexAccessor* accessor)
    : _accessor(accessor), _cursor(nullptr) {}

template<class Step>
RefactoredSingleServerEdgeCursor<Step>::LookupInfo::LookupInfo(
    LookupInfo&& other) = default;

template<class Step>
RefactoredSingleServerEdgeCursor<Step>::LookupInfo::~LookupInfo() = default;

template<class Step>
aql::Expression*
RefactoredSingleServerEdgeCursor<Step>::LookupInfo::getExpression() {
  return _accessor->getExpression();
}

template<class Step>
size_t RefactoredSingleServerEdgeCursor<Step>::LookupInfo::getCursorID() const {
  return _accessor->cursorId();
}

template<class Step>
void RefactoredSingleServerEdgeCursor<Step>::LookupInfo::rearmVertex(
    VertexType vertex, transaction::Methods* trx,
    arangodb::aql::Variable const* tmpVar) {
  auto* node = _accessor->getCondition();
  // We need to rewire the search condition for the new vertex
  TRI_ASSERT(node->numMembers() > 0);
  if (_accessor->getMemberToUpdate().has_value()) {
    // We have to inject _from/_to iff the condition needs it
    auto dirCmp =
        node->getMemberUnchecked(_accessor->getMemberToUpdate().value());
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
    auto dirCmp =
        expressionNode->getMemberUnchecked(expressionNode->numMembers() - 1);
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
    if (!_cursor->rearm(node, tmpVar, defaultIndexIteratorOptions)) {
      _cursor =
          std::make_unique<EmptyIndexIterator>(_cursor->collection(), trx);
    }
  } else {
    // rearming not supported - we need to throw away the index iterator
    // and create a new one
    _cursor = trx->indexScanForCondition(_accessor->indexHandle(), node, tmpVar,
                                         ::defaultIndexIteratorOptions,
                                         ReadOwnWrites::no);
  }
}

template<class Step>
IndexIterator& RefactoredSingleServerEdgeCursor<Step>::LookupInfo::cursor() {
  // If this kicks in, you forgot to call rearm with a specific vertex
  TRI_ASSERT(_cursor != nullptr);
  return *_cursor;
}

template<class Step>
RefactoredSingleServerEdgeCursor<Step>::RefactoredSingleServerEdgeCursor(
    transaction::Methods* trx, arangodb::aql::Variable const* tmpVar,
    std::vector<IndexAccessor>& globalIndexConditions,
    std::unordered_map<uint64_t, std::vector<IndexAccessor>>&
        depthBasedIndexConditions,
    arangodb::aql::FixedVarExpressionContext& expressionContext,
    bool requiresFullDocument)
    : _tmpVar(tmpVar),
      _trx(trx),
      _expressionCtx(expressionContext),
      _requiresFullDocument(requiresFullDocument) {
  // We need at least one indexCondition, otherwise nothing to serve
  TRI_ASSERT(!globalIndexConditions.empty());
  _lookupInfo.reserve(globalIndexConditions.size());
  _depthLookupInfo.reserve(depthBasedIndexConditions.size());

  for (auto& idxCond : globalIndexConditions) {
    _lookupInfo.emplace_back(&idxCond);
  }
  for (auto& obj : depthBasedIndexConditions) {
    // Need to reset cursor ID.
    // The cursorID relates to the used collection
    // not the condition
    auto& [depth, idxCondArray] = obj;

    std::vector<LookupInfo> tmpLookupVec;
    for (auto& idxCond : idxCondArray) {
      tmpLookupVec.emplace_back(&idxCond);
    }
    _depthLookupInfo.try_emplace(depth, std::move(tmpLookupVec));
  }
}

template<class Step>
void RefactoredSingleServerEdgeCursor<
    Step>::LookupInfo::calculateIndexExpressions(Ast* ast,
                                                 ExpressionContext& ctx) {
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

template<class Step>
RefactoredSingleServerEdgeCursor<Step>::~RefactoredSingleServerEdgeCursor() {}

template<class Step>
void RefactoredSingleServerEdgeCursor<Step>::rearm(VertexType vertex,
                                                   uint64_t depth) {
  for (auto& info : getLookupInfos(depth)) {
    info.rearmVertex(vertex, _trx, _tmpVar);
  }
}

template<class Step>
void RefactoredSingleServerEdgeCursor<Step>::readAll(
    SingleServerProvider<Step>& provider, aql::TraversalStats& stats,
    size_t depth, Callback const& callback) {
  TRI_ASSERT(!getLookupInfos(depth).empty());
  VPackBuilder tmpBuilder;

  auto evaluateEdgeExpressionHelper = [&](aql::Expression* expression,
                                          EdgeDocumentToken edgeToken,
                                          VPackSlice edge) {
    if (edge.isString()) {
      tmpBuilder.clear();
      provider.insertEdgeIntoResult(edgeToken, tmpBuilder);
      edge = tmpBuilder.slice();
    }
    return evaluateEdgeExpression(expression, edge);
  };

  for (auto& lookupInfo : getLookupInfos(depth)) {
    auto cursorID = lookupInfo.getCursorID();
    // we can only have a cursorID that is within the amount of collections in
    // use.
    TRI_ASSERT(cursorID < _lookupInfo.size());

    auto& cursor = lookupInfo.cursor();
    LogicalCollection* collection = cursor.collection();
    auto cid = collection->id();
    bool hasExtra = !_requiresFullDocument && cursor.hasExtra();
    auto* expression = lookupInfo.getExpression();

    if (hasExtra) {
      cursor.allExtra([&](LocalDocumentId const& token, VPackSlice edge) {
        stats.addScannedIndex(1);
#ifdef USE_ENTERPRISE
        if (_trx->skipInaccessible() && CheckInaccessible(_trx, edge)) {
          return false;
        }
#endif

        EdgeDocumentToken edgeToken(cid, token);
        // evaluate expression if available
        if (expression != nullptr &&
            !evaluateEdgeExpressionHelper(expression, edgeToken, edge)) {
          stats.incrFiltered();
          return false;
        }

        callback(std::move(edgeToken), edge, cursorID);
        return true;
      });
    } else {
      cursor.all([&](LocalDocumentId const& token) {
        return collection->getPhysical()
            ->read(
                _trx, token,
                [&](LocalDocumentId const&, VPackSlice edgeDoc) {
                  stats.addScannedIndex(1);
#ifdef USE_ENTERPRISE
                  if (_trx->skipInaccessible()) {
                    // TODO: we only need to check one of these
                    VPackSlice from =
                        transaction::helpers::extractFromFromDocument(edgeDoc);
                    VPackSlice to =
                        transaction::helpers::extractToFromDocument(edgeDoc);
                    if (CheckInaccessible(_trx, from) ||
                        CheckInaccessible(_trx, to)) {
                      return false;
                    }
                  }
#endif
                  // eval depth-based expression first if available
                  EdgeDocumentToken edgeToken(cid, token);

                  // evaluate expression if available
                  if (expression != nullptr &&
                      !evaluateEdgeExpressionHelper(expression, edgeToken,
                                                    edgeDoc)) {
                    stats.incrFiltered();
                    return false;
                  }

                  callback(std::move(edgeToken), edgeDoc, cursorID);
                  return true;
                },
                ReadOwnWrites::no)
            .ok();
      });
    }
  }
}

template<class Step>
bool RefactoredSingleServerEdgeCursor<Step>::evaluateEdgeExpression(
    arangodb::aql::Expression* expression, VPackSlice value) {
  if (expression == nullptr) {
    return true;
  }

  TRI_ASSERT(value.isObject() || value.isNull());

  aql::AqlValue edgeVal(aql::AqlValueHintDocumentNoCopy(value.begin()));
  _expressionCtx.setVariableValue(_tmpVar, edgeVal);
  ScopeGuard defer([&]() noexcept {
    try {
      _expressionCtx.clearVariableValue(_tmpVar);
    } catch (...) {
      // This method could in theory throw, if the
      // _tmpVar is not in the list. However this is
      // guaranteed by this code. If it would throw
      // with not found, nothing bad has happened
    }
  });

  bool mustDestroy = false;
  aql::AqlValue res = expression->execute(&_expressionCtx, mustDestroy);
  aql::AqlValueGuard guard(res, mustDestroy);
  TRI_ASSERT(res.isBoolean());

  return res.toBoolean();
}

template<class Step>
auto RefactoredSingleServerEdgeCursor<Step>::getLookupInfos(uint64_t depth)
    -> std::vector<LookupInfo>& {
  auto const& depthInfo = _depthLookupInfo.find(depth);
  if (depthInfo == _depthLookupInfo.end()) {
    return _lookupInfo;
  }
  return depthInfo->second;
}

template<class Step>
void RefactoredSingleServerEdgeCursor<Step>::prepareIndexExpressions(
    aql::Ast* ast) {
  for (auto& it : _lookupInfo) {
    it.calculateIndexExpressions(ast, _expressionCtx);
  }

  for (auto& [unused, infos] : _depthLookupInfo) {
    for (auto& info : infos) {
      info.calculateIndexExpressions(ast, _expressionCtx);
    }
  }
}

template class arangodb::graph::RefactoredSingleServerEdgeCursor<
    SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template class arangodb::graph::RefactoredSingleServerEdgeCursor<
    enterprise::SmartGraphStep>;
#endif
