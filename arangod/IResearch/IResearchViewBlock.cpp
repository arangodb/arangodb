////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "AqlHelper.h"
#include "IResearchCommon.h"
#include "IResearchDocument.h"
#include "IResearchFilterFactory.h"
#include "IResearchOrderFactory.h"
#include "IResearchView.h"
#include "IResearchViewBlock.h"
#include "IResearchViewNode.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/TransactionState.h"
#include "StorageEngine/TransactionCollection.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include "search/boolean_filter.hpp"
#include "search/score.hpp"

NS_LOCAL

inline arangodb::aql::RegisterId getRegister(
    arangodb::aql::Variable const& var,
    arangodb::aql::ExecutionNode const& node
) noexcept {
  auto const& vars = node.getRegisterPlan()->varInfo;
  auto const it = vars.find(var.id);

  return vars.end() == it
    ? arangodb::aql::ExecutionNode::MaxRegisterId
    : it->second.registerId;
  }

NS_END // NS_LOCAL

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

using namespace arangodb::aql;

// -----------------------------------------------------------------------------
// --SECTION--                              ViewExpressionContext implementation
// -----------------------------------------------------------------------------

size_t ViewExpressionContext::numRegisters() const {
  return _data->getNrRegs();
}

AqlValue ViewExpressionContext::getVariableValue(
    Variable const* var, bool doCopy, bool& mustDestroy
) const {
  TRI_ASSERT(var);

  if (var == &_node->outVariable()) {
    // self-reference
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  mustDestroy = false;
  auto const reg = getRegister(*var, *_node);

  if (reg == arangodb::aql::ExecutionNode::MaxRegisterId) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  auto& value = _data->getValueReference(_pos, reg);

  if (doCopy) {
    mustDestroy = true;
    return value.clone();
  }

  return value;
}

// -----------------------------------------------------------------------------
// --SECTION--                             IResearchViewBlockBase implementation
// -----------------------------------------------------------------------------

IResearchViewBlockBase::IResearchViewBlockBase(
    PrimaryKeyIndexReader const& reader,
    ExecutionEngine& engine,
    IResearchViewNode const& en)
  : ExecutionBlock(&engine, &en),
    _filterCtx(1), // arangodb::iresearch::ExpressionExecutionContext
    _ctx(en),
    _reader(reader),
    _filter(irs::filter::prepared::empty()),
    _execCtx(*_trx, _ctx),
    _inflight(0),
    _hasMore(true), // has more data initially
    _volatileSort(true),
    _volatileFilter(true) {
  TRI_ASSERT(_trx);

  // add expression execution context
  _filterCtx.emplace(_execCtx);
}

std::pair<ExecutionState, Result> IResearchViewBlockBase::initializeCursor(
    AqlItemBlock* items, size_t pos) {
  const auto res = ExecutionBlock::initializeCursor(items, pos);

  if (res.first == ExecutionState::WAITING ||
      !res.second.ok()) {
    // If we need to wait or get an error we return as is.
    return res;
  }

  _hasMore = true; // has more data initially
  _inflight = 0;

  return res;
}

void IResearchViewBlockBase::reset() {
  TRI_ASSERT(!_buffer.empty());

  // setup expression context
  _ctx._data = _buffer.front();
  _ctx._pos = _pos;

  auto& viewNode = *ExecutionNode::castTo<IResearchViewNode const*>(getPlanNode());
  auto* plan = const_cast<ExecutionPlan*>(viewNode.plan());

  arangodb::iresearch::QueryContext const queryCtx = {
    _trx, plan, plan->getAst(), &_ctx, &viewNode.outVariable()
  };

  if (_volatileFilter) { // `_volatileSort` implies `_volatileFilter`
    irs::Or root;

    if (!arangodb::iresearch::FilterFactory::filter(&root, queryCtx, viewNode.filterCondition())) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failed to build filter while querying iResearch view , query '"
          << viewNode.filterCondition().toVelocyPack(true)->toJson() << "'";

      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }

    if (_volatileSort) {
      irs::order order;
      irs::sort::ptr scorer;

      for (auto const& sort : viewNode.sortCondition()) {
        TRI_ASSERT(sort.node);

        if (!arangodb::iresearch::OrderFactory::scorer(&scorer, *sort.node, queryCtx)) {
          // failed to append sort
          THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
        }

        order.add(sort.asc, std::move(scorer));
      }

      // compile order
      _order = order.prepare();
    }

    // compile filter
    _filter = root.prepare(_reader, _order, irs::boost::no_boost(), _filterCtx);

    auto const& volatility = viewNode.volatility();
    _volatileSort = volatility.second;
    _volatileFilter = _volatileSort || volatility.first;
  }
}

