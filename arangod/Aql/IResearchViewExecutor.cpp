////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/ExecutionStats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchView.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <analysis/token_attributes.hpp>
#include <search/boolean_filter.hpp>
#include <search/score.hpp>
#include <search/cost.hpp>
#include <utility>

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
using namespace arangodb::basics;
using namespace arangodb::iresearch;

namespace {

const irs::payload NoPayload;

[[maybe_unused]] size_t calculateSkipAllCount(CountApproximate approximation,
                                              size_t currentPos,
                                              irs::doc_iterator* docs) {
  TRI_ASSERT(docs);
  size_t skipped{0};
  switch (approximation) {
    case CountApproximate::Cost: {
      auto* cost = irs::get<irs::cost>(*docs);
      if (ADB_LIKELY(cost)) {
        skipped = cost->estimate();
        skipped -= std::min(skipped, currentPos);
        break;
      }
    }
      [[fallthrough]];
    default:
      // check for unknown approximation or absence of the cost attribute.
      // fallback to exact anyway
      TRI_ASSERT(CountApproximate::Exact == approximation);
      while (docs->next()) {
        skipped++;
      }
      break;
  }
  return skipped;
}

[[maybe_unused]] inline std::shared_ptr<arangodb::LogicalCollection>
lookupCollection(arangodb::transaction::Methods& trx, DataSourceId cid,
                 aql::QueryContext& query) {
  TRI_ASSERT(trx.state());

  // `Methods::documentCollection(DataSourceId)` may throw exception
  auto* collection =
      trx.state()->collection(cid, arangodb::AccessMode::Type::READ);

  if (!collection) {
    std::stringstream msg;
    msg << "failed to find collection while reading document from arangosearch "
           "view, cid '"
        << cid << "'";
    query.warnings().registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                     msg.str());

    return nullptr;  // not a valid collection reference
  }

  return collection->collection();
}

[[maybe_unused]] inline irs::doc_iterator::ptr pkColumn(
    irs::sub_reader const& segment) {
  auto const* reader = segment.column(DocumentPrimaryKey::PK());

  return reader ? reader->iterator(irs::ColumnHint::kNormal) : nullptr;
}

[[maybe_unused]] inline irs::doc_iterator::ptr sortColumn(
    irs::sub_reader const& segment) {
  auto const* reader = segment.sort();

  return reader ? reader->iterator(irs::ColumnHint::kNormal) : nullptr;
}

[[maybe_unused]] inline void reset(ColumnIterator& column,
                                   irs::doc_iterator::ptr&& itr) noexcept {
  TRI_ASSERT(itr);
  column.itr = std::move(itr);
  column.value = irs::get<irs::payload>(*column.itr);
  if (ADB_UNLIKELY(!column.value)) {
    column.value = &NoPayload;
  }
}

class BufferHeapSortContext {
 public:
  explicit BufferHeapSortContext(
      size_t numScoreRegisters,
      std::span<std::pair<size_t, bool> const> scoresSort,
      std::span<float_t const> scoreBuffer)
      : _numScoreRegisters(numScoreRegisters),
        _scoresSort(scoresSort),
        _scoreBuffer(scoreBuffer) {}

  bool operator()(size_t a, size_t b) const noexcept {
    auto const* rhs_scores = &_scoreBuffer[b * _numScoreRegisters];
    auto lhs_scores = &_scoreBuffer[a * _numScoreRegisters];
    for (auto const& cmp : _scoresSort) {
      if (lhs_scores[cmp.first] != rhs_scores[cmp.first]) {
        return cmp.second ? lhs_scores[cmp.first] < rhs_scores[cmp.first]
                          : lhs_scores[cmp.first] > rhs_scores[cmp.first];
      }
    }
    return false;
  }

  bool compareInput(size_t lhsIdx, float_t const* rhs_scores) const noexcept {
    auto lhs_scores = &_scoreBuffer[lhsIdx * _numScoreRegisters];
    for (auto const& cmp : _scoresSort) {
      if (lhs_scores[cmp.first] != rhs_scores[cmp.first]) {
        return cmp.second ? lhs_scores[cmp.first] < rhs_scores[cmp.first]
                          : lhs_scores[cmp.first] > rhs_scores[cmp.first];
      }
    }
    return false;
  }

 private:
  size_t _numScoreRegisters;
  std::span<std::pair<size_t, bool> const> _scoresSort;
  std::span<float_t const> _scoreBuffer;
};  //  BufferHeapSortContext

}  // namespace

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    ViewSnapshotPtr reader, OutRegisters outRegisters,
    RegisterId searchDocRegister, std::vector<RegisterId> scoreRegisters,
    arangodb::aql::QueryContext& query, std::vector<SearchFunc> const& scorers,
    std::pair<arangodb::iresearch::IResearchSortBase const*, size_t> sort,
    IResearchViewStoredValues const& storedValues, ExecutionPlan const& plan,
    Variable const& outVariable, aql::AstNode const& filterCondition,
    std::pair<bool, bool> volatility,
    IResearchViewExecutorInfos::VarInfoMap const& varInfoMap, int depth,
    IResearchViewNode::ViewValuesRegisters&& outNonMaterializedViewRegs,
    iresearch::CountApproximate countApproximate,
    iresearch::FilterOptimization filterOptimization,
    std::vector<std::pair<size_t, bool>> scorersSort, size_t scorersSortLimit,
    iresearch::SearchMeta const* meta)
    : _searchDocOutReg{searchDocRegister},
      _scoreRegisters{std::move(scoreRegisters)},
      _scoreRegistersCount{_scoreRegisters.size()},
      _reader{std::move(reader)},
      _query{query},
      _scorers{scorers},
      _sort{std::move(sort)},
      _storedValues{storedValues},
      _plan{plan},
      _outVariable{outVariable},
      _filterCondition{filterCondition},
      _varInfoMap{varInfoMap},
      _outNonMaterializedViewRegs{std::move(outNonMaterializedViewRegs)},
      _countApproximate{countApproximate},
      _filterOptimization{filterOptimization},
      _scorersSort{std::move(scorersSort)},
      _scorersSortLimit{scorersSortLimit},
      _meta{meta},
      _depth{depth},
      _filterConditionIsEmpty{isFilterConditionEmpty(&_filterCondition)},
      _volatileSort{volatility.second},
      // `_volatileSort` implies `_volatileFilter`
      _volatileFilter{_volatileSort || volatility.first} {
  TRI_ASSERT(_reader != nullptr);
  std::tie(_documentOutReg, _collectionPointerReg) = std::visit(
      overload{
          [&](aql::IResearchViewExecutorInfos::MaterializeRegisters regs) {
            return std::pair{regs.documentOutReg,
                             aql::RegisterPlan::MaxRegisterId};
          },
          [&](aql::IResearchViewExecutorInfos::LateMaterializeRegister regs) {
            return std::pair{regs.documentOutReg, regs.collectionOutReg};
          },
          [&](aql::IResearchViewExecutorInfos::NoMaterializeRegisters) {
            return std::pair{aql::RegisterPlan::MaxRegisterId,
                             aql::RegisterPlan::MaxRegisterId};
          }},
      outRegisters);
}

IResearchViewNode::ViewValuesRegisters const&
IResearchViewExecutorInfos::getOutNonMaterializedViewRegs() const noexcept {
  return _outNonMaterializedViewRegs;
}

ViewSnapshotPtr IResearchViewExecutorInfos::getReader() const noexcept {
  return _reader;
}

aql::QueryContext& IResearchViewExecutorInfos::getQuery() noexcept {
  return _query;
}

const std::vector<arangodb::iresearch::SearchFunc>&
IResearchViewExecutorInfos::scorers() const noexcept {
  return _scorers;
}

std::vector<RegisterId> const& IResearchViewExecutorInfos::getScoreRegisters()
    const noexcept {
  return _scoreRegisters;
}

ExecutionPlan const& IResearchViewExecutorInfos::plan() const noexcept {
  return _plan;
}

Variable const& IResearchViewExecutorInfos::outVariable() const noexcept {
  return _outVariable;
}

aql::AstNode const& IResearchViewExecutorInfos::filterCondition()
    const noexcept {
  return _filterCondition;
}

const IResearchViewExecutorInfos::VarInfoMap&
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

bool IResearchViewExecutorInfos::isOldMangling() const noexcept {
  return _meta == nullptr;
}

const std::pair<const arangodb::iresearch::IResearchSortBase*, size_t>&
IResearchViewExecutorInfos::sort() const noexcept {
  return _sort;
}

IResearchViewStoredValues const& IResearchViewExecutorInfos::storedValues()
    const noexcept {
  return _storedValues;
}

auto IResearchViewExecutorInfos::getDocumentRegister() const noexcept
    -> RegisterId {
  return _documentOutReg;
}

auto IResearchViewExecutorInfos::getCollectionRegister() const noexcept
    -> RegisterId {
  return _collectionPointerReg;
}

