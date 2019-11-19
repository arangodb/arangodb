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

#include "Aql/ExecutionStats.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/OutputAqlItemRow.h"
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

#include <analysis/token_attributes.hpp>
#include <search/boolean_filter.hpp>
#include <search/score.hpp>

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

}  // namespace

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                       IResearchViewExecutorBase
///////////////////////////////////////////////////////////////////////////////

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    ExecutorInfos&& infos, std::shared_ptr<const IResearchView::Snapshot> reader,
    RegisterId firstOutputRegister, RegisterId numScoreRegisters, Query& query,
    std::vector<Scorer> const& scorers,
    std::pair<arangodb::iresearch::IResearchViewSort const*, size_t> const& sort,
    ExecutionPlan const& plan, Variable const& outVariable,
    aql::AstNode const& filterCondition, std::pair<bool, bool> volatility,
    IResearchViewExecutorInfos::VarInfoMap const& varInfoMap, int depth)
    : ExecutorInfos(std::move(infos)),
      _firstOutputRegister(firstOutputRegister),
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

RegisterId IResearchViewExecutorInfos::getOutputRegister() const noexcept {
  return _firstOutputRegister + getNumScoreRegisters();
}

RegisterId IResearchViewExecutorInfos::getNumScoreRegisters() const noexcept {
  return _numScoreRegisters;
}

RegisterId IResearchViewExecutorInfos::getFirstScoreRegister() const noexcept {
  TRI_ASSERT(getNumScoreRegisters() > 0);
  return _firstOutputRegister;
}

std::shared_ptr<const arangodb::iresearch::IResearchView::Snapshot> IResearchViewExecutorInfos::getReader() const
    noexcept {
  return _reader;
}

Query& IResearchViewExecutorInfos::getQuery() const noexcept { return _query; }

const std::vector<arangodb::iresearch::Scorer>& IResearchViewExecutorInfos::scorers() const
    noexcept {
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

const IResearchViewExecutorInfos::VarInfoMap& IResearchViewExecutorInfos::varInfoMap() const
    noexcept {
  return _varInfoMap;
}

int IResearchViewExecutorInfos::getDepth() const noexcept { return _depth; }

bool IResearchViewExecutorInfos::volatileSort() const noexcept {
  return _volatileSort;
}

bool IResearchViewExecutorInfos::volatileFilter() const noexcept {
  return _volatileFilter;
}

const std::pair<const arangodb::iresearch::IResearchViewSort*, size_t>& IResearchViewExecutorInfos::sort() const
    noexcept {
  return _sort;
}

bool IResearchViewExecutorInfos::isScoreReg(RegisterId reg) const noexcept {
  return getNumScoreRegisters() > 0 &&
         getFirstScoreRegister() <= reg && reg < getFirstScoreRegister() + getNumScoreRegisters();
}

IResearchViewStats::IResearchViewStats() noexcept : _scannedIndex(0) {}
void IResearchViewStats::incrScanned() noexcept { _scannedIndex++; }
void IResearchViewStats::incrScanned(size_t value) noexcept {
  _scannedIndex = _scannedIndex + value;
}
std::size_t IResearchViewStats::getScanned() const noexcept {
  return _scannedIndex;
}
ExecutionStats& aql::operator+=(ExecutionStats& executionStats,
                                const IResearchViewStats& iResearchViewStats) noexcept {
  executionStats.scannedIndex += iResearchViewStats.getScanned();
  return executionStats;
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                       IResearchViewExecutorBase
///////////////////////////////////////////////////////////////////////////////

template <typename Impl, typename Traits>
IndexIterator::DocumentCallback IResearchViewExecutorBase<Impl, Traits>::ReadContext::copyDocumentCallback(
    ReadContext& ctx) {
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
          return true;
        };
      },

      [](ReadContext& ctx) {
        // capture only one reference to potentially avoid heap allocation
        return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
          AqlValue a{AqlValueHintDocumentNoCopy(doc.begin())};
          bool mustDestroy = true;
          AqlValueGuard guard{a, mustDestroy};
          ctx.outputRow.moveValueInto(ctx.docOutReg, ctx.inputRow, guard);
          return true;
        };
      }};

  return callbackFactories[size_t(engine->useRawDocumentPointers())](ctx);
}
template <typename Impl, typename Traits>
IResearchViewExecutorBase<Impl, Traits>::ReadContext::ReadContext(
    aql::RegisterId docOutReg, InputAqlItemRow& inputRow, OutputAqlItemRow& outputRow)
    : docOutReg(docOutReg),
      inputRow(inputRow),
      outputRow(outputRow),
      callback(copyDocumentCallback(*this)) {}

