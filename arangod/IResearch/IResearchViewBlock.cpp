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
#include "IResearchViewBlock.h"
#include "IResearchViewNode.h"
#include "IResearchView.h"
#include "IResearchDocument.h"
#include "IResearchFeature.h"
#include "IResearchFilterFactory.h"
#include "IResearchOrderFactory.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include "search/boolean_filter.hpp"
#include "search/score.hpp"

#include <iostream>

namespace {

inline arangodb::iresearch::IResearchViewNode const& getViewNode(
    arangodb::iresearch::IResearchViewBlockBase const& block
) noexcept {
  TRI_ASSERT(block.getPlanNode());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  return dynamic_cast<arangodb::iresearch::IResearchViewNode const&>(*block.getPlanNode());
#else
  return static_cast<arangodb::iresearch::IResearchViewNode const&>(*block.getPlanNode());
#endif
}

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
}

namespace arangodb {
namespace iresearch {

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

  if (var == _node->outVariable()) {
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
    CompoundReader&& reader,
    ExecutionEngine& engine,
    IResearchViewNode const& en)
  : ExecutionBlock(&engine, &en),
    _filterCtx(1), // arangodb::iresearch::ExpressionExecutionContext
    _ctx(getViewNode(*this)),
    _reader(std::move(reader)),
    _filter(irs::filter::prepared::empty()),
    _execCtx(*_trx, _ctx),
    _hasMore(true), // has more data initially
    _volatileSort(true),
    _volatileFilter(true) {
  TRI_ASSERT(_trx);

  // add expression execution context
  _filterCtx.emplace(_execCtx);
}

int IResearchViewBlockBase::initializeCursor(AqlItemBlock* items, size_t pos) {
  DEBUG_BEGIN_BLOCK();
  const int res = ExecutionBlock::initializeCursor(items, pos);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  _hasMore = true; // has more data initially

  return TRI_ERROR_NO_ERROR;
  DEBUG_END_BLOCK();
}

void IResearchViewBlockBase::reset() {
  TRI_ASSERT(!_buffer.empty());

  // setup expression context
  _ctx._data = _buffer.front();
  _ctx._pos = _pos;

  auto& viewNode = getViewNode(*this);
  auto* plan = const_cast<ExecutionPlan*>(viewNode.plan());

  arangodb::iresearch::QueryContext const queryCtx = {
    _trx, plan, plan->getAst(), &_ctx, viewNode.outVariable()
  };

  if (_volatileFilter) { // `_volatileSort` implies `_volatileFilter`
    irs::Or root;

    if (!arangodb::iresearch::FilterFactory::filter(&root, queryCtx, viewNode.filterCondition())) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "failed to build filter while querying iResearch view , query '"
          << viewNode.filterCondition().toVelocyPack(true)->toJson() << "'";

      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }

    if (_volatileSort) {
      irs::order order;
      irs::sort::ptr scorer;

      for (auto const& sort : viewNode.sortCondition()) {
        TRI_ASSERT(sort.node);

        if (!arangodb::iresearch::OrderFactory::scorer(&scorer, *sort.node, *queryCtx.ref)) {
          // failed to append sort
          THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
        }

        order.add(sort.asc, std::move(scorer));
      }

      // compile order
      _order = order.prepare();
      _volatileSort = viewNode.volatile_sort();
    }

    // compile filter
    _filter = root.prepare(_reader, _order, irs::boost::no_boost(), _filterCtx);
    _volatileFilter = _volatileSort || viewNode.volatile_filter();
  }
}

bool IResearchViewBlockBase::readDocument(
    size_t subReaderId,
    irs::doc_id_t const docId
) {
  const auto& pkValues = _reader.pkColumn(subReaderId);
  arangodb::iresearch::DocumentPrimaryKey docPk;
  irs::bytes_ref tmpRef;

  if (!pkValues(docId, tmpRef) || !docPk.read(tmpRef)) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "failed to read document primary key while reading document from iResearch view, doc_id '" << docId << "'";

    return false; // not a valid document reference
  }

  static const std::string unknown("<unknown>");

  _trx->addCollectionAtRuntime(docPk.cid(), unknown);

  auto* collection = _trx->documentCollection(docPk.cid());

  if (!collection) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "failed to find collection while reading document from iResearch view, cid '" << docPk.cid()
      << "', rid '" << docPk.rid() << "'";

    return false; // not a valid collection reference
  }

  return collection->readDocument(
    _trx, arangodb::LocalDocumentId(docPk.rid()), _mmdr
  );
}

