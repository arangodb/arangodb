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

#pragma once

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/MultiGet.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/VarInfoMap.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchExpressionContext.h"
#include "IResearch/IResearchFilterContext.h"
#include "IResearch/SearchDoc.h"
#include "IResearch/ViewSnapshot.h"

#include <utils/empty.hpp>

namespace irs {
struct score;
}

namespace arangodb {

namespace iresearch {
class IResearchOptimizeTopK;
struct IResearchExecutionPool;
struct SearchFunc;
}  // namespace iresearch

namespace aql {
struct AqlCall;
class AqlItemBlockInputRange;
class ExecutionPlan;
struct ExecutionStats;
class OutputAqlItemRow;
class QueryContext;

template<typename Impl>
struct IResearchViewExecutorTraits;

struct PushTag {};

struct ColumnIterator {
  irs::doc_iterator::ptr itr;
  irs::payload const* value{};
};

void resetColumn(ColumnIterator& column, irs::doc_iterator::ptr&& itr) noexcept;

void commonReadPK(ColumnIterator& it, irs::doc_id_t docId,
                  LocalDocumentId& documentId);

size_t calculateSkipAllCount(iresearch::CountApproximate approximation,
                             size_t currentPos, irs::doc_iterator* docs);

class ScoreIterator {
 public:
  ScoreIterator(std::span<float_t> scoreBuffer, size_t keyIdx,
                size_t numScores) noexcept;

  std::span<float_t>::iterator begin() noexcept;
  std::span<float_t>::iterator end() noexcept;

 private:
  std::span<float_t> _scoreBuffer;
  size_t _scoreBaseIdx;
  size_t _numScores;
};

union DocumentValue {
  irs::doc_id_t docId;
  LocalDocumentId id{};
  size_t result;
};

union HeapSortValue {
  irs::score_t score;
  velocypack::Slice slice;
};

struct ExecutorValue {
  ExecutorValue() = default;

  explicit ExecutorValue(iresearch::ViewSegment const& segment,
                         LocalDocumentId id) noexcept {
    translate(segment, id);
  }

  void translate(iresearch::ViewSegment const& segment,
                 LocalDocumentId id) noexcept {
    _value.id = id;
    _reader.segment = &segment;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_state == State::IResearch);
    _state = State::RocksDB;
#endif
  }

  void translate(size_t i) noexcept {
    _value.result = i;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_state == State::RocksDB);
    _state = State::Executor;
#endif
  }

  [[nodiscard]] iresearch::ViewSegment const* segment() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_state != State::IResearch);
#endif
    return _reader.segment;
  }

  [[nodiscard]] auto const& value() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_state != State::IResearch);
#endif
    return _value;
  }

 protected:
  DocumentValue _value;
  union {
    size_t offset;
    iresearch::ViewSegment const* segment;
  } _reader;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  enum class State {
    IResearch = 0,
    RocksDB,
    Executor,
  } _state{};
#endif
};

struct HeapSortExecutorValue : ExecutorValue {
  using ExecutorValue::ExecutorValue;

  explicit HeapSortExecutorValue(size_t offset, irs::doc_id_t docId) noexcept {
    _value.docId = docId;
    _reader.offset = offset;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _state = State::IResearch;
#endif
  }

  [[nodiscard]] size_t readerOffset() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_state == State::IResearch);
#endif
    return _reader.offset;
  }

  [[nodiscard]] irs::doc_id_t docId() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_state == State::IResearch);
#endif
    return _value.docId;
  }
};

#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
static_assert(sizeof(HeapSortExecutorValue) <= 16,
              "HeapSortExecutorValue size is not optimal");
#endif

// Holds and encapsulates the data read from the iresearch index.
template<typename ValueType, bool copyStored>
class IndexReadBuffer {
 public:
  using KeyValueType = ValueType;

  explicit IndexReadBuffer(size_t numScoreRegisters, ResourceMonitor& monitor);

  ValueType const& getValue(size_t idx) const noexcept {
    assertSizeCoherence();
    TRI_ASSERT(_keyBuffer.size() > idx);
    return _keyBuffer[idx];
  }

  ValueType& getValue(size_t idx) noexcept {
    TRI_ASSERT(_keyBuffer.size() > idx);
    return _keyBuffer[idx];
  }

  iresearch::SearchDoc const& getSearchDoc(size_t idx) noexcept {
    TRI_ASSERT(_searchDocs.size() > idx);
    return _searchDocs[idx];
  }

  ScoreIterator getScores(size_t idx) noexcept;