template <typename Impl, typename Traits>
IResearchViewExecutorBase<Impl, Traits>::IndexReadBufferEntry::IndexReadBufferEntry(std::size_t keyIdx) noexcept
    : _keyIdx(keyIdx) {}

template <typename Impl, typename Traits>
template <typename ValueType>
IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::IndexReadBuffer::ScoreIterator::ScoreIterator(
    std::vector<AqlValue>& scoreBuffer, std::size_t keyIdx, std::size_t numScores) noexcept
    : _scoreBuffer(scoreBuffer), _scoreBaseIdx(keyIdx * numScores), _numScores(numScores) {
  TRI_ASSERT(_scoreBaseIdx + _numScores <= _scoreBuffer.size());
}

template <typename Impl, typename Traits>
template <typename ValueType>
std::vector<AqlValue>::iterator IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<
    ValueType>::IndexReadBuffer::ScoreIterator::begin() noexcept {
  return _scoreBuffer.begin() + _scoreBaseIdx;
}

template <typename Impl, typename Traits>
template <typename ValueType>
std::vector<AqlValue>::iterator IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<
    ValueType>::IndexReadBuffer::ScoreIterator::end() noexcept {
  return _scoreBuffer.begin() + _scoreBaseIdx + _numScores;
}

template <typename Impl, typename Traits>
template <typename ValueType>
IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::IndexReadBuffer(std::size_t const numScoreRegisters)
    : _keyBuffer(), _scoreBuffer(), _numScoreRegisters(numScoreRegisters), _keyBaseIdx(0) {}

template <typename Impl, typename Traits>
template <typename ValueType>
ValueType const& IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::getValue(
    const IResearchViewExecutorBase::IndexReadBufferEntry bufferEntry) const noexcept {
  assertSizeCoherence();
  TRI_ASSERT(bufferEntry._keyIdx < _keyBuffer.size());
  return _keyBuffer[bufferEntry._keyIdx];
}

template <typename Impl, typename Traits>
template <typename ValueType>
typename IResearchViewExecutorBase<Impl, Traits>::template IndexReadBuffer<ValueType>::ScoreIterator
IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::getScores(
    const IResearchViewExecutorBase::IndexReadBufferEntry bufferEntry) noexcept {
  assertSizeCoherence();
  return ScoreIterator{_scoreBuffer, bufferEntry._keyIdx, _numScoreRegisters};
}

template <typename Impl, typename Traits>
template <typename ValueType>
template <typename... Args>
void IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::pushValue(Args&&... args) {
  _keyBuffer.emplace_back(std::forward<Args>(args)...);
}

template <typename Impl, typename Traits>
template <typename ValueType>
void IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::pushScore(float_t const scoreValue) {
  _scoreBuffer.emplace_back(AqlValueHintDouble{scoreValue});
}

template <typename Impl, typename Traits>
template <typename ValueType>
void IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::pushScoreNone() {
  _scoreBuffer.emplace_back();
}

template <typename Impl, typename Traits>
template <typename ValueType>
void IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::reset() noexcept {
  // Should only be called after everything was consumed
  TRI_ASSERT(empty());
  _keyBaseIdx = 0;
  _keyBuffer.clear();
  _scoreBuffer.clear();
}

template <typename Impl, typename Traits>
template <typename ValueType>
std::size_t IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::size() const
    noexcept {
  assertSizeCoherence();
  return _keyBuffer.size() - _keyBaseIdx;
}

template <typename Impl, typename Traits>
template <typename ValueType>
bool IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::empty() const
    noexcept {
  return size() == 0;
}