bool IResearchViewBlockBase::readDocument(
    size_t subReaderId,
    irs::doc_id_t const docId,
    IndexIterator::DocumentCallback const& callback
) {
  const auto& pkValues = _reader.pkColumn(subReaderId);
  arangodb::iresearch::DocumentPrimaryKey docPk;
  irs::bytes_ref tmpRef;

  if (!pkValues(docId, tmpRef) || !docPk.read(tmpRef)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to read document primary key while reading document from iResearch view, doc_id '" << docId << "'";

    return false; // not a valid document reference
  }

  TRI_ASSERT(_trx->state());

  // `Methods::documentCollection(TRI_voc_cid_t)` may throw exception
  auto* collection = _trx->state()->collection(docPk.cid(), arangodb::AccessMode::Type::READ);

  if (!collection) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to find collection while reading document from iResearch view, cid '" << docPk.cid()
      << "', rid '" << docPk.rid() << "'";

    return false; // not a valid collection reference
  }

  TRI_ASSERT(collection->collection());

  return collection->collection()->readDocumentWithCallback(
    _trx, arangodb::LocalDocumentId(docPk.rid()), callback
  );
}

std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>>
IResearchViewBlockBase::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);

  if (_done) {
    traceGetSomeEnd(nullptr, ExecutionState::DONE);
    TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
    return {ExecutionState::DONE, nullptr};
  }

  bool needMore;
  AqlItemBlock* cur = nullptr;

  ReadContext ctx(getNrInputRegisters());

  RegisterId const nrOutRegs = getNrOutputRegisters();

  do {
    do {
      needMore = false;

      if (_buffer.empty()) {
        size_t const toFetch = (std::min)(DefaultBatchSize(), atMost);
        auto upstreamRes = ExecutionBlock::getBlock(toFetch);
        if (upstreamRes.first == ExecutionState::WAITING) {
          traceGetSomeEnd(nullptr, ExecutionState::WAITING);
          return {upstreamRes.first, nullptr};
        }
        _upstreamState = upstreamRes.first;
        if (!upstreamRes.second) {
          _done = true;
          traceGetSomeEnd(nullptr, ExecutionState::DONE);
          return {ExecutionState::DONE, nullptr};
        }
        _pos = 0;  // this is in the first block
        reset();
      }

      // If we get here, we do have _buffer.front()
      cur = _buffer.front();

      if (!_hasMore) {
        needMore = true;
        _hasMore = true;

        if (++_pos >= cur->size()) {
          _buffer.pop_front();  // does not throw
          returnBlock(cur);
          _pos = 0;
        } else {
          // we have exhausted this cursor
          // re-initialize fetching of documents
          reset();
        }
      }
    } while (needMore);

    TRI_ASSERT(cur);
    TRI_ASSERT(ctx.curRegs == cur->getNrRegs());

    ctx.res.reset(requestBlock(atMost, nrOutRegs));
    // automatically freed if we throw
    TRI_ASSERT(ctx.curRegs <= ctx.res->getNrRegs());

    // only copy 1st row of registers inherited from previous frame(s)
    inheritRegisters(cur, ctx.res.get(), _pos);

    throwIfKilled();  // check if we were aborted

    TRI_IF_FAILURE("EnumerateViewBlock::moreDocuments") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    _hasMore = next(ctx, atMost);

    // If the collection is actually empty we cannot forward an empty block
  } while (ctx.pos == 0);

  TRI_ASSERT(ctx.res);

  // aggregate stats
   _engine->_stats.scannedIndex += static_cast<int64_t>(ctx.pos);

  if (ctx.pos < atMost) {
    // The collection did not have enough results
    ctx.res->shrink(ctx.pos);
  }

  // Clear out registers no longer needed later:
  clearRegisters(ctx.res.get());

  traceGetSomeEnd(ctx.res.get(), getHasMoreState());
  return {getHasMoreState(), std::move(ctx.res)};
}