IResearchViewStats::IResearchViewStats() noexcept : _scannedIndex(0) {}
void IResearchViewStats::incrScanned() noexcept { _scannedIndex++; }
void IResearchViewStats::incrScanned(size_t value) noexcept {
  _scannedIndex = _scannedIndex + value;
}
size_t IResearchViewStats::getScanned() const noexcept { return _scannedIndex; }
ExecutionStats& aql::operator+=(
    ExecutionStats& executionStats,
    const IResearchViewStats& iResearchViewStats) noexcept {
  executionStats.scannedIndex += iResearchViewStats.getScanned();
  return executionStats;
}

template<typename Impl, typename ExecutionTraits>
template<arangodb::iresearch::MaterializeType, typename>
IndexIterator::DocumentCallback IResearchViewExecutorBase<
    Impl, ExecutionTraits>::ReadContext::copyDocumentCallback(ReadContext&
                                                                  ctx) {
  typedef std::function<IndexIterator::DocumentCallback(ReadContext&)>
      CallbackFactory;

  static CallbackFactory const callbackFactory{[](ReadContext& ctx) {
    return [&ctx](LocalDocumentId /*id*/, VPackSlice doc) {
      ctx.outputRow.moveValueInto(ctx.getDocumentReg(), ctx.inputRow, doc);
      return true;
    };
  }};

  return callbackFactory(ctx);
}
template<typename Impl, typename ExecutionTraits>
IResearchViewExecutorBase<Impl, ExecutionTraits>::ReadContext::ReadContext(
    aql::RegisterId documentOutReg, aql::RegisterId collectionPointerReg,
    InputAqlItemRow& inputRow, OutputAqlItemRow& outputRow)
    : inputRow(inputRow),
      outputRow(outputRow),
      documentOutReg(documentOutReg),
      collectionPointerReg(collectionPointerReg) {
  if constexpr (static_cast<unsigned int>(
                    Traits::MaterializeType &
                    iresearch::MaterializeType::Materialize) ==
                static_cast<unsigned int>(
                    iresearch::MaterializeType::Materialize)) {
    callback = this->copyDocumentCallback(*this);
  }
}

IndexReadBufferEntry::IndexReadBufferEntry(size_t keyIdx) noexcept
    : _keyIdx(keyIdx) {}

ScoreIterator::ScoreIterator(std::span<float_t> scoreBuffer, size_t keyIdx,
                             size_t numScores) noexcept
    : _scoreBuffer(scoreBuffer),
      _scoreBaseIdx(keyIdx * numScores),
      _numScores(numScores) {
  TRI_ASSERT(_scoreBaseIdx + _numScores <= _scoreBuffer.size());
}

auto ScoreIterator::begin() noexcept {
  return _scoreBuffer.begin() + static_cast<ptrdiff_t>(_scoreBaseIdx);
}

auto ScoreIterator::end() noexcept {
  return _scoreBuffer.begin() +
         static_cast<ptrdiff_t>(_scoreBaseIdx + _numScores);
}

template<typename ValueType, bool copyStored>
IndexReadBuffer<ValueType, copyStored>::IndexReadBuffer(
    size_t const numScoreRegisters, ResourceMonitor& monitor)
    : _numScoreRegisters{numScoreRegisters},
      _keyBaseIdx{0},
      _maxSize{0},
      _heapSizeLeft{0},
      _storedValuesCount{0},
      _memoryTracker{monitor} {
  // FIXME(gnusi): reserve memory for vectors?
}

template<typename ValueType, bool copyStored>
ValueType const& IndexReadBuffer<ValueType, copyStored>::getValue(
    const IndexReadBufferEntry bufferEntry) const noexcept {
  assertSizeCoherence();
  TRI_ASSERT(bufferEntry._keyIdx < _keyBuffer.size());
  return _keyBuffer[bufferEntry._keyIdx];
}

template<typename ValueType, bool copyStored>
ScoreIterator IndexReadBuffer<ValueType, copyStored>::getScores(
    const IndexReadBufferEntry bufferEntry) noexcept {
  assertSizeCoherence();
  return ScoreIterator{_scoreBuffer, bufferEntry._keyIdx, _numScoreRegisters};
}

template<typename ValueType, bool copySorted>
template<typename... Args>
void IndexReadBuffer<ValueType, copySorted>::pushValue(Args&&... args) {
  _keyBuffer.emplace_back(std::forward<Args>(args)...);
}

template<typename ValueType, bool copySorted>
void IndexReadBuffer<ValueType, copySorted>::finalizeHeapSort() {
  std::sort(
      _rows.begin(), _rows.end(),
      BufferHeapSortContext{_numScoreRegisters, _scoresSort, _scoreBuffer});
  if (_heapSizeLeft) {
    // heap was not filled up to the limit. So fill buffer here.
    _storedValuesBuffer.resize(_keyBuffer.size() * _storedValuesCount);
  }
}

template<typename ValueType, bool copySorted>
void IndexReadBuffer<ValueType, copySorted>::pushSortedValue(
    ValueType&& value, float_t const* scores, size_t count) {
  BufferHeapSortContext sortContext(_numScoreRegisters, _scoresSort,
                                    _scoreBuffer);
  TRI_ASSERT(_maxSize);
  if (!_heapSizeLeft) {
    if (sortContext.compareInput(_rows.front(), scores)) {
      return;  // not interested in this document
    }
    std::pop_heap(_rows.begin(), _rows.end(), sortContext);
    // now last contains "free" index in the buffer
    _keyBuffer[_rows.back()] = std::move(value);
    auto const base = _rows.back() * _numScoreRegisters;
    size_t i{0};
    auto bufferIt = _scoreBuffer.begin() + base;
    for (; i < count; ++i) {
      *bufferIt = scores[i];
      ++bufferIt;
    }
    while (i < _numScoreRegisters) {
      *bufferIt = std::numeric_limits<float_t>::quiet_NaN();
      ++i;
      ++bufferIt;
    }
    std::push_heap(_rows.begin(), _rows.end(), sortContext);
  } else {
    _keyBuffer.emplace_back(std::move(value));
    size_t i = 0;
    for (; i < count; ++i) {
      _scoreBuffer.emplace_back(scores[i]);
    }
    TRI_ASSERT(i <= _numScoreRegisters);
    _scoreBuffer.resize(_scoreBuffer.size() + _numScoreRegisters - i,
                        std::numeric_limits<float_t>::quiet_NaN());
    _rows.push_back(_rows.size());
    if ((--_heapSizeLeft) == 0) {
      _storedValuesBuffer.resize(_keyBuffer.size() * _storedValuesCount);
      std::make_heap(_rows.begin(), _rows.end(), sortContext);
    }
  }
}

template<typename ValueType, bool copyStored>
typename IndexReadBuffer<ValueType, copyStored>::StoredValuesContainer const&
IndexReadBuffer<ValueType, copyStored>::getStoredValues() const noexcept {
  return _storedValuesBuffer;
}

template<typename ValueType, bool copyStored>
irs::score_t* IndexReadBuffer<ValueType, copyStored>::pushNoneScores(
    size_t count) {
  const size_t prevSize = _scoreBuffer.size();
  // count is likely 1 as using several different scores in one query
  // makes not much sense
  while (count--) {
    _scoreBuffer.emplace_back(std::numeric_limits<float_t>::quiet_NaN());
  }
  return _scoreBuffer.data() + prevSize;
}

template<typename ValueType, bool copyStored>
void IndexReadBuffer<ValueType, copyStored>::clear() noexcept {
  _keyBaseIdx = 0;
  _heapSizeLeft = _maxSize;
  _keyBuffer.clear();
  _scoreBuffer.clear();
  _searchDocs.clear();
  _storedValuesBuffer.clear();
  _rows.clear();
}

template<typename ValueType, bool copyStored>
size_t IndexReadBuffer<ValueType, copyStored>::size() const noexcept {
  assertSizeCoherence();
  return _keyBuffer.size() - _keyBaseIdx;
}

template<typename ValueType, bool copyStored>
bool IndexReadBuffer<ValueType, copyStored>::empty() const noexcept {
  return size() == 0;
}

template<typename ValueType, bool copyStored>
IndexReadBufferEntry
IndexReadBuffer<ValueType, copyStored>::pop_front() noexcept {
  TRI_ASSERT(!empty());
  TRI_ASSERT(_keyBaseIdx < _keyBuffer.size());
  assertSizeCoherence();
  size_t key = _keyBaseIdx;
  if (!_rows.empty()) {
    TRI_ASSERT(!_scoresSort.empty());
    key = _rows[_keyBaseIdx];
  }
  IndexReadBufferEntry entry{key};
  ++_keyBaseIdx;
  return entry;
}

template<typename ValueType, bool copyStored>
void IndexReadBuffer<ValueType, copyStored>::assertSizeCoherence()
    const noexcept {
  TRI_ASSERT(_scoreBuffer.size() == _keyBuffer.size() * _numScoreRegisters);
}

