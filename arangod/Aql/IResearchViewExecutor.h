////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/ExecutionState.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/MultiGet.h"
#include "Aql/RegisterInfos.h"
#include "Aql/VarInfoMap.h"
#include "Containers/FlatHashSet.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchExpressionContext.h"
#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchView.h"
#include "IResearch/SearchDoc.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/IResearch/IResearchOptimizeTopK.h"
#endif

#include <formats/formats.hpp>
#include <index/heap_iterator.hpp>
#include <utils/empty.hpp>

#include <utility>
#include <variant>

namespace irs {
struct score;
}  // namespace irs

namespace arangodb {
class LogicalCollection;

namespace iresearch {
struct SearchFunc;
class SearchMeta;
}  // namespace iresearch

namespace aql {
struct AqlCall;
class AqlItemBlockInputRange;
struct ExecutionStats;
class OutputAqlItemRow;
template<BlockPassthrough>
class SingleRowFetcher;
class QueryContext;

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
      VarInfoMap const& varInfoMap, int depth,
      iresearch::IResearchViewNode::ViewValuesRegisters&&
          outNonMaterializedViewRegs,
      iresearch::CountApproximate, iresearch::FilterOptimization,
      std::vector<iresearch::HeapSortElement> const& heapSort,
      size_t heapSortLimit, iresearch::SearchMeta const* meta);

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
  iresearch::SearchMeta const* _meta;
  int const _depth;
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

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  IResearchViewStats const& viewStats) noexcept;

struct ColumnIterator {
  irs::doc_iterator::ptr itr;
  irs::payload const* value{};
};

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

// LateMater: storing LocDocIds as invalid()
// For MultiMaterializerNode-> DocIdReg = SearchDoc
// CollectionPtr May be dropped as SearchDoc contains Cid! But needs caching to
// avoid multiple access rights check! Adding buffering for
// LateMaterializeExecutor!

class ScoreIterator {
 public:
  ScoreIterator(std::span<float_t> scoreBuffer, size_t keyIdx,
                size_t numScores) noexcept;

  auto begin() noexcept;
  auto end() noexcept;

 private:
  std::span<float_t> _scoreBuffer;
  size_t _scoreBaseIdx;
  size_t _numScores;
};