std::pair<ExecutionState, size_t> IResearchViewBlockBase::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);
  if (_done) {
    size_t skipped = _inflight;
    _inflight = 0;
    traceSkipSomeEnd(skipped, ExecutionState::DONE);
    return {ExecutionState::DONE, skipped};
  }

  while (_inflight < atMost) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      auto upstreamRes = getBlock(toFetch);
      if (upstreamRes.first == ExecutionState::WAITING) {
        traceSkipSomeEnd(0, upstreamRes.first);
        return {upstreamRes.first, 0};
      }
      _upstreamState = upstreamRes.first;
      if (!upstreamRes.second) {
        _done = true;
        size_t skipped = _inflight;
        _inflight = 0;
        traceSkipSomeEnd(skipped, ExecutionState::DONE);
        return {ExecutionState::DONE, skipped};
      }
      _pos = 0;  // this is in the first block
      reset();
    }

    // if we get here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();

    _inflight += skip(atMost - _inflight);

    if (_inflight < atMost) {
      // not skipped enough re-initialize fetching of documents
      if (++_pos >= cur->size()) {
        _buffer.pop_front();  // does not throw
        returnBlock(cur);
        _pos = 0;
      } else {
        // we have exhausted this cursor
        // re-initialize fetching of documents
        reset();
      }
    }
  }

  // aggregate stats
  _engine->_stats.scannedIndex += static_cast<int64_t>(_inflight);

  // We skipped atLeast documents

  size_t skipped = _inflight;
  _inflight = 0;
  ExecutionState state = getHasMoreState();
  traceSkipSomeEnd(skipped, state);
  return {state, skipped};
}

// -----------------------------------------------------------------------------
// --SECTION--                                 IResearchViewBlock implementation
// -----------------------------------------------------------------------------

IResearchViewBlock::IResearchViewBlock(
    PrimaryKeyIndexReader const& reader,
    aql::ExecutionEngine& engine,
    IResearchViewNode const& node
): IResearchViewUnorderedBlock(reader, engine, node),
    _scr(&irs::score::no_score()) {
  _volatileSort = true;
}

void IResearchViewBlock::resetIterator() {
  auto& segmentReader = _reader[_readerOffset];

  _itr = segmentReader.mask(_filter->execute(
    segmentReader, _order, _filterCtx
  ));

  _scr = _itr->attributes().get<irs::score>().get();

  if (_scr) {
    _scrVal = _scr->value();
  } else {
    _scr = &irs::score::no_score();
    _scrVal = irs::bytes_ref::NIL;
  }
}

bool IResearchViewBlock::next(
    ReadContext& ctx,
    size_t limit) {
  TRI_ASSERT(_filter);
  auto const& viewNode = *ExecutionNode::castTo<IResearchViewNode const*>(getPlanNode());
  auto const numSorts = viewNode.sortCondition().size();

  // capture only one reference
  // to potentially avoid heap allocation
  IndexIterator::DocumentCallback const copyDocument = [&ctx] (
      LocalDocumentId /*id*/, VPackSlice doc
  ) {
    ctx.res->setValue(ctx.pos, ctx.curRegs, AqlValue(doc));
  };

  for (size_t count = _reader.size(); _readerOffset < count; ) {
    bool done = false;

    if (!_itr) {
      resetIterator();
    }

    while (limit && _itr->next()) {
      if (readDocument(_readerOffset, _itr->value(), copyDocument)) {
        // The result is in the first variable of this depth,
        // we do not need to do a lookup in
        // getPlanNode()->_registerPlan->varInfo,
        // but can just take cur->getNrRegs() as registerId:

        // evaluate scores
        TRI_ASSERT(!viewNode.sortCondition().empty());
        _scr->evaluate();

        // copy scores, registerId's are sequential
        auto scoreRegs = ctx.curRegs;

        for (size_t i = 0; i < numSorts; ++i) {
          ctx.res->setValue(
            ctx.pos,
            ++scoreRegs,
            _order.to_string<AqlValue, std::char_traits<char>>(_scrVal.c_str(), i)
          );
        }
      }

      // FIXME why?
      if (ctx.pos > 0) {
        // re-use already copied AQLValues
        ctx.res->copyValuesFromFirstRow(ctx.pos, static_cast<RegisterId>(ctx.curRegs));
      }
      ++ctx.pos;

      done = (0 == --limit);
    }

    if (done) {
      break; // do not change iterator if already reached limit
    }

    ++_readerOffset;
    _itr.reset();
  }

  // FIXME will still return 'true' if reached end of last iterator
  return (limit == 0);
}