  void setHeapSort(std::span<iresearch::HeapSortElement const> s,
                   iresearch::IResearchViewNode::ViewValuesRegisters const&
                       storedValues) noexcept {
    TRI_ASSERT(_heapSort.empty());
    TRI_ASSERT(_heapOnlyColumnsCount == 0);
    auto const storedValuesCount = storedValues.size();
    size_t j = 0;
    for (auto const& c : s) {
      auto& sort = _heapSort.emplace_back(BufferHeapSortElement{c});
      if (c.isScore()) {
        continue;
      }
      auto it = storedValues.find(c.source);
      if (it != storedValues.end()) {
        sort.container = &_storedValuesBuffer;
        sort.multiplier = storedValuesCount;
        sort.offset =
            static_cast<size_t>(std::distance(storedValues.begin(), it));
      } else {
        sort.container = &_heapOnlyStoredValuesBuffer;
        sort.offset = j++;
        ++_heapOnlyColumnsCount;
      }
    }
    for (auto& c : _heapSort) {
      if (c.container == &_heapOnlyStoredValuesBuffer) {
        c.multiplier = _heapOnlyColumnsCount;
      }
    }
  }

  irs::score_t* pushNoneScores(size_t count);

  template<typename T, typename Id>
  void makeValue(T idx, iresearch::ViewSegment const& segment, Id id) {
    if constexpr (std::is_same_v<T, PushTag>) {
      _keyBuffer.emplace_back(segment, id);
    } else {
      _keyBuffer[idx] = ValueType{segment, id};
    }
  }

  template<typename T>
  void makeSearchDoc(T idx, iresearch::ViewSegment const& segment,
                     irs::doc_id_t docId) {
    if constexpr (std::is_same_v<T, PushTag>) {
      _searchDocs.emplace_back(segment, docId);
    } else {
      _searchDocs[idx] = iresearch::SearchDoc{segment, docId};
    }
  }

  template<typename ColumnReaderProvider>
  void pushSortedValue(ColumnReaderProvider& columnReader, ValueType&& value,
                       std::span<float_t const> scores, irs::score& score,
                       irs::score_t& threshold);

  void finalizeHeapSort();
  // A note on the scores: instead of saving an array of AqlValues, we could
  // save an array of floats plus a bitfield noting which entries should be None

  void reset() noexcept {
    // Should only be called after everything was consumed
    TRI_ASSERT(empty());
    clear();
  }

  void clear() noexcept;

  size_t size() const noexcept;

  bool empty() const noexcept;

  size_t pop_front() noexcept;

  void skip(size_t count) noexcept { _keyBaseIdx += count; }

  // This is violated while documents and scores are pushed, but must hold
  // before and after.
  void assertSizeCoherence() const noexcept;

  size_t memoryUsage(size_t maxSize, size_t scores,
                     size_t stored) const noexcept {
    auto res = maxSize * sizeof(_keyBuffer[0]) +
               maxSize * scores * sizeof(_scoreBuffer[0]) +
               maxSize * sizeof(_searchDocs[0]) +
               maxSize * stored * sizeof(_storedValuesBuffer[0]);
    if (auto sortSize = _heapSort.size(); sortSize != 0) {
      res += maxSize * sizeof(_rows[0]);
      res += maxSize * _heapOnlyColumnsCount *
             sizeof(_heapOnlyStoredValuesBuffer[0]);
      res += _heapOnlyColumnsCount * sizeof(_currentDocumentBuffer[0]);
      res += maxSize * sortSize * sizeof(_heapSortValues[0]);
    }
    return res;
  }

  void setForParallelAccess(size_t atMost, size_t scores, size_t stored) {
    _keyBuffer.resize(atMost);
    _searchDocs.resize(atMost);
    _scoreBuffer.resize(atMost * scores,
                        std::numeric_limits<float_t>::quiet_NaN());
    _storedValuesBuffer.resize(atMost * stored);
  }

  irs::score_t* getScoreBuffer(size_t idx) noexcept {
    TRI_ASSERT(idx < _scoreBuffer.size());
    return _scoreBuffer.data() + idx;
  }

