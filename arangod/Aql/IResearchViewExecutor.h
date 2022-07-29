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

#include "Aql/ExecutionState.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/RegisterInfos.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchExpressionContext.h"
#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchView.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

#include <formats/formats.hpp>
#include <index/heap_iterator.hpp>

#include <utility>
#include <variant>

namespace iresearch {
struct score;
struct document;
}  // namespace iresearch

namespace arangodb {
class LogicalCollection;

namespace iresearch {
struct Scorer;
}

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
  using VarInfoMap = std::unordered_map<aql::VariableId, aql::VarInfo>;

  struct LateMaterializeRegister {
    RegisterId documentOutReg;
    RegisterId collectionOutReg;
  };

  struct MaterializeRegisters {
    RegisterId documentOutReg;
  };

  struct NoMaterializeRegisters {};

  using OutRegisters =
      std::variant<MaterializeRegisters, LateMaterializeRegister,
                   NoMaterializeRegisters>;

  IResearchViewExecutorInfos(
      iresearch::ViewSnapshotPtr reader, OutRegisters outRegister,
      std::vector<RegisterId> scoreRegisters, aql::QueryContext& query,
      std::vector<iresearch::Scorer> const& scorers,
      std::pair<iresearch::IResearchSortBase const*, size_t> sort,
      iresearch::IResearchViewStoredValues const& storedValues,
      ExecutionPlan const& plan, Variable const& outVariable,
      aql::AstNode const& filterCondition, std::pair<bool, bool> volatility,
      bool isOldMangling, VarInfoMap const& varInfoMap, int depth,
      iresearch::IResearchViewNode::ViewValuesRegisters&&
          outNonMaterializedViewRegs,
      iresearch::CountApproximate, iresearch::FilterOptimization,
      std::vector<std::pair<size_t, bool>> scorersSort,
      size_t scorersSortLimit);

  auto getDocumentRegister() const noexcept -> RegisterId;
  auto getCollectionRegister() const noexcept -> RegisterId;

  std::vector<RegisterId> const& getScoreRegisters() const noexcept;

  iresearch::IResearchViewNode::ViewValuesRegisters const&
  getOutNonMaterializedViewRegs() const noexcept;
  iresearch::ViewSnapshotPtr getReader() const noexcept;
  aql::QueryContext& getQuery() noexcept;
  std::vector<iresearch::Scorer> const& scorers() const noexcept;
  ExecutionPlan const& plan() const noexcept;
  Variable const& outVariable() const noexcept;
  aql::AstNode const& filterCondition() const noexcept;
  bool filterConditionIsEmpty() const noexcept {
    return _filterConditionIsEmpty;
  }
  VarInfoMap const& varInfoMap() const noexcept;
  int getDepth() const noexcept;
  bool volatileSort() const noexcept;
  bool volatileFilter() const noexcept;
  bool isOldMangling() const noexcept;
  iresearch::CountApproximate countApproximate() const noexcept {
    return _countApproximate;
  }
  iresearch::FilterOptimization filterOptimization() const noexcept {
    return _filterOptimization;
  }

  // first - sort
  // second - number of sort conditions to take into account
  std::pair<iresearch::IResearchSortBase const*, size_t> const& sort()
      const noexcept;

  iresearch::IResearchViewStoredValues const& storedValues() const noexcept;

  size_t scoresSortLimit() const noexcept { return _scorersSortLimit; }

  auto scoresSort() const noexcept { return std::span{_scorersSort}; }

  size_t scoreRegistersCount() const noexcept { return _scoreRegistersCount; }