size_t IResearchViewBlock::skip(size_t limit) {
  TRI_ASSERT(_filter);
  size_t skipped{};

  for (size_t count = _reader.size(); _readerOffset < count;) {
    bool done = false;

    if (!_itr) {
      resetIterator();
    }

    while (limit && _itr->next()) {
      ++skipped;
      done = (0 == --limit);
    }

    if (done) {
      break; // do not change iterator if already reached limit
    }

    ++_readerOffset;
    _itr.reset();
  }

  return skipped;
}

// -----------------------------------------------------------------------------
// --SECTION--                        IResearchViewUnorderedBlock implementation
// -----------------------------------------------------------------------------

IResearchViewUnorderedBlock::IResearchViewUnorderedBlock(
    PrimaryKeyIndexReader const& reader,
    aql::ExecutionEngine& engine,
    IResearchViewNode const& node
): IResearchViewBlockBase(reader, engine, node), _readerOffset(0) {
  _volatileSort = false; // do not evaluate sort
}

bool IResearchViewUnorderedBlock::next(
    ReadContext& ctx,
    size_t limit) {
  TRI_ASSERT(_filter);

  // capture only one reference
  // to potentially avoid heap allocation
  IndexIterator::DocumentCallback const copyDocument = [&ctx] (
      LocalDocumentId /*id*/, VPackSlice doc
  ) {
    ctx.res->setValue(ctx.pos, ctx.curRegs, AqlValue(doc));
  };

  for (size_t count = _reader.size(); _readerOffset < count; ) {
    bool done = false;

    if (!_itr) {
      auto& segmentReader = _reader[_readerOffset];

      _itr = segmentReader.mask(_filter->execute(
        segmentReader, _order, _filterCtx
      ));
    }

    while (limit && _itr->next()) {
      readDocument(_readerOffset, _itr->value(), copyDocument);
      // The result is in the first variable of this depth,
      // we do not need to do a lookup in
      // getPlanNode()->_registerPlan->varInfo,
      // but can just take cur->getNrRegs() as registerId:

      // FIXME why?
      if (ctx.pos > 0) {
        // re-use already copied AQLValues
        ctx.res->copyValuesFromFirstRow(ctx.pos, ctx.curRegs);
      }
      ++ctx.pos;

      done = (0 == --limit);
    }

    if (done) {
      break; // do not change iterator if already reached limit
    }

    ++_readerOffset;
    _itr.reset();
  }

  // FIXME will still return 'true' if reached end of last iterator
  return (limit == 0);
}

size_t IResearchViewUnorderedBlock::skip(size_t limit) {
  TRI_ASSERT(_filter);
  size_t skipped{};

  for (size_t count = _reader.size(); _readerOffset < count;) {
    bool done = false;

    if (!_itr) {
      auto& segmentReader = _reader[_readerOffset];

      _itr = segmentReader.mask(_filter->execute(
        segmentReader, _order, _filterCtx
      ));
    }

    while (limit && _itr->next()) {
      ++skipped;
      done = (0 == --limit);
    }

    if (done) {
      break; // do not change iterator if already reached limit
    }

    ++_readerOffset;
    _itr.reset();
  }

  return skipped;
}