  void preAllocateStoredValuesBuffer(size_t atMost, size_t scores,
                                     size_t stored) {
    TRI_ASSERT(_storedValuesBuffer.empty());
    if (_keyBuffer.capacity() < atMost) {
      auto newMemoryUsage = memoryUsage(atMost, scores, stored);
      auto tracked = _memoryTracker.tracked();
      if (newMemoryUsage > tracked) {
        _memoryTracker.increase(newMemoryUsage - tracked);
      }
      _keyBuffer.reserve(atMost);
      _searchDocs.reserve(atMost);
      _scoreBuffer.reserve(atMost * scores);
      if (!_heapSort.empty()) {
        _rows.reserve(atMost);
        _currentDocumentBuffer.reserve(_heapSort.size());
        // resize is important here as we want
        // indexed access for setting values
        _storedValuesBuffer.resize(atMost * stored);
        _heapOnlyStoredValuesBuffer.resize(_heapOnlyColumnsCount * atMost);
        if (!scores) {
          // save ourselves 1 "if" during pushing values
          _scoreBuffer.push_back(irs::score_t{});
        }
      } else {
        _storedValuesBuffer.reserve(atMost * stored);
      }
    }
    _maxSize = atMost;
    _heapSizeLeft = _maxSize;
    _storedValuesCount = stored;
  }

  std::vector<size_t> getMaterializeRange(size_t skip) const;

  template<typename T>
  void makeStoredValue(T idx, irs::bytes_view value) {
    if constexpr (std::is_same_v<T, PushTag>) {
      TRI_ASSERT(_storedValuesBuffer.size() < _storedValuesBuffer.capacity());
      _storedValuesBuffer.emplace_back(value.data(), value.size());
    } else {
      TRI_ASSERT(idx < _storedValuesBuffer.size());
      _storedValuesBuffer[idx] = value;
    }
  }

  using StoredValue =
      std::conditional_t<copyStored, irs::bstring, irs::bytes_view>;

  using StoredValues = std::vector<StoredValue>;

  StoredValues const& getStoredValues() const noexcept {
    return _storedValuesBuffer;
  }

  struct BufferHeapSortElement : iresearch::HeapSortElement {
    StoredValues* container{};
    size_t offset{};
    size_t multiplier{};
  };

 private:
  // _keyBuffer, _scoreBuffer, _storedValuesBuffer together hold all the
  // information read from the iresearch index.
  // For the _scoreBuffer, it holds that
  //   _scoreBuffer.size() == _keyBuffer.size() * infos().getNumScoreRegisters()
  // and all entries
  //   _scoreBuffer[i]
  // correspond to
  //   _keyBuffer[i / getNumScoreRegisters()]

  template<typename ColumnReaderProvider>
  velocypack::Slice readHeapSortColumn(iresearch::HeapSortElement const& cmp,
                                       irs::doc_id_t doc,
                                       ColumnReaderProvider& readerProvider,
                                       size_t readerSlot);
  template<bool fullHeap, typename ColumnReaderProvider>
  void finalizeHeapSortDocument(size_t idx, irs::doc_id_t doc,
                                std::span<float_t const> scores,
                                ColumnReaderProvider& readerProvider);

  std::vector<ValueType> _keyBuffer;
  // FIXME(gnusi): compile time
  std::vector<iresearch::SearchDoc> _searchDocs;
  std::vector<irs::score_t> _scoreBuffer;
  StoredValues _storedValuesBuffer;
  // Heap Sort facilities
  // atMost * <num heap only values>
  StoredValues _heapOnlyStoredValuesBuffer;
  // <num heap only values>
  std::vector<ColumnIterator> _heapOnlyStoredValuesReaders;
  // <num heap only values>
  StoredValues _currentDocumentBuffer;
  IRS_NO_UNIQUE_ADDRESS
  irs::utils::Need<!copyStored, std::vector<velocypack::Slice>>
      _currentDocumentSlices;
  std::vector<HeapSortValue> _heapSortValues;
  std::vector<BufferHeapSortElement> _heapSort;
  size_t _heapOnlyColumnsCount{0};
  size_t _currentReaderOffset{std::numeric_limits<size_t>::max()};

  size_t _numScoreRegisters;
  size_t _keyBaseIdx;
  std::vector<size_t> _rows;
  size_t _maxSize;
  size_t _heapSizeLeft;
  size_t _storedValuesCount;
  ResourceUsageScope _memoryTracker;
};

class IResearchViewExecutorInfos {
 public:
  IResearchViewExecutorInfos(
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
      uint32_t immutableParts, VarInfoMap const& varInfoMap, int depth,
      iresearch::IResearchViewNode::ViewValuesRegisters&&
          outNonMaterializedViewRegs,
      iresearch::CountApproximate, iresearch::FilterOptimization,
      std::vector<iresearch::HeapSortElement> const& heapSort,
      size_t heapSortLimit, iresearch::SearchMeta const* meta,
      size_t parallelism,
      iresearch::IResearchExecutionPool& parallelExecutionPool);

  auto getDocumentRegister() const noexcept { return _documentOutReg; }

  auto searchDocIdRegId() const noexcept { return _searchDocOutReg; }

  auto const& getScoreRegisters() const noexcept { return _scoreRegisters; }

