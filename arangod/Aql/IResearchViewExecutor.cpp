////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewExecutor.h"

#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchView.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "search/boolean_filter.hpp"
#include "search/score.hpp"

// TODO Eliminate access to the plan if possible!
// I think it is used for two things only:
//  - to get the Ast, which can simply be passed on its own, and
//  - to call plan->getVarSetBy() in aql::Expression, which could be removed by
//    passing (a reference to) plan->_varSetBy instead.
// But changing this, even only for the IResearch part, would involve more
// refactoring than I currently find appropriate for this.
#include "Aql/ExecutionPlan.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::iresearch;

namespace {

inline std::shared_ptr<arangodb::LogicalCollection> lookupCollection(  // find collection
    arangodb::transaction::Methods& trx,  // transaction
    TRI_voc_cid_t cid,                    // collection identifier
    Query& query) {
  TRI_ASSERT(trx.state());

  // this is necessary for MMFiles
  trx.pinData(cid);

  // `Methods::documentCollection(TRI_voc_cid_t)` may throw exception
  auto* collection = trx.state()->collection(cid, arangodb::AccessMode::Type::READ);

  if (!collection) {
    std::stringstream msg;
    msg << "failed to find collection while reading document from arangosearch "
           "view, cid '"
        << cid << "'";
    query.registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg.str());

    return nullptr;  // not a valid collection reference
  }

  return collection->collection();
}

inline irs::columnstore_reader::values_reader_f pkColumn(irs::sub_reader const& segment) {
  auto const* reader = segment.column_reader(DocumentPrimaryKey::PK());

  return reader ? reader->values() : irs::columnstore_reader::values_reader_f{};
}

inline irs::columnstore_reader::values_reader_f sortColumn(irs::sub_reader const& segment) {
  auto const* reader = segment.sort();

  return reader ? reader->values() : irs::columnstore_reader::values_reader_f{};
}

LocalDocumentId readPK(
    irs::doc_id_t docId,
    irs::columnstore_reader::values_reader_f const& values) {
  LocalDocumentId documentId;
  irs::bytes_ref key;
  if (values(docId, key)) {
    bool const readSuccess = DocumentPrimaryKey::read(documentId, key);

    TRI_ASSERT(readSuccess == documentId.isSet());

    if (!readSuccess) {
      LOG_TOPIC("6423f", WARN, arangodb::iresearch::TOPIC)
          << "failed to read document primary key while reading document "
             "from arangosearch view, doc_id '"
          << docId << "'";
    }
  }

  return documentId;
}

// Returns true unless the iterator is exhausted. documentId will always be
// written. It will always be unset when readPK returns false, but may also be
// unset if readPK returns true.
bool readPK(irs::doc_iterator& it,
            irs::columnstore_reader::values_reader_f const& values,
            LocalDocumentId& documentId) {
  if (it.next()) {
    irs::bytes_ref key;
    irs::doc_id_t const docId = it.value();
    if (values(docId, key)) {
      bool const readSuccess = DocumentPrimaryKey::read(documentId, key);

      TRI_ASSERT(readSuccess == documentId.isSet());

      if (!readSuccess) {
        LOG_TOPIC("6442f", WARN, arangodb::iresearch::TOPIC)
            << "failed to read document primary key while reading document "
               "from arangosearch view, doc_id '"
            << docId << "'";
      }
    }

    return true;
  } else {
    documentId = {};
  }

  return false;
}

}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                       IResearchViewExecutorBase
///////////////////////////////////////////////////////////////////////////////

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    ExecutorInfos&& infos,
    std::shared_ptr<const IResearchView::Snapshot> reader,
    RegisterId firstOutputRegister,
    RegisterId numScoreRegisters,
    Query& query,
    std::vector<Scorer> const& scorers,
    IResearchViewSort const* sort,
    ExecutionPlan const& plan,
    Variable const& outVariable,
    aql::AstNode const& filterCondition,
    std::pair<bool, bool> volatility,
    IResearchViewExecutorInfos::VarInfoMap const& varInfoMap,
    int depth)
  : ExecutorInfos(std::move(infos)),
    _outputRegister(firstOutputRegister),
    _numScoreRegisters(numScoreRegisters),
    _reader(std::move(reader)),
    _query(query),
    _scorers(scorers),
    _sort(sort),
    _plan(plan),
    _outVariable(outVariable),
    _filterCondition(filterCondition),
    _volatileSort(volatility.second),
    // `_volatileSort` implies `_volatileFilter`
    _volatileFilter(_volatileSort || volatility.first),
    _varInfoMap(varInfoMap),
    _depth(depth) {
  TRI_ASSERT(_reader != nullptr);
  TRI_ASSERT(getOutputRegisters()->find(firstOutputRegister) !=
             getOutputRegisters()->end());
}