template <typename Impl, typename Traits>
template <typename ValueType>
typename IResearchViewExecutorBase<Impl, Traits>::IndexReadBufferEntry
IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::pop_front() noexcept {
  TRI_ASSERT(!empty());
  TRI_ASSERT(_keyBaseIdx < _keyBuffer.size());
  assertSizeCoherence();
  IndexReadBufferEntry entry{_keyBaseIdx};
  ++_keyBaseIdx;
  return entry;
}

template <typename Impl, typename Traits>
template <typename ValueType>
void IResearchViewExecutorBase<Impl, Traits>::IndexReadBuffer<ValueType>::assertSizeCoherence() const
    noexcept {
  TRI_ASSERT(_scoreBuffer.size() == _keyBuffer.size() * _numScoreRegisters);
}

template <typename Impl, typename Traits>
IResearchViewExecutorBase<Impl, Traits>::IResearchViewExecutorBase(
    IResearchViewExecutorBase::Fetcher& fetcher, IResearchViewExecutorBase::Infos& infos)
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

template <typename Impl, typename Traits>
std::pair<ExecutionState, typename IResearchViewExecutorBase<Impl, Traits>::Stats>
IResearchViewExecutorBase<Impl, Traits>::produceRows(OutputAqlItemRow& output) {
  IResearchViewStats stats{};
  bool documentWritten = false;

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
      static_cast<Impl&>(*this).reset();
    }

    ReadContext ctx(infos().getOutputRegister(), _inputRow, output);
    documentWritten = next(ctx);

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
std::tuple<ExecutionState, typename IResearchViewExecutorBase<Impl, Traits>::Stats, size_t>
IResearchViewExecutorBase<Impl, Traits>::skipRows(size_t toSkip) {
  TRI_ASSERT(_indexReadBuffer.empty());
  IResearchViewStats stats{};
  size_t skipped = 0;

  auto& impl = static_cast<Impl&>(*this);

  if (!_inputRow.isInitialized()) {
    if (_upstreamState == ExecutionState::DONE) {
      // There will be no more rows, stop fetching.
      return std::make_tuple(ExecutionState::DONE, stats,
                             0);  // tupple, cannot use initializer list due to build failure
    }

    std::tie(_upstreamState, _inputRow) = _fetcher.fetchRow();

    if (_upstreamState == ExecutionState::WAITING) {
      return std::make_tuple(_upstreamState, stats, 0);  // tupple, cannot use initializer list due to build failure
    }

    if (!_inputRow.isInitialized()) {
      return std::make_tuple(ExecutionState::DONE, stats,
                             0);  // tupple, cannot use initializer list due to build failure
    }

    // reset must be called exactly after we've got a new and valid input row.
    impl.reset();
  }

  TRI_ASSERT(_inputRow.isInitialized());

  skipped = static_cast<Impl&>(*this).skip(toSkip);
  TRI_ASSERT(_indexReadBuffer.empty());
  stats.incrScanned(skipped);

  if (skipped < toSkip) {
    _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  }

  return std::make_tuple(ExecutionState::HASMORE, stats,
                         skipped);  // tupple, cannot use initializer list due to build failure
}

template <typename Impl, typename Traits>
bool IResearchViewExecutorBase<Impl, Traits>::next(ReadContext& ctx) {
  auto& impl = static_cast<Impl&>(*this);

  while (true) {
    if (_indexReadBuffer.empty()) {
      impl.fillBuffer(ctx);
    }

    if (_indexReadBuffer.empty()) {
      return false;
    }

    IndexReadBufferEntry bufferEntry = _indexReadBuffer.pop_front();

    if (impl.writeRow(ctx, bufferEntry)) {
      break;
    } else {
      // to get correct stats we should continue looking for
      // other documents inside this one call
      LOG_TOPIC("550cd", TRACE, arangodb::iresearch::TOPIC)
          << "failed to write row in node executor";
    }
  }
  return true;
}

template <typename Impl, typename Traits>
typename IResearchViewExecutorBase<Impl, Traits>::Infos const&
IResearchViewExecutorBase<Impl, Traits>::infos() const noexcept {
  return _infos;
}