template<typename Impl, typename ExecutionTraits>
IResearchViewExecutorBase<Impl, ExecutionTraits>::IResearchViewExecutorBase(
    Fetcher&, Infos& infos)
    : _trx(infos.getQuery().newTrxContext()),
      _infos(infos),
      _inputRow(CreateInvalidInputRowHint{}),  // TODO: Remove me after refactor
      _indexReadBuffer(_infos.getScoreRegisters().size(),
                       _infos.getQuery().resourceMonitor()),
      _ctx(_trx, infos.getQuery(), _aqlFunctionsInternalCache,
           infos.outVariable(), infos.varInfoMap(), infos.getDepth()),
      _filterCtx(_ctx),  // arangodb::iresearch::ExpressionExecutionContext
      _reader(infos.getReader()),
      _filter(irs::filter::prepared::empty()),
      _isInitialized(false) {
  if constexpr (ExecutionTraits::EmitSearchDoc) {
    TRI_ASSERT(this->infos().searchDocIdRegId().isValid());
    auto const* key = &infos.outVariable();
    _filterCookie = &ensureFilterCookie(_trx, key).filter;
  }

  if (auto const* meta = infos.meta(); meta != nullptr) {
    auto const& vocbase = _trx.vocbase();
    auto const& analyzerFeature =
        vocbase.server().getFeature<IResearchAnalyzerFeature>();
    TRI_ASSERT(_trx.state());
    auto const& revision = _trx.state()->analyzersRevision();
    auto getAnalyzer = [&](std::string_view shortName) {
      auto analyzer = analyzerFeature.get(shortName, vocbase, revision);
      if (!analyzer) {
        return emptyAnalyzer();
      }
      return FieldMeta::Analyzer{std::move(analyzer), std::string{shortName}};
    };
    _provider = meta->createProvider(getAnalyzer);
  }
}

template<typename Impl, typename ExecutionTraits>
void IResearchViewExecutorBase<Impl, ExecutionTraits>::initializeCursor() {
  _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  _isInitialized = false;
}

template<typename Impl, typename ExecutionTraits>
std::tuple<ExecutorState,
           typename IResearchViewExecutorBase<Impl, ExecutionTraits>::Stats,
           AqlCall>
IResearchViewExecutorBase<Impl, ExecutionTraits>::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  IResearchViewStats stats{};
  AqlCall upstreamCall{};
  upstreamCall.fullCount = output.getClientCall().fullCount;
  while (inputRange.hasDataRow() && !output.isFull()) {
    bool documentWritten = false;

    while (!documentWritten) {
      if (!_inputRow.isInitialized()) {
        std::tie(std::ignore, _inputRow) = inputRange.peekDataRow();

        if (!_inputRow.isInitialized()) {
          return {inputRange.upstreamState(), stats, upstreamCall};
        }

        // reset must be called exactly after we've got a new and valid input
        // row.
        static_cast<Impl&>(*this).reset();
      }

      ReadContext ctx(infos().getDocumentRegister(),
                      infos().getCollectionRegister(), _inputRow, output);
      documentWritten = next(ctx, stats);

      if (documentWritten) {
        if constexpr (!Traits::ExplicitScanned) {
          stats.incrScanned();
        }
        output.advanceRow();
      } else {
        _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
        inputRange.advanceDataRow();
        // no document written, repeat.
      }
    }
  }
  auto reportedState = inputRange.upstreamState();
  if constexpr (Traits::ExplicitScanned) {
    if (inputRange.upstreamState() == ExecutorState::DONE &&
        output.getClientCall().fullCount &&
        static_cast<Impl&>(*this).canSkipAll()) {
      // force skipAll call
      reportedState = ExecutorState::HASMORE;
    }
  }
  return {reportedState, stats, upstreamCall};
}

template<typename Impl, typename ExecutionTraits>
std::tuple<ExecutorState,
           typename IResearchViewExecutorBase<Impl, ExecutionTraits>::Stats,
           size_t, AqlCall>
IResearchViewExecutorBase<Impl, ExecutionTraits>::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& call) {
  TRI_ASSERT(_indexReadBuffer.empty() ||
             (!this->infos().scoresSort().empty() && call.needsFullCount()));
  auto& impl = static_cast<Impl&>(*this);
  IResearchViewStats stats{};
  while (inputRange.hasDataRow() && call.shouldSkip()) {
    if (!_inputRow.isInitialized()) {
      auto rowState = ExecutorState::HASMORE;
      std::tie(rowState, _inputRow) = inputRange.peekDataRow();

      if (!_inputRow.isInitialized()) {
        TRI_ASSERT(rowState == ExecutorState::DONE);
        break;
      }

      // reset must be called exactly after we've got a new and valid input row.
      impl.reset();
    }
    TRI_ASSERT(_inputRow.isInitialized());
    if (call.getOffset() > 0) {
      // OffsetPhase need to skip atMost offset
      call.didSkip(impl.skip(call.getOffset(), stats));
    } else {
      TRI_ASSERT(call.getLimit() == 0 && call.hasHardLimit());
      // skip all - fullCount phase
      call.didSkip(impl.skipAll(stats));
    }
    // only heapsort version could possibly fetch more than skip requested
    TRI_ASSERT(_indexReadBuffer.empty() || !this->infos().scoresSort().empty());

    if (call.shouldSkip()) {
      // We still need to fetch more
      // trigger refetch of new input row
      inputRange.advanceDataRow();
      _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
    }
  }
  if constexpr (!Traits::ExplicitScanned) {
    stats.incrScanned(call.getSkipCount());
  }

  AqlCall upstreamCall{};
  if (!call.needsFullCount()) {
    // Do not overfetch too much.
    upstreamCall.softLimit =
        call.getOffset() + std::min(call.softLimit, call.hardLimit);
    // else we do unlimited softLimit.
    // we are going to return everything anyways.
  }

  auto const reportedState = inputRange.upstreamState();
  if constexpr (Traits::ExplicitScanned) {
    if (inputRange.upstreamState() == ExecutorState::DONE &&
        call.needsFullCount() && impl.canSkipAll() && call.getOffset() == 0) {
      // execute skipAll immediately
      call.didSkip(impl.skipAll(stats));
    }
  }

  return {reportedState, stats, call.getSkipCount(), upstreamCall};
}

template<typename Impl, typename ExecutionTraits>
bool IResearchViewExecutorBase<Impl, ExecutionTraits>::next(
    ReadContext& ctx, IResearchViewStats& stats) {
  auto& impl = static_cast<Impl&>(*this);

  while (true) {
    if (_indexReadBuffer.empty()) {
      impl.fillBuffer(ctx);
      if constexpr (Traits::ExplicitScanned) {
        if (!_indexReadBuffer.empty()) {
          stats.incrScanned(static_cast<Impl&>(*this).getScanned());
        }
      }
    }
    if (_indexReadBuffer.empty()) {
      return false;
    }
    IndexReadBufferEntry const bufferEntry = _indexReadBuffer.pop_front();

    if (ADB_LIKELY(impl.writeRow(ctx, bufferEntry))) {
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

template<typename Impl, typename ExecutionTraits>
void IResearchViewExecutorBase<Impl, ExecutionTraits>::reset() {
  _ctx._inputRow = _inputRow;

  // `_volatileSort` implies `_volatileFilter`
  if (infos().volatileFilter() || !_isInitialized) {
    irs::Or root;

    iresearch::QueryContext queryCtx{
        .trx = &_trx,
        .ast = infos().plan().getAst(),
        .ctx = &_ctx,
        .index = _reader.get(),
        .ref = &infos().outVariable(),
        .filterOptimization = infos().filterOptimization(),
        .isSearchQuery = true,
        .isOldMangling = infos().isOldMangling(),
        .hasNestedFields = _reader->hasNestedFields()};

    // The analyzer is referenced in the FilterContext and used during the
    // following ::makeFilter() call, so can't be a temporary.
    FieldMeta::Analyzer const identity{IResearchAnalyzerFeature::identity()};
    AnalyzerProvider* fieldAnalyzerProvider = nullptr;
    auto const* contextAnalyzer = &identity;
    if (!infos().isOldMangling()) {
      fieldAnalyzerProvider = &_provider;
      contextAnalyzer = &emptyAnalyzer();
    }
    FilterContext const filterCtx{
        .fieldAnalyzerProvider = fieldAnalyzerProvider,
        .contextAnalyzer = *contextAnalyzer};

    auto const rv = FilterFactory::filter(&root, queryCtx, filterCtx,
                                          infos().filterCondition());

    if (rv.fail()) {
      arangodb::velocypack::Builder builder;
      infos().filterCondition().toVelocyPack(builder, true);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          rv.errorNumber(),
          StringUtils::concatT("failed to build filter while querying "
                               "arangosearch view, query '",
                               builder.toJson(), "': ", rv.errorMessage()));
    }

    if (infos().volatileSort() || !_isInitialized) {
      auto const& scorers = infos().scorers();

      containers::SmallVector<irs::sort::ptr, 2> order;
      order.reserve(scorers.size());

      for (irs::sort::ptr scorer; auto const& scorerNode : scorers) {
        TRI_ASSERT(scorerNode.node);

        if (!order_factory::scorer(&scorer, *scorerNode.node, queryCtx)) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "failed to build scorers while querying arangosearch view");
        }

        assert(scorer);
        order.emplace_back(std::move(scorer));
      }

      // compile order
      _order = irs::Order::Prepare(order);
    }

    // compile filter
    _filter = root.prepare(*_reader, _order, irs::kNoBoost, &_filterCtx);

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(_filterCookie);
      *_filterCookie = _filter.get();
    }

    _isInitialized = true;
  }
}