 private:
  aql::RegisterId _documentOutReg;
  aql::RegisterId _collectionPointerReg;
  std::vector<RegisterId> _scoreRegisters;
  size_t _scoreRegistersCount;
  iresearch::ViewSnapshotPtr const _reader;
  aql::QueryContext& _query;
  std::vector<iresearch::Scorer> const& _scorers;
  std::pair<iresearch::IResearchSortBase const*, size_t> _sort;
  iresearch::IResearchViewStoredValues const& _storedValues;
  ExecutionPlan const& _plan;
  Variable const& _outVariable;
  aql::AstNode const& _filterCondition;
  bool const _volatileSort;
  bool const _volatileFilter;
  bool const _isOldMangling;
  VarInfoMap const& _varInfoMap;
  int const _depth;
  iresearch::IResearchViewNode::ViewValuesRegisters _outNonMaterializedViewRegs;
  iresearch::CountApproximate _countApproximate;
  bool _filterConditionIsEmpty;
  iresearch::FilterOptimization _filterOptimization;
  std::vector<std::pair<size_t, bool>> _scorersSort;
  size_t _scorersSortLimit;
};  // IResearchViewExecutorInfos

class IResearchViewStats {
 public:
  IResearchViewStats() noexcept;

  void incrScanned() noexcept;
  void incrScanned(size_t value) noexcept;

  void operator+=(IResearchViewStats const& stats) {
    _scannedIndex += stats._scannedIndex;
  }

  size_t getScanned() const noexcept;

 private:
  size_t _scannedIndex;
};  // IResearchViewStats

ExecutionStats& operator+=(
    ExecutionStats& executionStats,
    IResearchViewStats const& iResearchViewStats) noexcept;

struct ColumnIterator {
  irs::doc_iterator::ptr itr;
  const irs::payload* value{};
};  // ColumnIterator

struct FilterCtx final : irs::attribute_provider {
  explicit FilterCtx(iresearch::ViewExpressionContext& ctx) noexcept
      : _execCtx(ctx) {}

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::type<iresearch::ExpressionExecutionContext>::id() == type
               ? &_execCtx
               : nullptr;
  }

  iresearch::ExpressionExecutionContext
      _execCtx;  // expression execution context
};               // FilterCtx

template<typename ValueType, bool>
class IndexReadBuffer;

class IndexReadBufferEntry {
 public:
  size_t getKeyIdx() const noexcept { return _keyIdx; };

 private:
  template<typename ValueType, bool>
  friend class IndexReadBuffer;

  explicit inline IndexReadBufferEntry(size_t keyIdx) noexcept;

 protected:
  size_t _keyIdx;
};  // IndexReadBufferEntry

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

// Holds and encapsulates the data read from the iresearch index.
template<typename ValueType, bool copyStored>
class IndexReadBuffer {
 public:
  using KeyValueType = ValueType;

  explicit IndexReadBuffer(size_t numScoreRegisters, ResourceMonitor& monitor);

  ValueType const& getValue(IndexReadBufferEntry bufferEntry) const noexcept;

  ValueType& getValue(size_t idx) noexcept {
    TRI_ASSERT(_keyBuffer.size() > idx);
    return _keyBuffer[idx];
  }

  ScoreIterator getScores(IndexReadBufferEntry bufferEntry) noexcept;

  void setScoresSort(std::span<std::pair<size_t, bool> const> s) noexcept {
    _scoresSort = s;
  }

  irs::score_t* pushNoneScores(size_t count);

  template<typename... Args>
  void pushValue(Args&&... args);

  void pushSortedValue(ValueType&& value, float_t const* scores, size_t count);

  void finalizeHeapSort();
  // A note on the scores: instead of saving an array of AqlValues, we could
  // save an array of floats plus a bitfield noting which entries should be
  // None.

  void reset() noexcept {
    // Should only be called after everything was consumed
    TRI_ASSERT(empty());
    clear();
  }

  void clear() noexcept;

  size_t size() const noexcept;

  bool empty() const noexcept;

  IndexReadBufferEntry pop_front() noexcept;

  // This is violated while documents and scores are pushed, but must hold
  // before and after.
  void assertSizeCoherence() const noexcept;

