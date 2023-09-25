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
#include "Aql/MultiGet.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "ApplicationFeatures/ApplicationServer.h"
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

namespace arangodb::aql {

inline size_t calculateSkipAllCount(iresearch::CountApproximate approximation,
                                    size_t currentPos,
                                    irs::doc_iterator* docs) {
  TRI_ASSERT(docs);
  size_t skipped{0};
  switch (approximation) {
    case iresearch::CountApproximate::Cost: {
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
      TRI_ASSERT(iresearch::CountApproximate::Exact == approximation);
      while (docs->next()) {
        skipped++;
      }
      break;
  }
  return skipped;
}

inline void resetColumn(ColumnIterator& column,
                        irs::doc_iterator::ptr&& itr) noexcept {
  TRI_ASSERT(itr);
  column.itr = std::move(itr);
  column.value = irs::get<irs::payload>(*column.itr);
  if (ADB_UNLIKELY(!column.value)) {
    column.value = &iresearch::NoPayload;
  }
}

inline constexpr int kSortMultiplier[]{-1, 1};

inline constexpr irs::bytes_view kNullSlice{VPackSlice::nullSliceData, 1};

template<typename ValueType, typename HeapSortType>
class BufferHeapSortContext {
 public:
  explicit BufferHeapSortContext(std::span<HeapSortValue const> heapSortValues,
                                 std::span<HeapSortType const> heapSort)
      : _heapSortValues(heapSortValues), _heapSort(heapSort) {}

  bool operator()(size_t a, size_t b) const noexcept {
    TRI_ASSERT(_heapSortValues.size() > a * _heapSort.size())
        << "Size:" << _heapSortValues.size() << " Index:" << a;
    TRI_ASSERT(_heapSortValues.size() > b * _heapSort.size())
        << "Size:" << _heapSortValues.size() << " Index:" << b;
    auto lhs_stored = &_heapSortValues[a * _heapSort.size()];
    auto rhs_stored = &_heapSortValues[b * _heapSort.size()];
    for (auto const& cmp : _heapSort) {
      if (cmp.isScore()) {
        if (lhs_stored->score != rhs_stored->score) {
          return cmp.ascending ? lhs_stored->score < rhs_stored->score
                               : lhs_stored->score > rhs_stored->score;
        }
      } else {
        auto res = basics::VelocyPackHelper::compare(lhs_stored->slice,
                                                     rhs_stored->slice, true);
        if (res != 0) {
          return (kSortMultiplier[size_t{cmp.ascending}] * res) < 0;
        }
      }
      ++lhs_stored;
      ++rhs_stored;
    }
    return false;
  }

  template<typename StoredValueProvider>
  bool compareInput(size_t lhsIdx, float_t const* rhs_scores,
                    StoredValueProvider const& stored) const noexcept {
    TRI_ASSERT(_heapSortValues.size() > lhsIdx * _heapSort.size());
    auto lhs_values = &_heapSortValues[lhsIdx * _heapSort.size()];
    for (auto const& cmp : _heapSort) {
      if (cmp.isScore()) {
        if (lhs_values->score != rhs_scores[cmp.source]) {
          return cmp.ascending ? lhs_values->score < rhs_scores[cmp.source]
                               : lhs_values->score > rhs_scores[cmp.source];
        }
      } else {
        auto value = stored(cmp);
        auto res =
            basics::VelocyPackHelper::compare(lhs_values->slice, value, true);
        if (res != 0) {
          return (kSortMultiplier[size_t{cmp.ascending}] * res) < 0;
        }
      }
      ++lhs_values;
    }
    return false;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool descScore() const noexcept {
    return !_heapSort.empty() && _heapSort.front().isScore() &&
           !_heapSort.front().ascending;
  }
#endif

 private:
  std::span<HeapSortValue const> _heapSortValues;
  std::span<HeapSortType const> _heapSort;
};

inline velocypack::Slice getStoredValue(
    irs::bytes_view storedValue, iresearch::HeapSortElement const& sort) {
  TRI_ASSERT(!sort.isScore());
  TRI_ASSERT(!storedValue.empty());
  auto* start = storedValue.data();
  [[maybe_unused]] auto* end = start + storedValue.size();
  velocypack::Slice slice{start};
  for (size_t i = 0; i != sort.fieldNumber; ++i) {
    start += slice.byteSize();
    TRI_ASSERT(start < end);
    slice = velocypack::Slice{start};
  }
  TRI_ASSERT(!slice.isNone());
  if (sort.postfix.empty()) {
    return slice;
  }
  if (!slice.isObject()) {
    return velocypack::Slice::nullSlice();
  }
  auto value = slice.get(sort.postfix);
  return !value.isNone() ? value : velocypack::Slice::nullSlice();
}

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    iresearch::ViewSnapshotPtr reader, RegisterId outRegister,
    RegisterId searchDocRegister, std::vector<RegisterId> scoreRegisters,
    QueryContext& query,
#ifdef USE_ENTERPRISE
    iresearch::IResearchOptimizeTopK const& optimizeTopK,
#endif
    std::vector<iresearch::SearchFunc> const& scorers,
    std::pair<iresearch::IResearchSortBase const*, size_t> sort,
    iresearch::IResearchViewStoredValues const& storedValues,
    ExecutionPlan const& plan, Variable const& outVariable,
    AstNode const& filterCondition, std::pair<bool, bool> volatility,
    VarInfoMap const& varInfoMap, int depth,
    iresearch::IResearchViewNode::ViewValuesRegisters&&
        outNonMaterializedViewRegs,
    iresearch::CountApproximate countApproximate,
    iresearch::FilterOptimization filterOptimization,
    std::vector<iresearch::HeapSortElement> const& heapSort,
    size_t heapSortLimit, iresearch::SearchMeta const* meta)
    : _searchDocOutReg{searchDocRegister},
      _documentOutReg{outRegister},
      _scoreRegisters{std::move(scoreRegisters)},
      _scoreRegistersCount{_scoreRegisters.size()},
      _reader{std::move(reader)},
      _query{query},
#ifdef USE_ENTERPRISE
      _optimizeTopK{optimizeTopK},
#endif
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
      _meta{meta},
      _depth{depth},
      _filterConditionIsEmpty{
          iresearch::isFilterConditionEmpty(&_filterCondition) &&
          !_reader->hasNestedFields()},
      _volatileSort{volatility.second},
      // `_volatileSort` implies `_volatileFilter`
      _volatileFilter{_volatileSort || volatility.first} {
  TRI_ASSERT(_reader != nullptr);
}

inline ExecutionStats& operator+=(
    ExecutionStats& executionStats,
    IResearchViewStats const& viewStats) noexcept {
  executionStats.scannedIndex += viewStats.getScanned();
  return executionStats;
}

template<typename Impl, typename ExecutionTraits>
IResearchViewExecutorBase<Impl, ExecutionTraits>::ReadContext::ReadContext(
    RegisterId documentOutReg, InputAqlItemRow& inputRow,
    OutputAqlItemRow& outputRow)
    : inputRow(inputRow),
      outputRow(outputRow),
      documentOutReg(documentOutReg) {}

template<typename Impl, typename ExecutionTraits>
void IResearchViewExecutorBase<Impl, ExecutionTraits>::ReadContext::moveInto(
    std::unique_ptr<uint8_t[]> data) noexcept {
  static_assert(isMaterialized);
  AqlValue value{std::move(data)};
  bool mustDestroy = true;
  AqlValueGuard guard{value, mustDestroy};
  // TODO(MBkkt) add moveValueInto overload for std::unique_ptr<uint8_t[]>
  outputRow.moveValueInto(documentOutReg, inputRow, guard);
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
            BufferHeapSortContext<StoredValue, BufferHeapSortElement>{
                _heapSortValues, _heapSort});
  if (_heapSizeLeft) {
    // heap was not filled up to the limit. So fill buffer here.
    _storedValuesBuffer.resize(_keyBuffer.size() * _storedValuesCount);
  }
}

template<typename ValueType, bool copySorted>
template<typename ColumnReaderProvider>
velocypack::Slice IndexReadBuffer<ValueType, copySorted>::readHeapSortColumn(
    iresearch::HeapSortElement const& cmp, irs::doc_id_t doc,
    ColumnReaderProvider& readerProvider, size_t readerSlot) {
  TRI_ASSERT(_heapOnlyStoredValuesReaders.size() >= readerSlot);
  TRI_ASSERT(!cmp.isScore());
  if (_heapOnlyStoredValuesReaders.size() == readerSlot) {
    _heapOnlyStoredValuesReaders.push_back(readerProvider(cmp.source));
  }
  auto& reader = _heapOnlyStoredValuesReaders[readerSlot];
  irs::bytes_view val = kNullSlice;
  TRI_ASSERT(!reader.itr || reader.itr->value() <= doc);
  if (reader.itr && doc == reader.itr->seek(doc) &&
      !reader.value->value.empty()) {
    val = reader.value->value;
  }
  TRI_ASSERT(_currentDocumentBuffer.capacity() > _currentDocumentBuffer.size());
  _currentDocumentBuffer.emplace_back(val);
  if constexpr (copySorted) {
    return getStoredValue(_currentDocumentBuffer.back(), cmp);
  } else {
    return _currentDocumentSlices.emplace_back(
        getStoredValue(_currentDocumentBuffer.back(), cmp));
  }
}

template<typename ValueType, bool copySorted>
template<bool fullHeap, typename ColumnReaderProvider>
void IndexReadBuffer<ValueType, copySorted>::finalizeHeapSortDocument(
    size_t idx, irs::doc_id_t doc, std::span<float_t const> scores,
    ColumnReaderProvider& readerProvider) {
  auto const heapSortSize = _heapSort.size();
  size_t heapSortValuesIndex = idx * heapSortSize;
  size_t storedSliceIdx{0};
  if constexpr (!copySorted) {
    TRI_ASSERT(_currentDocumentSlices.size() == _currentDocumentBuffer.size());
  }
  if constexpr (fullHeap) {
    TRI_ASSERT(heapSortValuesIndex < _heapSortValues.size());
  } else {
    TRI_ASSERT(heapSortValuesIndex == _heapSortValues.size());
  }
  for (auto const& cmp : _heapSort) {
    if (cmp.isScore()) {
      if constexpr (fullHeap) {
        _heapSortValues[heapSortValuesIndex].score = scores[cmp.source];
      } else {
        _heapSortValues.push_back(HeapSortValue{.score = scores[cmp.source]});
      }
    } else {
      TRI_ASSERT(cmp.container);
      auto& valueSlot =
          *(cmp.container->data() + idx * cmp.multiplier + cmp.offset);
      velocypack::Slice slice;
      if (storedSliceIdx < _currentDocumentBuffer.size()) {
        valueSlot = std::move(_currentDocumentBuffer[storedSliceIdx]);
        if constexpr (copySorted) {
          slice = getStoredValue(valueSlot, cmp);
        } else {
          slice = _currentDocumentSlices[storedSliceIdx];
        }
      } else {
        TRI_ASSERT(_heapOnlyStoredValuesReaders.size() >= storedSliceIdx);
        if (_heapOnlyStoredValuesReaders.size() == storedSliceIdx) {
          _heapOnlyStoredValuesReaders.push_back(readerProvider(cmp.source));
        }
        auto& reader = _heapOnlyStoredValuesReaders[storedSliceIdx];
        TRI_ASSERT(!reader.itr || reader.itr->value() <= doc);
        irs::bytes_view val = kNullSlice;
        if (reader.itr && doc == reader.itr->seek(doc) &&
            !reader.value->value.empty()) {
          val = reader.value->value;
        }
        valueSlot = val;
        slice = getStoredValue(valueSlot, cmp);
      }
      if constexpr (fullHeap) {
        _heapSortValues[heapSortValuesIndex].slice = slice;
      } else {
        _heapSortValues.push_back(HeapSortValue{.slice = slice});
      }
      TRI_ASSERT(getStoredValue(valueSlot, cmp).begin() ==
                 _heapSortValues[heapSortValuesIndex].slice.begin());
      ++storedSliceIdx;
    }
    ++heapSortValuesIndex;
  }
}

template<typename ValueType, bool copySorted>
template<typename ColumnReaderProvider>
void IndexReadBuffer<ValueType, copySorted>::pushSortedValue(
    ColumnReaderProvider& columnReader, ValueType&& value,
    std::span<float_t const> scores, irs::score& score,
    irs::score_t& threshold) {
  TRI_ASSERT(_maxSize);
  using HeapSortContext =
      BufferHeapSortContext<StoredValue, BufferHeapSortElement>;
  if constexpr (!copySorted) {
    _currentDocumentSlices.clear();
  }
  _currentDocumentBuffer.clear();
  if (_currentReaderOffset != value.readerOffset()) {
    _currentReaderOffset = value.readerOffset();
    _heapOnlyStoredValuesReaders.clear();
  }
  if (ADB_LIKELY(!_heapSizeLeft)) {
    HeapSortContext sortContext(_heapSortValues, _heapSort);
    size_t readerSlot{0};
    if (sortContext.compareInput(_rows.front(), scores.data(),
                                 [&](iresearch::HeapSortElement const& cmp) {
                                   return readHeapSortColumn(cmp, value.docId(),
                                                             columnReader,
                                                             readerSlot++);
                                 })) {
      return;  // not interested in this document
    }
    std::pop_heap(_rows.begin(), _rows.end(), sortContext);
    // now last contains "free" index in the buffer
    finalizeHeapSortDocument<true>(_rows.back(), value.docId(), scores,
                                   columnReader);
    _keyBuffer[_rows.back()] = std::move(value);
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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(!sortContext.descScore() ||
               threshold <= _scoreBuffer[_rows.front() * _numScoreRegisters]);
#endif
    threshold = _scoreBuffer[_rows.front() * _numScoreRegisters];
    score.Min(threshold);
  } else {
    finalizeHeapSortDocument<false>(_rows.size(), value.docId(), scores,
                                    columnReader);
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
      HeapSortContext sortContext(_heapSortValues, _heapSort);
      _storedValuesBuffer.resize(_keyBuffer.size() * _storedValuesCount);
      std::make_heap(_rows.begin(), _rows.end(), sortContext);
    }
  }
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

template<typename ValueType, bool copyStored>
std::vector<size_t> IndexReadBuffer<ValueType, copyStored>::getMaterializeRange(
    size_t skip) const {
  // TODO(MBkkt) avoid unnecessary copies and conditions here!
  auto const size = _keyBuffer.size();
  TRI_ASSERT(_keyBaseIdx <= size);
  TRI_ASSERT(skip <= _rows.size());
  TRI_ASSERT(_rows.size() <= size);
  skip = skip == 0 ? _keyBaseIdx : skip;
  std::vector<size_t> rows(size - skip);
  if (!_rows.empty()) {
    std::copy(_rows.begin() + skip, _rows.end(), rows.begin());
  } else {
    std::iota(rows.begin(), rows.end(), skip);
  }
  return rows;
}

template<typename Impl, typename ExecutionTraits>
IResearchViewExecutorBase<Impl, ExecutionTraits>::IResearchViewExecutorBase(
    Fetcher&, Infos& infos)
    : _trx(infos.getQuery().newTrxContext()),
      _memory(infos.getQuery().resourceMonitor()),
      _infos(infos),
      _inputRow(CreateInvalidInputRowHint{}),  // TODO: Remove me after refactor
      _indexReadBuffer(_infos.getScoreRegisters().size(),
                       _infos.getQuery().resourceMonitor()),
      _ctx(_trx, infos.getQuery(), _aqlFunctionsInternalCache,
           infos.outVariable(), infos.varInfoMap(), infos.getDepth()),
      _filterCtx(_ctx),
      _reader(infos.getReader()),
      _filter(irs::filter::prepared::empty()) {
  if constexpr (ExecutionTraits::EmitSearchDoc) {
    TRI_ASSERT(this->infos().searchDocIdRegId().isValid());
    auto const* key = &infos.outVariable();
    _filterCookie = &iresearch::ensureFilterCookie(_trx, key).filter;
  }

  if (auto const* meta = infos.meta(); meta != nullptr) {
    auto const& vocbase = _trx.vocbase();
    auto const& analyzerFeature =
        vocbase.server().getFeature<iresearch::IResearchAnalyzerFeature>();
    TRI_ASSERT(_trx.state());
    auto const& revision = _trx.state()->analyzersRevision();
    auto getAnalyzer =
        [&](std::string_view shortName) -> iresearch::FieldMeta::Analyzer {
      auto analyzer = analyzerFeature.get(shortName, vocbase, revision,
                                          _trx.state()->operationOrigin());
      if (!analyzer) {
        return iresearch::makeEmptyAnalyzer();
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
  while (inputRange.hasDataRow() && call.needSkipMore()) {
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
    TRI_ASSERT(_indexReadBuffer.empty() || !infos().heapSort().empty());

    if (call.needSkipMore()) {
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
void IResearchViewExecutorBase<Impl, ExecutionTraits>::materialize() {
  // TODO(MBkkt) unify it more with MaterializeExecutor
  auto pkReadingOrder = _indexReadBuffer.getMaterializeRange(0);
  auto it = pkReadingOrder.begin();
  auto end = pkReadingOrder.end();
  iresearch::ViewSegment const* segment{};
  if constexpr (Impl::kSorted) {
    std::sort(it, end, [&](size_t lhs, size_t rhs) {
      auto const& lhs_val = _indexReadBuffer.getValue(lhs);
      auto const& rhs_val = _indexReadBuffer.getValue(rhs);
      auto const* lhs_segment = lhs_val.segment();
      auto const* rhs_segment = rhs_val.segment();
      return lhs_segment->snapshot < rhs_segment->snapshot;
    });
  } else {
    segment = static_cast<Impl&>(*this)._segment;
    _context.snapshot = segment->snapshot;
  }
  StorageSnapshot const* lastSnapshot{};
  auto fillKeys = [&] {
    if (it == end) {
      return MultiGetState::kLast;
    }
    auto& doc = _indexReadBuffer.getValue(*it);
    if constexpr (Impl::kSorted) {
      segment = doc.segment();
      auto const* snapshot = segment->snapshot;
      if (lastSnapshot != snapshot) {
        lastSnapshot = snapshot;
        return MultiGetState::kWork;
      }
      _context.snapshot = snapshot;
    }

    ++it;
    auto const storage = doc.value().id;
    TRI_ASSERT(storage.isSet());

    auto& logical = *segment->collection;
    auto const i = _context.serialize(_trx, logical, storage);
    doc.translate(i);

    return MultiGetState::kNext;
  };
  _context.multiGet(pkReadingOrder.size(), fillKeys);
  _isMaterialized = true;
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
        stats.incrScanned(impl.getScanned());
      }
    }
    if constexpr (isMaterialized) {
      if (ADB_UNLIKELY(!_isMaterialized)) {
        materialize();
      }
    }

    auto const idx = _indexReadBuffer.pop_front();
    if (ADB_LIKELY(impl.writeRow(ctx, idx))) {
      break;
    } else {
      // to get correct stats we should continue looking for
      // other documents inside this one call
      LOG_TOPIC("550cd", TRACE, iresearch::TOPIC)
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
        .namePrefix = iresearch::nestedRoot(_reader->hasNestedFields()),
        .isSearchQuery = true,
        .isOldMangling = infos().isOldMangling(),
    };

    // The analyzer is referenced in the FilterContext and used during the
    // following ::makeFilter() call, so can't be a temporary.
    auto const emptyAnalyzer = iresearch::makeEmptyAnalyzer();
    iresearch::AnalyzerProvider* fieldAnalyzerProvider = nullptr;
    auto const* contextAnalyzer = &iresearch::FieldMeta::identity();
    if (!infos().isOldMangling()) {
      fieldAnalyzerProvider = &_provider;
      contextAnalyzer = &emptyAnalyzer;
    }

    iresearch::FilterContext const filterCtx{
        .query = queryCtx,
        .contextAnalyzer = *contextAnalyzer,
        .fieldAnalyzerProvider = fieldAnalyzerProvider,
    };

    irs::Or root;
    auto const rv = iresearch::FilterFactory::filter(&root, filterCtx,
                                                     infos().filterCondition());

    if (rv.fail()) {
      velocypack::Builder builder;
      infos().filterCondition().toVelocyPack(builder, true);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          rv.errorNumber(),
          absl::StrCat("failed to build filter while querying arangosearch "
                       "view, query '",
                       builder.toJson(), "': ", rv.errorMessage()));
    }

    if (infos().volatileSort() || !_isInitialized) {
      auto const& scorers = infos().scorers();

      _scorersContainer.clear();
      _scorersContainer.reserve(scorers.size());

      for (irs::Scorer::ptr scorer; auto const& scorerNode : scorers) {
        TRI_ASSERT(scorerNode.node);

        if (!iresearch::order_factory::scorer(&scorer, *scorerNode.node,
                                              queryCtx)) {
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
    _filter = root.prepare({
        .index = *_reader,
        .memory = _memory,
        .scorers = _scorers,
        .ctx = &_filterCtx,
    });

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(_filterCookie);
      *_filterCookie = _filter.get();
    }

    _isInitialized = true;
  }
}

template<typename Impl, typename ExecutionTraits>
void IResearchViewExecutorBase<Impl, ExecutionTraits>::writeSearchDoc(
    ReadContext& ctx, iresearch::SearchDoc const& doc, RegisterId reg) {
  TRI_ASSERT(doc.isValid());
  AqlValue value{doc.encode(_buf)};
  AqlValueGuard guard{value, true};
  ctx.outputRow.moveValueInto(reg, ctx.inputRow, guard);
}

// make overload with string_view as input that not depends on SV container type
template<typename Impl, typename ExecutionTraits>
inline bool IResearchViewExecutorBase<Impl, ExecutionTraits>::writeStoredValue(
    ReadContext& ctx,
    typename IndexReadBufferType::StoredValues const& storedValues,
    size_t index, FieldRegisters const& fieldsRegs) {
  TRI_ASSERT(index < storedValues.size());
  auto const& storedValue = storedValues[index];
  return writeStoredValue(ctx, storedValue, fieldsRegs);
}

template<typename Impl, typename ExecutionTraits>
inline bool IResearchViewExecutorBase<Impl, ExecutionTraits>::writeStoredValue(
    ReadContext& ctx, irs::bytes_view storedValue,
    FieldRegisters const& fieldsRegs) {
  TRI_ASSERT(!storedValue.empty());
  auto* start = storedValue.data();
  [[maybe_unused]] auto* end = start + storedValue.size();
  velocypack::Slice slice{start};
  size_t i = 0;
  for (auto const& [fieldNum, registerId] : fieldsRegs) {
    while (i < fieldNum) {
      start += slice.byteSize();
      TRI_ASSERT(start < end);
      slice = velocypack::Slice{start};
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
    auto reg = infos().searchDocIdRegId();
    TRI_ASSERT(reg.isValid());

    writeSearchDoc(ctx, _indexReadBuffer.getSearchDoc(idx), reg);
  }
  if constexpr (isMaterialized) {
    auto& r = _context.values[value.result];
    if (ADB_UNLIKELY(!r)) {
      return false;
    }
    ctx.moveInto(std::move(r));
  }
  if constexpr (isLateMaterialized) {
    writeSearchDoc(ctx, value, ctx.getDocumentIdReg());
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
  } else if constexpr (Traits::MaterializeType ==
                           iresearch::MaterializeType::NotMaterialize &&
                       !Traits::Ordered && !Traits::EmitSearchDoc) {
    ctx.outputRow.copyRow(ctx.inputRow);
  }
  // in the ordered case we have to write scores as well as a document
  // move to separate function that gets ScoresIterator
  if constexpr (Traits::Ordered) {
    // scorer register are placed right before the document output register
    auto const& scoreRegisters = infos().getScoreRegisters();
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
    _indexReadBuffer.pushStoredValue(kNullSlice);
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
    irs::SubReader const& segmentReader, size_t storedValuesIndex /*= 0*/) {
  auto const& columnsFieldsRegs = _infos.getOutNonMaterializedViewRegs();
  static constexpr bool kHeapSort =
      std::is_same_v<HeapSortExecutorValue,
                     typename Traits::IndexBufferValueType>;
  if (!columnsFieldsRegs.empty()) {
    auto columnFieldsRegs = columnsFieldsRegs.cbegin();
    auto index = storedValuesIndex * columnsFieldsRegs.size();
    if (iresearch::IResearchViewNode::kSortColumnNumber ==
        columnFieldsRegs->first) {
      if (!kHeapSort || !_storedColumnsMask.contains(columnFieldsRegs->first)) {
        auto sortReader = iresearch::sortColumn(segmentReader);
        if (ADB_UNLIKELY(!sortReader)) {
          LOG_TOPIC("bc5bd", WARN, iresearch::TOPIC)
              << "encountered a sub-reader without a sort column while "
                 "executing a query, ignoring";
          return false;
        }
        resetColumn(_storedValuesReaders[index++], std::move(sortReader));
      }
      ++columnFieldsRegs;
    }
    // if stored values exist
    if (columnFieldsRegs != columnsFieldsRegs.cend()) {
      auto const& columns = _infos.storedValues().columns();
      TRI_ASSERT(!columns.empty());
      for (; columnFieldsRegs != columnsFieldsRegs.cend(); ++columnFieldsRegs) {
        TRI_ASSERT(iresearch::IResearchViewNode::kSortColumnNumber <
                   columnFieldsRegs->first);
        if constexpr (kHeapSort) {
          if (_storedColumnsMask.contains(columnFieldsRegs->first)) {
            continue;
          }
        }
        auto const storedColumnNumber =
            static_cast<size_t>(columnFieldsRegs->first);
        TRI_ASSERT(storedColumnNumber < columns.size());
        auto const* storedValuesReader =
            segmentReader.column(columns[storedColumnNumber].name);
        if (ADB_UNLIKELY(!storedValuesReader)) {
          LOG_TOPIC("af7ec", WARN, iresearch::TOPIC)
              << "encountered a sub-reader without a stored value column while "
                 "executing a query, ignoring";
          return false;
        }
        resetColumn(_storedValuesReaders[index++],
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
  TRI_ASSERT(infos.heapSort().empty());
}

inline void commonReadPK(ColumnIterator& it, irs::doc_id_t docId,
                         LocalDocumentId& documentId) {
  TRI_ASSERT(it.itr);
  TRI_ASSERT(it.value);
  bool r = it.itr->seek(docId) == docId;
  if (ADB_LIKELY(r)) {
    r = iresearch::DocumentPrimaryKey::read(documentId, it.value->value);
    TRI_ASSERT(r == documentId.isSet());
  }
  if (ADB_UNLIKELY(!r)) {
    LOG_TOPIC("6442f", WARN, iresearch::TOPIC)
        << "failed to read document primary key while reading document from "
           "arangosearch view, doc_id: "
        << docId;
  }
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::readPK(
    LocalDocumentId& documentId) {
  TRI_ASSERT(!documentId.isSet());
  TRI_ASSERT(_itr);
  if (!_itr->next()) {
    return false;
  }
  ++_totalPos;
  ++_currentSegmentPos;
  if constexpr (Base::isMaterialized) {
    TRI_ASSERT(_doc);
    commonReadPK(_pkReader, _doc->value, documentId);
  }
  return true;
}

template<typename ExecutionTraits>
IResearchViewHeapSortExecutor<ExecutionTraits>::IResearchViewHeapSortExecutor(
    Fetcher& fetcher, Infos& infos)
    : Base{fetcher, infos} {
  this->_indexReadBuffer.setHeapSort(
      this->_infos.heapSort(), this->_infos.getOutNonMaterializedViewRegs());
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
  this->_isMaterialized = false;
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
  auto const size = this->_indexReadBuffer.size();
  if (limit < size) {
    this->_indexReadBuffer.skip(limit);
    return limit;
  }
  this->_indexReadBuffer.clear();
  return size;
}

template<typename ExecutionTraits>
void IResearchViewHeapSortExecutor<ExecutionTraits>::reset(
    [[maybe_unused]] bool needFullCount) {
  Base::reset();
#ifdef USE_ENTERPRISE
  if (!needFullCount) {
    this->_wand = this->_infos.optimizeTopK().makeWandContext(
        this->_infos.heapSort(), this->_scorers);
  }
#endif
  _totalCount = 0;
  _bufferedCount = 0;
  _bufferFilled = false;
  this->_storedValuesReaders.resize(
      this->_reader->size() *
      this->_infos.getOutNonMaterializedViewRegs().size());
}

template<typename ExecutionTraits>
bool IResearchViewHeapSortExecutor<ExecutionTraits>::writeRow(ReadContext& ctx,
                                                              size_t idx) {
  static_assert(!Base::isLateMaterialized,
                "HeapSort superseeds LateMaterialization");
  auto const& value = this->_indexReadBuffer.getValue(idx);
  return Base::writeRowImpl(ctx, idx, value.value());
}

template<typename ExecutionTraits>
bool IResearchViewHeapSortExecutor<ExecutionTraits>::fillBuffer(ReadContext&) {
  fillBufferInternal(0);
  // FIXME: Use additional flag from fillBufferInternal
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
  this->_isMaterialized = false;
  this->_indexReadBuffer.reset();
  this->_indexReadBuffer.preAllocateStoredValuesBuffer(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  auto const count = this->_reader->size();

  for (auto const& cmp : this->_infos.heapSort()) {
    if (!cmp.isScore()) {
      this->_storedColumnsMask.insert(cmp.source);
    }
  }

  containers::SmallVector<irs::score_t, 4> scores;
  if constexpr (ExecutionTraits::Ordered) {
    scores.resize(this->infos().scorers().size());
  }

  irs::doc_iterator::ptr itr;
  irs::document const* doc{};
  irs::score* scr = const_cast<irs::score*>(&irs::score::kNoScore);
  size_t numScores{0};
  irs::score_t threshold = 0.f;
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
        auto* score = irs::get_mutable<irs::score>(itr.get());
        if (score != nullptr) {
          scr = score;
          numScores = scores.size();
          scr->Min(threshold);
        }
      }
      itr = segmentReader.mask(std::move(itr));
      TRI_ASSERT(itr);
    }
    if (!itr->next()) {
      ++readerOffset;
      itr.reset();
      doc = nullptr;
      scr = const_cast<irs::score*>(&irs::score::kNoScore);
      numScores = 0;
      continue;
    }
    ++_totalCount;

    (*scr)(scores.data());
    auto provider = [this, readerOffset](ptrdiff_t column) {
      auto& segmentReader = (*this->_reader)[readerOffset];
      ColumnIterator it;
      if (iresearch::IResearchViewNode::kSortColumnNumber == column) {
        auto sortReader = iresearch::sortColumn(segmentReader);
        if (ADB_LIKELY(sortReader)) {
          resetColumn(it, std::move(sortReader));
        }
      } else {
        auto const& columns = this->_infos.storedValues().columns();
        auto const* storedValuesReader =
            segmentReader.column(columns[column].name);
        if (ADB_LIKELY(storedValuesReader)) {
          resetColumn(it,
                      storedValuesReader->iterator(irs::ColumnHint::kNormal));
        }
      }
      return it;
    };
    this->_indexReadBuffer.pushSortedValue(
        provider, HeapSortExecutorValue{doc->value, readerOffset},
        std::span{scores.data(), numScores}, *scr, threshold);
  }
  this->_indexReadBuffer.finalizeHeapSort();
  _bufferedCount = this->_indexReadBuffer.size();
  if (skip >= _bufferedCount) {
    return true;
  }
  auto pkReadingOrder = this->_indexReadBuffer.getMaterializeRange(skip);
  std::sort(pkReadingOrder.begin(), pkReadingOrder.end(),
            [buffer = &this->_indexReadBuffer](size_t lhs, size_t rhs) {
              auto const& lhs_val = buffer->getValue(lhs);
              auto const& rhs_val = buffer->getValue(rhs);
              auto const lhs_offset = lhs_val.readerOffset();
              auto const rhs_offset = rhs_val.readerOffset();
              if (lhs_offset != rhs_offset) {
                return lhs_offset < rhs_offset;
              }
              return lhs_val.docId() < rhs_val.docId();
            });

  size_t lastSegmentIdx = count;
  ColumnIterator pkReader;
  iresearch::ViewSegment const* segment{};
  auto orderIt = pkReadingOrder.begin();
  while (orderIt != pkReadingOrder.end()) {
    auto& value = this->_indexReadBuffer.getValue(*orderIt);
    auto const docId = value.docId();
    auto const segmentIdx = value.readerOffset();
    if (lastSegmentIdx != segmentIdx) {
      auto& segmentReader = (*this->_reader)[segmentIdx];
      auto pkIt = iresearch::pkColumn(segmentReader);
      pkReader.itr.reset();
      segment = &this->_reader->segment(segmentIdx);
      if (ADB_UNLIKELY(!pkIt)) {
        LOG_TOPIC("bd02b", WARN, iresearch::TOPIC)
            << "encountered a sub-reader without a primary key column "
               "while executing a query, ignoring";
        continue;
      }
      resetColumn(pkReader, std::move(pkIt));
      if constexpr (Base::contains(
                        iresearch::MaterializeType::UseStoredValues)) {
        if (ADB_UNLIKELY(
                !this->getStoredValuesReaders(segmentReader, segmentIdx))) {
          LOG_TOPIC("bd02c", WARN, iresearch::TOPIC)
              << "encountered a sub-reader without stored values column "
                 "while executing a query, ignoring";
          continue;
        }
      }
      lastSegmentIdx = segmentIdx;
    }

    LocalDocumentId documentId;
    commonReadPK(pkReader, docId, documentId);

    TRI_ASSERT(segment);
    value.translate(documentId, *segment);

    if constexpr (Base::usesStoredValues) {
      auto const& columnsFieldsRegs =
          this->infos().getOutNonMaterializedViewRegs();
      TRI_ASSERT(!columnsFieldsRegs.empty());
      auto readerIndex = segmentIdx * columnsFieldsRegs.size();
      size_t valueIndex = *orderIt * columnsFieldsRegs.size();
      for (auto it = columnsFieldsRegs.begin(), end = columnsFieldsRegs.end();
           it != end; ++it) {
        if (this->_storedColumnsMask.contains(it->first)) {
          valueIndex++;
          continue;
        }
        TRI_ASSERT(readerIndex < this->_storedValuesReaders.size());
        auto const& reader = this->_storedValuesReaders[readerIndex++];
        TRI_ASSERT(reader.itr);
        TRI_ASSERT(reader.value);
        auto const& payload = reader.value->value;
        bool const found = (docId == reader.itr->seek(docId));
        if (found && !payload.empty()) {
          this->_indexReadBuffer.setStoredValue(valueIndex++, payload);
        } else {
          this->_indexReadBuffer.setStoredValue(valueIndex++, kNullSlice);
        }
      }
    }

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(this->infos().searchDocIdRegId().isValid());
      this->_indexReadBuffer.pushSearchDoc(docId, *segment);
    }

    this->_indexReadBuffer.assertSizeCoherence();
    ++orderIt;
  }
  return true;
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::fillBuffer(ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);
  size_t const atMost = ctx.outputRow.numRowsLeft();
  TRI_ASSERT(this->_indexReadBuffer.empty());
  this->_isMaterialized = false;
  this->_indexReadBuffer.reset();
  this->_indexReadBuffer.preAllocateStoredValuesBuffer(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  size_t const count = this->_reader->size();
  bool gotData{false};
  auto reset = [&] {
    ++_readerOffset;
    _currentSegmentPos = 0;
    _itr.reset();
    _doc = nullptr;
  };
  for (; _readerOffset < count;) {
    if (!_itr) {
      if (!this->_indexReadBuffer.empty()) {
        // We may not reset the iterator and continue with the next reader if
        // we still have documents in the buffer, as the cid changes with each
        // reader.
        break;
      }

      if (!resetIterator()) {
        reset();
        continue;
      }

      // segment is constant until the next resetIterator().
      // save it to don't have to look it up every time.
      if constexpr (Base::isMaterialized) {
        _segment = &this->_reader->segment(_readerOffset);
      }
      this->_isMaterialized = false;
      this->_indexReadBuffer.reset();
    }

    if constexpr (Base::isMaterialized) {
      TRI_ASSERT(_pkReader.itr);
      TRI_ASSERT(_pkReader.value);
    }
    LocalDocumentId documentId;

    // try to read a document PK from iresearch
    bool const iteratorExhausted = !readPK(documentId);

    if (iteratorExhausted) {
      // The iterator is exhausted, we need to continue with the next
      // reader.
      reset();
      if (gotData) {
        // Here we have at least one document in _indexReadBuffer, so we may not
        // add documents from a new reader.
        break;
      }
      continue;
    }
    if constexpr (Base::isMaterialized) {
      // The collection must stay the same for all documents in the buffer
      TRI_ASSERT(_segment == &this->_reader->segment(_readerOffset));
      if (!documentId.isSet()) {
        // No document read, we cannot write it.
        continue;
      }
    }
    if constexpr (Base::isLateMaterialized) {
      this->_indexReadBuffer.pushValue(this->_reader->segment(_readerOffset),
                                       _doc->value);
    } else {
      this->_indexReadBuffer.pushValue(documentId);
    }
    gotData = true;

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(this->infos().searchDocIdRegId().isValid());
      this->_indexReadBuffer.pushSearchDoc(
          _doc->value, this->_reader->segment(_readerOffset));
    }

    // in the ordered case we have to write scores as well as a document
    if constexpr (ExecutionTraits::Ordered) {
      // Writes into _scoreBuffer
      this->fillScores(*_scr);
    }

    if constexpr (Base::usesStoredValues) {
      TRI_ASSERT(_doc);
      this->pushStoredValues(*_doc);
    }
    // doc and scores are both pushed, sizes must now be coherent
    this->_indexReadBuffer.assertSizeCoherence();

    if (iteratorExhausted) {
      // The iterator is exhausted, we need to continue with the next reader.
      reset();

      // Here we have at least one document in _indexReadBuffer, so we may not
      // add documents from a new reader.
      break;
    }
    if (this->_indexReadBuffer.size() >= atMost) {
      break;
    }
  }
  return gotData;
}

template<typename ExecutionTraits>
bool IResearchViewExecutor<ExecutionTraits>::resetIterator() {
  TRI_ASSERT(this->_filter);
  TRI_ASSERT(!_itr);

  auto& segmentReader = (*this->_reader)[_readerOffset];

  if constexpr (Base::isMaterialized) {
    auto it = iresearch::pkColumn(segmentReader);

    if (ADB_UNLIKELY(!it)) {
      LOG_TOPIC("bd01b", WARN, iresearch::TOPIC)
          << "encountered a sub-reader without a primary key column while "
             "executing a query, ignoring";
      return false;
    }

    resetColumn(_pkReader, std::move(it));
  }

  if constexpr (Base::usesStoredValues) {
    if (ADB_UNLIKELY(!this->getStoredValuesReaders(segmentReader))) {
      return false;
    }
  }

  _itr = this->_filter->execute({
      .segment = segmentReader,
      .scorers = this->_scorers,
      .ctx = &this->_filterCtx,
      .wand = {},
  });
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
void IResearchViewExecutor<ExecutionTraits>::reset(
    [[maybe_unused]] bool needFullCount) {
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
  if constexpr (Base::isMaterialized) {
    saveCollection();
  }
  return toSkip - limit;
}

template<typename ExecutionTraits>
size_t IResearchViewExecutor<ExecutionTraits>::skipAll(IResearchViewStats&) {
  TRI_ASSERT(this->_indexReadBuffer.empty());
  TRI_ASSERT(this->_filter);
  size_t skipped = 0;
  auto const count = this->_reader->size();
  if (_readerOffset >= count) {
    return skipped;
  }
  if (this->infos().filterConditionIsEmpty()) {
    skipped = this->_reader->live_docs_count();
    TRI_ASSERT(_totalPos <= skipped);
    skipped -= std::min(skipped, _totalPos);
    _readerOffset = count;
    _currentSegmentPos = 0;
  } else {
    auto const approximate = this->infos().countApproximate();
    for (; _readerOffset != count; ++_readerOffset, _currentSegmentPos = 0) {
      if (!_itr && !resetIterator()) {
        continue;
      }
      skipped +=
          calculateSkipAllCount(approximate, _currentSegmentPos, _itr.get());
      _itr.reset();
      _doc = nullptr;
    }
  }
  return skipped;
}

template<typename ExecutionTraits>
void IResearchViewExecutor<ExecutionTraits>::saveCollection() {
  // We're in the middle of a reader, save the collection in case produceRows()
  // needs it.
  if (_itr) {
    // collection is constant until the next resetIterator().
    // save it to don't have to look it up every time.
    _segment = &this->_reader->segment(_readerOffset);
    this->_isMaterialized = false;
    this->_indexReadBuffer.reset();
  }
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
    : Base{fetcher, infos}, _heap_it{*infos.sort().first, infos.sort().second} {
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
    irs::doc_iterator::ptr&& pkReader, size_t index,
    irs::doc_iterator* sortReaderRef, irs::payload const* sortReaderValue,
    irs::doc_iterator::ptr&& sortReader) noexcept
    : docs(std::move(docs)),
      doc(&doc),
      score(&score),
      numScores(numScores),
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
    resetColumn(this->pkReader, std::move(pkReader));
    TRI_ASSERT(this->pkReader.itr);
    TRI_ASSERT(this->pkReader.value);
  }
}

template<typename ExecutionTraits>
IResearchViewMergeExecutor<ExecutionTraits>::MinHeapContext::MinHeapContext(
    iresearch::IResearchSortBase const& sort, size_t sortBuckets) noexcept
    : _less{sort, sortBuckets} {}

template<typename ExecutionTraits>
bool IResearchViewMergeExecutor<ExecutionTraits>::MinHeapContext::operator()(
    Value& segment) const {
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
    Value const& lhs, Value const& rhs) const {
  return _less.Compare(lhs.sortValue->value, rhs.sortValue->value) < 0;
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
      iresearch::IResearchViewNode::kSortColumnNumber ==
          columnsFieldsRegs.cbegin()->first;

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
    irs::doc_iterator::ptr pkReader;
    if constexpr (Base::isMaterialized) {
      pkReader = iresearch::pkColumn(segment);
      if (ADB_UNLIKELY(!pkReader)) {
        LOG_TOPIC("ee041", WARN, iresearch::TOPIC)
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

      _segments.emplace_back(std::move(it), *doc, *score, numScores,
                             std::move(pkReader), i, sortReader.itr.get(),
                             sortReader.value, nullptr);
    } else {
      auto itr = iresearch::sortColumn(segment);

      if (ADB_UNLIKELY(!itr)) {
        LOG_TOPIC("af4cd", WARN, iresearch::TOPIC)
            << "encountered a sub-reader without a sort column while "
               "executing a query, ignoring";
        continue;
      }

      irs::payload const* sortValue = irs::get<irs::payload>(*itr);
      if (ADB_UNLIKELY(!sortValue)) {
        sortValue = &iresearch::NoPayload;
      }

      _segments.emplace_back(std::move(it), *doc, *score, numScores,
                             std::move(pkReader), i, itr.get(), sortValue,
                             std::move(itr));
    }
  }

  _heap_it.Reset(_segments);
}

template<typename ExecutionTraits>
LocalDocumentId IResearchViewMergeExecutor<ExecutionTraits>::readPK(
    Segment& segment) {
  LocalDocumentId documentId;
  TRI_ASSERT(segment.doc);
  commonReadPK(segment.pkReader, segment.doc->value, documentId);
  return documentId;
}

template<typename ExecutionTraits>
bool IResearchViewMergeExecutor<ExecutionTraits>::fillBuffer(ReadContext& ctx) {
  TRI_ASSERT(this->_filter != nullptr);

  size_t const atMost =
      Base::isLateMaterialized ? 1 : ctx.outputRow.numRowsLeft();
  TRI_ASSERT(this->_indexReadBuffer.empty());
  this->_isMaterialized = false;
  this->_indexReadBuffer.reset();
  this->_indexReadBuffer.preAllocateStoredValuesBuffer(
      atMost, this->_infos.getScoreRegisters().size(),
      this->_infos.getOutNonMaterializedViewRegs().size());
  bool gotData = false;
  while (_heap_it.Next()) {
    auto& segment = _heap_it.Lead();
    ++segment.segmentPos;
    TRI_ASSERT(segment.docs);
    TRI_ASSERT(segment.doc);
    TRI_ASSERT(segment.score);
    // try to read a document PK from iresearch
    LocalDocumentId documentId;

    if constexpr (Base::isMaterialized) {
      documentId = readPK(segment);
      if (!documentId.isSet()) {
        continue;  // No document read, we cannot write it.
      }
    }
    // TODO(MBkkt) cache viewSegment?
    auto& viewSegment = this->_reader->segment(segment.segmentIndex);
    if constexpr (Base::isLateMaterialized) {
      this->_indexReadBuffer.pushValue(viewSegment, segment.doc->value);
    } else {
      this->_indexReadBuffer.pushValue(documentId, viewSegment);
    }
    gotData = true;
    // in the ordered case we have to write scores as well as a document
    if constexpr (ExecutionTraits::Ordered) {
      // Writes into _scoreBuffer
      this->fillScores(*segment.score);
    }

    if constexpr (Base::usesStoredValues) {
      TRI_ASSERT(segment.doc);
      this->pushStoredValues(*segment.doc, segment.segmentIndex);
    }

    if constexpr (ExecutionTraits::EmitSearchDoc) {
      TRI_ASSERT(this->infos().searchDocIdRegId().isValid() ==
                 ExecutionTraits::EmitSearchDoc);
      this->_indexReadBuffer.pushSearchDoc(segment.doc->value, viewSegment);
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

  auto const toSkip = limit;

  while (limit && _heap_it.Next()) {
    ++_heap_it.Lead().segmentPos;
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
  // 0 || 0 -- _heap_it exhausted, we don't need to skip anything
  // 0 || 1 -- _heap_it not exhausted, we need to skip tail
  // 1 || 0 -- never called _heap_it.Next(), we need to skip all
  // 1 || 1 -- impossible, asserted
  if (!_heap_it.Initilized() || _heap_it.Size() != 0) {
    TRI_ASSERT(_heap_it.Initilized() || _heap_it.Size() == 0);
    auto& infos = this->infos();
    for (auto& segment : _segments) {
      TRI_ASSERT(segment.docs);
      if (infos.filterConditionIsEmpty()) {
        TRI_ASSERT(segment.segmentIndex < this->_reader->size());
        auto const live_docs_count =
            (*this->_reader)[segment.segmentIndex].live_docs_count();
        TRI_ASSERT(segment.segmentPos <= live_docs_count);
        skipped += live_docs_count - segment.segmentPos;
      } else {
        skipped += calculateSkipAllCount(
            infos.countApproximate(), segment.segmentPos, segment.docs.get());
      }
    }
    // Adjusting by count of docs already consumed by heap but not consumed by
    // executor. This count is heap size minus 1 already consumed by executor
    // after heap.next. But we should adjust by the heap size only if the heap
    // was advanced at least once (heap is actually filled on first next) or we
    // have nothing consumed from doc iterators!
    if (infos.countApproximate() == iresearch::CountApproximate::Exact &&
        !infos.filterConditionIsEmpty() && _heap_it.Size() != 0) {
      skipped += (_heap_it.Size() - 1);
    }
  }
  _heap_it.Reset({});  // Make it uninitilized
  return skipped;
}

template<typename ExecutionTraits>
bool IResearchViewMergeExecutor<ExecutionTraits>::writeRow(ReadContext& ctx,
                                                           size_t idx) {
  auto const& value = this->_indexReadBuffer.getValue(idx);
  if constexpr (Base::isLateMaterialized) {
    return Base::writeRowImpl(ctx, idx, value);
  } else {
    return Base::writeRowImpl(ctx, idx, value.value());
  }
}

}  // namespace arangodb::aql