template<typename Impl, typename ExecutionTraits>
void IResearchViewExecutorBase<Impl, ExecutionTraits>::writeSearchDoc(
    ReadContext& ctx, SearchDoc const& doc, RegisterId reg) {
  TRI_ASSERT(doc.isValid());
  AqlValue value{doc.encode(_buf)};
  AqlValueGuard guard{value, true};
  ctx.outputRow.moveValueInto(reg, ctx.inputRow, guard);
}

template<typename Impl, typename ExecutionTraits>
template<arangodb::iresearch::MaterializeType, typename>
bool IResearchViewExecutorBase<Impl, ExecutionTraits>::writeLocalDocumentId(
    ReadContext& ctx, LocalDocumentId const& documentId,
    LogicalCollection const& collection) {
  // we will need collection Id also as View could produce documents from
  // multiple collections
  if (ADB_LIKELY(documentId.isSet())) {
    {
      // For sake of performance we store raw pointer to collection
      // It is safe as pipeline work inside one process
      static_assert(sizeof(void*) <= sizeof(uint64_t),
                    "Pointer doesn't fit uint64_t");

      AqlValueHintUInt const value{reinterpret_cast<uint64_t>(&collection)};
      ctx.outputRow.moveValueInto(ctx.getCollectionPointerReg(), ctx.inputRow,
                                  value);
    }
    {
      AqlValueHintUInt const value{documentId.id()};
      ctx.outputRow.moveValueInto(ctx.getDocumentIdReg(), ctx.inputRow, value);
    }
    return true;
  } else {
    return false;
  }
}

template<typename Impl, typename ExecutionTraits>
inline bool IResearchViewExecutorBase<Impl, ExecutionTraits>::writeStoredValue(
    ReadContext& ctx,
    typename IResearchViewExecutorBase<Impl, ExecutionTraits>::
        IndexReadBufferType::StoredValuesContainer const& storedValues,
    size_t index, std::map<size_t, RegisterId> const& fieldsRegs) {
  TRI_ASSERT(index < storedValues.size());
  auto const& storedValue = storedValues[index];
  TRI_ASSERT(!storedValue.empty());
  auto const totalSize = storedValue.size();
  VPackSlice slice{storedValue.c_str()};
  size_t size = 0;
  size_t i = 0;
  for (auto const& [fieldNum, registerId] : fieldsRegs) {
    while (i < fieldNum) {
      size += slice.byteSize();
      TRI_ASSERT(size <= totalSize);
      if (ADB_UNLIKELY(size > totalSize)) {
        return false;
      }
      slice = VPackSlice{slice.end()};
      ++i;
    }
    TRI_ASSERT(!slice.isNone());

    ctx.outputRow.moveValueInto(registerId, ctx.inputRow, slice);
  }
  return true;
}

template<typename Impl, typename ExecutionTraits>
bool IResearchViewExecutorBase<Impl, ExecutionTraits>::writeRow(
    ReadContext& ctx, IndexReadBufferEntry bufferEntry,
    LocalDocumentId const& documentId, LogicalCollection const& collection) {
  TRI_ASSERT(documentId.isSet());

  if constexpr (ExecutionTraits::EmitSearchDoc) {
    auto reg = this->infos().searchDocIdRegId();
    TRI_ASSERT(reg.isValid());

    this->writeSearchDoc(
        ctx, this->_indexReadBuffer.getSearchDoc(bufferEntry.getKeyIdx()), reg);
  }
  if constexpr (Traits::MaterializeType == MaterializeType::Materialize) {
    // read document from underlying storage engine, if we got an id
    if (ADB_UNLIKELY(
            !collection.getPhysical()
                 ->read(&_trx, documentId, ctx.callback, ReadOwnWrites::no)
                 .ok())) {
      return false;
    }
  } else if constexpr ((Traits::MaterializeType &
                        MaterializeType::LateMaterialize) ==
                       MaterializeType::LateMaterialize) {
    // no need to look into collection. Somebody down the stream will do
    // materialization. Just emit LocalDocumentIds
    if (ADB_UNLIKELY(!writeLocalDocumentId(ctx, documentId, collection))) {
      return false;
    }
  }
  if constexpr ((Traits::MaterializeType & MaterializeType::UseStoredValues) ==
                MaterializeType::UseStoredValues) {
    auto const& columnsFieldsRegs = infos().getOutNonMaterializedViewRegs();
    TRI_ASSERT(!columnsFieldsRegs.empty());
    auto const& storedValues = _indexReadBuffer.getStoredValues();
    auto index = bufferEntry.getKeyIdx() * columnsFieldsRegs.size();
    TRI_ASSERT(index < storedValues.size());
    for (auto it = columnsFieldsRegs.cbegin(); it != columnsFieldsRegs.cend();
         ++it) {
      if (ADB_UNLIKELY(
              !writeStoredValue(ctx, storedValues, index++, it->second))) {
        return false;
      }
    }
  } else if constexpr (Traits::MaterializeType ==
                           MaterializeType::NotMaterialize &&
                       !Traits::Ordered && !Traits::EmitSearchDoc) {
    ctx.outputRow.copyRow(ctx.inputRow);
  }
  // in the ordered case we have to write scores as well as a document
  if constexpr (Traits::Ordered) {
    // scorer register are placed right before the document output register
    std::vector<RegisterId> const& scoreRegisters = infos().getScoreRegisters();
    auto scoreRegIter = scoreRegisters.begin();
    for (auto const& it : _indexReadBuffer.getScores(bufferEntry)) {
      TRI_ASSERT(scoreRegIter != scoreRegisters.end());
      auto const val = AqlValueHintDouble(it);
      ctx.outputRow.moveValueInto(*scoreRegIter, ctx.inputRow, val);
      ++scoreRegIter;
    }

    // we should have written exactly all score registers by now
    TRI_ASSERT(scoreRegIter == scoreRegisters.end());
  } else {
    UNUSED(bufferEntry);
  }
  return true;
}

template<typename Impl, typename ExecutionTraits>
void IResearchViewExecutorBase<Impl, ExecutionTraits>::readStoredValues(
    irs::document const& doc, size_t index) {
  TRI_ASSERT(index < _storedValuesReaders.size());
  auto const& reader = _storedValuesReaders[index];
  TRI_ASSERT(reader.itr);
  TRI_ASSERT(reader.value);
  auto const& payload = reader.value->value;
  bool const found = (doc.value == reader.itr->seek(doc.value));
  if (found && !payload.empty()) {
    _indexReadBuffer.pushStoredValue(payload);
  } else {
    _indexReadBuffer.pushStoredValue(
        ref<irs::byte_type>(VPackSlice::nullSlice()));
  }
}

template<typename Impl, typename ExecutionTraits>
void IResearchViewExecutorBase<Impl, ExecutionTraits>::pushStoredValues(
    irs::document const& doc, size_t storedValuesIndex /*= 0*/) {
  auto const& columnsFieldsRegs = _infos.getOutNonMaterializedViewRegs();
  TRI_ASSERT(!columnsFieldsRegs.empty());
  auto index = storedValuesIndex * columnsFieldsRegs.size();
  for (auto it = columnsFieldsRegs.cbegin(); it != columnsFieldsRegs.cend();
       ++it) {
    readStoredValues(doc, index++);
  }
}

template<typename Impl, typename ExecutionTraits>
bool IResearchViewExecutorBase<Impl, ExecutionTraits>::getStoredValuesReaders(
    irs::sub_reader const& segmentReader, size_t storedValuesIndex /*= 0*/) {
  auto const& columnsFieldsRegs = _infos.getOutNonMaterializedViewRegs();
  if (!columnsFieldsRegs.empty()) {
    auto columnFieldsRegs = columnsFieldsRegs.cbegin();
    auto index = storedValuesIndex * columnsFieldsRegs.size();
    if (IResearchViewNode::kSortColumnNumber == columnFieldsRegs->first) {
      auto sortReader = ::sortColumn(segmentReader);
      if (ADB_UNLIKELY(!sortReader)) {
        LOG_TOPIC("bc5bd", WARN, arangodb::iresearch::TOPIC)
            << "encountered a sub-reader without a sort column while "
               "executing a query, ignoring";
        return false;
      }
      ::reset(_storedValuesReaders[index++], std::move(sortReader));
      ++columnFieldsRegs;
    }
    // if stored values exist
    if (columnFieldsRegs != columnsFieldsRegs.cend()) {
      auto const& columns = _infos.storedValues().columns();
      TRI_ASSERT(!columns.empty());
      for (; columnFieldsRegs != columnsFieldsRegs.cend(); ++columnFieldsRegs) {
        TRI_ASSERT(IResearchViewNode::kSortColumnNumber <
                   columnFieldsRegs->first);
        auto const storedColumnNumber =
            static_cast<size_t>(columnFieldsRegs->first);
        TRI_ASSERT(storedColumnNumber < columns.size());
        auto const* storedValuesReader =
            segmentReader.column(columns[storedColumnNumber].name);
        if (ADB_UNLIKELY(!storedValuesReader)) {
          LOG_TOPIC("af7ec", WARN, arangodb::iresearch::TOPIC)
              << "encountered a sub-reader without a stored value column while "
                 "executing a query, ignoring";
          return false;
        }
        ::reset(_storedValuesReaders[index++],
                storedValuesReader->iterator(irs::ColumnHint::kNormal));
      }
    }
  }
  return true;
}