  size_t memoryUsage(size_t maxSize) const noexcept {
    auto res =
        maxSize * sizeof(typename decltype(_keyBuffer)::value_type) +
        maxSize * sizeof(typename decltype(_scoreBuffer)::value_type) +
        maxSize * sizeof(typename decltype(_storedValuesBuffer)::value_type);
    if (!_scoresSort.empty()) {
      res += maxSize * sizeof(typename decltype(_rows)::value_type);
    }
    return res;
  }

  void preAllocateStoredValuesBuffer(size_t atMost, size_t scores,
                                     size_t stored) {
    TRI_ASSERT(_storedValuesBuffer.empty());
    if (_keyBuffer.capacity() < atMost) {
      auto newMemoryUsage = memoryUsage(atMost);
      auto tracked = _memoryTracker.tracked();
      if (newMemoryUsage != tracked) {
        if (newMemoryUsage > tracked) {
          _memoryTracker.increase(newMemoryUsage - tracked);
        } else {
          _memoryTracker.decrease(tracked - newMemoryUsage);
        }
      }
      _keyBuffer.reserve(atMost);
      _scoreBuffer.reserve(atMost * scores);
      _storedValuesBuffer.reserve(atMost * stored);
      if (!_scoresSort.empty()) {
        _rows.reserve(atMost);
      }
    }
    _maxSize = atMost;
    _heapSizeLeft = _maxSize;
    _storedValuesCount = stored;
  }

  auto getMaterializeRange(size_t skip) const {
    auto start = _rows.size() > skip ? skip : _rows.size();
    return std::vector<size_t>{_rows.begin() + start, _rows.end()};
  }

  void setStoredValue(size_t idx, irs::bytes_ref value) {
    TRI_ASSERT(idx < _storedValuesBuffer.size());
    _storedValuesBuffer[idx] = value;
  }

  void pushStoredValue(irs::bytes_ref value) {
    TRI_ASSERT(_storedValuesBuffer.size() < _storedValuesBuffer.capacity());
    _storedValuesBuffer.emplace_back(value.c_str(), value.size());
  }

  using StoredValuesContainer =
      typename std::conditional<copyStored, std::vector<irs::bstring>,
                                std::vector<irs::bytes_ref>>::type;

  StoredValuesContainer const& getStoredValues() const noexcept;

 private:
  // _keyBuffer, _scoreBuffer, _storedValuesBuffer together hold all the
  // information read from the iresearch index.
  // For the _scoreBuffer, it holds that
  //   _scoreBuffer.size() == _keyBuffer.size() * infos().getNumScoreRegisters()
  // and all entries
  //   _scoreBuffer[i]
  // correspond to
  //   _keyBuffer[i / getNumScoreRegisters()]
  // .

  std::vector<ValueType> _keyBuffer;
  std::vector<float_t> _scoreBuffer;
  StoredValuesContainer _storedValuesBuffer;

  size_t _numScoreRegisters;
  size_t _keyBaseIdx;

  std::span<std::pair<size_t, bool> const> _scoresSort;
  std::vector<size_t> _rows;
  size_t _maxSize;
  size_t _heapSizeLeft;
  size_t _storedValuesCount;
  ResourceUsageScope _memoryTracker;
};  // IndexReadBuffer

template<typename Impl>
struct IResearchViewExecutorTraits;

template<typename Impl, typename Traits = IResearchViewExecutorTraits<Impl>>
class IResearchViewExecutorBase {
 public:
  struct Properties {
    // even with "ordered = true", this block preserves the order; it just
    // writes scorer information in additional register for a following sort
    // block to use.
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = IResearchViewExecutorInfos;
  using Stats = IResearchViewStats;
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

 protected:
  template<auto type>
  using enabled_for_materialize_type_t =
      std::enable_if_t<static_cast<unsigned int>(
                           Traits::MaterializeType& type) ==
                       static_cast<unsigned int>(type)>;