template <typename Impl, typename Traits>
void IResearchViewExecutorBase<Impl, Traits>::fillScores(ReadContext const& ctx,
                                                         float_t const* begin,
                                                         float_t const* end) {
  TRI_ASSERT(Traits::Ordered);

  // scorer registers are placed right before document output register
  // is used here currently only for assertions.
  RegisterId scoreReg = infos().getFirstScoreRegister();

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
  TRI_ASSERT(scoreReg == infos().getOutputRegister());
}

template <typename Impl, typename Traits>
void IResearchViewExecutorBase<Impl, Traits>::reset() {
  _ctx._inputRow = _inputRow;

  ExecutionPlan const* plan = &infos().plan();

  QueryContext const queryCtx = {infos().getQuery().trx(), plan, plan->getAst(),
                                 &_ctx, &infos().outVariable()};

  if (infos().volatileFilter() || !_isInitialized) {  // `_volatileSort` implies `_volatileFilter`
    irs::Or root;

    auto rv = FilterFactory::filter(&root, queryCtx, infos().filterCondition());

    if (rv.fail()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          rv.errorNumber(),
          "failed to build filter while querying arangosearch view, query '" +
              infos().filterCondition().toVelocyPack(true)->toJson() +
              "': " + rv.errorMessage());
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
    _filter = root.prepare(*_reader, _order, irs::no_boost(), _filterCtx);

    _isInitialized = true;
  }
}

template<typename Impl, typename Traits>
bool IResearchViewExecutorBase<Impl, Traits>::writeLocalDocumentId(
    ReadContext& ctx,
    LocalDocumentId const& documentId,
    LogicalCollection const& collection) {
  // we will need collection Id also as View could produce documents from multiple collections
  if (ADB_LIKELY(documentId.isSet())) {
    {
      // For sake of performance we store raw pointer to collection
      // It is safe as pipeline work inside one process
      static_assert(sizeof(void*) <= sizeof(uint64_t), "Pointer not fits in uint64_t");
      AqlValue a(AqlValueHintUInt(reinterpret_cast<uint64_t>(&collection)));
      bool mustDestroy = true;
      AqlValueGuard guard{ a, mustDestroy };
      ctx.outputRow.moveValueInto(ctx.getNmColPtrOutReg(), ctx.inputRow, guard);
    }
    {
      AqlValue a(AqlValueHintUInt(documentId.id()));
      bool mustDestroy = true;
      AqlValueGuard guard{ a, mustDestroy };
      ctx.outputRow.moveValueInto(ctx.getNmDocIdOutReg(), ctx.inputRow, guard);
    }
    return true;
  } else {
    return false;
  }
}