template<typename ExecutionTraits>
IResearchViewExecutor<ExecutionTraits>::IResearchViewExecutor(Fetcher& fetcher,
                                                              Infos& infos)
    : Base{fetcher, infos},
      _readerOffset{0},
      _currentSegmentPos{0},
      _totalPos{0},
      _scr{&irs::score::kNoScore},
      _numScores{0} {
  this->_storedValuesReaders.resize(
      this->_infos.getOutNonMaterializedViewRegs().size());
  TRI_ASSERT(infos.scoresSort().empty());
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::readPK(
    LocalDocumentId& documentId) {
  TRI_ASSERT(!documentId.isSet());
  TRI_ASSERT(_itr);
  TRI_ASSERT(_doc);
  TRI_ASSERT(_pkReader.itr);
  TRI_ASSERT(_pkReader.value);

  if (_itr->next()) {
    ++_totalPos;
    ++_currentSegmentPos;
    if (_doc->value == _pkReader.itr->seek(_doc->value)) {
      bool const readSuccess =
          DocumentPrimaryKey::read(documentId, _pkReader.value->value);

      TRI_ASSERT(readSuccess == documentId.isSet());

      if (ADB_UNLIKELY(!readSuccess)) {
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

template<typename ExecutionTraits>
IResearchViewHeapSortExecutor<ExecutionTraits>::IResearchViewHeapSortExecutor(
    Fetcher& fetcher, Infos& infos)
    : Base{fetcher, infos} {
  this->_indexReadBuffer.setScoresSort(this->_infos.scoresSort());
}

template<typename ExecutionTraits>
size_t IResearchViewHeapSortExecutor<ExecutionTraits>::skipAll(
    IResearchViewStats& stats) {
  TRI_ASSERT(this->_filter);
  size_t totalSkipped{0};
  if (_bufferFilled) {
    // we already know exact full count after buffer filling
    TRI_ASSERT(_bufferedCount >= this->_indexReadBuffer.size());
    totalSkipped =
        _totalCount - (_bufferedCount - this->_indexReadBuffer.size());
  } else {
    // Looks like this is currently unreachable.
    // If this assert is ever triggered, tests for such case should be added.
    // But implementations should work.
    TRI_ASSERT(false);
    size_t const count = this->_reader->size();
    for (size_t readerOffset = 0; readerOffset < count; ++readerOffset) {
      auto& segmentReader = (*this->_reader)[readerOffset];
      auto itr = this->_filter->execute({.segment = segmentReader,
                                         .scorers = this->_order,
                                         .ctx = &this->_filterCtx,
                                         .mode = irs::ExecutionMode::kAll});
      TRI_ASSERT(itr);
      if (!itr) {
        continue;
      }
      itr = segmentReader.mask(std::move(itr));
      if (this->infos().filterConditionIsEmpty()) {
        totalSkipped += this->_reader->live_docs_count();
      } else {
        totalSkipped += calculateSkipAllCount(this->infos().countApproximate(),
                                              0, itr.get());
      }
    }
    stats.incrScanned(totalSkipped);
  }

  // seal the executor anyway
  _bufferFilled = true;
  this->_indexReadBuffer.clear();
  _totalCount = 0;
  _bufferedCount = 0;
  return totalSkipped;
}

template<typename ExecutionTraits>
size_t IResearchViewHeapSortExecutor<ExecutionTraits>::skip(
    size_t limit, IResearchViewStats& stats) {
  if (fillBufferInternal(limit)) {
    stats.incrScanned(getScanned());
  }
  if (limit >= this->_indexReadBuffer.size()) {
    auto const skipped = this->_indexReadBuffer.size();
    this->_indexReadBuffer.clear();
    return skipped;
  } else {
    size_t const toSkip = limit;
    while (limit) {
      this->_indexReadBuffer.pop_front();
      --limit;
    }
    return toSkip;
  }
}

template<typename ExecutionTraits>
void IResearchViewHeapSortExecutor<ExecutionTraits>::reset() {
  Base::reset();
  _totalCount = 0;
  _bufferedCount = 0;
  _bufferFilled = false;
  this->_storedValuesReaders.resize(
      this->_reader->size() *
      this->_infos.getOutNonMaterializedViewRegs().size());
}

template<typename ExecutionTraits>
bool IResearchViewHeapSortExecutor<ExecutionTraits>::writeRow(
    IResearchViewHeapSortExecutor::ReadContext& ctx,
    IndexReadBufferEntry bufferEntry) {
  auto const& val = this->_indexReadBuffer.getValue(bufferEntry);
  return Base::writeRow(ctx, bufferEntry, val.documentId(),
                        *val.collectionPtr());
}

template<typename ExecutionTraits>
void IResearchViewHeapSortExecutor<ExecutionTraits>::fillBuffer(
    IResearchViewHeapSortExecutor::ReadContext&) {
  fillBufferInternal(0);
}

template<typename ExecutionTraits>
bool IResearchViewHeapSortExecutor<ExecutionTraits>::fillBufferInternal(
    size_t skip) {
  if (_bufferFilled) {
    return false;
  }
  _bufferFilled = true;
  TRI_ASSERT(!this->_infos.scoresSort().empty());
  TRI_ASSERT(this->_filter != nullptr);
  size_t const atMost = this->_infos.scoresSortLimit();
  TRI_ASSERT(atMost);
  this->_indexReadBuffer.reset();
  this->_indexReadBuffer.preAllocateStoredValuesBuffer(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  auto const count = this->_reader->size();

  containers::SmallVector<irs::score_t, 4> scores;
  if constexpr (ExecutionTraits::Ordered) {
    scores.resize(this->infos().scorers().size());
  }

  irs::doc_iterator::ptr itr;
  irs::document const* doc{};
  size_t numScores{0};
  irs::score const* scr;
  for (size_t readerOffset = 0; readerOffset < count;) {
    if (!itr) {
      auto& segmentReader = (*this->_reader)[readerOffset];
      itr = this->_filter->execute({.segment = segmentReader,
                                    .scorers = this->_order,
                                    .ctx = &this->_filterCtx,
                                    .mode = irs::ExecutionMode::kAll});
      TRI_ASSERT(itr);
      doc = irs::get<irs::document>(*itr);
      TRI_ASSERT(doc);
      if constexpr (ExecutionTraits::Ordered) {
        scr = irs::get<irs::score>(*itr);
        if (!scr) {
          scr = &irs::score::kNoScore;
          numScores = 0;
        } else {
          numScores = scores.size();
        }
      }
      itr = segmentReader.mask(std::move(itr));
      TRI_ASSERT(itr);
    }
    if (!itr->next()) {
      ++readerOffset;
      itr.reset();
      doc = nullptr;
      scr = nullptr;
      continue;
    }
    ++_totalCount;

    (*scr)(scores.data());

    this->_indexReadBuffer.pushSortedValue(
        typename decltype(this->_indexReadBuffer)::KeyValueType(doc->value,
                                                                readerOffset),
        scores.data(), numScores);
  }
  this->_indexReadBuffer.finalizeHeapSort();
  _bufferedCount = this->_indexReadBuffer.size();
  if (skip < _bufferedCount) {
    auto pkReadingOrder = this->_indexReadBuffer.getMaterializeRange(skip);
    std::sort(
        pkReadingOrder.begin(), pkReadingOrder.end(),
        [buffer = &this->_indexReadBuffer](size_t lhs, size_t rhs) -> bool {
          auto const& lhs_val = buffer->getValue(lhs);
          auto const& rhs_val = buffer->getValue(rhs);
          if (lhs_val.readerOffset() < rhs_val.readerOffset()) {
            return true;
          } else if (lhs_val.readerOffset() > rhs_val.readerOffset()) {
            return false;
          }
          return lhs_val.irsDocId() < rhs_val.irsDocId();
        });

    size_t lastSegmentIdx = count;
    ColumnIterator pkReader;
    aql::QueryContext& query = this->_infos.getQuery();
    std::shared_ptr<arangodb::LogicalCollection> collection;
    auto orderIt = pkReadingOrder.begin();
    while (orderIt != pkReadingOrder.end()) {
      auto& value = this->_indexReadBuffer.getValue(*orderIt);
      auto const irsDocId = value.irsDocId();
      auto const segmentIdx = value.readerOffset();
      if (lastSegmentIdx != segmentIdx) {
        lastSegmentIdx = segmentIdx;
        auto& segmentReader = (*this->_reader)[lastSegmentIdx];
        auto pkIt = ::pkColumn(segmentReader);
        pkReader.itr.reset();
        DataSourceId const cid = this->_reader->cid(lastSegmentIdx);
        collection = lookupCollection(this->_trx, cid, query);
        if (ADB_UNLIKELY(!pkIt || !collection)) {
          LOG_TOPIC("bd02b", WARN, arangodb::iresearch::TOPIC)
              << "encountered a sub-reader without a primary key column or "
                 "without the collection while "
                 "executing a query, ignoring";
          while ((++orderIt) != pkReadingOrder.end() &&
                 *orderIt == lastSegmentIdx) {
          }
          continue;
        }
        ::reset(pkReader, std::move(pkIt));
        if constexpr ((ExecutionTraits::MaterializeType &
                       MaterializeType::UseStoredValues) ==
                      MaterializeType::UseStoredValues) {
          if (ADB_UNLIKELY(
                  !this->getStoredValuesReaders(segmentReader, segmentIdx))) {
            LOG_TOPIC("bd02c", WARN, arangodb::iresearch::TOPIC)
                << "encountered a sub-reader without stored values column "
                   "while executing a query, ignoring";
            continue;
          }
        }
      }

      LocalDocumentId documentId;
      if (irsDocId == pkReader.itr->seek(irsDocId)) {
        bool const readSuccess =
            DocumentPrimaryKey::read(documentId, pkReader.value->value);

        TRI_ASSERT(readSuccess == documentId.isSet());

        if (ADB_UNLIKELY(!readSuccess)) {
          LOG_TOPIC("6424f", WARN, arangodb::iresearch::TOPIC)
              << "failed to read document primary key while reading document "
                 "from arangosearch view, doc_id '"
              << value.irsDocId() << "'";
        }
      }
      value.decode(documentId, collection.get());
      if constexpr ((ExecutionTraits::MaterializeType &
                     MaterializeType::UseStoredValues) ==
                    MaterializeType::UseStoredValues) {
        auto const& columnsFieldsRegs =
            this->infos().getOutNonMaterializedViewRegs();
        TRI_ASSERT(!columnsFieldsRegs.empty());
        auto readerIndex = segmentIdx * columnsFieldsRegs.size();
        size_t valueIndex = *orderIt * columnsFieldsRegs.size();
        for (auto it = columnsFieldsRegs.cbegin();
             it != columnsFieldsRegs.cend(); ++it) {
          TRI_ASSERT(readerIndex < this->_storedValuesReaders.size());
          auto const& reader = this->_storedValuesReaders[readerIndex++];
          TRI_ASSERT(reader.itr);
          TRI_ASSERT(reader.value);
          auto const& payload = reader.value->value;
          bool const found = (irsDocId == reader.itr->seek(irsDocId));
          if (found && !payload.empty()) {
            this->_indexReadBuffer.setStoredValue(valueIndex++, payload);
          } else {
            this->_indexReadBuffer.setStoredValue(
                valueIndex++, ref<irs::byte_type>(VPackSlice::nullSlice()));
          }
        }
      }

      if constexpr (ExecutionTraits::EmitSearchDoc) {
        TRI_ASSERT(this->infos().searchDocIdRegId().isValid());
        this->_indexReadBuffer.pushSearchDoc((*this->_reader)[segmentIdx],
                                             irsDocId);
      }

      this->_indexReadBuffer.assertSizeCoherence();
      ++orderIt;
    }
  }
  return true;
}

template<typename ExecutionTraits>
void IResearchViewExecutor<ExecutionTraits>::fillBuffer(
    IResearchViewExecutor::ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);
  size_t const atMost = ctx.outputRow.numRowsLeft();
  TRI_ASSERT(this->_indexReadBuffer.empty());
  this->_indexReadBuffer.reset();
  this->_indexReadBuffer.preAllocateStoredValuesBuffer(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  size_t const count = this->_reader->size();

  for (; _readerOffset < count;) {
    if (!_itr) {
      if (!this->_indexReadBuffer.empty()) {
        // We may not reset the iterator and continue with the next reader if
        // we still have documents in the buffer, as the cid changes with each
        // reader.
        break;
      }

      if (!resetIterator()) {
        continue;
      }

      // CID is constant until the next resetIterator(). Save the
      // corresponding collection so we don't have to look it up every time.

      DataSourceId const cid = this->_reader->cid(_readerOffset);
      aql::QueryContext& query = this->_infos.getQuery();
      auto collection = lookupCollection(this->_trx, cid, query);

      if (!collection) {
        std::stringstream msg;
        msg << "failed to find collection while reading document from "
               "arangosearch view, cid '"
            << cid.id() << "'";
        query.warnings().registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                         msg.str());

        // We don't have a collection, skip the current reader.
        ++_readerOffset;
        _currentSegmentPos = 0;
        _itr.reset();
        _doc = nullptr;
        continue;
      }

      _collection = collection.get();
      this->_indexReadBuffer.reset();
    }

    TRI_ASSERT(_pkReader.itr);
    TRI_ASSERT(_pkReader.value);

    LocalDocumentId documentId;

    // try to read a document PK from iresearch
    bool const iteratorExhausted = !readPK(documentId);

    if (!documentId.isSet()) {
      // No document read, we cannot write it.

      if (iteratorExhausted) {
        // The iterator is exhausted, we need to continue with the next
        // reader.
        ++_readerOffset;
        _currentSegmentPos = 0;
        _itr.reset();
        _doc = nullptr;
      }
      continue;
    }

    // The CID must stay the same for all documents in the buffer
    TRI_ASSERT(this->_collection->id() == this->_reader->cid(_readerOffset));

    this->_indexReadBuffer.pushValue(documentId);

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(this->infos().searchDocIdRegId().isValid());
      this->_indexReadBuffer.pushSearchDoc((*this->_reader)[_readerOffset],
                                           _doc->value);
    }

    // in the ordered case we have to write scores as well as a document
    if constexpr (ExecutionTraits::Ordered) {
      // Writes into _scoreBuffer
      this->fillScores(*_scr);
    }

    if constexpr ((ExecutionTraits::MaterializeType &
                   MaterializeType::UseStoredValues) ==
                  MaterializeType::UseStoredValues) {
      TRI_ASSERT(_doc);
      this->pushStoredValues(*_doc);
    }
    // doc and scores are both pushed, sizes must now be coherent
    this->_indexReadBuffer.assertSizeCoherence();

    if (iteratorExhausted) {
      // The iterator is exhausted, we need to continue with the next reader.
      ++_readerOffset;
      _currentSegmentPos = 0;
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

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::resetIterator() {
  TRI_ASSERT(this->_filter);
  TRI_ASSERT(!_itr);

  auto& segmentReader = (*this->_reader)[_readerOffset];

  auto it = ::pkColumn(segmentReader);

  if (ADB_UNLIKELY(!it)) {
    LOG_TOPIC("bd01b", WARN, arangodb::iresearch::TOPIC)
        << "encountered a sub-reader without a primary key column while "
           "executing a query, ignoring";
    return false;
  }

  ::reset(_pkReader, std::move(it));

  if constexpr ((ExecutionTraits::MaterializeType &
                 MaterializeType::UseStoredValues) ==
                MaterializeType::UseStoredValues) {
    if (ADB_UNLIKELY(!this->getStoredValuesReaders(segmentReader))) {
      return false;
    }
  }

  _itr = this->_filter->execute({.segment = segmentReader,
                                 .scorers = this->_order,
                                 .ctx = &this->_filterCtx,
                                 .mode = irs::ExecutionMode::kAll});
  TRI_ASSERT(_itr);
  _doc = irs::get<irs::document>(*_itr);
  TRI_ASSERT(_doc);

  if constexpr (ExecutionTraits::Ordered) {
    _scr = irs::get<irs::score>(*_itr);

    if (!_scr) {
      _scr = &irs::score::kNoScore;
      _numScores = 0;
    } else {
      _numScores = this->infos().scorers().size();
    }
  }

  _itr = segmentReader.mask(std::move(_itr));
  TRI_ASSERT(_itr);
  _currentSegmentPos = 0;
  return true;
}

template<typename ExecutionTraits>
void IResearchViewExecutor<ExecutionTraits>::reset() {
  Base::reset();

  // reset iterator state
  _itr.reset();
  _doc = nullptr;
  _readerOffset = 0;
  _currentSegmentPos = 0;
  _totalPos = 0;
}

template<typename ExecutionTraits>
size_t IResearchViewExecutor<ExecutionTraits>::skip(size_t limit,
                                                    IResearchViewStats&) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter);

  size_t const toSkip = limit;

  for (size_t count = this->_reader->size(); _readerOffset < count;
       ++_readerOffset) {
    if (!_itr && !resetIterator()) {
      continue;
    }

    while (limit && _itr->next()) {
      ++_currentSegmentPos;
      ++_totalPos;
      --limit;
    }

    if (!limit) {
      break;  // do not change iterator if already reached limit
    }
    _itr.reset();
    _doc = nullptr;
  }
  saveCollection();
  return toSkip - limit;
}

template<typename ExecutionTraits>
size_t IResearchViewExecutor<ExecutionTraits>::skipAll(IResearchViewStats&) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter);

  size_t skipped = 0;

  if (_readerOffset < this->_reader->size()) {
    if (this->infos().filterConditionIsEmpty()) {
      skipped = this->_reader->live_docs_count();
      TRI_ASSERT(_totalPos <= skipped);
      skipped -= std::min(skipped, _totalPos);
      _readerOffset = this->_reader->size();
      _currentSegmentPos = 0;
    } else {
      for (size_t count = this->_reader->size(); _readerOffset < count;
           ++_readerOffset, _currentSegmentPos = 0) {
        if (!_itr && !resetIterator()) {
          continue;
        }
        skipped += calculateSkipAllCount(this->infos().countApproximate(),
                                         _currentSegmentPos, _itr.get());
        _itr.reset();
        _doc = nullptr;
      }
    }
  }
  return skipped;
}

template<typename ExecutionTraits>
void IResearchViewExecutor<ExecutionTraits>::saveCollection() {
  // We're in the middle of a reader, save the collection in case produceRows()
  // needs it.
  if (_itr) {
    // CID is constant until the next resetIterator(). Save the corresponding
    // collection so we don't have to look it up every time.

    DataSourceId const cid = this->_reader->cid(_readerOffset);
    aql::QueryContext& query = this->_infos.getQuery();
    auto collection = lookupCollection(this->_trx, cid, query);

    if (!collection) {
      std::stringstream msg;
      msg << "failed to find collection while reading document from "
             "arangosearch view, cid '"
          << cid.id() << "'";
      query.warnings().registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                       msg.str());

      // We don't have a collection, skip the current reader.
      ++_readerOffset;
      _currentSegmentPos = 0;
      _itr.reset();
      _doc = nullptr;
    }

    this->_indexReadBuffer.reset();
    _collection = collection.get();
  }
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::writeRow(
    IResearchViewExecutor::ReadContext& ctx, IndexReadBufferEntry bufferEntry) {
  TRI_ASSERT(_collection);

  auto const& val = this->_indexReadBuffer.getValue(bufferEntry);
  return Base::writeRow(ctx, bufferEntry, val, *_collection);
}

template<typename ExecutionTraits>
IResearchViewMergeExecutor<ExecutionTraits>::IResearchViewMergeExecutor(
    Fetcher& fetcher, Infos& infos)
    : Base{fetcher, infos},
      _heap_it{
          MinHeapContext{*infos.sort().first, infos.sort().second, _segments}} {
  TRI_ASSERT(infos.sort().first);
  TRI_ASSERT(!infos.sort().first->empty());
  TRI_ASSERT(infos.sort().first->size() >= infos.sort().second);
  TRI_ASSERT(infos.sort().second);
  TRI_ASSERT(infos.scoresSort().empty());
}

template<typename ExecutionTraits>
IResearchViewMergeExecutor<ExecutionTraits>::Segment::Segment(
    irs::doc_iterator::ptr&& docs, irs::document const& doc,
    irs::score const& score, size_t numScores,
    LogicalCollection const& collection, irs::doc_iterator::ptr&& pkReader,
    size_t index, irs::doc_iterator* sortReaderRef,
    irs::payload const* sortReaderValue,
    irs::doc_iterator::ptr&& sortReader) noexcept
    : docs(std::move(docs)),
      doc(&doc),
      score(&score),
      numScores(numScores),
      collection(&collection),
      segmentIndex(index),
      sortReaderRef(sortReaderRef),
      sortValue(sortReaderValue),
      sortReader(std::move(sortReader)) {
  TRI_ASSERT(this->docs);
  TRI_ASSERT(this->doc);
  TRI_ASSERT(this->score);
  TRI_ASSERT(this->collection);
  TRI_ASSERT(this->sortReaderRef);
  TRI_ASSERT(this->sortValue);
  ::reset(this->pkReader, std::move(pkReader));
  TRI_ASSERT(this->pkReader.itr);
  TRI_ASSERT(this->pkReader.value);
}

template<typename ExecutionTraits>
IResearchViewMergeExecutor<ExecutionTraits>::MinHeapContext::MinHeapContext(
    IResearchSortBase const& sort, size_t sortBuckets,
    std::vector<Segment>& segments) noexcept
    : _less(sort, sortBuckets), _segments(&segments) {}

template<typename ExecutionTraits>
bool IResearchViewMergeExecutor<ExecutionTraits>::MinHeapContext::operator()(
    size_t const i) const {
  assert(i < _segments->size());
  auto& segment = (*_segments)[i];
  while (segment.docs->next()) {
    auto const doc = segment.docs->value();

    if (doc == segment.sortReaderRef->seek(doc)) {
      return true;
    }

    // FIXME read pk as well
  }
  return false;
}

template<typename ExecutionTraits>
bool IResearchViewMergeExecutor<ExecutionTraits>::MinHeapContext::operator()(
    size_t const lhs, size_t const rhs) const {
  assert(lhs < _segments->size());
  assert(rhs < _segments->size());
  return _less((*_segments)[rhs].sortValue->value,
               (*_segments)[lhs].sortValue->value);
}

template<typename ExecutionTraits>
void IResearchViewMergeExecutor<ExecutionTraits>::reset() {
  Base::reset();

  _segments.clear();
  auto const size = this->_reader->size();
  _segments.reserve(size);

  auto const& columnsFieldsRegs = this->_infos.getOutNonMaterializedViewRegs();
  auto const storedValuesCount = columnsFieldsRegs.size();
  this->_storedValuesReaders.resize(size * storedValuesCount);
  auto isSortReaderUsedInStoredValues =
      storedValuesCount > 0 &&
      IResearchViewNode::kSortColumnNumber == columnsFieldsRegs.cbegin()->first;

  for (size_t i = 0; i < size; ++i) {
    auto& segment = (*this->_reader)[i];

    auto it = segment.mask(
        this->_filter->execute({.segment = segment,
                                .scorers = this->_order,
                                .ctx = &this->_filterCtx,
                                .mode = irs::ExecutionMode::kAll}));
    TRI_ASSERT(it);

    auto const* doc = irs::get<irs::document>(*it);
    TRI_ASSERT(doc);

    auto const* score = &irs::score::kNoScore;
    size_t numScores = 0;

    if constexpr (ExecutionTraits::Ordered) {
      auto* scoreRef = irs::get<irs::score>(*it);

      if (scoreRef) {
        score = scoreRef;
        numScores = this->infos().scorers().size();
      }
    }

    DataSourceId const cid = this->_reader->cid(i);
    aql::QueryContext& query = this->_infos.getQuery();
    auto collection = lookupCollection(this->_trx, cid, query);

    if (!collection) {
      std::stringstream msg;
      msg << "failed to find collection while reading document from "
             "arangosearch view, cid '"
          << cid.id() << "'";
      query.warnings().registerWarning(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                       msg.str());

      // We don't have a collection, skip the current segment.
      continue;
    }

    auto pkReader = ::pkColumn(segment);

    if (ADB_UNLIKELY(!pkReader)) {
      LOG_TOPIC("ee041", WARN, arangodb::iresearch::TOPIC)
          << "encountered a sub-reader without a primary key column while "
             "executing a query, ignoring";
      continue;
    }

    if constexpr ((ExecutionTraits::MaterializeType &
                   MaterializeType::UseStoredValues) ==
                  MaterializeType::UseStoredValues) {
      if (ADB_UNLIKELY(!this->getStoredValuesReaders(segment, i))) {
        continue;
      }
    } else {
      TRI_ASSERT(!isSortReaderUsedInStoredValues);
    }

    if (isSortReaderUsedInStoredValues) {
      TRI_ASSERT(i * storedValuesCount < this->_storedValuesReaders.size());
      auto& sortReader = this->_storedValuesReaders[i * storedValuesCount];

      _segments.emplace_back(std::move(it), *doc, *score, numScores,
                             *collection, std::move(pkReader), i,
                             sortReader.itr.get(), sortReader.value, nullptr);
    } else {
      auto itr = ::sortColumn(segment);

      if (ADB_UNLIKELY(!itr)) {
        LOG_TOPIC("af4cd", WARN, arangodb::iresearch::TOPIC)
            << "encountered a sub-reader without a sort column while "
               "executing a query, ignoring";
        continue;
      }

      irs::payload const* sortValue = irs::get<irs::payload>(*itr);
      if (ADB_UNLIKELY(!sortValue)) {
        sortValue = &NoPayload;
      }

      _segments.emplace_back(std::move(it), *doc, *score, numScores,
                             *collection, std::move(pkReader), i, itr.get(),
                             sortValue, std::move(itr));
    }
  }

  _heap_it.reset(_segments.size());
}

template<typename ExecutionTraits>
LocalDocumentId IResearchViewMergeExecutor<ExecutionTraits>::readPK(
    IResearchViewMergeExecutor<ExecutionTraits>::Segment const& segment) {
  TRI_ASSERT(segment.doc);
  TRI_ASSERT(segment.pkReader.itr);
  TRI_ASSERT(segment.pkReader.value);

  LocalDocumentId documentId;

  if (segment.doc->value == segment.pkReader.itr->seek(segment.doc->value)) {
    bool const readSuccess =
        DocumentPrimaryKey::read(documentId, segment.pkReader.value->value);

    TRI_ASSERT(readSuccess == documentId.isSet());

    if (ADB_UNLIKELY(!readSuccess)) {
      LOG_TOPIC("6423f", WARN, arangodb::iresearch::TOPIC)
          << "failed to read document primary key while reading document "
             "from arangosearch view, doc_id '"
          << segment.doc->value << "'";
    }
  }

  return documentId;
}

template<typename ExecutionTraits>
void IResearchViewMergeExecutor<ExecutionTraits>::fillBuffer(ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);

  size_t const atMost = ctx.outputRow.numRowsLeft();
  TRI_ASSERT(this->_indexReadBuffer.empty());
  this->_indexReadBuffer.reset();
  this->_indexReadBuffer.preAllocateStoredValuesBuffer(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());

  while (_heap_it.next()) {
    auto& segment = _segments[_heap_it.value()];
    ++segment.segmentPos;
    TRI_ASSERT(segment.docs);
    TRI_ASSERT(segment.doc);
    TRI_ASSERT(segment.score);
    TRI_ASSERT(segment.collection);
    TRI_ASSERT(segment.pkReader.itr);
    TRI_ASSERT(segment.pkReader.value);

    // try to read a document PK from iresearch
    LocalDocumentId const documentId = readPK(segment);

    if (!documentId.isSet()) {
      // No document read, we cannot write it.
      continue;
    }

    this->_indexReadBuffer.pushValue(documentId, segment.collection);

    // in the ordered case we have to write scores as well as a document
    if constexpr (ExecutionTraits::Ordered) {
      // Writes into _scoreBuffer
      this->fillScores(*segment.score);
    }

    if constexpr ((ExecutionTraits::MaterializeType &
                   MaterializeType::UseStoredValues) ==
                  MaterializeType::UseStoredValues) {
      TRI_ASSERT(segment.doc);
      this->pushStoredValues(*segment.doc, segment.segmentIndex);
    }

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(this->infos().searchDocIdRegId().isValid());
      this->_indexReadBuffer.pushSearchDoc(
          (*this->_reader)[segment.segmentIndex], segment.doc->value);
    }

    // doc and scores are both pushed, sizes must now be coherent
    this->_indexReadBuffer.assertSizeCoherence();

    if (this->_indexReadBuffer.size() >= atMost) {
      break;
    }
  }
}

template<typename ExecutionTraits>
size_t IResearchViewMergeExecutor<ExecutionTraits>::skip(size_t limit,
                                                         IResearchViewStats&) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter != nullptr);

  size_t const toSkip = limit;

  while (limit && _heap_it.next()) {
    ++this->_segments[_heap_it.value()].segmentPos;
    --limit;
  }

  return toSkip - limit;
}