  class ReadContext {
   public:
    explicit ReadContext(aql::RegisterId documentOutReg,
                         aql::RegisterId collectionPointerReg,
                         InputAqlItemRow& inputRow,
                         OutputAqlItemRow& outputRow);

    InputAqlItemRow& inputRow;
    OutputAqlItemRow& outputRow;

    template<iresearch::MaterializeType t =
                 iresearch::MaterializeType::LateMaterialize,
             typename E = enabled_for_materialize_type_t<t>>
    [[nodiscard]] auto getCollectionPointerReg() const noexcept
        -> aql::RegisterId {
      return collectionPointerReg;
    }
    template<iresearch::MaterializeType t =
                 iresearch::MaterializeType::LateMaterialize,
             typename E = enabled_for_materialize_type_t<t>>
    [[nodiscard]] auto getDocumentIdReg() const noexcept -> aql::RegisterId {
      return documentOutReg;
    }

    template<
        iresearch::MaterializeType t = iresearch::MaterializeType::Materialize,
        typename E = enabled_for_materialize_type_t<t>>
    auto getDocumentReg() const noexcept -> aql::RegisterId {
      return documentOutReg;
    }

    template<
        iresearch::MaterializeType t = iresearch::MaterializeType::Materialize,
        typename E = enabled_for_materialize_type_t<t>>
    auto getDocumentCallback() const noexcept
        -> IndexIterator::DocumentCallback const& {
      return callback;
    }

   private:
    template<
        iresearch::MaterializeType t = iresearch::MaterializeType::Materialize,
        typename E = enabled_for_materialize_type_t<t>>
    static IndexIterator::DocumentCallback copyDocumentCallback(
        ReadContext& ctx);

    aql::RegisterId documentOutReg;
    aql::RegisterId collectionPointerReg;

   public:
    IndexIterator::DocumentCallback callback;
  };  // ReadContext

 public:
  IResearchViewExecutorBase() = delete;
  IResearchViewExecutorBase(IResearchViewExecutorBase const&) = delete;

 protected:
  IResearchViewExecutorBase(IResearchViewExecutorBase&&) = default;
  IResearchViewExecutorBase(Fetcher& fetcher, Infos&);
  ~IResearchViewExecutorBase() = default;

  Infos const& infos() const noexcept;

  void fillScores(irs::score const& score);

  bool writeRow(ReadContext& ctx, IndexReadBufferEntry bufferEntry,
                LocalDocumentId const& documentId,
                LogicalCollection const& collection);

  template<iresearch::MaterializeType t =
               iresearch::MaterializeType::LateMaterialize,
           typename E = enabled_for_materialize_type_t<t>>
  bool writeLocalDocumentId(ReadContext& ctx, LocalDocumentId const& documentId,
                            LogicalCollection const& collection);

  void reset();

  bool writeStoredValue(
      ReadContext& ctx,
      typename IndexReadBufferType::StoredValuesContainer const& storedValues,
      size_t index, std::map<size_t, RegisterId> const& fieldsRegs);

  void readStoredValues(irs::document const& doc, size_t index);

  void pushStoredValues(irs::document const& doc, size_t storedValuesIndex = 0);

  bool getStoredValuesReaders(irs::sub_reader const& segmentReader,
                              size_t storedValuesIndex = 0);

 private:
  bool next(ReadContext& ctx, IResearchViewStats& stats);