RegisterId IResearchViewExecutorInfos::getOutputRegister() const {
  return _outputRegister;
};

RegisterId IResearchViewExecutorInfos::getNumScoreRegisters() const {
  return _numScoreRegisters;
};

Query& IResearchViewExecutorInfos::getQuery() const noexcept { return _query; }

std::shared_ptr<const IResearchView::Snapshot> IResearchViewExecutorInfos::getReader() const {
  return _reader;
}

bool IResearchViewExecutorInfos::isScoreReg(RegisterId reg) const {
  return getOutputRegister() < reg && reg <= getOutputRegister() + getNumScoreRegisters();
}

std::vector<Scorer> const& IResearchViewExecutorInfos::scorers() const noexcept {
  return _scorers;
}

ExecutionPlan const& IResearchViewExecutorInfos::plan() const noexcept {
  return _plan;
}

Variable const& IResearchViewExecutorInfos::outVariable() const noexcept {
  return _outVariable;
}

aql::AstNode const& IResearchViewExecutorInfos::filterCondition() const noexcept {
  return _filterCondition;
}

IResearchViewExecutorInfos::VarInfoMap const&
IResearchViewExecutorInfos::varInfoMap() const noexcept {
  return _varInfoMap;
}

int IResearchViewExecutorInfos::getDepth() const noexcept { return _depth; }

bool IResearchViewExecutorInfos::volatileSort() const noexcept {
  return _volatileSort;
}

bool IResearchViewExecutorInfos::volatileFilter() const noexcept {
  return _volatileFilter;
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                       IResearchViewExecutorBase
///////////////////////////////////////////////////////////////////////////////

template<typename Impl, typename Traits>
IndexIterator::DocumentCallback
IResearchViewExecutorBase<Impl, Traits>::ReadContext::copyDocumentCallback(ReadContext& ctx) {
  auto* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  typedef std::function<IndexIterator::DocumentCallback(ReadContext&)> CallbackFactory;

  static CallbackFactory const callbackFactories[]{
      [](ReadContext& ctx) {
        // capture only one reference to potentially avoid heap allocation
        return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
          AqlValue a{AqlValueHintCopy(doc.begin())};
          bool mustDestroy = true;
          AqlValueGuard guard{a, mustDestroy};
          ctx.outputRow.moveValueInto(ctx.docOutReg, ctx.inputRow, guard);
        };
      },

      [](ReadContext& ctx) {
        // capture only one reference to potentially avoid heap allocation
        return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
          AqlValue a{AqlValueHintDocumentNoCopy(doc.begin())};
          bool mustDestroy = true;
          AqlValueGuard guard{a, mustDestroy};
          ctx.outputRow.moveValueInto(ctx.docOutReg, ctx.inputRow, guard);
        };
      }};

  return callbackFactories[size_t(engine->useRawDocumentPointers())](ctx);
}