union HeapSortValue {
  irs::score_t score;
  velocypack::Slice slice;
};

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

  template<typename... Args>
  void pushValue(Args&&... args) {
    _keyBuffer.emplace_back(std::forward<Args>(args)...);
  }

  void pushSearchDoc(irs::doc_id_t docId,
                     iresearch::ViewSegment const& segment) {
    _searchDocs.emplace_back(segment, docId);
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

  void setStoredValue(size_t idx, irs::bytes_view value) {
    TRI_ASSERT(idx < _storedValuesBuffer.size());
    _storedValuesBuffer[idx] = value;
  }

  void pushStoredValue(irs::bytes_view value) {
    TRI_ASSERT(_storedValuesBuffer.size() < _storedValuesBuffer.capacity());
    _storedValuesBuffer.emplace_back(value.data(), value.size());
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

template<bool copyStored, bool ordered, bool emitSearchDoc,
         iresearch::MaterializeType materializeType>
struct ExecutionTraits {
  static constexpr bool EmitSearchDoc = emitSearchDoc;
  static constexpr bool Ordered = ordered;
  static constexpr bool CopyStored = copyStored;
  static constexpr iresearch::MaterializeType MaterializeType = materializeType;
};

template<typename Impl>
struct IResearchViewExecutorTraits;

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

    void moveInto(std::unique_ptr<std::string> data) noexcept;

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

  void readStoredValues(irs::document const& doc, size_t index);

  void pushStoredValues(irs::document const& doc, size_t storedValuesIndex = 0);

  bool getStoredValuesReaders(irs::SubReader const& segmentReader,
                              size_t storedValuesIndex = 0);

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

template<typename ExecutionTraits>
class IResearchViewExecutor
    : public IResearchViewExecutorBase<IResearchViewExecutor<ExecutionTraits>,
                                       ExecutionTraits> {
 public:
  using Base = IResearchViewExecutorBase<IResearchViewExecutor<ExecutionTraits>,
                                         ExecutionTraits>;
  using Fetcher = typename Base::Fetcher;
  using Infos = typename Base::Infos;

  static constexpr bool kSorted = false;

  IResearchViewExecutor(IResearchViewExecutor&&) = default;
  IResearchViewExecutor(Fetcher& fetcher, Infos&);

 private:
  friend Base;
  using ReadContext = typename Base::ReadContext;

  size_t skip(size_t toSkip, IResearchViewStats&);
  size_t skipAll(IResearchViewStats&);

  void saveCollection();

  bool fillBuffer(ReadContext& ctx);

  bool writeRow(ReadContext& ctx, size_t idx);

  bool resetIterator();

  void reset(bool needFullCount);

  // Returns true unless the iterator is exhausted. documentId will always be
  // written. It will always be unset when readPK returns false, but may also be
  // unset if readPK returns true.
  bool readPK(LocalDocumentId& documentId);

  ColumnIterator _pkReader;  // current primary key reader
  irs::doc_iterator::ptr _itr;
  irs::document const* _doc{};
  size_t _readerOffset;
  size_t _currentSegmentPos;  // current document iterator position in segment
  size_t _totalPos;           // total position for full snapshot
  iresearch::ViewSegment const* _segment{};

  // case ordered only:
  irs::score const* _scr;
  size_t _numScores;
};

struct DocumentValue {
  explicit DocumentValue(LocalDocumentId id) noexcept : id{id} {}

  auto const& value() const noexcept { return *this; }

  void translate(size_t i) noexcept { result = i; }

  union {
    LocalDocumentId id;
    size_t result;
  };
};

template<typename ExecutionTraits>
struct IResearchViewExecutorTraits<IResearchViewExecutor<ExecutionTraits>> {
  using IndexBufferValueType =
      std::conditional_t<(ExecutionTraits::MaterializeType &
                          iresearch::MaterializeType::LateMaterialize) ==
                             iresearch::MaterializeType::LateMaterialize,
                         iresearch::SearchDoc, DocumentValue>;
  static constexpr bool ExplicitScanned = false;
};

template<typename ExecutionTraits>
class IResearchViewMergeExecutor
    : public IResearchViewExecutorBase<
          IResearchViewMergeExecutor<ExecutionTraits>, ExecutionTraits> {
 public:
  using Base =
      IResearchViewExecutorBase<IResearchViewMergeExecutor<ExecutionTraits>,
                                ExecutionTraits>;
  using Fetcher = typename Base::Fetcher;
  using Infos = typename Base::Infos;

  static constexpr bool Ordered = ExecutionTraits::Ordered;
  static constexpr bool kSorted = true;

  IResearchViewMergeExecutor(IResearchViewMergeExecutor&&) = default;
  IResearchViewMergeExecutor(Fetcher& fetcher, Infos&);

 private:
  friend Base;
  using ReadContext = typename Base::ReadContext;

  struct Segment {
    Segment(irs::doc_iterator::ptr&& docs, irs::document const& doc,
            irs::score const& score, size_t numScores,
            irs::doc_iterator::ptr&& pkReader, size_t index,
            irs::doc_iterator* sortReaderRef,
            irs::payload const* sortReaderValue,
            irs::doc_iterator::ptr&& sortReader) noexcept;
    Segment(Segment const&) = delete;
    Segment(Segment&&) = default;
    Segment& operator=(Segment const&) = delete;
    Segment& operator=(Segment&&) = delete;

    irs::doc_iterator::ptr docs;
    irs::document const* doc{};
    irs::score const* score{};
    size_t numScores{};
    ColumnIterator pkReader;
    size_t segmentIndex;  // first stored values index
    irs::doc_iterator* sortReaderRef;
    irs::payload const* sortValue;
    irs::doc_iterator::ptr sortReader;
    size_t segmentPos{0};
  };

  class MinHeapContext {
   public:
    using Value = Segment;

    MinHeapContext(iresearch::IResearchSortBase const& sort,
                   size_t sortBuckets) noexcept;

    // advance
    bool operator()(Value& segment) const;

    // compare
    bool operator()(const Value& lhs, const Value& rhs) const;

   private:
    iresearch::VPackComparer<iresearch::IResearchSortBase> _less;
  };

  // reads local document id from a specified segment
  LocalDocumentId readPK(Segment& segment);

  bool fillBuffer(ReadContext& ctx);

  bool writeRow(ReadContext& ctx, size_t idx);

  void reset(bool needFullCount);
  size_t skip(size_t toSkip, IResearchViewStats&);
  size_t skipAll(IResearchViewStats&);

  std::vector<Segment> _segments;
  irs::ExternalMergeIterator<MinHeapContext> _heap_it;
};

struct MergeSortExecutorValue {
  explicit MergeSortExecutorValue(
      LocalDocumentId id, iresearch::ViewSegment const& segment) noexcept
      : _value{id}, _segment{&segment} {}

  auto const& value() const noexcept { return _value; }
  auto const* segment() const noexcept { return _segment; }

  void translate(size_t i) noexcept { _value.result = i; }

 private:
  DocumentValue _value;
  iresearch::ViewSegment const* _segment;
};

template<typename ExecutionTraits>
struct IResearchViewExecutorTraits<
    IResearchViewMergeExecutor<ExecutionTraits>> {
  using IndexBufferValueType =
      std::conditional_t<(ExecutionTraits::MaterializeType &
                          iresearch::MaterializeType::LateMaterialize) ==
                             iresearch::MaterializeType::LateMaterialize,
                         iresearch::SearchDoc, MergeSortExecutorValue>;
  static constexpr bool ExplicitScanned = false;
};

template<typename ExecutionTraits>
class IResearchViewHeapSortExecutor
    : public IResearchViewExecutorBase<
          IResearchViewHeapSortExecutor<ExecutionTraits>, ExecutionTraits> {
 public:
  using Base =
      IResearchViewExecutorBase<IResearchViewHeapSortExecutor<ExecutionTraits>,
                                ExecutionTraits>;
  using Fetcher = typename Base::Fetcher;
  using Infos = typename Base::Infos;

  static constexpr bool kSorted = true;

  IResearchViewHeapSortExecutor(IResearchViewHeapSortExecutor&&) = default;
  IResearchViewHeapSortExecutor(Fetcher& fetcher, Infos&);

 private:
  friend Base;
  using ReadContext = typename Base::ReadContext;

  size_t skip(size_t toSkip, IResearchViewStats& stats);
  size_t skipAll(IResearchViewStats& stats);
  size_t getScanned() const noexcept { return _totalCount; }
  bool canSkipAll() const noexcept { return _bufferFilled && _totalCount; }

  void reset(bool needFullCount);
  bool fillBuffer(ReadContext& ctx);
  bool fillBufferInternal(size_t skip);

  bool writeRow(ReadContext& ctx, size_t idx);

  size_t _totalCount{};
  size_t _scannedCount{};
  size_t _bufferedCount{};
  bool _bufferFilled{};
};

struct HeapSortExecutorValue {
  explicit HeapSortExecutorValue(irs::doc_id_t docId,
                                 size_t readerOffset) noexcept
      : _irsId{docId}, _readerOffset{readerOffset} {}

  void translate(LocalDocumentId docId,
                 iresearch::ViewSegment const& segment) noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(!std::exchange(_translated, true));
#endif
    _value.id = docId;
    _segment = &segment;
  }

  void translate(size_t i) noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_translated);
#endif
    _value.result = i;
  }

  [[nodiscard]] irs::doc_id_t docId() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(!_translated);
#endif
    return _irsId;
  }

  [[nodiscard]] size_t readerOffset() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(!_translated);
#endif
    return _readerOffset;
  }

  [[nodiscard]] DocumentValue const& value() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_translated);
#endif
    return _value;
  }

  [[nodiscard]] iresearch::ViewSegment const* segment() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_translated);
#endif
    return _segment;
  }

 private:
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool _translated{false};
#endif
  union {
    irs::doc_id_t _irsId;
    DocumentValue _value;
  };
  union {
    size_t _readerOffset;
    iresearch::ViewSegment const* _segment;
  };
};

#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
static_assert(sizeof(HeapSortExecutorValue) <= 16,
              "HeapSortExecutorValue size is not optimal");
#endif

template<typename ExecutionTraits>
struct IResearchViewExecutorTraits<
    IResearchViewHeapSortExecutor<ExecutionTraits>> {
  using IndexBufferValueType = HeapSortExecutorValue;
  static constexpr bool ExplicitScanned = true;
};

}  // namespace aql
}  // namespace arangodb