template<typename ExecutionTraits>
size_t IResearchViewMergeExecutor<ExecutionTraits>::skipAll(
    IResearchViewStats&) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter != nullptr);

  size_t skipped = 0;
  if (_heap_it.size()) {
    for (auto& segment : _segments) {
      TRI_ASSERT(segment.docs);
      if (this->infos().filterConditionIsEmpty()) {
        TRI_ASSERT(segment.segmentIndex < this->_reader->size());
        auto const live_docs_count =
            (*this->_reader)[segment.segmentIndex].live_docs_count();
        TRI_ASSERT(segment.segmentPos <= live_docs_count);
        skipped += live_docs_count - segment.segmentPos;
      } else {
        skipped +=
            calculateSkipAllCount(this->infos().countApproximate(),
                                  segment.segmentPos, segment.docs.get());
      }
    }
    // Adjusting by count of docs already consumed by heap but not consumed by
    // executor. This count is heap size minus 1 already consumed by executor
    // after heap.next. But we should adjust by the heap size only if the heap
    // was advanced at least once (heap is actually filled on first next) or we
    // have nothing consumed from doc iterators!
    if (!_segments.empty() &&
        this->infos().countApproximate() == CountApproximate::Exact &&
        !this->infos().filterConditionIsEmpty() && _heap_it.size() &&
        this->_segments[_heap_it.value()].segmentPos) {
      skipped += (_heap_it.size() - 1);
    }
    _heap_it.reset(0);
  }
  return skipped;
}