template<typename Impl, typename Traits>
bool IResearchViewExecutorBase<Impl, Traits>::writeRow(ReadContext& ctx,
                                                       IndexReadBufferEntry bufferEntry,
                                                       LocalDocumentId const& documentId,
                                                       LogicalCollection const& collection) {
  TRI_ASSERT(documentId.isSet());
  bool writeDocOk;
  if (Traits::Materialized) {
    // read document from underlying storage engine, if we got an id
    writeDocOk = collection.readDocumentWithCallback(infos().getQuery().trx(), documentId, ctx.callback);
  } else {
    // no need to look into collection. Somebody down the stream will do materialization. Just emit LocalDocumentIds
    writeDocOk = writeLocalDocumentId(ctx, documentId, collection);
  }
  if (writeDocOk) {
    // in the ordered case we have to write scores as well as a document
    if /* constexpr */ (Traits::Ordered) {
      // scorer register are placed right before the document output register
      RegisterId scoreReg = infos().getFirstScoreRegister();
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

template <bool ordered, bool materialized>
IResearchViewExecutor<ordered, materialized>::IResearchViewExecutor(Fetcher& fetcher, Infos& infos)
    : Base(fetcher, infos),
      _pkReader(),
      _itr(),
      _readerOffset(0),
      _scr(&irs::score::no_score()),
      _scrVal() {
  TRI_ASSERT(ordered == (infos.getNumScoreRegisters() != 0));
}

template <bool ordered, bool materialized>
void IResearchViewExecutor<ordered, materialized>::evaluateScores(ReadContext const& ctx) {
  // This must not be called in the unordered case.
  TRI_ASSERT(ordered);

  // evaluate scores
  _scr->evaluate();

  // in arangodb we assume all scorers return float_t
  auto begin = reinterpret_cast<const float_t*>(_scrVal.begin());
  auto end = reinterpret_cast<const float_t*>(_scrVal.end());

  this->fillScores(ctx, begin, end);
}

template<bool ordered, bool materialized>
bool IResearchViewExecutor<ordered, materialized>::readPK(LocalDocumentId& documentId) {
  TRI_ASSERT(!documentId.isSet());
  TRI_ASSERT(_itr);
  TRI_ASSERT(_doc);
  TRI_ASSERT(_pkReader);

  if (_itr->next()) {
    if (_pkReader(_doc->value, this->_pk)) {
      bool const readSuccess = DocumentPrimaryKey::read(documentId, this->_pk);

      TRI_ASSERT(readSuccess == documentId.isSet());

      if (!readSuccess) {
        LOG_TOPIC("6442f", WARN, arangodb::iresearch::TOPIC)
            << "failed to read document primary key while reading document "
               "from arangosearch view, doc_id '"
            << _doc->value << "'";
      }
    }

    return true;
  }

  return false;
}

template <bool ordered, bool materialized>
void IResearchViewExecutor<ordered, materialized>::fillBuffer(IResearchViewExecutor::ReadContext& ctx) {
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
      auto collection = lookupCollection(*query.trx(), cid, query);

      if (!collection) {
        std::stringstream msg;
        msg << "failed to find collection while reading document from "
               "arangosearch view, cid '"
            << cid << "'";
        query.registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg.str());

        // We don't have a collection, skip the current reader.
        ++_readerOffset;
        _itr.reset();
        _doc = nullptr;
        continue;
      }

      _collection = collection.get();
      this->_indexReadBuffer.reset();
    }

    TRI_ASSERT(_pkReader);

    LocalDocumentId documentId;

    // try to read a document PK from iresearch
    bool const iteratorExhausted = !readPK(documentId);

    if (!documentId.isSet()) {
      // No document read, we cannot write it.

      if (iteratorExhausted) {
        // The iterator is exhausted, we need to continue with the next reader.
        ++_readerOffset;
        _itr.reset();
        _doc = nullptr;
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
      _doc = nullptr;

      // Here we have at least one document in _indexReadBuffer, so we may not
      // add documents from a new reader.
      break;
    }
    if (this->_indexReadBuffer.size() >= atMost) {
      break;
    }
  }
}

template <bool ordered, bool materialized>
bool IResearchViewExecutor<ordered, materialized>::resetIterator() {
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

  _itr = segmentReader.mask(
      this->_filter->execute(segmentReader, this->_order, this->_filterCtx));
  TRI_ASSERT(_itr);
  _doc = _itr->attributes().get<irs::document>().get();
  TRI_ASSERT(_doc);

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

template <bool ordered, bool materialized>
void IResearchViewExecutor<ordered, materialized>::reset() {
  Base::reset();

  // reset iterator state
  _itr.reset();
  _doc = nullptr;
  _readerOffset = 0;
}

template <bool ordered, bool materialized>
size_t IResearchViewExecutor<ordered, materialized>::skip(size_t limit) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter);

  size_t const toSkip = limit;

  for (size_t count = this->_reader->size(); _readerOffset < count;) {
    if (!_itr && !resetIterator()) {
      continue;
    }

    while (limit && _itr->next()) {
      --limit;
    }

    if (!limit) {
      break;  // do not change iterator if already reached limit
    }

    ++_readerOffset;
    _itr.reset();
    _doc = nullptr;
  }

  // We're in the middle of a reader, save the collection in case produceRows()
  // needs it.
  if (_itr) {
    // CID is constant until the next resetIterator(). Save the corresponding
    // collection so we don't have to look it up every time.

    TRI_voc_cid_t const cid = this->_reader->cid(_readerOffset);
    Query& query = this->infos().getQuery();
    auto collection = lookupCollection(*query.trx(), cid, query);

    if (!collection) {
      std::stringstream msg;
      msg << "failed to find collection while reading document from "
             "arangosearch view, cid '"
          << cid << "'";
      query.registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg.str());

      // We don't have a collection, skip the current reader.
      ++_readerOffset;
      _itr.reset();
      _doc = nullptr;
    }

    this->_indexReadBuffer.reset();
    _collection = collection.get();
  }

  return toSkip - limit;
}