template<typename Impl, typename Traits>
IResearchViewExecutorBase<Impl, Traits>::IResearchViewExecutorBase(IResearchViewExecutorBase::Fetcher& fetcher,
                                                           IResearchViewExecutorBase::Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _inputRow(CreateInvalidInputRowHint{}),
      _upstreamState(ExecutionState::HASMORE),
      _indexReadBuffer(_infos.getNumScoreRegisters()),
      _filterCtx(1),  // arangodb::iresearch::ExpressionExecutionContext
      _ctx(&infos.getQuery(), infos.numberOfOutputRegisters(),
           infos.outVariable(), infos.varInfoMap(), infos.getDepth()),
      _reader(infos.getReader()),
      _filter(irs::filter::prepared::empty()),
      _execCtx(*infos.getQuery().trx(), _ctx),
      _inflight(0),
      _hasMore(true),  // has more data initially
      _isInitialized(false) {
  TRI_ASSERT(infos.getQuery().trx() != nullptr);

  // add expression execution context
  _filterCtx.emplace(_execCtx);
}

template<typename Impl, typename Traits>
std::pair<ExecutionState, typename IResearchViewExecutorBase<Impl, Traits>::Stats>
IResearchViewExecutorBase<Impl, Traits>::produceRows(OutputAqlItemRow& output) {
  IResearchViewStats stats{};
  bool documentWritten = false;

  auto& impl = static_cast<Impl&>(*this);

  while (!documentWritten) {
    if (!_inputRow.isInitialized()) {
      if (_upstreamState == ExecutionState::DONE) {
        // There will be no more rows, stop fetching.
        return {ExecutionState::DONE, stats};
      }

      std::tie(_upstreamState, _inputRow) = _fetcher.fetchRow();

      if (_upstreamState == ExecutionState::WAITING) {
        return {_upstreamState, stats};
      }

      if (!_inputRow.isInitialized()) {
        return {ExecutionState::DONE, stats};
      }

      // reset must be called exactly after we've got a new and valid input row.
      impl.reset();
    }

    ReadContext ctx(infos().getOutputRegister(), _inputRow, output);
    documentWritten = impl.next(ctx);

    if (documentWritten) {
      stats.incrScanned();
    } else {
      _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
      // no document written, repeat.
    }
  }

  return {ExecutionState::HASMORE, stats};
}

template <typename Impl, typename Traits>
bool IResearchViewExecutorBase<Impl, Traits>::next(ReadContext& ctx) {
  auto& impl = static_cast<Impl&>(*this);

  if (_indexReadBuffer.empty()) {
    impl.fillBuffer(ctx);
  }

  if (_indexReadBuffer.empty()) {
    return false;
  }

  IndexReadBufferEntry bufferEntry = _indexReadBuffer.pop_front();

  if (impl.writeRow(ctx, bufferEntry)) {
    // we read and wrote a document, return true. we don't know if there are more.
    return true;  // do not change iterator if already reached limit
  }

  // no documents found, we're exhausted.
  return false;
}

template<typename Impl, typename Traits>
void IResearchViewExecutorBase<Impl, Traits>::fillScores(
    ReadContext const& ctx,
    float_t const* begin,
    float_t const* end) {
  TRI_ASSERT(Traits::Ordered);

  // scorer register are placed consecutively after the document output register
  // is used here currently only for assertions.
  RegisterId scoreReg = ctx.docOutReg + 1;

  // copy scores, registerId's are sequential
  for (; begin != end; ++begin, ++scoreReg) {
    TRI_ASSERT(infos().isScoreReg(scoreReg));
    _indexReadBuffer.pushScore(*begin);
  }

  // We should either have no _scrVals to evaluate, or all
  TRI_ASSERT(begin == nullptr || !infos().isScoreReg(scoreReg));

  while (infos().isScoreReg(scoreReg)) {
    _indexReadBuffer.pushScoreNone();
    ++scoreReg;
  }

  // we should have written exactly all score registers by now
  TRI_ASSERT(!infos().isScoreReg(scoreReg));
  TRI_ASSERT(scoreReg - ctx.docOutReg - 1 == infos().getNumScoreRegisters());
}

template<typename Impl, typename Traits>
void IResearchViewExecutorBase<Impl, Traits>::reset() {
  _ctx._inputRow = _inputRow;

  ExecutionPlan const* plan = &infos().plan();

  QueryContext const queryCtx = {infos().getQuery().trx(),
                                 plan, plan->getAst(), &_ctx,
                                 &infos().outVariable()};

  if (infos().volatileFilter() || !_isInitialized) {  // `_volatileSort` implies `_volatileFilter`
    irs::Or root;

    if (!FilterFactory::filter(&root, queryCtx, infos().filterCondition())) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "failed to build filter while querying arangosearch view, query '" +
              infos().filterCondition().toVelocyPack(true)->toJson() + "'");
    }

    if (infos().volatileSort() || !_isInitialized) {
      irs::order order;
      irs::sort::ptr scorer;

      for (auto const& scorerNode : infos().scorers()) {
        TRI_ASSERT(scorerNode.node);

        if (!OrderFactory::scorer(&scorer, *scorerNode.node, queryCtx)) {
          // failed to append sort
          THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
        }

        // sorting order doesn't matter
        order.add(true, std::move(scorer));
      }

      // compile order
      _order = order.prepare();
    }

    // compile filter
    _filter = root.prepare(*_reader, _order, irs::boost::no_boost(), _filterCtx);

    _isInitialized = true;
  }
}