AqlItemBlock* IResearchViewBlockBase::getSome(size_t, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  traceGetSomeBegin();

  if (_done) {
    traceGetSomeEnd(nullptr);
    return nullptr;
  }

  bool needMore;
  AqlItemBlock* cur = nullptr;
  size_t send = 0;
  std::unique_ptr<AqlItemBlock> res;

  auto const& planNode = getViewNode(*this);

  do {
    do {
      needMore = false;

      if (_buffer.empty()) {
        size_t const toFetch = (std::min)(DefaultBatchSize(), atMost);
        if (!ExecutionBlock::getBlock(toFetch, toFetch)) {
          _done = true;
          return nullptr;
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

    auto const curRegs = cur->getNrRegs();
    auto const nrRegs = planNode.getRegisterPlan()->nrRegs[planNode.getDepth()];

    res.reset(requestBlock(atMost, nrRegs));
    // automatically freed if we throw
    TRI_ASSERT(curRegs <= res->getNrRegs());

    // only copy 1st row of registers inherited from previous frame(s)
    inheritRegisters(cur, res.get(), _pos);

    throwIfKilled();  // check if we were aborted

    TRI_IF_FAILURE("EnumerateViewBlock::moreDocuments") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    _hasMore = next(*res, curRegs, send, atMost);

    // If the collection is actually empty we cannot forward an empty block
  } while (send == 0);

  TRI_ASSERT(res);

  // aggregate stats
   _engine->_stats.scannedIndex += static_cast<int64_t>(send);

  if (send < atMost) {
    // The collection did not have enough results
    res->shrink(send);
  }

  // Clear out registers no longer needed later:
  clearRegisters(res.get());

  traceGetSomeEnd(res.get());

  return res.release();

  DEBUG_END_BLOCK();
}

size_t IResearchViewBlockBase::skipSome(size_t atLeast, size_t atMost) {
  DEBUG_BEGIN_BLOCK();
  size_t skipped = 0;

  if (_done) {
    return skipped;
  }

  while (skipped < atLeast) {
    if (_buffer.empty()) {
      size_t toFetch = (std::min)(DefaultBatchSize(), atMost);
      if (!getBlock(toFetch, toFetch)) {
        _done = true;
        return skipped;
      }
      _pos = 0;  // this is in the first block
      reset();
    }

    // if we get here, then _buffer.front() exists
    AqlItemBlock* cur = _buffer.front();

    skipped += skip(atMost - skipped);

    if (skipped < atLeast) {
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
  _engine->_stats.scannedIndex += static_cast<int64_t>(skipped);

  // We skipped atLeast documents
  return skipped;

  // cppcheck-suppress style
  DEBUG_END_BLOCK();
}

// -----------------------------------------------------------------------------
// --SECTION--                                 IResearchViewBlock implementation
// -----------------------------------------------------------------------------

IResearchViewBlock::IResearchViewBlock(
    arangodb::iresearch::CompoundReader&& reader,
    aql::ExecutionEngine& engine,
    IResearchViewNode const& node
) : IResearchViewUnorderedBlock(std::move(reader), engine, node),
    _scr(&irs::score::no_score()) {
  _volatileSort = true;
}

bool IResearchViewBlock::next(
    AqlItemBlock& res,
    aql::RegisterId curRegs,
    size_t& pos,
    size_t limit) {
  TRI_ASSERT(_filter);
  bool done;

  for (size_t count = _reader.size(); _readerOffset < count; ) {
    done = false;

    if (!_itr) {
      auto& segmentReader = _reader[_readerOffset];

      _itr = segmentReader.mask(_filter->execute(
        segmentReader, _order, _filterCtx
      ));

      _scr = _itr->attributes().get<irs::score>().get();

      if (_scr) {
        _scrVal = _scr->value();
      } else {
        _scr = &irs::score::no_score();
        _scrVal = irs::bytes_ref::nil;
      }
    }

    while (limit && _itr->next()) {
      if (readDocument(_readerOffset, _itr->value())) {
        // The result is in the first variable of this depth,
        // we do not need to do a lookup in
        // getPlanNode()->_registerPlan->varInfo,
        // but can just take cur->getNrRegs() as registerId:
        uint8_t const* vpack = _mmdr.vpack();

        if (_mmdr.canUseInExternal()) {
          res.setValue(pos, curRegs, AqlValue(AqlValueHintDocumentNoCopy(vpack)));
        } else {
          res.setValue(pos, curRegs, AqlValue(AqlValueHintCopy(vpack)));
        }

        // evaluate scores
        _scr->evaluate();

        // FIXME
        // copy scores
        // registerId's are sequential
        auto scoreRegs = curRegs;
        for (size_t i = 0, size = getViewNode(*this).sortCondition().size(); i < size; ++i) {
          auto const scoreValue = _order.get<float_t>(_scrVal.c_str(), i);

          res.setValue(pos, ++scoreRegs, AqlValue(AqlValueHintDouble(scoreValue)));
        }
      }

      // FIXME why?
      if (pos > 0) {
        // re-use already copied AQLValues
        res.copyValuesFromFirstRow(pos, static_cast<RegisterId>(curRegs));
      }
      ++pos;

      done = 0 ==--limit;
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
  bool done;

  for (size_t count = _reader.size(); _readerOffset < count;) {
    done = false;

    if (!_itr) {
      auto& segmentReader = _reader[_readerOffset];

      _itr = segmentReader.mask(_filter->execute(
        segmentReader, _order, _filterCtx
      ));

      _scr = _itr->attributes().get<irs::score>().get();

      if (_scr) {
        _scrVal = _scr->value();
      } else {
        _scr = &irs::score::no_score();
        _scrVal = irs::bytes_ref::nil;
      }
    }

    while (limit && _itr->next()) {
      ++skipped;
      done = 0 == --limit;
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
    arangodb::iresearch::CompoundReader&& reader,
    aql::ExecutionEngine& engine,
    IResearchViewNode const& node
) : IResearchViewBlockBase(std::move(reader), engine, node),
    _readerOffset(0) {
  _volatileSort = false; // do not evaluate sort
}

bool IResearchViewUnorderedBlock::next(
    AqlItemBlock& res,
    aql::RegisterId curRegs,
    size_t& pos,
    size_t limit) {
  TRI_ASSERT(_filter);
  bool done;

  for (size_t count = _reader.size(); _readerOffset < count; ) {
    done = false;

    if (!_itr) {
      auto& segmentReader = _reader[_readerOffset];

      _itr = segmentReader.mask(_filter->execute(
        segmentReader, _order, _filterCtx
      ));
    }

    while (limit && _itr->next()) {
      if (readDocument(_readerOffset, _itr->value())) {
        // The result is in the first variable of this depth,
        // we do not need to do a lookup in
        // getPlanNode()->_registerPlan->varInfo,
        // but can just take cur->getNrRegs() as registerId:
        uint8_t const* vpack = _mmdr.vpack();

        if (_mmdr.canUseInExternal()) {
          res.setValue(pos, curRegs, AqlValue(AqlValueHintDocumentNoCopy(vpack)));
        } else {
          res.setValue(pos, curRegs, AqlValue(AqlValueHintCopy(vpack)));
        }
      }

      // FIXME why?
      if (pos > 0) {
        // re-use already copied AQLValues
        res.copyValuesFromFirstRow(pos, curRegs);
      }
      ++pos;

      done = 0 ==--limit;
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
  bool done;

  for (size_t count = _reader.size(); _readerOffset < count;) {
    done = false;

    if (!_itr) {
      auto& segmentReader = _reader[_readerOffset];

      _itr = segmentReader.mask(_filter->execute(
        segmentReader, _order, _filterCtx
      ));
    }

    while (limit && _itr->next()) {
      ++skipped;
      done = 0 == --limit;
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
    arangodb::iresearch::CompoundReader&& reader,
    aql::ExecutionEngine& engine,
    IResearchViewNode const& node
) : IResearchViewBlockBase(std::move(reader), engine, node) {
}

bool IResearchViewOrderedBlock::next(
    aql::AqlItemBlock& res,
    aql::RegisterId curRegs,
    size_t& pos,
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
      LOG_TOPIC(ERR, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "failed to retrieve document score attribute while iterating iResearch view, ignoring: reader_id '" << i << "'";
      IR_STACK_TRACE();

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
      LOG_TOPIC(ERR, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "document count less than the document count during the previous iteration on the same query while iterating iResearch view'";

      break; // if here then there is probably a bug in the iResearch library
    }
  }

  // iterate through documents
  while (limit && tokenItr != tokenEnd) {
    auto& token = tokenItr->second;

    if (readDocument(token.first, token.second)) {
      // The result is in the first variable of this depth,
      // we do not need to do a lookup in
      // getPlanNode()->_registerPlan->varInfo,
      // but can just take cur->getNrRegs() as registerId:
      uint8_t const* vpack = _mmdr.vpack();

      if (_mmdr.canUseInExternal()) {
        res.setValue(pos, curRegs, AqlValue(AqlValueHintDocumentNoCopy(vpack)));
      } else {
        res.setValue(pos, curRegs, AqlValue(AqlValueHintCopy(vpack)));
      }
    }

    // FIXME why?
    if (pos > 0) {
      // re-use already copied AQLValues
      res.copyValuesFromFirstRow(pos, curRegs);
    }
    ++pos;

    ++tokenItr;
    ++_skip;
    --limit;
  }

  return (limit == 0); // exceeded limit
}

size_t IResearchViewOrderedBlock::skip(uint64_t limit) {
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

//    while (limit > skipped && itr->next()) {
//      if (skip) {
//        --skip;
//      } else {
//        ++skipped;
//      }
//    }
  }

  _skip += skipped;

  return skipped;
}

} // iresearch
} // arangodb