template <bool ordered, bool materialized>
bool IResearchViewExecutor<ordered, materialized>::writeRow(IResearchViewExecutor::ReadContext& ctx,
                                              IResearchViewExecutor::IndexReadBufferEntry bufferEntry) {
  TRI_ASSERT(_collection);

  return Base::writeRow(ctx, bufferEntry,
                        this->_indexReadBuffer.getValue(bufferEntry), *_collection);
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                      IResearchViewMergeExecutor
///////////////////////////////////////////////////////////////////////////////

template <bool ordered, bool materialized>
IResearchViewMergeExecutor<ordered, materialized>::IResearchViewMergeExecutor(Fetcher& fetcher, Infos& infos)
    : Base{fetcher, infos},
      _heap_it{MinHeapContext{*infos.sort().first, infos.sort().second, _segments}} {
  TRI_ASSERT(infos.sort().first);
  TRI_ASSERT(!infos.sort().first->empty());
  TRI_ASSERT(infos.sort().first->size() >= infos.sort().second);
  TRI_ASSERT(infos.sort().second);
  TRI_ASSERT(ordered == (infos.getNumScoreRegisters() != 0));
}

template <bool ordered, bool materialized>
IResearchViewMergeExecutor<ordered, materialized>::Segment::Segment(
    irs::doc_iterator::ptr&& docs, irs::document const& doc,
    irs::score const& score, LogicalCollection const& collection,
    irs::columnstore_reader::values_reader_f&& sortReader,
    irs::columnstore_reader::values_reader_f&& pkReader) noexcept
    : docs(std::move(docs)),
      doc(&doc),
      score(&score),
      collection(&collection),
      sortReader(std::move(sortReader)),
      pkReader(std::move(pkReader)) {
  TRI_ASSERT(this->docs);
  TRI_ASSERT(this->doc);
  TRI_ASSERT(this->score);
  TRI_ASSERT(this->collection);
  TRI_ASSERT(this->pkReader);
}

template <bool ordered, bool materialized>
IResearchViewMergeExecutor<ordered, materialized>::MinHeapContext::MinHeapContext(
    const IResearchViewSort& sort, size_t sortBuckets, std::vector<Segment>& segments) noexcept
    : _less(sort, sortBuckets), _segments(&segments) {}

template <bool ordered, bool materialized>
bool IResearchViewMergeExecutor<ordered, materialized>::MinHeapContext::operator()(const size_t i) const {
  assert(i < _segments->size());
  auto& segment = (*_segments)[i];
  while (segment.docs->next()) {
    auto const doc = segment.docs->value();

    if (segment.sortReader(doc, segment.sortValue)) {
      return true;
    }

    // FIXME read pk as well
  }
  return false;
}

template <bool ordered, bool materialized>
bool IResearchViewMergeExecutor<ordered, materialized>::MinHeapContext::operator()(const size_t lhs,
                                                                     const size_t rhs) const {
  assert(lhs < _segments->size());
  assert(rhs < _segments->size());
  return _less((*_segments)[rhs].sortValue, (*_segments)[lhs].sortValue);
}

template <bool ordered, bool materialized>
void IResearchViewMergeExecutor<ordered, materialized>::evaluateScores(ReadContext const& ctx,
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

template <bool ordered, bool materialized>
void IResearchViewMergeExecutor<ordered, materialized>::reset() {
  Base::reset();

  _segments.clear();
  _segments.reserve(this->_reader->size());

  for (size_t i = 0, size = this->_reader->size(); i < size; ++i) {
    auto& segment = (*this->_reader)[i];

    auto sortReader = ::sortColumn(segment);

    if (!sortReader) {
      LOG_TOPIC("af4cd", WARN, arangodb::iresearch::TOPIC)
          << "encountered a sub-reader without a sort column while "
             "executing a query, ignoring";
      continue;
    }

    irs::doc_iterator::ptr it =
        segment.mask(this->_filter->execute(segment, this->_order, this->_filterCtx));
    TRI_ASSERT(it);

    auto const* doc = it->attributes().get<irs::document>().get();
    TRI_ASSERT(doc);

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
    auto collection = lookupCollection(*query.trx(), cid, query);

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
      LOG_TOPIC("ee041", WARN, arangodb::iresearch::TOPIC)
          << "encountered a sub-reader without a primary key column while "
             "executing a query, ignoring";
      continue;
    }

    _segments.emplace_back(std::move(it), *doc, *score, *collection,
                           std::move(sortReader), std::move(pkReader));
  }

  _heap_it.reset(_segments.size());
}

template <bool ordered, bool materialized>
LocalDocumentId IResearchViewMergeExecutor<ordered, materialized>::readPK(
    IResearchViewMergeExecutor<ordered, materialized>::Segment const& segment) {
  LocalDocumentId documentId;

  if (segment.pkReader(segment.doc->value, this->_pk)) {
    bool const readSuccess = DocumentPrimaryKey::read(documentId, this->_pk);

    TRI_ASSERT(readSuccess == documentId.isSet());

    if (!readSuccess) {
      LOG_TOPIC("6423f", WARN, arangodb::iresearch::TOPIC)
          << "failed to read document primary key while reading document "
             "from arangosearch view, doc_id '"
          << segment.doc->value << "'";
    }
  }

  return documentId;
}

template <bool ordered, bool materialized>
void IResearchViewMergeExecutor<ordered, materialized>::fillBuffer(ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);

  std::size_t const atMost = ctx.outputRow.numRowsLeft();

  while (_heap_it.next()) {
    auto& segment = _segments[_heap_it.value()];
    TRI_ASSERT(segment.docs);
    TRI_ASSERT(segment.doc);
    TRI_ASSERT(segment.score);
    TRI_ASSERT(segment.collection);
    TRI_ASSERT(segment.pkReader);

    // try to read a document PK from iresearch
    LocalDocumentId const documentId = readPK(segment);

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

template <bool ordered, bool materialized>
size_t IResearchViewMergeExecutor<ordered, materialized>::skip(size_t limit) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter != nullptr);

  size_t const toSkip = limit;

  while (limit && _heap_it.next()) {
    --limit;
  }

  return toSkip - limit;
}