template<typename Impl, typename Traits>
bool IResearchViewExecutorBase<Impl, Traits>::writeRow(ReadContext& ctx,
                                                       IndexReadBufferEntry bufferEntry,
                                                       LocalDocumentId const& documentId,
                                                       LogicalCollection const& collection) {
  TRI_ASSERT(documentId.isSet());

  // read document from underlying storage engine, if we got an id
  if (collection.readDocumentWithCallback(infos().getQuery().trx(), documentId, ctx.callback)) {
    // in the ordered case we have to write scores as well as a document
    if /* constexpr */ (Traits::Ordered) {
      // scorer register are placed consecutively after the document output register
      RegisterId scoreReg = ctx.docOutReg + 1;

      for (auto& it : _indexReadBuffer.getScores(bufferEntry)) {
        TRI_ASSERT(infos().isScoreReg(scoreReg));
        bool mustDestroy = false;
        AqlValueGuard guard{it, mustDestroy};
        ctx.outputRow.moveValueInto(scoreReg, ctx.inputRow, guard);
        ++scoreReg;
      }

      // we should have written exactly all score registers by now
      TRI_ASSERT(!infos().isScoreReg(scoreReg));
    }

    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                           IResearchViewExecutor
///////////////////////////////////////////////////////////////////////////////

template <bool ordered>
IResearchViewExecutor<ordered>::IResearchViewExecutor(Fetcher& fetcher, Infos& infos)
    : Base(fetcher, infos),
      _pkReader(),
      _itr(),
      _readerOffset(0),
      _scr(&irs::score::no_score()),
      _scrVal() {
  TRI_ASSERT(ordered == (infos.getNumScoreRegisters() != 0));
}

template <bool ordered>
void IResearchViewExecutor<ordered>::evaluateScores(ReadContext const& ctx) {
  // This must not be called in the unordered case.
  TRI_ASSERT(ordered);

  // evaluate scores
  _scr->evaluate();

  // in arangodb we assume all scorers return float_t
  auto begin = reinterpret_cast<const float_t*>(_scrVal.begin());
  auto end = reinterpret_cast<const float_t*>(_scrVal.end());

  this->fillScores(ctx, begin, end);
}

template <bool ordered>
void IResearchViewExecutor<ordered>::fillBuffer(IResearchViewExecutor::ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);

  std::size_t const atMost = ctx.outputRow.numRowsLeft();

  size_t const count = this->_reader->size();
  for (; _readerOffset < count;) {
    if (!_itr) {
      if (!this->_indexReadBuffer.empty()) {
        // We may not reset the iterator and continue with the next reader if we
        // still have documents in the buffer, as the cid changes with each
        // reader.
        break;
      }

      if (!resetIterator()) {
        continue;
      }

      // CID is constant until the next resetIterator(). Save the corresponding
      // collection so we don't have to look it up every time.

      TRI_voc_cid_t const cid = this->_reader->cid(_readerOffset);
      Query& query = this->infos().getQuery();
      std::shared_ptr<arangodb::LogicalCollection> collection =
          lookupCollection(*query.trx(), cid, query);

      if (!collection) {
        std::stringstream msg;
        msg << "failed to find collection while reading document from "
               "arangosearch view, cid '"
            << cid << "'";
        query.registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg.str());

        // We don't have a collection, skip the current reader.
        ++_readerOffset;
        _itr.reset();
        continue;
      }

      _collection = collection.get();
      this->_indexReadBuffer.reset();
    }

    TRI_ASSERT(_pkReader);

    LocalDocumentId documentId;

    // try to read a document PK from iresearch
    bool const iteratorExhausted = !::readPK(*_itr, _pkReader, documentId);

    if (!documentId.isSet()) {
      // No document read, we cannot write it.

      if (iteratorExhausted) {
        // The iterator is exhausted, we need to continue with the next reader.
        ++_readerOffset;
        _itr.reset();
      }
      continue;
    }

    // The CID must stay the same for all documents in the buffer
    TRI_ASSERT(this->_collection->id() == this->_reader->cid(_readerOffset));

    this->_indexReadBuffer.pushValue(documentId);

    // in the ordered case we have to write scores as well as a document
    if /* constexpr */ (ordered) {
      // Writes into _scoreBuffer
      evaluateScores(ctx);
    }

    // doc and scores are both pushed, sizes must now be coherent
    this->_indexReadBuffer.assertSizeCoherence();

    if (iteratorExhausted) {
      // The iterator is exhausted, we need to continue with the next reader.
      ++_readerOffset;
      _itr.reset();

      // Here we have at least one document in _indexReadBuffer, so we may not
      // add documents from a new reader.
      break;
    }
    if (this->_indexReadBuffer.size() >= atMost) {
      break;
    }
  }
}