template<typename ExecutionTraits>
bool IResearchViewMergeExecutor<ExecutionTraits>::writeRow(
    IResearchViewMergeExecutor::ReadContext& ctx,
    IndexReadBufferEntry bufferEntry) {
  auto const& id = this->_indexReadBuffer.getValue(bufferEntry);
  LocalDocumentId const& documentId = id.first;
  TRI_ASSERT(documentId.isSet());
  LogicalCollection const* collection = id.second;
  TRI_ASSERT(collection);

  return Base::writeRow(ctx, bufferEntry, documentId, *collection);
}

template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, false, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, false, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, false, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    false, false, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    false, false, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, false, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, false, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, false, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    false, false, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    false, false, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

// stored values copying implementation should be used only when stored values
// are used
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    true, false, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    true, false, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    true, false, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    true, false, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, true, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, true, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, true, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    false, true, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    false, true, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, true, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, true, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutor<
    ExecutionTraits<false, true, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    false, true, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    false, true, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

// stored values copying implementation should be used only when stored values
// are used
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    true, true, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    true, true, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    true, true, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
    true, true, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, false, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, false, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, false, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    false, false, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    false, false, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, false, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, false, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, false, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    false, false, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    false, false, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

// stored values copying implementation should be used only when stored values
// are used
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    true, false, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    true, false, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    true, false, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    true, false, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, true, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, true, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, true, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    false, true, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    false, true, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, true, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, true, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<
    ExecutionTraits<false, true, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    false, true, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    false, true, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