template <bool ordered, bool materialized>
bool IResearchViewMergeExecutor<ordered, materialized>::writeRow(
    IResearchViewMergeExecutor::ReadContext& ctx,
    IResearchViewMergeExecutor::IndexReadBufferEntry bufferEntry) {
  auto const& id = this->_indexReadBuffer.getValue(bufferEntry);
  LocalDocumentId const& documentId = id.first;
  TRI_ASSERT(documentId.isSet());
  LogicalCollection const* collection = id.second;
  TRI_ASSERT(collection);

  return Base::writeRow(ctx, bufferEntry, documentId, *collection);
}

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                 explicit template instantiation
///////////////////////////////////////////////////////////////////////////////

template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<false, true>>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<true, true>>;
template class ::arangodb::aql::IResearchViewExecutor<false, true>;
template class ::arangodb::aql::IResearchViewExecutor<true, true>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewMergeExecutor<false, true>>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewMergeExecutor<true, true>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<false, true>;
template class ::arangodb::aql::IResearchViewMergeExecutor<true, true>;

template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<false, false>>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewExecutor<true, false>>;
template class ::arangodb::aql::IResearchViewExecutor<false, false>;
template class ::arangodb::aql::IResearchViewExecutor<true, false>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewMergeExecutor<false, false>>;
template class ::arangodb::aql::IResearchViewExecutorBase<::arangodb::aql::IResearchViewMergeExecutor<true, false>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<false, false>;
template class ::arangodb::aql::IResearchViewMergeExecutor<true, false>;