template <bool ordered>
bool IResearchViewExecutor<ordered>::resetIterator() {
  TRI_ASSERT(this->_filter);
  TRI_ASSERT(!_itr);

  auto& segmentReader = (*this->_reader)[_readerOffset];

  _pkReader = ::pkColumn(segmentReader);

  if (!_pkReader) {
    LOG_TOPIC("bd01b", WARN, arangodb::iresearch::TOPIC)
        << "encountered a sub-reader without a primary key column while "
           "executing a query, ignoring";
    return false;
  }

  _itr = segmentReader.mask(this->_filter->execute(segmentReader, this->_order, this->_filterCtx));

  if /* constexpr */ (ordered) {
    _scr = _itr->attributes().get<irs::score>().get();

    if (_scr) {
      _scrVal = _scr->value();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      auto const numScores =
          static_cast<size_t>(std::distance(_scrVal.begin(), _scrVal.end())) /
          sizeof(float_t);

      TRI_ASSERT(numScores == this->infos().scorers().size());
#endif
    } else {
      _scr = &irs::score::no_score();
      _scrVal = irs::bytes_ref::NIL;
    }
  }

  return true;
}

template <bool ordered>
void IResearchViewExecutor<ordered>::reset() {
  Base::reset();

  // reset iterator state
  _itr.reset();
  _readerOffset = 0;
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                      IResearchViewMergeExecutor
///////////////////////////////////////////////////////////////////////////////

template <bool ordered>
IResearchViewMergeExecutor<ordered>::IResearchViewMergeExecutor(Fetcher& fetcher, Infos& infos)
    : Base{fetcher, infos},
      _heap_it{ MinHeapContext{ *infos.sort(), _segments } } {
  TRI_ASSERT(infos.sort());
  TRI_ASSERT(!infos.sort()->empty());
  TRI_ASSERT(ordered == (infos.getNumScoreRegisters() != 0));
}

template <bool ordered>
void IResearchViewMergeExecutor<ordered>::evaluateScores(
    ReadContext const& ctx,
    irs::score const& score) {
  // This must not be called in the unordered case.
  TRI_ASSERT(ordered);

  // evaluate scores
  score.evaluate();

  // in arangodb we assume all scorers return float_t
  auto& scrVal = score.value();
  auto begin = reinterpret_cast<const float_t*>(scrVal.c_str());
  auto end = reinterpret_cast<const float_t*>(scrVal.c_str() + scrVal.size());

  this->fillScores(ctx, begin, end);
}

template <bool ordered>
void IResearchViewMergeExecutor<ordered>::reset() {
  Base::reset();

  _segments.clear();
  _segments.reserve(this->_reader->size());

  for (size_t i = 0, size = this->_reader->size(); i < size; ++i) {
    auto& segment = (*this->_reader)[i];

    auto sortReader = ::sortColumn(segment);

    if (!sortReader) {
      LOG_TOPIC("ad14z", WARN, arangodb::iresearch::TOPIC)
          << "encountered a sub-reader without a sort column while "
             "executing a query, ignoring";
      continue;
    }

    irs::doc_iterator::ptr it = segment.mask(this->_filter->execute(segment, this->_order, this->_filterCtx));

    auto const* score = &irs::score::no_score();

    if /* constexpr */ (ordered) {
      auto& scoreRef = it->attributes().get<irs::score>();

      if (scoreRef) {
        score = scoreRef.get();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        irs::bytes_ref const scrVal = score->value();

        auto const numScores =
            static_cast<size_t>(std::distance(scrVal.begin(), scrVal.end())) /
            sizeof(float_t);

        TRI_ASSERT(numScores == this->infos().scorers().size());
#endif
      }
    }

    TRI_voc_cid_t const cid = this->_reader->cid(i);
    Query& query = this->infos().getQuery();
    std::shared_ptr<arangodb::LogicalCollection> collection =
        lookupCollection(*query.trx(), cid, query);

    if (!collection) {
      std::stringstream msg;
      msg << "failed to find collection while reading document from "
             "arangosearch view, cid '"
          << cid << "'";
      query.registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg.str());

      // We don't have a collection, skip the current segment.
      continue;
    }

    auto pkReader = ::pkColumn(segment);

    if (!pkReader) {
      LOG_TOPIC("bd81z", WARN, arangodb::iresearch::TOPIC)
          << "encountered a sub-reader without a primary key column while "
             "executing a query, ignoring";
      continue;
    }

    _segments.emplace_back(std::move(it),
                           *score,
                           *collection,
                           std::move(sortReader),
                           std::move(pkReader));
  }

  _heap_it.reset(_segments.size());
}