 protected:
  transaction::Methods _trx;
  AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  Infos& _infos;
  InputAqlItemRow _inputRow;
  IndexReadBufferType _indexReadBuffer;
  iresearch::ViewExpressionContext _ctx;
  FilterCtx _filterCtx;  // filter context
  iresearch::ViewSnapshotPtr _reader;
  irs::filter::prepared::ptr _filter;
  irs::Order _order;
  std::vector<ColumnIterator>
      _storedValuesReaders;  // current stored values readers
  bool _isInitialized;
};  // IResearchViewExecutorBase

template<bool copyStored, bool ordered,
         iresearch::MaterializeType materializeType>
class IResearchViewExecutor
    : public IResearchViewExecutorBase<
          IResearchViewExecutor<copyStored, ordered, materializeType>> {
 public:
  using Base = IResearchViewExecutorBase<
      IResearchViewExecutor<copyStored, ordered, materializeType>>;
  using Fetcher = typename Base::Fetcher;
  using Infos = typename Base::Infos;

  IResearchViewExecutor(IResearchViewExecutor&&) = default;
  IResearchViewExecutor(Fetcher& fetcher, Infos&);

 private:
  friend Base;

  using ReadContext = typename Base::ReadContext;

  size_t skip(size_t toSkip, IResearchViewStats&);
  size_t skipAll(IResearchViewStats&);

  void saveCollection();

  void fillBuffer(ReadContext& ctx);

  bool writeRow(ReadContext& ctx, IndexReadBufferEntry bufferEntry);

  bool resetIterator();

  void reset();

 private:
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
  LogicalCollection const* _collection{};

  // case ordered only:
  irs::score const* _scr;
  size_t _numScores;
};  // IResearchViewExecutor

template<bool copyStored, bool ordered,
         iresearch::MaterializeType materializeType>
struct IResearchViewExecutorTraits<
    IResearchViewExecutor<copyStored, ordered, materializeType>> {
  using IndexBufferValueType = LocalDocumentId;
  static constexpr bool Ordered = ordered;
  static constexpr iresearch::MaterializeType MaterializeType = materializeType;
  static constexpr bool CopyStored = copyStored;
  static constexpr bool ExplicitScanned = false;
};

template<bool copyStored, bool ordered,
         iresearch::MaterializeType materializeType>
class IResearchViewMergeExecutor
    : public IResearchViewExecutorBase<
          IResearchViewMergeExecutor<copyStored, ordered, materializeType>> {
 public:
  using Base = IResearchViewExecutorBase<
      IResearchViewMergeExecutor<copyStored, ordered, materializeType>>;
  using Fetcher = typename Base::Fetcher;
  using Infos = typename Base::Infos;

  static constexpr bool Ordered = ordered;

  IResearchViewMergeExecutor(IResearchViewMergeExecutor&&) = default;
  IResearchViewMergeExecutor(Fetcher& fetcher, Infos&);

 private:
  friend Base;

  using ReadContext = typename Base::ReadContext;

  struct Segment {
    Segment(irs::doc_iterator::ptr&& docs, irs::document const& doc,
            irs::score const& score, size_t numScores,
            LogicalCollection const& collection,
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
    arangodb::LogicalCollection const*
        collection{};                   // collecton associated with a segment
    ColumnIterator pkReader;            // primary key reader
    size_t segmentIndex;                // first stored values index
    irs::doc_iterator* sortReaderRef;   // pointer to sort column reader
    irs::payload const* sortValue;      // sort column value
    irs::doc_iterator::ptr sortReader;  // sort column reader
    size_t segmentPos{0};
  };

  class MinHeapContext {
   public:
    MinHeapContext(iresearch::IResearchSortBase const& sort, size_t sortBuckets,
                   std::vector<Segment>& segments) noexcept;

    // advance
    bool operator()(size_t i) const;

    // compare
    bool operator()(size_t lhs, size_t rhs) const;

    iresearch::VPackComparer<iresearch::IResearchSortBase> _less;
    std::vector<Segment>* _segments;
  };

  // reads local document id from a specified segment
  LocalDocumentId readPK(Segment const& segment);

  void fillBuffer(ReadContext& ctx);

  bool writeRow(ReadContext& ctx, IndexReadBufferEntry bufferEntry);

  void reset();
  size_t skip(size_t toSkip, IResearchViewStats&);
  size_t skipAll(IResearchViewStats&);

 private:
  std::vector<Segment> _segments;
  irs::ExternalHeapIterator<MinHeapContext> _heap_it;
};  // IResearchViewMergeExecutor

template<bool copyStored, bool ordered,
         iresearch::MaterializeType materializeType>
struct IResearchViewExecutorTraits<
    IResearchViewMergeExecutor<copyStored, ordered, materializeType>> {
  using IndexBufferValueType =
      std::pair<LocalDocumentId, LogicalCollection const*>;
  static constexpr bool Ordered = ordered;
  static constexpr iresearch::MaterializeType MaterializeType = materializeType;
  static constexpr bool CopyStored = copyStored;
  static constexpr bool ExplicitScanned = false;
};

template<bool copyStored, bool ordered,
         iresearch::MaterializeType materializeType>
class IResearchViewHeapSortExecutor
    : public IResearchViewExecutorBase<
          IResearchViewHeapSortExecutor<copyStored, ordered, materializeType>> {
 public:
  using Base = IResearchViewExecutorBase<
      IResearchViewHeapSortExecutor<copyStored, ordered, materializeType>>;
  using Fetcher = typename Base::Fetcher;
  using Infos = typename Base::Infos;

  IResearchViewHeapSortExecutor(IResearchViewHeapSortExecutor&&) = default;
  IResearchViewHeapSortExecutor(Fetcher& fetcher, Infos&);

 private:
  friend Base;
  using ReadContext = typename Base::ReadContext;

  size_t skip(size_t toSkip, IResearchViewStats& stats);
  size_t skipAll(IResearchViewStats& stats);
  size_t getScanned() const noexcept { return _totalCount; }
  bool canSkipAll() const noexcept { return _bufferFilled && _totalCount; }

  void reset();
  void fillBuffer(ReadContext& ctx);
  bool fillBufferInternal(size_t skip);

  bool writeRow(ReadContext& ctx, IndexReadBufferEntry bufferEntry);

  size_t _totalCount{};
  size_t _scannedCount{0};
  size_t _bufferedCount{};
  bool _bufferFilled{false};
};  // ResearchViewHeapSortExecutor

union UnitedDocumentId {
  irs::doc_id_t irsId;
  typename LocalDocumentId::BaseType adbId;
};

union UnitedSourceId {
  size_t readerOffset;
  LogicalCollection const* collection;
};

struct HeapSortExecutorValue {
  HeapSortExecutorValue(irs::doc_id_t doc, size_t readerOffset) {
    documentKey.irsId = doc;
    collection.readerOffset = readerOffset;
  }

  void decode(LocalDocumentId docId, LogicalCollection const* col) noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(!decoded);
    decoded = true;
#endif

    documentKey.adbId = docId.id();
    collection.collection = col;
  }

  [[nodiscard]] irs::doc_id_t irsDocId() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(!decoded);
#endif
    return documentKey.irsId;
  }

  [[nodiscard]] size_t readerOffset() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(!decoded);
#endif
    return collection.readerOffset;
  }

  [[nodiscard]] LocalDocumentId documentId() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(decoded);
#endif
    return LocalDocumentId(documentKey.adbId);
  }

  [[nodiscard]] LogicalCollection const* collectionPtr() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(decoded);
#endif
    return collection.collection;
  }

 private:
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool decoded{false};
#endif
  UnitedDocumentId documentKey;
  UnitedSourceId collection;
};

#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
static_assert(sizeof(HeapSortExecutorValue) <= 16,
              "HeapSortExecutorValue size is not optimal");
#endif

template<bool copyStored, bool ordered,
         iresearch::MaterializeType materializeType>
struct IResearchViewExecutorTraits<
    IResearchViewHeapSortExecutor<copyStored, ordered, materializeType>> {
  using IndexBufferValueType = HeapSortExecutorValue;
  static constexpr bool Ordered = ordered;
  static constexpr iresearch::MaterializeType MaterializeType = materializeType;
  static constexpr bool CopyStored = copyStored;
  static constexpr bool ExplicitScanned = true;
};

}  // namespace aql
}  // namespace arangodb
