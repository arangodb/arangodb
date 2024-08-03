////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Aql/AttributeNamePath.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/NonConstExpression.h"
#include "Aql/Projections.h"
#include "Basics/StaticStrings.h"
#include "Graph/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
// TODO: Needed for the IndexAccessor, should be modified
#include "Graph/Providers/SingleServerProvider.h"
#include "Graph/Steps/SingleServerProviderStep.h"
#include "Indexes/IndexIterator.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#endif

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace {
#ifdef USE_ENTERPRISE
static bool CheckInaccessible(transaction::Methods* trx, VPackSlice edge) {
  // for skipInaccessibleCollections we need to check the edge
  // document, in that case nextWithExtra has no benefit
  TRI_ASSERT(edge.isString());
  std::string_view str(edge.stringView());
  size_t pos = str.find('/');
  TRI_ASSERT(pos != std::string_view::npos);
  return trx->isInaccessibleCollection(str.substr(0, pos));
}
#endif
}  // namespace

template<class Step>
RefactoredSingleServerEdgeCursor<Step>::LookupInfo::LookupInfo(
    IndexAccessor* accessor)
    : _accessor(accessor),
      _cursor(nullptr),
      _coveringIndexPosition(aql::Projections::kNoCoveringIndexPosition) {}

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
uint16_t RefactoredSingleServerEdgeCursor<
    Step>::LookupInfo::coveringIndexPosition() const noexcept {
  return _coveringIndexPosition;
}

