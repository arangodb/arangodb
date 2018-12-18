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
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "StorageEngine/TransactionCollection.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include "search/boolean_filter.hpp"
#include "search/score.hpp"

namespace {

typedef std::vector<arangodb::iresearch::DocumentPrimaryKey::type> pks_t;

pks_t::iterator readPKs(
    irs::doc_iterator& it,
    irs::columnstore_reader::values_reader_f const& values,
    pks_t& keys,
    size_t limit
) {
  keys.clear();
  keys.resize(limit);

  auto begin = keys.begin();
  auto end = keys.end();

  for (irs::bytes_ref key; begin != end && it.next(); ) {
    if (values(it.value(), key) && arangodb::iresearch::DocumentPrimaryKey::read(*begin, key)) {
      ++begin;
    }
  }

  return begin;
}

inline irs::columnstore_reader::values_reader_f pkColumn(
    irs::sub_reader const& segment
) {
  auto const* reader = segment.column_reader(
    arangodb::iresearch::DocumentPrimaryKey::PK()
  );

  return reader
    ? reader->values()
    : irs::columnstore_reader::values_reader_f{};
}

}

namespace arangodb {
namespace iresearch {

using namespace arangodb::aql;

// -----------------------------------------------------------------------------
// --SECTION--                             IResearchViewBlockBase implementation
// -----------------------------------------------------------------------------

/*static*/ IndexIterator::DocumentCallback IResearchViewBlockBase::ReadContext::copyDocumentCallback(
    IResearchViewBlockBase::ReadContext& ctx
) {
  auto* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  typedef std::function<
    IndexIterator::DocumentCallback(IResearchViewBlockBase::ReadContext&)
  > CallbackFactory;

  static CallbackFactory const callbackFactories[] {
    [](ReadContext& ctx) {
      // capture only one reference to potentially avoid heap allocation
      return [&ctx] (LocalDocumentId /*id*/, VPackSlice doc) {
        ctx.res->emplaceValue(ctx.pos, ctx.curRegs, AqlValueHintCopy(doc.begin()));
      };
    },

    [](ReadContext& ctx) {
      // capture only one reference to potentially avoid heap allocation
      return [&ctx] (LocalDocumentId /*id*/, VPackSlice doc) {
        ctx.res->emplaceValue(ctx.pos, ctx.curRegs, AqlValueHintDocumentNoCopy(doc.begin()));
      };
    }
  };

  return callbackFactories[size_t(engine->useRawDocumentPointers())](ctx);
}

IResearchViewBlockBase::IResearchViewBlockBase(
    irs::index_reader const& reader,
    ExecutionEngine& engine,
    IResearchViewNode const& en)
  : ExecutionBlock(&engine, &en),
    _filterCtx(1), // arangodb::iresearch::ExpressionExecutionContext
    _ctx(engine.getQuery(), en),
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
          << "failed to build filter while querying arangosearch view , query '"
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
    DocumentPrimaryKey::type const& docPk,
    IndexIterator::DocumentCallback const& callback
) {
  TRI_ASSERT(_trx->state());

  // this is necessary for MMFiles
  _trx->pinData(docPk.first);

  // `Methods::documentCollection(TRI_voc_cid_t)` may throw exception
  auto* collection = _trx->state()->collection(docPk.first, arangodb::AccessMode::Type::READ);

  if (!collection) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to find collection while reading document from arangosearch view, cid '" << docPk.first
      << "', rid '" << docPk.second << "'";

    return false; // not a valid collection reference
  }

  TRI_ASSERT(collection->collection());

  return collection->collection()->readDocumentWithCallback(
    _trx, arangodb::LocalDocumentId(docPk.second), callback
  );
}