template <bool ordered>
void IResearchViewMergeExecutor<ordered>::fillBuffer(ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);

  std::size_t const atMost = ctx.outputRow.numRowsLeft();

  for (; _heap_it.next();) {
    auto& segment = _segments[_heap_it.value()];
    TRI_ASSERT(segment.docs);
    TRI_ASSERT(segment.score);
    TRI_ASSERT(segment.collection);
    TRI_ASSERT(segment.pkReader);

    // try to read a document PK from iresearch
    LocalDocumentId const documentId = ::readPK(segment.docs->value(), segment.pkReader);

    if (!documentId.isSet()) {
      // No document read, we cannot write it.
      continue;
    }

    this->_indexReadBuffer.pushValue(documentId, segment.collection);

    // in the ordered case we have to write scores as well as a document
    if /* constexpr */ (ordered) {
      // Writes into _scoreBuffer
      evaluateScores(ctx, *segment.score);
    }

    // doc and scores are both pushed, sizes must now be coherent
    this->_indexReadBuffer.assertSizeCoherence();

    if (this->_indexReadBuffer.size() >= atMost) {
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                 explicit template instantiation
///////////////////////////////////////////////////////////////////////////////

template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<false>>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<true>>;
template class ::arangodb::aql::IResearchViewExecutor<false>;
template class ::arangodb::aql::IResearchViewExecutor<true>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewMergeExecutor<false>>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewMergeExecutor<true>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<false>;
template class ::arangodb::aql::IResearchViewMergeExecutor<true>;