// -----------------------------------------------------------------------------
// --SECTION--                          IResearchViewOrderedBlock implementation
// -----------------------------------------------------------------------------

IResearchViewOrderedBlock::IResearchViewOrderedBlock(
    PrimaryKeyIndexReader const& reader,
    aql::ExecutionEngine& engine,
    IResearchViewNode const& node
): IResearchViewBlockBase(reader, engine, node) {
}

bool IResearchViewOrderedBlock::next(
    ReadContext& ctx,
    size_t limit
) {
  TRI_ASSERT(_filter);

  auto scoreLess = [this](irs::bstring const& lhs, irs::bstring const& rhs) {
    return _order.less(lhs.c_str(), rhs.c_str());
  };

  // FIXME use irs::memory_pool
  typedef std::pair<size_t, irs::doc_id_t> DocumentToken;

  std::multimap<irs::bstring, DocumentToken, decltype(scoreLess)> orderedDocTokens(scoreLess);
  auto const maxDocCount = _skip + limit;

  for (size_t i = 0, count = _reader.size(); i < count; ++i) {
    auto& segmentReader = _reader[i];
    auto itr = segmentReader.mask(_filter->execute(segmentReader, _order, _filterCtx));
    irs::score const* score = itr->attributes().get<irs::score>().get();

    if (!score) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "failed to retrieve document score attribute while iterating iResearch view, ignoring: reader_id '" << i << "'";
      IR_LOG_STACK_TRACE();

      continue; // if here then there is probably a bug in IResearchView while querying
    }

#if defined(__GNUC__) && !defined(_GLIBCXX_USE_CXX11_ABI)
    // workaround for std::basic_string's COW with old compilers
    const irs::bytes_ref scoreValue = score->value();
#else
    const auto& scoreValue = score->value();
#endif

    while (itr->next()) {
      score->evaluate(); // compute a score for the current document

      orderedDocTokens.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(scoreValue),
        std::forward_as_tuple(i, itr->value())
      );

      if (orderedDocTokens.size() > maxDocCount) {
        orderedDocTokens.erase(--(orderedDocTokens.end())); // remove element with the least score
      }
    }
  }

  auto tokenItr = orderedDocTokens.begin();
  auto tokenEnd = orderedDocTokens.end();

  // skip documents previously returned
  for (size_t i = _skip; i; --i, ++tokenItr) {
    if (tokenItr == tokenEnd) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "document count less than the document count during the previous iteration on the same query while iterating iResearch view'";

      break; // if here then there is probably a bug in the iResearch library
    }
  }

  // capture only one reference
  // to potentially avoid heap allocation
  IndexIterator::DocumentCallback const copyDocument = [&ctx] (
      LocalDocumentId /*id*/, VPackSlice doc
  ) {
    ctx.res->setValue(ctx.pos, ctx.curRegs, AqlValue(doc));
  };

  // iterate through documents
  while (limit && tokenItr != tokenEnd) {
    auto& token = tokenItr->second;

    readDocument(token.first, token.second, copyDocument);
    // The result is in the first variable of this depth,
    // we do not need to do a lookup in
    // getPlanNode()->_registerPlan->varInfo,
    // but can just take cur->getNrRegs() as registerId:

    // FIXME why?
    if (ctx.pos > 0) {
      // re-use already copied AQLValues
      ctx.res->copyValuesFromFirstRow(ctx.pos, ctx.curRegs);
    }
    ++ctx.pos;

    ++tokenItr;
    ++_skip;
    --limit;
  }

  return (limit == 0); // exceeded limit
}

size_t IResearchViewOrderedBlock::skip(size_t limit) {
  TRI_ASSERT(_filter);

  size_t skipped = 0;
  auto skip = _skip;

  for (size_t i = 0, readerCount = _reader.size(); i < readerCount; ++i) {
    auto& segmentReader = _reader[i];

    auto itr = segmentReader.mask(_filter->execute(
      segmentReader, irs::order::prepared::unordered(), _filterCtx
    ));

    while (skip && itr->next()) {
      --skip;
    }

    while (limit > skipped && itr->next()) {
      ++skipped;
    }
  }

  _skip += skipped;

  return skipped;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