  auto const& getOutNonMaterializedViewRegs() const noexcept {
    return _outNonMaterializedViewRegs;
  }

  auto getReader() const noexcept { return _reader; }

  auto& getQuery() noexcept { return _query; }

  auto const& scorers() const noexcept { return _scorers; }

  auto const& plan() const noexcept { return _plan; }

  auto const& outVariable() const noexcept { return _outVariable; }

  auto const& filterCondition() const noexcept { return _filterCondition; }

  bool filterConditionIsEmpty() const noexcept {
    return _filterConditionIsEmpty;
  }

  auto const& varInfoMap() const noexcept { return _varInfoMap; }

  int getDepth() const noexcept { return _depth; }

  uint32_t immutableParts() const noexcept { return _immutableParts; }

  bool volatileSort() const noexcept { return _volatileSort; }

  bool volatileFilter() const noexcept { return _volatileFilter; }

  bool isOldMangling() const noexcept { return _meta == nullptr; }

  auto countApproximate() const noexcept { return _countApproximate; }

  auto filterOptimization() const noexcept { return _filterOptimization; }

#ifdef USE_ENTERPRISE
  auto const& optimizeTopK() const noexcept { return _optimizeTopK; }
#endif

  auto const& sort() const noexcept { return _sort; }

  auto const& storedValues() const noexcept { return _storedValues; }

  auto heapSortLimit() const noexcept { return _heapSortLimit; }

  auto heapSort() const noexcept { return std::span{_heapSort}; }

  auto scoreRegistersCount() const noexcept { return _scoreRegistersCount; }

  auto const* meta() const noexcept { return _meta; }

  auto parallelism() const noexcept { return _parallelism; }

  auto& parallelExecutionPool() const noexcept {
    return _parallelExecutionPool;
  }

 private:
  RegisterId _searchDocOutReg;
  RegisterId _documentOutReg;
  std::vector<RegisterId> _scoreRegisters;
  size_t _scoreRegistersCount;
  iresearch::ViewSnapshotPtr const _reader;
  QueryContext& _query;
#ifdef USE_ENTERPRISE
  iresearch::IResearchOptimizeTopK const& _optimizeTopK;
#endif
  std::vector<iresearch::SearchFunc> const& _scorers;
  std::pair<iresearch::IResearchSortBase const*, size_t> _sort;
  iresearch::IResearchViewStoredValues const& _storedValues;
  ExecutionPlan const& _plan;
  Variable const& _outVariable;
  AstNode const& _filterCondition;
  VarInfoMap const& _varInfoMap;
  iresearch::IResearchViewNode::ViewValuesRegisters _outNonMaterializedViewRegs;
  iresearch::CountApproximate _countApproximate;
  iresearch::FilterOptimization _filterOptimization;
  std::vector<iresearch::HeapSortElement> const& _heapSort;
  size_t _heapSortLimit;
  size_t _parallelism;
  iresearch::SearchMeta const* _meta;
  iresearch::IResearchExecutionPool& _parallelExecutionPool;
  int const _depth;
  uint32_t _immutableParts;
  bool _filterConditionIsEmpty;
  bool const _volatileSort;
  bool const _volatileFilter;
};

class IResearchViewStats {
 public:
  void incrScanned() noexcept { ++_scannedIndex; }
  void incrScanned(size_t value) noexcept { _scannedIndex += value; }
  void operator+=(IResearchViewStats const& stats) {
    _scannedIndex += stats._scannedIndex;
  }
  size_t getScanned() const noexcept { return _scannedIndex; }

 private:
  size_t _scannedIndex{};
};

ExecutionStats& operator+=(ExecutionStats& executionStats,
                           IResearchViewStats const& viewStats) noexcept;

// LateMater: storing LocDocIds as invalid()
// For MultiMaterializerNode-> DocIdReg = SearchDoc
// CollectionPtr May be dropped as SearchDoc contains Cid! But needs caching to
// avoid multiple access rights check! Adding buffering for
// LateMaterializeExecutor!

struct FilterCtx final : irs::attribute_provider {
  explicit FilterCtx(iresearch::ViewExpressionContext& ctx) noexcept
      : _execCtx(ctx) {}

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::type<iresearch::ExpressionExecutionContext>::id() == type
               ? &_execCtx
               : nullptr;
  }

  iresearch::ExpressionExecutionContext _execCtx;
};

inline constexpr irs::bytes_view kNullSlice{VPackSlice::nullSliceData, 1};

template<bool copyStored, bool ordered, bool emitSearchDoc,
         iresearch::MaterializeType materializeType>