// stored values copying implementation should be used only when stored values
// are used
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    true, true, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    true, true, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    true, true, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
    true, true, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, false, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, false, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, false, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    false, false, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    false, false, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, false, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, false, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, false, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    false, false, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    false, false, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

// stored values copying implementation should be used only when stored values
// are used
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    true, false, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    true, false, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    true, false, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    true, false, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, true, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, true, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, true, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    false, true, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    false, true, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, true, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, true, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<
    ExecutionTraits<false, true, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    false, true, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    false, true, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

// stored values copying implementation should be used only when stored values
// are used
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    true, true, false,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    true, true, false,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    true, true, true,
    MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
    true, true, true,
    MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, false, false, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, false, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, false, false, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, false, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, false, false, MaterializeType::Materialize>>,
    ExecutionTraits<false, false, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        false, false, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        false, false, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, false, true, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, false, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, false, true, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, false, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, false, true, MaterializeType::Materialize>>,
    ExecutionTraits<false, false, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        false, false, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        false, false, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        true, false, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        true, false, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        true, false, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        true, false, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, true, false, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, true, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, true, false, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, true, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, true, false, MaterializeType::Materialize>>,
    ExecutionTraits<false, true, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        false, true, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        false, true, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, true, true, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, true, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, true, true, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, true, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<
        ExecutionTraits<false, true, true, MaterializeType::Materialize>>,
    ExecutionTraits<false, true, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        false, true, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        false, true, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        true, true, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        true, true, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        true, true, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewExecutor<ExecutionTraits<
        true, true, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, false, false, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, false, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, false, false, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, false, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, false, false, MaterializeType::Materialize>>,
    ExecutionTraits<false, false, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        false, false, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        false, false, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, false, true, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, false, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, false, true, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, false, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, false, true, MaterializeType::Materialize>>,
    ExecutionTraits<false, false, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        false, false, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        false, false, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        true, false, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        true, false, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        true, false, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        true, false, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, true, false, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, true, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, true, false, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, true, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, true, false, MaterializeType::Materialize>>,
    ExecutionTraits<false, true, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        false, true, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        false, true, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, true, true, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, true, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, true, true, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, true, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<
        ExecutionTraits<false, true, true, MaterializeType::Materialize>>,
    ExecutionTraits<false, true, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        false, true, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        false, true, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        true, true, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        true, true, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        true, true, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewMergeExecutor<ExecutionTraits<
        true, true, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, false, false, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, false, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, false, false, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, false, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, false, false, MaterializeType::Materialize>>,
    ExecutionTraits<false, false, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        false, false, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        false, false, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, false, true, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, false, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, false, true, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, false, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, false, true, MaterializeType::Materialize>>,
    ExecutionTraits<false, false, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        false, false, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        false, false, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, false, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        true, false, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        true, false, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        true, false, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        true, false, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, false, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, true, false, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, true, false, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, true, false, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, true, false, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, true, false, MaterializeType::Materialize>>,
    ExecutionTraits<false, true, false, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        false, true, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        false, true, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, true, true, MaterializeType::NotMaterialize>>,
    ExecutionTraits<false, true, true, MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, true, true, MaterializeType::LateMaterialize>>,
    ExecutionTraits<false, true, true, MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<
        ExecutionTraits<false, true, true, MaterializeType::Materialize>>,
    ExecutionTraits<false, true, true, MaterializeType::Materialize>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        false, true, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        false, true, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<false, true, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        true, true, false,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, false,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        true, true, false,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, false,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        true, true, true,
        MaterializeType::NotMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, true,
                    MaterializeType::NotMaterialize |
                        MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::IResearchViewExecutorBase<
    ::arangodb::aql::IResearchViewHeapSortExecutor<ExecutionTraits<
        true, true, true,
        MaterializeType::LateMaterialize | MaterializeType::UseStoredValues>>,
    ExecutionTraits<true, true, true,
                    MaterializeType::LateMaterialize |
                        MaterializeType::UseStoredValues>>;
