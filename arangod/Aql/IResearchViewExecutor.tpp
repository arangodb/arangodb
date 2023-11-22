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

#pragma once

#include "IResearchViewExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/ExecutionStats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Futures/Try.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchReadUtils.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <absl/strings/str_cat.h>
#include <analysis/token_attributes.hpp>
#include <search/boolean_filter.hpp>
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

inline constexpr irs::bytes_view kNullSlice{VPackSlice::nullSliceData, 1};

inline PushTag& operator*=(PushTag& self, size_t) { return self; }
inline PushTag operator++(PushTag&, int) { return {}; }

inline size_t calculateSkipAllCount(CountApproximate approximation,
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

[[maybe_unused]] inline void resetColumn(
    ColumnIterator& column, irs::doc_iterator::ptr&& itr) noexcept {
  TRI_ASSERT(itr);
  column.itr = std::move(itr);
  column.value = irs::get<irs::payload>(*column.itr);
  if (ADB_UNLIKELY(!column.value)) {
    column.value = &NoPayload;
  }
}

class BufferHeapSortContext {
 public:
  explicit BufferHeapSortContext(size_t numScoreRegisters,
                                 std::span<HeapSortElement const> scoresSort,
                                 std::span<float_t const> scoreBuffer)
      : _numScoreRegisters(numScoreRegisters),
        _heapSort(scoresSort),
        _scoreBuffer(scoreBuffer) {}

  bool operator()(size_t a, size_t b) const noexcept {
    auto const* rhs_scores = &_scoreBuffer[b * _numScoreRegisters];
    auto lhs_scores = &_scoreBuffer[a * _numScoreRegisters];
    for (auto const& cmp : _heapSort) {
      if (lhs_scores[cmp.source] != rhs_scores[cmp.source]) {
        return cmp.ascending ? lhs_scores[cmp.source] < rhs_scores[cmp.source]
                             : lhs_scores[cmp.source] > rhs_scores[cmp.source];
      }
    }
    return false;
  }

  bool compareInput(size_t lhsIdx, float_t const* rhs_scores) const noexcept {
    auto lhs_scores = &_scoreBuffer[lhsIdx * _numScoreRegisters];
    for (auto const& cmp : _heapSort) {
      if (lhs_scores[cmp.source] != rhs_scores[cmp.source]) {
        return cmp.ascending ? lhs_scores[cmp.source] < rhs_scores[cmp.source]
                             : lhs_scores[cmp.source] > rhs_scores[cmp.source];
      }
    }
    return false;
  }

 private:
  size_t _numScoreRegisters;
  std::span<HeapSortElement const> _heapSort;
  std::span<float_t const> _scoreBuffer;
};

}  // namespace

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    ViewSnapshotPtr reader, RegisterId outRegister,
    RegisterId searchDocRegister, std::vector<RegisterId> scoreRegisters,
    arangodb::aql::QueryContext& query, std::vector<SearchFunc> const& scorers,
    std::pair<arangodb::iresearch::IResearchSortBase const*, size_t> sort,
    IResearchViewStoredValues const& storedValues, ExecutionPlan const& plan,
    Variable const& outVariable, aql::AstNode const& filterCondition,
    std::pair<bool, bool> volatility, aql::VarInfoMap const& varInfoMap,
    int depth,
    IResearchViewNode::ViewValuesRegisters&& outNonMaterializedViewRegs,
    iresearch::CountApproximate countApproximate,
    iresearch::FilterOptimization filterOptimization,
    std::vector<iresearch::HeapSortElement> const& heapSort,
    size_t heapSortLimit, iresearch::SearchMeta const* meta, size_t parallelism,
    iresearch::IResearchExecutionPool& parallelExecutionPool)
    : _searchDocOutReg{searchDocRegister},
      _documentOutReg{outRegister},
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
      _heapSort{heapSort},
      _heapSortLimit{heapSortLimit},
      _parallelism{parallelism},
      _meta{meta},
      _parallelExecutionPool{parallelExecutionPool},
      _depth{depth},
      _filterConditionIsEmpty{isFilterConditionEmpty(&_filterCondition) &&
                              !_reader->hasNestedFields()},
      _volatileSort{volatility.second},
      // `_volatileSort` implies `_volatileFilter`
      _volatileFilter{_volatileSort || volatility.first} {
  TRI_ASSERT(_reader != nullptr);
}