struct ExecutionTraits {
  static constexpr bool EmitSearchDoc = emitSearchDoc;
  static constexpr bool Ordered = ordered;
  static constexpr bool CopyStored = copyStored;
  static constexpr iresearch::MaterializeType MaterializeType = materializeType;
};

template<typename Impl, typename ExecutionTraits>
class IResearchViewExecutorBase {
 public:
  struct Traits : ExecutionTraits, IResearchViewExecutorTraits<Impl> {};

  struct Properties {
    // even with "ordered = true", this block preserves the order; it just
    // writes scorer information in additional register for a following sort
    // block to use.
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = IResearchViewExecutorInfos;
  using Stats = IResearchViewStats;

  static consteval bool contains(auto type) noexcept {
    return static_cast<uint32_t>(Traits::MaterializeType & type) ==
           static_cast<uint32_t>(type);
  }

  static constexpr bool isLateMaterialized =
      contains(iresearch::MaterializeType::LateMaterialize);

  static constexpr bool isMaterialized =
      contains(iresearch::MaterializeType::Materialize);

  static constexpr bool usesStoredValues =
      contains(iresearch::MaterializeType::UseStoredValues);

  using IndexReadBufferType =
      IndexReadBuffer<typename Traits::IndexBufferValueType,
                      Traits::CopyStored>;
  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& input, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

  void initializeCursor();

  IResearchViewExecutorBase() = delete;
  IResearchViewExecutorBase(IResearchViewExecutorBase const&) = delete;

 protected:
  class ReadContext {
   public:
    explicit ReadContext(RegisterId documentOutReg, InputAqlItemRow& inputRow,
                         OutputAqlItemRow& outputRow);

    InputAqlItemRow& inputRow;
    OutputAqlItemRow& outputRow;

    auto getDocumentIdReg() const noexcept {
      static_assert(isLateMaterialized);
      return documentOutReg;
    }

    void moveInto(aql::DocumentData data) noexcept;

   private:
    RegisterId documentOutReg;
  };

  IResearchViewExecutorBase(IResearchViewExecutorBase&&) = default;
  IResearchViewExecutorBase(Fetcher& fetcher, Infos&);
  ~IResearchViewExecutorBase() = default;

  Infos const& infos() const noexcept { return _infos; }

  void fillScores(irs::score const& score) {
    TRI_ASSERT(Traits::Ordered);

    // Scorer registers are placed right before document output register.
    // Allocate block for scores (registerId's are sequential) and fill it.
    score(_indexReadBuffer.pushNoneScores(infos().scoreRegistersCount()));
  }

  template<typename Value>
  bool writeRowImpl(ReadContext& ctx, size_t idx, Value const& value);

  void writeSearchDoc(ReadContext& ctx, iresearch::SearchDoc const& doc,
                      RegisterId reg);

  void reset();

  bool writeStoredValue(
      ReadContext& ctx,
      typename IndexReadBufferType::StoredValues const& storedValues,
      size_t index, FieldRegisters const& fieldsRegs);
  bool writeStoredValue(ReadContext& ctx, irs::bytes_view storedValue,
                        FieldRegisters const& fieldsRegs);

  template<typename T>
  void makeStoredValues(T idx, irs::doc_id_t docId, size_t readerIndex);

  bool getStoredValuesReaders(irs::SubReader const& segmentReader,
                              size_t readerIndex);

  transaction::Methods _trx;
  iresearch::MonitorManager _memory;
  AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  Infos& _infos;
  InputAqlItemRow _inputRow;
  IndexReadBufferType _indexReadBuffer;
  IRS_NO_UNIQUE_ADDRESS
  irs::utils::Need<isMaterialized, MultiGetContext> _context;
  iresearch::ViewExpressionContext _ctx;
  FilterCtx _filterCtx;
  iresearch::ViewSnapshotPtr _reader;
  irs::proxy_filter::cache_ptr _cache;
  irs::filter::prepared::ptr _filter;
  irs::filter::prepared const** _filterCookie{};
  containers::SmallVector<irs::Scorer::ptr, 2> _scorersContainer;
  irs::Scorers _scorers;
  irs::WandContext _wand;
  std::vector<ColumnIterator> _storedValuesReaders;
  containers::FlatHashSet<ptrdiff_t> _storedColumnsMask;
  std::array<char, iresearch::kSearchDocBufSize> _buf;
  bool _isInitialized{};
  bool _isMaterialized{};
  // new mangling only:
  iresearch::AnalyzerProvider _provider;

 private:
  void materialize();
  bool next(ReadContext& ctx, IResearchViewStats& stats);
};

}  // namespace aql
}  // namespace arangodb
