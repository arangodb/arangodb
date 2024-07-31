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
/// @author Tobias Gödderz
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewExecutorBase.h"

#include "Aql/ExecutionPlan.h"
#include "Aql/QueryContext.h"
#include "IResearch/Search.h"
#include "IResearch/IResearchFilterFactoryCommon.h"
#include "IResearch/IResearchReadUtils.h"

namespace arangodb::aql {

inline PushTag& operator*=(PushTag& self, size_t) { return self; }
inline PushTag operator++(PushTag&, int) { return {}; }

inline constexpr int kSortMultiplier[]{-1, 1};

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
  if constexpr (std::is_same_v<ValueType, HeapSortExecutorValue>) {
    std::copy(_rows.begin() + skip, _rows.end(), rows.begin());
  } else {
    std::iota(rows.begin(), rows.end(), skip);
  }
  return rows;
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
    aql::DocumentData data) noexcept {
  static_assert(isMaterialized);
  outputRow.moveValueInto(documentOutReg, inputRow, &data);
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
  // TODO: This could be always true if we enable
  // always filling all buffer for simple executor with parallelism = 1
  bool const haveMultipleSnapshots =
      this->_infos.parallelism() > 1 || Impl::kSorted;
  if (haveMultipleSnapshots) {
    std::sort(it, end, [&](size_t lhs, size_t rhs) {
      auto const& lhs_val = _indexReadBuffer.getValue(lhs);
      auto const& rhs_val = _indexReadBuffer.getValue(rhs);
      auto const* lhs_segment = lhs_val.segment();
      auto const* rhs_segment = rhs_val.segment();
      return lhs_segment->snapshot < rhs_segment->snapshot;
    });
  } else {
    TRI_ASSERT(!pkReadingOrder.empty());
    segment = _indexReadBuffer.getValue(pkReadingOrder.front()).segment();
    _context.snapshot = segment->snapshot;
  }
  StorageSnapshot const* lastSnapshot{};
  auto fillKeys = [&] {
    if (it == end) {
      return MultiGetState::kLast;
    }
    auto& doc = _indexReadBuffer.getValue(*it);
    if (haveMultipleSnapshots) {
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
  if (_isInitialized && !infos().volatileFilter()) {
    return;
  }

  auto immutableParts = infos().immutableParts();
  TRI_ASSERT(immutableParts == 0 || !infos().volatileSort());

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
  auto* cond = &infos().filterCondition();
  Result r;
  irs::And mutableAnd;
  irs::Or mutableOr;
  irs::boolean_filter* root = &mutableOr;
  if (immutableParts == 0) {
    r = iresearch::FilterFactory::filter(root, filterCtx, *cond);
  } else {
    if (immutableParts != std::numeric_limits<uint32_t>::max()) {
      while (cond->numMembers() == 1) {
        cond = cond->getMemberUnchecked(0);
      }
      if (cond->type == NODE_TYPE_OPERATOR_NARY_AND) {
        root = &mutableAnd;
      }
    }
    size_t i = 0;
    auto& proxy = append<irs::proxy_filter>(*root, filterCtx);
    if (_cache) {
      proxy.set_cache(_cache);
      i = immutableParts;
    } else {
      // TODO(MBkkt) simplify via additional template parameter for set_filter
      irs::boolean_filter* immutableRoot{};
      if (root == &mutableOr) {
        auto c = proxy.set_filter<irs::Or>(_memory);
        immutableRoot = &c.first;
        _cache = std::move(c.second);
      } else {
        auto c = proxy.set_filter<irs::And>(_memory);
        immutableRoot = &c.first;
        _cache = std::move(c.second);
      }
      if (immutableParts == std::numeric_limits<uint32_t>::max()) {
        r = iresearch::FilterFactory::filter(immutableRoot, filterCtx, *cond);
        i = immutableParts;
      } else {
        for (; i != immutableParts && r.ok(); ++i) {
          auto& member = iresearch::append<irs::Or>(*immutableRoot, filterCtx);
          r = iresearch::FilterFactory::filter(&member, filterCtx,
                                               *cond->getMemberUnchecked(i));
        }
      }
    }
    for (auto members = cond->numMembers(); i < members && r.ok(); ++i) {
      auto& member = iresearch::append<irs::Or>(*root, filterCtx);
      r = iresearch::FilterFactory::filter(&member, filterCtx,
                                           *cond->getMemberUnchecked(i));
    }
  }
  if (!r.ok()) {
    velocypack::Builder builder;
    cond->toVelocyPack(builder, true);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        r.errorNumber(),
        absl::StrCat("failed to build filter while querying arangosearch "
                     "view, query '",
                     builder.toJson(), "': ", r.errorMessage()));
  }

  if (!_isInitialized || infos().volatileSort()) {
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
  _filter = root->prepare({
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

template<typename Impl, typename ExecutionTraits>
void IResearchViewExecutorBase<Impl, ExecutionTraits>::writeSearchDoc(
    ReadContext& ctx, iresearch::SearchDoc const& doc, RegisterId reg) {
  TRI_ASSERT(doc.isValid());
  AqlValue value{doc.encode(_buf)};
  AqlValueGuard guard{value, true};
  ctx.outputRow.moveValueInto(reg, ctx.inputRow, &guard);
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
  // TODO(MBkkt) optimize case bstring + single field
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
    auto& r = _context.values[value.value().result];
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
  static constexpr bool kHeapSort =
      std::is_same_v<HeapSortExecutorValue,
                     typename Traits::IndexBufferValueType>;
  if (!columnsFieldsRegs.empty()) {
    auto columnFieldsRegs = columnsFieldsRegs.cbegin();
    readerIndex *= columnsFieldsRegs.size();
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
        resetColumn(_storedValuesReaders[readerIndex++], std::move(sortReader));
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
        resetColumn(_storedValuesReaders[readerIndex++],
                    storedValuesReader->iterator(irs::ColumnHint::kNormal));
      }
    }
  }
  return true;
}

}  // namespace arangodb::aql