bool IResearchViewBlockBase::readDocument(
    irs::doc_id_t const docId,
    irs::columnstore_reader::values_reader_f const& pkValues,
    IndexIterator::DocumentCallback const& callback
) {
  TRI_ASSERT(pkValues);

  arangodb::iresearch::DocumentPrimaryKey::type docPk;
  irs::bytes_ref tmpRef;

  if (!pkValues(docId, tmpRef) || !arangodb::iresearch::DocumentPrimaryKey::read(docPk, tmpRef)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to read document primary key while reading document from arangosearch view, doc_id '" << docId << "'";

    return false; // not a valid document reference
  }

  TRI_ASSERT(_trx->state());

  // this is necessary for MMFiles
  _trx->pinData(docPk.first);

  // `Methods::documentCollection(TRI_voc_cid_t)` may throw exception
  auto* collection = _trx->state()->collection(docPk.first, arangodb::AccessMode::Type::READ);

  if (!collection) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to find collection while reading document from arangosearch view, cid '" << docPk.first
      << "', rid '" << docPk.second << "'";

    return false; // not a valid collection reference
  }

  TRI_ASSERT(collection->collection());

  return collection->collection()->readDocumentWithCallback(
    _trx, arangodb::LocalDocumentId(docPk.second), callback
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

  ReadContext ctx(getNrInputRegisters());
  RegisterId const nrOutRegs = getNrOutputRegisters();

  do {
    if (_buffer.empty()) {
      size_t const toFetch = (std::min)(DefaultBatchSize(), atMost);

      switch (getBlockIfNeeded(toFetch)) {
       case BufferState::NO_MORE_BLOCKS:
        TRI_ASSERT(_inflight == 0);
        _done = true;
        TRI_ASSERT(getHasMoreState() == ExecutionState::DONE);
        traceGetSomeEnd(nullptr, ExecutionState::DONE);
        return {ExecutionState::DONE, nullptr};
       case BufferState::WAITING:
        traceGetSomeEnd(nullptr, ExecutionState::WAITING);
        return {ExecutionState::WAITING, nullptr};
       default:
        reset();
      }
    }

    // If we get here, we do have _buffer.front()
    auto* cur = _buffer.front();

    TRI_ASSERT(cur);
    TRI_ASSERT(ctx.curRegs == cur->getNrRegs());

    ctx.res.reset(requestBlock(atMost, nrOutRegs));
    // automatically freed if we throw
    TRI_ASSERT(ctx.curRegs <= ctx.res->getNrRegs());

    // only copy 1st row of registers inherited from previous frame(s)
    inheritRegisters(cur, ctx.res.get(), _pos);

    throwIfKilled();  // check if we were aborted

    TRI_IF_FAILURE("IResearchViewBlockBase::getSome") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    _hasMore = next(ctx, atMost);

    if (!_hasMore) {
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
    // aggregate stats
    _engine->_stats.scannedIndex += static_cast<int64_t>(_inflight);
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
        // aggregate stats
        _engine->_stats.scannedIndex += static_cast<int64_t>(_inflight);
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
    irs::index_reader const& reader,
    aql::ExecutionEngine& engine,
    IResearchViewNode const& node
): IResearchViewUnorderedBlock(reader, engine, node),
    _scr(&irs::score::no_score()) {
  _volatileSort = true;
}

bool IResearchViewBlock::resetIterator() {
  // refresh _pkReader and _itr first
  if (!IResearchViewUnorderedBlock::resetIterator()) {
    return false;
  }

  _scr = _itr->attributes().get<irs::score>().get();

  if (_scr) {
    _scrVal = _scr->value();
  } else {
    _scr = &irs::score::no_score();
    _scrVal = irs::bytes_ref::NIL;
  }

  return true;
}

bool IResearchViewBlock::next(
    ReadContext& ctx,
    size_t limit) {
  TRI_ASSERT(_filter);
  auto const& viewNode = *ExecutionNode::castTo<IResearchViewNode const*>(getPlanNode());
  auto const numSorts = viewNode.sortCondition().size();

  for (size_t count = _reader.size(); _readerOffset < count; ) {
    if (!_itr && !resetIterator()) {
      continue;
    }

    TRI_ASSERT(_pkReader);

    while (limit && _itr->next()) {
      if (!readDocument(_itr->value(), _pkReader, ctx.callback)) {
        continue;
      }

      // evaluate scores
      TRI_ASSERT(!viewNode.sortCondition().empty());
      _scr->evaluate();

      // copy scores, registerId's are sequential
      auto scoreRegs = ctx.curRegs;

      for (size_t i = 0; i < numSorts; ++i) {
        // in 3.4 we assume all scorers return float_t
        auto const score = _order.get<float_t>(_scrVal.c_str(), i);

        ctx.res->setValue(
          ctx.pos,
          ++scoreRegs,
#if 0
          _order.to_string<AqlValue, std::char_traits<char>>(_scrVal.c_str(), i)
#else
          AqlValue(AqlValueHintDouble(double_t(score)))
#endif
        );
      }

      if (ctx.pos > 0) {
        // re-use already copied AQLValues
        ctx.res->copyValuesFromFirstRow(ctx.pos, static_cast<RegisterId>(ctx.curRegs));
      }

      ++ctx.pos;
      --limit;
    }

    if (!limit) {
      // return 'true' if we've reached the requested limit,
      // but don't know exactly are there any more data
      return true; // do not change iterator if already reached limit
    }

    ++_readerOffset;
    _itr.reset();
  }

  // return 'true' if we've reached the requested limit,
  // but don't know exactly are there any more data
  return (limit == 0);
}

size_t IResearchViewBlock::skip(size_t limit) {
  TRI_ASSERT(_filter);
  size_t skipped{};

  for (size_t count = _reader.size(); _readerOffset < count;) {
    if (!_itr && !resetIterator()) {
      continue;
    }

    while (limit && _itr->next()) {
      ++skipped;
      --limit;
    }

    if (!limit) {
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
    irs::index_reader const& reader,
    aql::ExecutionEngine& engine,
    IResearchViewNode const& node
): IResearchViewBlockBase(reader, engine, node), _readerOffset(0) {
  _volatileSort = false; // do not evaluate sort
}

bool IResearchViewUnorderedBlock::resetIterator() {
  TRI_ASSERT(!_itr);

  auto& segmentReader = _reader[_readerOffset];

  _pkReader = ::pkColumn(segmentReader);

  if (!_pkReader) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "encountered a sub-reader without a primary key column while executing a query, ignoring";
    return false;
  }

  _itr = segmentReader.mask(_filter->execute(
    segmentReader, _order, _filterCtx
  ));

  return true;
}

bool IResearchViewUnorderedBlock::next(
    ReadContext& ctx,
    size_t limit) {
  TRI_ASSERT(_filter);

  for (size_t count = _reader.size(); _readerOffset < count; ) {
    if (!_itr && !resetIterator()) {
      continue;
    }

    TRI_ASSERT(_pkReader);

    // read document PKs from iresearch
    auto end = readPKs(*_itr, _pkReader, _keys, limit);

    // read documents from underlying storage engine
    for (auto begin = _keys.begin(); begin != end; ++begin) {
      if (!readDocument(*begin, ctx.callback)) {
        continue;
      }

      if (ctx.pos > 0) {
        // re-use already copied AQLValues
        ctx.res->copyValuesFromFirstRow(ctx.pos, ctx.curRegs);
      }

      ++ctx.pos;
      --limit;
    }

    if (!limit) {
      // return 'true' since we've reached the requested limit,
      // but don't know exactly are there any more data
      return true; // do not change iterator if already reached limit
    }

    ++_readerOffset;
    _itr.reset();
  }

  // return 'true' if we've reached the requested limit,
  // but don't know exactly are there any more data
  return (limit == 0);
}

size_t IResearchViewUnorderedBlock::skip(size_t limit) {
  TRI_ASSERT(_filter);
  size_t skipped{};

  for (size_t count = _reader.size(); _readerOffset < count;) {
    if (!_itr && !resetIterator()) {
      continue;
    }

    while (limit && _itr->next()) {
      ++skipped;
      --limit;
    }

    if (!limit) {
      break; // do not change iterator if already reached limit
    }

    ++_readerOffset;
    _itr.reset();
  }

  return skipped;
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