template<class Step>
void RefactoredSingleServerEdgeCursor<Step>::LookupInfo::rearmVertex(
    VertexType vertex, ResourceMonitor& monitor, transaction::Methods* trx,
    arangodb::aql::Variable const* tmpVar, aql::TraversalStats& stats,
    bool useCache) {
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

  IndexIteratorOptions indexIteratorOptions;
  // forward "useCache" traversal option to actual index iterator
  indexIteratorOptions.useCache = useCache;

  // We need to reset the cursor

  // check if the underlying index iterator supports rearming
  if (_cursor != nullptr && _cursor->canRearm()) {
    // rearming supported
    stats.incrCursorsRearmed();

    if (!_cursor->rearm(node, tmpVar, indexIteratorOptions)) {
      _cursor =
          std::make_unique<EmptyIndexIterator>(_cursor->collection(), trx);
    }
  } else {
    // rearming not supported - we need to throw away the index iterator
    // and create a new one
    stats.incrCursorsCreated();
    auto index = _accessor->indexHandle();

    _cursor = trx->indexScanForCondition(
        monitor, index, node, tmpVar, indexIteratorOptions, ReadOwnWrites::no,
        static_cast<int>(_accessor->getMemberToUpdate().has_value()
                             ? _accessor->getMemberToUpdate().value()
                             : transaction::Methods::kNoMutableConditionIdx));

    uint16_t coveringPosition = aql::Projections::kNoCoveringIndexPosition;

    // projections we want to cover
    std::vector<aql::AttributeNamePath> paths = {};
    paths.emplace_back(
        aql::AttributeNamePath({StaticStrings::FromString}, monitor));
    paths.emplace_back(
        aql::AttributeNamePath({StaticStrings::ToString}, monitor));
    aql::Projections edgeProjections(std::move(paths));

    if (index->covers(edgeProjections)) {
      // find opposite attribute
      edgeProjections.setCoveringContext(index->collection().id(), index);

      TRI_edge_direction_e dir = _accessor->direction();
      TRI_ASSERT(dir == TRI_EDGE_IN || dir == TRI_EDGE_OUT);

      if (dir == TRI_EDGE_OUT) {
        coveringPosition = edgeProjections.coveringIndexPosition(
            aql::AttributeNamePath::Type::ToAttribute);
      } else {
        coveringPosition = edgeProjections.coveringIndexPosition(
            aql::AttributeNamePath::Type::FromAttribute);
      }

      TRI_ASSERT(aql::Projections::isCoveringIndexPosition(coveringPosition));
    }

    _coveringIndexPosition = coveringPosition;
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
    ResourceMonitor& monitor, transaction::Methods* trx,
    arangodb::aql::Variable const* tmpVar,
    std::vector<IndexAccessor>& globalIndexConditions,
    std::unordered_map<uint64_t, std::vector<IndexAccessor>>&
        depthBasedIndexConditions,
    arangodb::aql::FixedVarExpressionContext& expressionContext,
    bool requiresFullDocument, bool useCache)
    : _tmpVar(tmpVar),
      _monitor(monitor),
      _trx(trx),
      _expressionCtx(expressionContext),
      _requiresFullDocument(requiresFullDocument),
      _useCache(useCache) {
  // We need at least one indexCondition, otherwise nothing to serve
  TRI_ASSERT(!globalIndexConditions.empty());
  if (globalIndexConditions.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "index conditions in SingleServerEdgeCursor should not be empty");
  }
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
    TRI_ASSERT(exp != nullptr);
    bool mustDestroy;
    AqlValue a = exp->execute(&ctx, mustDestroy);
    AqlValueGuard guard(a, mustDestroy);

    AqlValueMaterializer materializer(&(ctx.trx().vpackOptions()));
    VPackSlice slice = materializer.slice(a);
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
RefactoredSingleServerEdgeCursor<Step>::~RefactoredSingleServerEdgeCursor() =
    default;

template<class Step>
void RefactoredSingleServerEdgeCursor<Step>::rearm(VertexType vertex,
                                                   uint64_t depth,
                                                   aql::TraversalStats& stats) {
  for (auto& info : getLookupInfos(depth)) {
    info.rearmVertex(vertex, _monitor, _trx, _tmpVar, stats, _useCache);
  }
}

template<class Step>
void RefactoredSingleServerEdgeCursor<Step>::readAll(
    SingleServerProvider<Step>& provider, aql::TraversalStats& stats,
    size_t depth, Callback const& callback) {
  TRI_ASSERT(!getLookupInfos(depth).empty());
  transaction::BuilderLeaser tmpBuilder(_trx);

  auto evaluateEdgeExpressionHelper = [&](aql::Expression* expression,
                                          EdgeDocumentToken edgeToken,
                                          VPackSlice edge) {
    if (edge.isString()) {
      tmpBuilder->clear();
      provider.insertEdgeIntoResult(edgeToken, *tmpBuilder);
      edge = tmpBuilder->slice();
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
    auto* expression = lookupInfo.getExpression();
    uint16_t coveringPosition = lookupInfo.coveringIndexPosition();

    if (!_requiresFullDocument &&
        aql::Projections::isCoveringIndexPosition(coveringPosition)) {
      // use covering index and projections
      cursor.allCovering(
          [&](LocalDocumentId token, IndexIteratorCoveringData& covering) {
            stats.incrScannedIndex(1);

            TRI_ASSERT(covering.isArray());
            VPackSlice edge = covering.at(coveringPosition);
            TRI_ASSERT(edge.isString());

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
      // fetch full documents
      auto cb = [&](LocalDocumentId token, aql::DocumentData&&,
                    VPackSlice edgeDoc) {
        stats.incrScannedIndex(1);
#ifdef USE_ENTERPRISE
        if (_trx->skipInaccessible()) {
          // TODO: we only need to check one of these
          VPackSlice from =
              transaction::helpers::extractFromFromDocument(edgeDoc);
          VPackSlice to = transaction::helpers::extractToFromDocument(edgeDoc);
          if (CheckInaccessible(_trx, from) || CheckInaccessible(_trx, to)) {
            return false;
          }
        }
#endif
        // eval depth-based expression first if available
        EdgeDocumentToken edgeToken(cid, token);

        // evaluate expression if available
        if (expression != nullptr &&
            !evaluateEdgeExpressionHelper(expression, edgeToken, edgeDoc)) {
          stats.incrFiltered();
          return false;
        }

        callback(std::move(edgeToken), edgeDoc, cursorID);
        return true;
      };
      cursor.all([&](LocalDocumentId token) {
        return collection->getPhysical()
            ->lookup(_trx, token, cb, {.countBytes = true})
            .ok();
      });
    }

    // update cache hits and misses
    auto [ch, cm] = cursor.getAndResetCacheStats();
    stats.incrCacheHits(ch);
    stats.incrCacheMisses(cm);
  }
}

template<class Step>
bool RefactoredSingleServerEdgeCursor<Step>::evaluateEdgeExpression(
    arangodb::aql::Expression* expression, VPackSlice value) {
  if (expression == nullptr) {
    return true;
  }

  TRI_ASSERT(value.isObject() || value.isNull());

  // register temporary variables in expression context
  _expressionCtx.setVariableValue(
      _tmpVar, aql::AqlValue{aql::AqlValueHintSliceNoCopy{value}});
  ScopeGuard defer(
      [&]() noexcept { _expressionCtx.clearVariableValue(_tmpVar); });

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

template<class StepType>
bool RefactoredSingleServerEdgeCursor<StepType>::hasDepthSpecificLookup(
    uint64_t depth) const noexcept {
  return _depthLookupInfo.find(depth) != _depthLookupInfo.end();
}

template class arangodb::graph::RefactoredSingleServerEdgeCursor<
    SingleServerProviderStep>;

#ifdef USE_ENTERPRISE
template class arangodb::graph::RefactoredSingleServerEdgeCursor<
    enterprise::SmartGraphStep>;
#endif