aql::VarInfoMap const& IResearchViewExecutorInfos::varInfoMap() const noexcept {
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

std::pair<arangodb::iresearch::IResearchSortBase const*, size_t> const&
IResearchViewExecutorInfos::sort() const noexcept {
  return _sort;
}

IResearchViewStoredValues const& IResearchViewExecutorInfos::storedValues()
    const noexcept {
  return _storedValues;
}

IResearchViewStats::IResearchViewStats() noexcept : _scannedIndex(0) {}
void IResearchViewStats::incrScanned() noexcept { _scannedIndex++; }
void IResearchViewStats::incrScanned(size_t value) noexcept {
  _scannedIndex = _scannedIndex + value;
}
size_t IResearchViewStats::getScanned() const noexcept { return _scannedIndex; }
ExecutionStats& aql::operator+=(
    ExecutionStats& executionStats,
    IResearchViewStats const& iResearchViewStats) noexcept {
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
    aql::RegisterId documentOutReg, InputAqlItemRow& inputRow,
    OutputAqlItemRow& outputRow)
    : inputRow(inputRow), outputRow(outputRow), documentOutReg(documentOutReg) {
  if constexpr (static_cast<unsigned int>(
                    Traits::MaterializeType &
                    iresearch::MaterializeType::Materialize) ==
                static_cast<unsigned int>(
                    iresearch::MaterializeType::Materialize)) {
    callback = this->copyDocumentCallback(*this);
  }
}

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
ScoreIterator IndexReadBuffer<ValueType, copyStored>::getScores(
    size_t idx) noexcept {
  assertSizeCoherence();
  return ScoreIterator{_scoreBuffer, idx, _numScoreRegisters};
}

template<typename ValueType, bool copySorted>
void IndexReadBuffer<ValueType, copySorted>::finalizeHeapSort() {
  std::sort(_rows.begin(), _rows.end(),
            BufferHeapSortContext{_numScoreRegisters, _heapSort, _scoreBuffer});
  if (_heapSizeLeft) {
    // heap was not filled up to the limit. So fill buffer here.
    _storedValuesBuffer.resize(_keyBuffer.size() * _storedValuesCount);
  }
}

template<typename ValueType, bool copySorted>
void IndexReadBuffer<ValueType, copySorted>::pushSortedValue(
    ValueType&& value, std::span<float_t const> scores) {
  BufferHeapSortContext sortContext(_numScoreRegisters, _heapSort,
                                    _scoreBuffer);
  TRI_ASSERT(_maxSize);
  TRI_ASSERT(!scores.empty());
  if (ADB_LIKELY(!_heapSizeLeft)) {
    if (sortContext.compareInput(_rows.front(), scores.data())) {
      return;  // not interested in this document
    }
    std::pop_heap(_rows.begin(), _rows.end(), sortContext);
    // now last contains "free" index in the buffer
    _keyBuffer[_rows.back()] = ValueType{std::move(value)};
    auto const base = _rows.back() * _numScoreRegisters;
    size_t i{0};
    auto bufferIt = _scoreBuffer.begin() + base;
    for (; i < scores.size(); ++i) {
      *bufferIt = scores[i];
      ++bufferIt;
    }
    for (; i < _numScoreRegisters; ++i) {
      *bufferIt = std::numeric_limits<float_t>::quiet_NaN();
      ++bufferIt;
    }
    std::push_heap(_rows.begin(), _rows.end(), sortContext);
  } else {
    _keyBuffer.emplace_back(std::move(value));
    size_t i = 0;
    for (; i < scores.size(); ++i) {
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
size_t IndexReadBuffer<ValueType, copyStored>::pop_front() noexcept {
  TRI_ASSERT(!empty());
  TRI_ASSERT(_keyBaseIdx < _keyBuffer.size());
  assertSizeCoherence();
  size_t key = _keyBaseIdx;
  if (std::is_same_v<ValueType, HeapSortExecutorValue> && !_rows.empty()) {
    TRI_ASSERT(!_heapSort.empty());
    key = _rows[_keyBaseIdx];
  }
  ++_keyBaseIdx;
  return key;
}

template<typename ValueType, bool copyStored>
void IndexReadBuffer<ValueType, copyStored>::assertSizeCoherence()
    const noexcept {
  TRI_ASSERT((_numScoreRegisters == 0 && _scoreBuffer.size() == 1) ||
             (_scoreBuffer.size() == _keyBuffer.size() * _numScoreRegisters));
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
    auto getAnalyzer = [&](std::string_view shortName) -> FieldMeta::Analyzer {
      auto analyzer = analyzerFeature.get(shortName, vocbase, revision);
      if (!analyzer) {
        return makeEmptyAnalyzer();
      }
      return {std::move(analyzer), std::string{shortName}};
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
  bool const needFullCount = output.getClientCall().fullCount;
  upstreamCall.fullCount = needFullCount;
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
        static_cast<Impl&>(*this).reset(needFullCount);
      }

      ReadContext ctx(infos().getDocumentRegister(), _inputRow, output);
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
  bool const needFullCount = call.needsFullCount();
  TRI_ASSERT(_indexReadBuffer.empty() ||
             (!this->infos().heapSort().empty() && needFullCount));
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
      impl.reset(needFullCount);
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
    TRI_ASSERT(_indexReadBuffer.empty() || !this->infos().heapSort().empty());

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
      if (!impl.fillBuffer(ctx)) {
        return false;
      }
      if constexpr (Traits::ExplicitScanned) {
        stats.incrScanned(static_cast<Impl&>(*this).getScanned());
      }
    }
    const auto idx = _indexReadBuffer.pop_front();

    if (ADB_LIKELY(impl.writeRow(ctx, idx))) {
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
    iresearch::QueryContext queryCtx{
        .trx = &_trx,
        .ast = infos().plan().getAst(),
        .ctx = &_ctx,
        .index = _reader.get(),
        .ref = &infos().outVariable(),
        .filterOptimization = infos().filterOptimization(),
        .namePrefix = nestedRoot(_reader->hasNestedFields()),
        .isSearchQuery = true,
        .isOldMangling = infos().isOldMangling()};

    // The analyzer is referenced in the FilterContext and used during the
    // following ::makeFilter() call, so can't be a temporary.
    auto const emptyAnalyzer = makeEmptyAnalyzer();
    AnalyzerProvider* fieldAnalyzerProvider = nullptr;
    auto const* contextAnalyzer = &FieldMeta::identity();
    if (!infos().isOldMangling()) {
      fieldAnalyzerProvider = &_provider;
      contextAnalyzer = &emptyAnalyzer;
    }

    FilterContext const filterCtx{
        .query = queryCtx,
        .contextAnalyzer = *contextAnalyzer,
        .fieldAnalyzerProvider = fieldAnalyzerProvider};

    irs::Or root;
    auto const rv =
        FilterFactory::filter(&root, filterCtx, infos().filterCondition());

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

      _scorersContainer.clear();
      _scorersContainer.reserve(scorers.size());

      for (irs::Scorer::ptr scorer; auto const& scorerNode : scorers) {
        TRI_ASSERT(scorerNode.node);

        if (!order_factory::scorer(&scorer, *scorerNode.node, queryCtx)) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "failed to build scorers while querying arangosearch view");
        }

        TRI_ASSERT(scorer);
        _scorersContainer.emplace_back(std::move(scorer));
      }

      // compile scorers
      _scorers = irs::Scorers::Prepare(_scorersContainer);
    }

    // compile filter
    _filter = root.prepare(*_reader, _scorers, irs::kNoBoost, &_filterCtx);

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

// make overload with string_view as input that not depends on SV container type
template<typename Impl, typename ExecutionTraits>
inline bool IResearchViewExecutorBase<Impl, ExecutionTraits>::writeStoredValue(
    ReadContext& ctx,
    typename IResearchViewExecutorBase<Impl, ExecutionTraits>::
        IndexReadBufferType::StoredValuesContainer const& storedValues,
    size_t index, std::map<size_t, RegisterId> const& fieldsRegs) {
  TRI_ASSERT(index < storedValues.size());
  auto const& storedValue = storedValues[index];
  return writeStoredValue(ctx, storedValue, fieldsRegs);
}

template<typename Impl, typename ExecutionTraits>
inline bool IResearchViewExecutorBase<Impl, ExecutionTraits>::writeStoredValue(
    ReadContext& ctx, irs::bytes_view storedValue,
    std::map<size_t, RegisterId> const& fieldsRegs) {
  TRI_ASSERT(!storedValue.empty());
  auto const totalSize = storedValue.size();
  VPackSlice slice{storedValue.data()};
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
template<typename Value>
bool IResearchViewExecutorBase<Impl, ExecutionTraits>::writeRowImpl(
    ReadContext& ctx, size_t idx, Value const& value) {
  if constexpr (ExecutionTraits::EmitSearchDoc) {
    // FIXME: This could be avoided by using only late materialization register
    auto reg = this->infos().searchDocIdRegId();
    TRI_ASSERT(reg.isValid());

    this->writeSearchDoc(ctx, this->_indexReadBuffer.getSearchDoc(idx), reg);
  }
  if constexpr (isMaterialized) {
    TRI_ASSERT(value.value().id.isSet());
    TRI_ASSERT(value.segment());
    // read document from underlying storage engine, if we got an id
    if (ADB_UNLIKELY(
            !value.segment()->collection->getPhysical()->readFromSnapshot(
                &_trx, value.value().id, ctx.callback, ReadOwnWrites::no,
                *value.segment()->snapshot))
            .ok()) {
      return false;
    }
  }
  if constexpr (isLateMaterialized) {
    this->writeSearchDoc(ctx, value, ctx.getDocumentIdReg());
  }
  if constexpr (usesStoredValues) {
    auto const& columnsFieldsRegs = infos().getOutNonMaterializedViewRegs();
    TRI_ASSERT(!columnsFieldsRegs.empty());
    auto const& storedValues = _indexReadBuffer.getStoredValues();
    auto index = idx * columnsFieldsRegs.size();
    TRI_ASSERT(index < storedValues.size());
    for (auto it = columnsFieldsRegs.cbegin(); it != columnsFieldsRegs.cend();
         ++it) {
      if (ADB_UNLIKELY(
              !writeStoredValue(ctx, storedValues, index++, it->second))) {
        return false;
      }
    }
  } else if constexpr (ExecutionTraits::MaterializeType ==
                           MaterializeType::NotMaterialize &&
                       !ExecutionTraits::Ordered && !Traits::EmitSearchDoc) {
    ctx.outputRow.copyRow(ctx.inputRow);
  }
  // in the ordered case we have to write scores as well as a document
  // move to separate function that gets ScoresIterator
  if constexpr (ExecutionTraits::Ordered) {
    // scorer register are placed right before the document output register
    std::vector<RegisterId> const& scoreRegisters = infos().getScoreRegisters();
    auto scoreRegIter = scoreRegisters.begin();
    for (auto const& it : _indexReadBuffer.getScores(idx)) {
      TRI_ASSERT(scoreRegIter != scoreRegisters.end());
      auto const val = AqlValueHintDouble(it);
      ctx.outputRow.moveValueInto(*scoreRegIter, ctx.inputRow, val);
      ++scoreRegIter;
    }

    // we should have written exactly all score registers by now
    TRI_ASSERT(scoreRegIter == scoreRegisters.end());
  }
  return true;
}

template<typename Impl, typename ExecutionTraits>
template<typename T>
void IResearchViewExecutorBase<Impl, ExecutionTraits>::makeStoredValues(
    T idx, irs::doc_id_t docId, size_t readerIndex) {
  auto columnsFieldsRegsSize = _infos.getOutNonMaterializedViewRegs().size();
  TRI_ASSERT(columnsFieldsRegsSize != 0);
  readerIndex *= columnsFieldsRegsSize;
  idx *= columnsFieldsRegsSize;
  for (size_t i = 0; i != columnsFieldsRegsSize; ++i) {
    TRI_ASSERT(readerIndex < _storedValuesReaders.size());
    auto const& reader = _storedValuesReaders[readerIndex++];
    TRI_ASSERT(reader.itr);
    TRI_ASSERT(reader.value);
    auto const& payload = reader.value->value;
    bool const found = docId == reader.itr->seek(docId);
    if (found && !payload.empty()) {
      _indexReadBuffer.makeStoredValue(idx++, payload);
    } else {
      _indexReadBuffer.makeStoredValue(idx++, kNullSlice);
    }
  }
}

template<typename Impl, typename ExecutionTraits>
bool IResearchViewExecutorBase<Impl, ExecutionTraits>::getStoredValuesReaders(
    irs::SubReader const& segmentReader, size_t readerIndex) {
  auto const& columnsFieldsRegs = _infos.getOutNonMaterializedViewRegs();
  if (!columnsFieldsRegs.empty()) {
    auto columnFieldsRegs = columnsFieldsRegs.cbegin();
    readerIndex *= columnsFieldsRegs.size();
    if (IResearchViewNode::kSortColumnNumber == columnFieldsRegs->first) {
      auto sortReader = ::sortColumn(segmentReader);
      if (ADB_UNLIKELY(!sortReader)) {
        LOG_TOPIC("bc5bd", WARN, arangodb::iresearch::TOPIC)
            << "encountered a sub-reader without a sort column while "
               "executing a query, ignoring";
        return false;
      }
      ::resetColumn(_storedValuesReaders[readerIndex++], std::move(sortReader));
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
        resetColumn(_storedValuesReaders[readerIndex++],
                    storedValuesReader->iterator(irs::ColumnHint::kNormal));
      }
    }
  }
  return true;
}

template<typename ExecutionTraits>
IResearchViewExecutor<ExecutionTraits>::IResearchViewExecutor(Fetcher& fetcher,
                                                              Infos& infos)
    : Base{fetcher, infos}, _segmentOffset{0} {
  this->_storedValuesReaders.resize(
      this->_infos.getOutNonMaterializedViewRegs().size() *
      infos.parallelism());
  TRI_ASSERT(infos.heapSort().empty());
  TRI_ASSERT(infos.parallelism() > 0);
  _segmentReaders.resize(infos.parallelism());
}

template<typename ExecutionTraits>
IResearchViewExecutor<ExecutionTraits>::~IResearchViewExecutor() {
  if (_allocatedThreads || _demandedThreads) {
    this->_infos.parallelExecutionPool().releaseThreads(_allocatedThreads,
                                                        _demandedThreads);
  }
}

inline void commonReadPK(ColumnIterator& it, irs::doc_id_t docId,
                         LocalDocumentId& documentId) {
  TRI_ASSERT(it.itr);
  TRI_ASSERT(it.value);
  bool r = it.itr->seek(docId) == docId;
  if (ADB_LIKELY(r)) {
    r = DocumentPrimaryKey::read(documentId, it.value->value);
    TRI_ASSERT(r == documentId.isSet());
  }
  if (ADB_UNLIKELY(!r)) {
    LOG_TOPIC("6442f", WARN, TOPIC)
        << "failed to read document primary key while reading document from "
           "arangosearch view, doc_id: "
        << docId;
  }
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::readPK(LocalDocumentId& documentId,
                                                    SegmentReader& reader) {
  TRI_ASSERT(!documentId.isSet());
  TRI_ASSERT(reader.itr);
  if (!reader.itr->next()) {
    return false;
  }
  ++reader.totalPos;
  ++reader.currentSegmentPos;
  if constexpr (Base::isMaterialized) {
    TRI_ASSERT(reader.doc);
    commonReadPK(reader.pkReader, reader.doc->value, documentId);
  }
  return true;
}

template<typename ExecutionTraits>
IResearchViewHeapSortExecutor<ExecutionTraits>::IResearchViewHeapSortExecutor(
    Fetcher& fetcher, Infos& infos)
    : Base{fetcher, infos} {
  this->_indexReadBuffer.setHeapSort(this->_infos.heapSort());
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
      auto itr = this->_filter->execute({
          .segment = segmentReader,
          .scorers = this->_scorers,
          .ctx = &this->_filterCtx,
          .wand = {},
      });
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
void IResearchViewHeapSortExecutor<ExecutionTraits>::reset(
    [[maybe_unused]] bool needFullCount) {
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
    IResearchViewHeapSortExecutor::ReadContext& ctx, size_t idx) {
  static_assert(!Base::isLateMaterialized,
                "HeapSort superseeds LateMaterialization");
  auto const& value = this->_indexReadBuffer.getValue(idx);
  return Base::writeRowImpl(ctx, idx, value);
}

template<typename ExecutionTraits>
bool IResearchViewHeapSortExecutor<ExecutionTraits>::fillBuffer(
    IResearchViewHeapSortExecutor::ReadContext&) {
  fillBufferInternal(0);
  // FIXME: Use additional flag from FillBufferInternal
  return !this->_indexReadBuffer.empty();
}

template<typename ExecutionTraits>
bool IResearchViewHeapSortExecutor<ExecutionTraits>::fillBufferInternal(
    size_t skip) {
  if (_bufferFilled) {
    return false;
  }
  _bufferFilled = true;
  TRI_ASSERT(!this->_infos.heapSort().empty());
  TRI_ASSERT(this->_filter != nullptr);
  size_t const atMost = this->_infos.heapSortLimit();
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
  irs::score_threshold* threshold{};
  irs::score_t threshold_value = 0.f;
  size_t numScores{0};
  irs::score const* scr;
  for (size_t readerOffset = 0; readerOffset < count;) {
    if (!itr) {
      auto& segmentReader = (*this->_reader)[readerOffset];
      itr = this->_filter->execute({
          .segment = segmentReader,
          .scorers = this->_scorers,
          .ctx = &this->_filterCtx,
          .wand = this->_wand,
      });
      TRI_ASSERT(itr);
      doc = irs::get<irs::document>(*itr);
      TRI_ASSERT(doc);
      if constexpr (ExecutionTraits::Ordered) {
        scr = irs::get<irs::score>(*itr);
        if (!scr) {
          scr = &irs::score::kNoScore;
          numScores = 0;
          threshold = nullptr;
        } else {
          numScores = scores.size();
          threshold = irs::get_mutable<irs::score_threshold>(itr.get());
          if (threshold != nullptr && this->_wand.Enabled()) {
            TRI_ASSERT(threshold->min == 0.f);
            threshold->min = threshold_value;
          } else {
            threshold = nullptr;
          }
        }
      }
      itr = segmentReader.mask(std::move(itr));
      TRI_ASSERT(itr);
    }
    if (!itr->next()) {
      if (threshold != nullptr) {
        TRI_ASSERT(threshold_value <= threshold->min);
        threshold_value = threshold->min;
      }
      ++readerOffset;
      itr.reset();
      doc = nullptr;
      scr = nullptr;
      continue;
    }
    ++_totalCount;

    (*scr)(scores.data());

    this->_indexReadBuffer.pushSortedValue(
        HeapSortExecutorValue{readerOffset, doc->value},
        std::span{scores.data(), numScores});
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
          return lhs_val.docId() < rhs_val.docId();
        });

    size_t lastSegmentIdx = count;
    ColumnIterator pkReader;
    auto orderIt = pkReadingOrder.begin();
    iresearch::ViewSegment const* segment{};
    while (orderIt != pkReadingOrder.end()) {
      auto& value = this->_indexReadBuffer.getValue(*orderIt);
      auto const docId = value.docId();
      auto const segmentIdx = value.readerOffset();
      if (lastSegmentIdx != segmentIdx) {
        lastSegmentIdx = segmentIdx;
        auto& segmentReader = (*this->_reader)[lastSegmentIdx];
        auto pkIt = ::pkColumn(segmentReader);
        if (ADB_UNLIKELY(!pkIt)) {
          LOG_TOPIC("bd02b", WARN, arangodb::iresearch::TOPIC)
              << "encountered a sub-reader without a primary key column "
                 "executing a query, ignoring";
          while ((++orderIt) != pkReadingOrder.end() &&
                 *orderIt == lastSegmentIdx) {
          }
          continue;
        }
        pkReader.itr.reset();
        segment = &this->_reader->segment(segmentIdx);
        ::resetColumn(pkReader, std::move(pkIt));
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
      commonReadPK(pkReader, docId, documentId);

      TRI_ASSERT(segment);
      value.translate(*segment, documentId);
      if constexpr (Base::usesStoredValues) {
        auto const& columnsFieldsRegs =
            this->infos().getOutNonMaterializedViewRegs();
        TRI_ASSERT(!columnsFieldsRegs.empty());
        auto readerIndex = segmentIdx * columnsFieldsRegs.size();
        size_t valueIndex = *orderIt * columnsFieldsRegs.size();
        for (auto it = columnsFieldsRegs.begin(), end = columnsFieldsRegs.end();
             it != end; ++it) {
          TRI_ASSERT(readerIndex < this->_storedValuesReaders.size());
          auto const& reader = this->_storedValuesReaders[readerIndex++];
          TRI_ASSERT(reader.itr);
          TRI_ASSERT(reader.value);
          auto const& payload = reader.value->value;
          bool const found = docId == reader.itr->seek(docId);
          if (found && !payload.empty()) {
            this->_indexReadBuffer.makeStoredValue(valueIndex++, payload);
          } else {
            this->_indexReadBuffer.makeStoredValue(valueIndex++, kNullSlice);
          }
        }
      }

      if constexpr (ExecutionTraits::EmitSearchDoc) {
        TRI_ASSERT(this->infos().searchDocIdRegId().isValid());
        this->_indexReadBuffer.makeSearchDoc(PushTag{}, *segment, docId);
      }

      this->_indexReadBuffer.assertSizeCoherence();
      ++orderIt;
    }
  }
  return true;
}

template<typename ExecutionTraits>
template<bool parallel>
bool IResearchViewExecutor<ExecutionTraits>::readSegment(
    SegmentReader& reader, std::atomic_size_t& bufferIdx) {
  bool gotData = false;
  while (reader.atMost) {
    if (!reader.itr) {
      if (!resetIterator(reader)) {
        reader.finalize();
        return gotData;
      }

      // segment is constant until the next resetIterator().
      // save it to don't have to look it up every time.
      if constexpr (Base::isMaterialized) {
        reader.segment = &this->_reader->segment(reader.readerOffset);
      }
    }

    if constexpr (Base::isMaterialized) {
      TRI_ASSERT(reader.pkReader.itr);
      TRI_ASSERT(reader.pkReader.value);
    }
    LocalDocumentId documentId;
    // try to read a document PK from iresearch
    bool const iteratorExhausted = !readPK(documentId, reader);

    if (iteratorExhausted) {
      // The iterator is exhausted, we need to continue with the next
      // reader.
      reader.finalize();
      return gotData;
    }
    if constexpr (Base::isMaterialized) {
      // The collection must stay the same for all documents in the buffer
      TRI_ASSERT(reader.segment ==
                 &this->_reader->segment(reader.readerOffset));
      if (!documentId.isSet()) {
        // No document read, we cannot write it.
        continue;
      }
    }
    auto current = [&] {
      if constexpr (parallel) {
        return bufferIdx.fetch_add(1);
      } else {
        return PushTag{};
      }
    }();
    auto& viewSegment = this->_reader->segment(reader.readerOffset);
    if constexpr (Base::isLateMaterialized) {
      this->_indexReadBuffer.makeValue(current, viewSegment, reader.doc->value);
    } else {
      this->_indexReadBuffer.makeValue(current, viewSegment, documentId);
    }
    --reader.atMost;
    gotData = true;

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(this->infos().searchDocIdRegId().isValid());
      this->_indexReadBuffer.makeSearchDoc(current, viewSegment,
                                           reader.doc->value);
    }

    // in the ordered case we have to write scores as well as a document
    if constexpr (ExecutionTraits::Ordered) {
      if constexpr (parallel) {
        (*reader.scr)(this->_indexReadBuffer.getScoreBuffer(
            current * this->infos().scoreRegistersCount()));
      } else {
        this->fillScores(*reader.scr);
      }
    }

    if constexpr (Base::usesStoredValues) {
      TRI_ASSERT(reader.doc);
      TRI_ASSERT(std::distance(_segmentReaders.data(), &reader) <
                 static_cast<ptrdiff_t>(_segmentReaders.size()));
      this->makeStoredValues(current, reader.doc->value,
                             std::distance(_segmentReaders.data(), &reader));
    }
    if constexpr (!parallel) {
      // doc and scores are both pushed, sizes must now be coherent
      this->_indexReadBuffer.assertSizeCoherence();
    }

    if (iteratorExhausted) {
      // The iterator is exhausted, we need to continue with the next reader.
      reader.finalize();

      // Here we have at least one document in _indexReadBuffer, so we may not
      // add documents from a new reader.
      return gotData;
    }
  }
  return gotData;
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::fillBuffer(ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);
  size_t const count = this->_reader->size();
  bool gotData = false;
  auto atMost = ctx.outputRow.numRowsLeft();
  TRI_ASSERT(this->_indexReadBuffer.empty());
  auto parallelism = std::min(count, this->_infos.parallelism());
  this->_indexReadBuffer.reset();
  std::atomic_size_t bufferIdx{0};
  // shortcut for sequential execution.
  if (parallelism == 1) {
    TRI_IF_FAILURE("IResearchFeature::failNonParallelQuery") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    this->_indexReadBuffer.preAllocateStoredValuesBuffer(
        atMost, this->_infos.getScoreRegisters().size(),
        this->_infos.getOutNonMaterializedViewRegs().size());
    auto& reader = _segmentReaders.front();
    while (!gotData && (_segmentOffset < count || reader.itr)) {
      if (!reader.itr) {
        reader.readerOffset = _segmentOffset++;
        TRI_ASSERT(reader.readerOffset < count);
      }
      reader.atMost = atMost;
      gotData = readSegment<false>(reader, bufferIdx);
    }
    return gotData;
  }
  auto const atMostInitial = atMost;
  // here parallelism can be used or not depending on the
  // current pipeline demand.
  auto const& clientCall = ctx.outputRow.getClientCall();
  auto limit = clientCall.getUnclampedLimit();
  bool const isUnlimited = limit == aql::AqlCall::Infinity{};
  TRI_ASSERT(isUnlimited || std::holds_alternative<std::size_t>(limit));
  if (!isUnlimited) {
    parallelism = std::clamp(std::get<std::size_t>(limit) / atMostInitial,
                             (size_t)1, parallelism);
  }
  // let's be greedy as it is more likely that we are
  // asked to read some "tail" documents to fill the block
  // and next time we would need all our parallelism again.
  auto& readersPool = this->infos().parallelExecutionPool();
  if (parallelism > (this->_allocatedThreads + 1)) {
    uint64_t deltaDemanded{0};
    if ((parallelism - 1) > _demandedThreads) {
      deltaDemanded = parallelism - 1 - _demandedThreads;
      _demandedThreads += deltaDemanded;
    }
    this->_allocatedThreads += readersPool.allocateThreads(
        static_cast<int>(parallelism - this->_allocatedThreads - 1),
        deltaDemanded);
    parallelism = this->_allocatedThreads + 1;
  }
  atMost = atMostInitial * parallelism;

  std::vector<futures::Future<bool>> results;
  this->_indexReadBuffer.preAllocateStoredValuesBuffer(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  this->_indexReadBuffer.setForParallelAccess(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  results.reserve(parallelism - 1);
  // we must wait for our threads before bailing out
  // as we most likely will release segments and
  // "this" will also be invalid.
  auto cleanupThreads = irs::Finally([&]() noexcept {
    results.erase(std::remove_if(results.begin(), results.end(),
                                 [](auto& f) { return !f.valid(); }),
                  results.end());
    if (results.empty()) {
      return;
    }
    auto runners = futures::collectAll(results);
    runners.wait();
  });
  while (bufferIdx.load() < atMostInitial) {
    TRI_ASSERT(results.empty());
    size_t i = 0;
    size_t selfExecute{std::numeric_limits<size_t>::max()};
    auto toFetch = atMost - bufferIdx.load();
    while (toFetch && i < _segmentReaders.size()) {
      auto& reader = _segmentReaders[i];
      if (!reader.itr) {
        if (_segmentOffset >= count) {
          // no new segments. But maybe some existing readers still alive
          ++i;
          continue;
        }
        reader.readerOffset = _segmentOffset++;
      }
      reader.atMost = std::min(atMostInitial, toFetch);
      TRI_ASSERT(reader.atMost);
      toFetch -= reader.atMost;
      if (selfExecute < parallelism) {
        futures::Promise<bool> promise;
        auto future = promise.getFuture();
        if (ADB_UNLIKELY(!readersPool.run(
                [&, ctx = &reader, pr = std::move(promise)]() mutable {
                  try {
                    pr.setValue(readSegment<true>(*ctx, bufferIdx));
                  } catch (...) {
                    pr.setException(std::current_exception());
                  }
                }))) {
          TRI_ASSERT(false);
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL,
              " Failed to schedule parallel search view reading");
        }
        // this should be noexcept as we've reserved above
        results.push_back(std::move(future));
      } else {
        selfExecute = i;
      }
      if (results.size() == (parallelism - 1)) {
        break;
      }
      ++i;
    }
    if (selfExecute < std::numeric_limits<size_t>::max()) {
      gotData |= readSegment<true>(_segmentReaders[selfExecute], bufferIdx);
    } else {
      TRI_ASSERT(results.empty());
      break;
    }
    // we run this in noexcept mode as with current implementation
    // we can not recover and properly wait for finish in case
    // of exception in the middle of collectAll or wait.
    [&]() noexcept {
      auto runners = futures::collectAll(results);
      runners.wait();
      for (auto& r : runners.result().get()) {
        gotData |= r.get();
      }
    }();
    results.clear();
  }
  // shrink to actual size so we can emit rows as usual
  this->_indexReadBuffer.setForParallelAccess(
      bufferIdx, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  return gotData;
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::resetIterator(
    SegmentReader& reader) {
  TRI_ASSERT(this->_filter);
  TRI_ASSERT(!reader.itr);

  auto& segmentReader = (*this->_reader)[reader.readerOffset];

  if constexpr (Base::isMaterialized) {
    auto it = ::pkColumn(segmentReader);

    if (ADB_UNLIKELY(!it)) {
      LOG_TOPIC("bd01b", WARN, arangodb::iresearch::TOPIC)
          << "encountered a sub-reader without a primary key column while "
             "executing a query, ignoring";
      return false;
    }

    resetColumn(reader.pkReader, std::move(it));
  }

  if constexpr (Base::usesStoredValues) {
    if (ADB_UNLIKELY(!this->getStoredValuesReaders(
            segmentReader, std::distance(_segmentReaders.data(), &reader)))) {
      return false;
    }
  }

  reader.itr = this->_filter->execute({
      .segment = segmentReader,
      .scorers = this->_scorers,
      .ctx = &this->_filterCtx,
      .wand = {},
  });
  TRI_ASSERT(reader.itr);
  reader.doc = irs::get<irs::document>(*reader.itr);
  TRI_ASSERT(reader.doc);

  if constexpr (ExecutionTraits::Ordered) {
    reader.scr = irs::get<irs::score>(*reader.itr);

    if (!reader.scr) {
      reader.scr = &irs::score::kNoScore;
      reader.numScores = 0;
    } else {
      reader.numScores = this->infos().scorers().size();
    }
  }

  reader.itr = segmentReader.mask(std::move(reader.itr));
  TRI_ASSERT(reader.itr);
  reader.currentSegmentPos = 0;
  return true;
}

template<typename ExecutionTraits>
void IResearchViewExecutor<ExecutionTraits>::reset(
    [[maybe_unused]] bool needFullCount) {
  Base::reset();

  // reset iterator state
  for (auto& r : _segmentReaders) {
    r.finalize();
    r.readerOffset = 0;
    r.totalPos = 0;
  }
  _segmentOffset = 0;
}

template<typename ExecutionTraits>
size_t IResearchViewExecutor<ExecutionTraits>::skip(size_t limit,
                                                    IResearchViewStats&) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter);

  size_t const toSkip = limit;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  for (auto& r : _segmentReaders) {
    TRI_ASSERT(r.currentSegmentPos == 0);
    TRI_ASSERT(r.totalPos == 0);
    TRI_ASSERT(r.itr == nullptr);
  }
#endif
  auto& reader = _segmentReaders.front();
  for (size_t count = this->_reader->size(); _segmentOffset < count;) {
    reader.readerOffset = _segmentOffset++;
    if (!resetIterator(reader)) {
      continue;
    }

    while (limit && reader.itr->next()) {
      ++reader.currentSegmentPos;
      ++reader.totalPos;
      --limit;
    }

    if (!limit) {
      break;  // do not change iterator if already reached limit
    }
    reader.finalize();
  }
  if constexpr (Base::isMaterialized) {
    reader.segment = &this->_reader->segment(reader.readerOffset);
    this->_indexReadBuffer.reset();
  }
  return toSkip - limit;
}

template<typename ExecutionTraits>
size_t IResearchViewExecutor<ExecutionTraits>::skipAll(IResearchViewStats&) {
  TRI_ASSERT(this->_filter);
  size_t skipped = 0;

  auto reset = [](SegmentReader& reader) {
    reader.itr.reset();
    reader.doc = nullptr;
    reader.currentSegmentPos = 0;
  };

  auto const count = this->_reader->size();
  if (_segmentOffset > count) {
    return skipped;
  }
  irs::Finally seal = [&]() noexcept {
    _segmentOffset = count + 1;
    this->_indexReadBuffer.clear();
  };
  if (this->infos().filterConditionIsEmpty()) {
    skipped = this->_reader->live_docs_count();
    size_t totalPos = std::accumulate(
        _segmentReaders.begin(), _segmentReaders.end(), size_t{0},
        [](size_t acc, auto const& r) { return acc + r.totalPos; });
    TRI_ASSERT(totalPos <= skipped);
    skipped -= std::min(skipped, totalPos);
  } else {
    auto const approximate = this->infos().countApproximate();
    // possible overfetch due to parallelisation
    skipped = this->_indexReadBuffer.size();
    for (auto& r : _segmentReaders) {
      if (r.itr) {
        skipped += calculateSkipAllCount(approximate, r.currentSegmentPos,
                                         r.itr.get());
        reset(r);
      }
    }
    auto& reader = _segmentReaders.front();
    while (_segmentOffset < count) {
      reader.readerOffset = _segmentOffset++;
      if (!resetIterator(reader)) {
        continue;
      }
      skipped += calculateSkipAllCount(approximate, 0, reader.itr.get());
      reset(reader);
    }
  }
  return skipped;
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::writeRow(ReadContext& ctx,
                                                      size_t idx) {
  auto const& value = this->_indexReadBuffer.getValue(idx);
  return Base::writeRowImpl(ctx, idx, value);
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
  TRI_ASSERT(infos.heapSort().empty());
}

template<typename ExecutionTraits>
IResearchViewMergeExecutor<ExecutionTraits>::Segment::Segment(
    irs::doc_iterator::ptr&& docs, irs::document const& doc,
    irs::score const& score, size_t numScores,
    LogicalCollection const* collection, irs::doc_iterator::ptr&& pkReader,
    size_t index, irs::doc_iterator* sortReaderRef,
    irs::payload const* sortReaderValue,
    irs::doc_iterator::ptr&& sortReader) noexcept
    : docs(std::move(docs)),
      doc(&doc),
      score(&score),
      numScores(numScores),
      collection(collection),
      segmentIndex(index),
      sortReaderRef(sortReaderRef),
      sortValue(sortReaderValue),
      sortReader(std::move(sortReader)) {
  TRI_ASSERT(this->docs);
  TRI_ASSERT(this->doc);
  TRI_ASSERT(this->score);
  TRI_ASSERT(this->sortReaderRef);
  TRI_ASSERT(this->sortValue);
  if constexpr (Base::isMaterialized) {
    ::resetColumn(this->pkReader, std::move(pkReader));
    TRI_ASSERT(this->pkReader.itr);
    TRI_ASSERT(this->pkReader.value);
  }
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
  return _less.Compare((*_segments)[rhs].sortValue->value,
                       (*_segments)[lhs].sortValue->value) < 0;
}

template<typename ExecutionTraits>
void IResearchViewMergeExecutor<ExecutionTraits>::reset(
    [[maybe_unused]] bool needFullCount) {
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

    auto it = segment.mask(this->_filter->execute({
        .segment = segment,
        .scorers = this->_scorers,
        .ctx = &this->_filterCtx,
        .wand = {},
    }));
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
    arangodb::LogicalCollection const* collection{nullptr};
    irs::doc_iterator::ptr pkReader;
    if constexpr (Base::isMaterialized) {
      collection = &this->_reader->collection(i);
      pkReader = ::pkColumn(segment);
      if (ADB_UNLIKELY(!pkReader)) {
        LOG_TOPIC("ee041", WARN, arangodb::iresearch::TOPIC)
            << "encountered a sub-reader without a primary key column while "
               "executing a query, ignoring";
        continue;
      }
    }

    if constexpr (Base::usesStoredValues) {
      if (ADB_UNLIKELY(!this->getStoredValuesReaders(segment, i))) {
        continue;
      }
    } else {
      TRI_ASSERT(!isSortReaderUsedInStoredValues);
    }

    if (isSortReaderUsedInStoredValues) {
      TRI_ASSERT(i * storedValuesCount < this->_storedValuesReaders.size());
      auto& sortReader = this->_storedValuesReaders[i * storedValuesCount];

      _segments.emplace_back(std::move(it), *doc, *score, numScores, collection,
                             std::move(pkReader), i, sortReader.itr.get(),
                             sortReader.value, nullptr);
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

      _segments.emplace_back(std::move(it), *doc, *score, numScores, collection,
                             std::move(pkReader), i, itr.get(), sortValue,
                             std::move(itr));
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
bool IResearchViewMergeExecutor<ExecutionTraits>::fillBuffer(ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);

  size_t const atMost =
      Base::isLateMaterialized ? 1 : ctx.outputRow.numRowsLeft();
  TRI_ASSERT(this->_indexReadBuffer.empty());
  this->_indexReadBuffer.reset();
  this->_indexReadBuffer.preAllocateStoredValuesBuffer(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  bool gotData{false};
  while (_heap_it.next()) {
    auto& segment = _segments[_heap_it.value()];
    ++segment.segmentPos;
    TRI_ASSERT(segment.docs);
    TRI_ASSERT(segment.doc);
    TRI_ASSERT(segment.score);
    // try to read a document PK from iresearch
    LocalDocumentId documentId;

    if constexpr (Base::isMaterialized) {
      TRI_ASSERT(segment.collection);
      TRI_ASSERT(segment.pkReader.itr);
      TRI_ASSERT(segment.pkReader.value);
      documentId = readPK(segment);

      if (!documentId.isSet()) {
        // No document read, we cannot write it.
        continue;
      }
    }
    auto& viewSegment = this->_reader->segment(segment.segmentIndex);
    if constexpr (Base::isLateMaterialized) {
      this->_indexReadBuffer.makeValue(PushTag{}, viewSegment,
                                       segment.doc->value);
    } else {
      this->_indexReadBuffer.makeValue(PushTag{}, viewSegment, documentId);
    }
    gotData = true;
    // in the ordered case we have to write scores as well as a document
    if constexpr (ExecutionTraits::Ordered) {
      // Writes into _scoreBuffer
      this->fillScores(*segment.score);
    }

    if constexpr (Base::usesStoredValues) {
      TRI_ASSERT(segment.doc);
      this->makeStoredValues(PushTag{}, segment.doc->value,
                             segment.segmentIndex);
    }

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(this->infos().searchDocIdRegId().isValid() ==
                 ExecutionTraits::EmitSearchDoc);
      this->_indexReadBuffer.makeSearchDoc(PushTag{}, viewSegment,
                                           segment.doc->value);
    }

    // doc and scores are both pushed, sizes must now be coherent
    this->_indexReadBuffer.assertSizeCoherence();

    if (this->_indexReadBuffer.size() >= atMost) {
      break;
    }
  }
  return gotData;
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
bool IResearchViewMergeExecutor<ExecutionTraits>::writeRow(ReadContext& ctx,
                                                           size_t idx) {
  auto const& value = this->_indexReadBuffer.getValue(idx);
  return Base::writeRowImpl(ctx, idx, value);
}
