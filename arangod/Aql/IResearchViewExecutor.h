////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H
#define ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H

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
class score;
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
template <BlockPassthrough>
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
      std::variant<MaterializeRegisters, LateMaterializeRegister, NoMaterializeRegisters>;

  IResearchViewExecutorInfos(
      std::shared_ptr<iresearch::IResearchView::Snapshot const> reader,
      OutRegisters outRegister, std::vector<RegisterId> scoreRegisters,
      aql::QueryContext& query, std::vector<iresearch::Scorer> const& scorers,
      std::pair<iresearch::IResearchViewSort const*, size_t> sort,
      iresearch::IResearchViewStoredValues const& storedValues, ExecutionPlan const& plan,
      Variable const& outVariable, aql::AstNode const& filterCondition,
      std::pair<bool, bool> volatility, VarInfoMap const& varInfoMap, int depth,
      iresearch::IResearchViewNode::ViewValuesRegisters&& outNonMaterializedViewRegs);

  auto getDocumentRegister() const noexcept -> RegisterId;
  auto getCollectionRegister() const noexcept -> RegisterId;

  std::vector<RegisterId> const& getScoreRegisters() const noexcept;

  iresearch::IResearchViewNode::ViewValuesRegisters const& getOutNonMaterializedViewRegs() const
      noexcept;
  std::shared_ptr<iresearch::IResearchView::Snapshot const> getReader() const noexcept;
  aql::QueryContext& getQuery() noexcept;
  std::vector<iresearch::Scorer> const& scorers() const noexcept;
  ExecutionPlan const& plan() const noexcept;
  Variable const& outVariable() const noexcept;
  aql::AstNode const& filterCondition() const noexcept;
  VarInfoMap const& varInfoMap() const noexcept;
  int getDepth() const noexcept;
  bool volatileSort() const noexcept;
  bool volatileFilter() const noexcept;

  // first - sort
  // second - number of sort conditions to take into account
  std::pair<iresearch::IResearchViewSort const*, size_t> const& sort() const noexcept;

  iresearch::IResearchViewStoredValues const& storedValues() const noexcept;

 private:
  aql::RegisterId _documentOutReg;
  aql::RegisterId _collectionPointerReg;
  std::vector<RegisterId> _scoreRegisters;
  std::shared_ptr<iresearch::IResearchView::Snapshot const> const _reader;
  aql::QueryContext& _query;
  std::vector<iresearch::Scorer> const& _scorers;
  std::pair<iresearch::IResearchViewSort const*, size_t> _sort;
  iresearch::IResearchViewStoredValues const& _storedValues;
  ExecutionPlan const& _plan;
  Variable const& _outVariable;
  aql::AstNode const& _filterCondition;
  bool const _volatileSort;
  bool const _volatileFilter;
  VarInfoMap const& _varInfoMap;
  int const _depth;
  iresearch::IResearchViewNode::ViewValuesRegisters _outNonMaterializedViewRegs;
};  // IResearchViewExecutorInfos

class IResearchViewStats {
 public:
  IResearchViewStats() noexcept;

  void incrScanned() noexcept;
  void incrScanned(size_t value) noexcept;

  void operator+= (IResearchViewStats const& stats) {
    _scannedIndex += stats._scannedIndex;
  }
  
  size_t getScanned() const noexcept;

 private:
  size_t _scannedIndex;
};  // IResearchViewStats

ExecutionStats& operator+=(ExecutionStats& executionStats,
                           IResearchViewStats const& iResearchViewStats) noexcept;

struct ColumnIterator {
  irs::doc_iterator::ptr itr;
  const irs::payload* value{};
}; // ColumnIterator

struct FilterCtx final : irs::attribute_provider {
  explicit FilterCtx(iresearch::ViewExpressionContext& ctx) noexcept
    : _execCtx(ctx) {
  }

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::type<iresearch::ExpressionExecutionContext>::id() == type
      ? &_execCtx
      : nullptr;
  }

  iresearch::ExpressionExecutionContext _execCtx;  // expression execution context
}; // FilterCtx

template <typename Impl>
struct IResearchViewExecutorTraits;

template <typename Impl, typename Traits = IResearchViewExecutorTraits<Impl>>
class IResearchViewExecutorBase {
 public:
  struct Properties {
    // even with "ordered = true", this block preserves the order; it just
    // writes scorer information in additional register for a following sort
    // block to use.
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = IResearchViewExecutorInfos;
  using Stats = IResearchViewStats;

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& input, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

 protected:
  template <auto type>
  using enabled_for_materialize_type_t =
      std::enable_if_t<static_cast<unsigned int>(Traits::MaterializeType& type) == static_cast<unsigned int>(type)>;

  class ReadContext {
   public:
    explicit ReadContext(aql::RegisterId documentOutReg, aql::RegisterId collectionPointerReg,
                         InputAqlItemRow& inputRow, OutputAqlItemRow& outputRow);

    InputAqlItemRow& inputRow;
    OutputAqlItemRow& outputRow;

    template <iresearch::MaterializeType t = iresearch::MaterializeType::LateMaterialize,
              typename E = enabled_for_materialize_type_t<t>>
    [[nodiscard]] auto getCollectionPointerReg() const noexcept -> aql::RegisterId {
      return collectionPointerReg;
    }
    template <iresearch::MaterializeType t = iresearch::MaterializeType::LateMaterialize,
              typename E = enabled_for_materialize_type_t<t>>
    [[nodiscard]] auto getDocumentIdReg() const noexcept -> aql::RegisterId {
      return documentOutReg;
    }

    template <iresearch::MaterializeType t = iresearch::MaterializeType::Materialize,
              typename E = enabled_for_materialize_type_t<t>>
    auto getDocumentReg() const noexcept -> aql::RegisterId {
      return documentOutReg;
    }
    template <iresearch::MaterializeType t = iresearch::MaterializeType::Materialize,
              typename E = enabled_for_materialize_type_t<t>>
    auto getDocumentCallback() const noexcept -> IndexIterator::DocumentCallback const& {
      return callback;
    }

   private:
    template <iresearch::MaterializeType t = iresearch::MaterializeType::Materialize,
              typename E = enabled_for_materialize_type_t<t>>
    static IndexIterator::DocumentCallback copyDocumentCallback(ReadContext& ctx);

    aql::RegisterId documentOutReg;
    aql::RegisterId collectionPointerReg;

   public:
    IndexIterator::DocumentCallback callback;
  };  // ReadContext

  template <typename ValueType>
  class IndexReadBuffer;

  class IndexReadBufferEntry {
   public:
    IndexReadBufferEntry() = delete;

    size_t getKeyIdx() const noexcept { return _keyIdx; };

   private:
    template <typename ValueType>
    friend class IndexReadBuffer;

    explicit inline IndexReadBufferEntry(size_t keyIdx) noexcept;

   protected:
    size_t _keyIdx;
  };

  // Holds and encapsulates the data read from the iresearch index.
  template <typename ValueType>
  class IndexReadBuffer {
   public:
    class ScoreIterator {
     public:
      ScoreIterator() = delete;
      ScoreIterator(std::vector<AqlValue>& scoreBuffer, size_t keyIdx, size_t numScores) noexcept;

      std::vector<AqlValue>::iterator begin() noexcept;

      std::vector<AqlValue>::iterator end() noexcept;

     private:
      std::vector<AqlValue>& _scoreBuffer;
      size_t _scoreBaseIdx;
      size_t _numScores;
    };

   public:
    IndexReadBuffer() = delete;
    explicit IndexReadBuffer(size_t numScoreRegisters);

    ValueType const& getValue(IndexReadBufferEntry bufferEntry) const noexcept;

    ScoreIterator getScores(IndexReadBufferEntry bufferEntry) noexcept;

    template <typename... Args>
    void pushValue(Args&&... args);

    // A note on the scores: instead of saving an array of AqlValues, we could
    // save an array of floats plus a bitfield noting which entries should be
    // None.

    void pushScore(float_t scoreValue);

    void pushScoreNone();

    void reset() noexcept;

    size_t size() const noexcept;

    bool empty() const noexcept;

    IndexReadBufferEntry pop_front() noexcept;

    // This is violated while documents and scores are pushed, but must hold
    // before and after.
    void assertSizeCoherence() const noexcept;

    std::vector<irs::bytes_ref>& getStoredValues() noexcept;

    std::vector<irs::bytes_ref> const& getStoredValues() const noexcept;

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
    std::vector<AqlValue> _scoreBuffer;
    std::vector<irs::bytes_ref> _storedValuesBuffer;
    size_t _numScoreRegisters;
    size_t _keyBaseIdx;
  };

 public:
  IResearchViewExecutorBase() = delete;
  IResearchViewExecutorBase(IResearchViewExecutorBase const&) = delete;

 protected:
  IResearchViewExecutorBase(IResearchViewExecutorBase&&) = default;
  IResearchViewExecutorBase(Fetcher& fetcher, Infos&);
  ~IResearchViewExecutorBase() = default;

  Infos const& infos() const noexcept;

  void fillScores(ReadContext const& ctx, float_t const* begin, float_t const* end);

  bool writeRow(ReadContext& ctx, IndexReadBufferEntry bufferEntry,
                LocalDocumentId const& documentId, LogicalCollection const& collection);

  template <iresearch::MaterializeType t = iresearch::MaterializeType::LateMaterialize,
            typename E = enabled_for_materialize_type_t<t>>
  bool writeLocalDocumentId(ReadContext& ctx, LocalDocumentId const& documentId,
                            LogicalCollection const& collection);

  void reset();

  bool writeStoredValue(ReadContext& ctx, std::vector<irs::bytes_ref> const& storedValues,
                        size_t index, std::map<size_t, RegisterId> const& fieldsRegs);

  void readStoredValues(irs::document const& doc, size_t index);

  void pushStoredValues(irs::document const& doc, size_t storedValuesIndex = 0);

  bool getStoredValuesReaders(irs::sub_reader const& segmentReader,
                              size_t storedValuesIndex = 0);

 private:
  bool next(ReadContext& ctx);

 protected:
  transaction::Methods _trx;
  AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  Infos& _infos;
  InputAqlItemRow _inputRow;
  IndexReadBuffer<typename Traits::IndexBufferValueType> _indexReadBuffer;
  iresearch::ViewExpressionContext _ctx;
  FilterCtx _filterCtx;  // filter context
  std::shared_ptr<iresearch::IResearchView::Snapshot const> _reader;
  irs::filter::prepared::ptr _filter;
  irs::order::prepared _order;
  std::vector<ColumnIterator> _storedValuesReaders;  // current stored values readers
  bool _isInitialized;
};  // IResearchViewExecutorBase

template <bool ordered, iresearch::MaterializeType materializeType>
class IResearchViewExecutor
    : public IResearchViewExecutorBase<IResearchViewExecutor<ordered, materializeType>> {
 public:
  using Base = IResearchViewExecutorBase<IResearchViewExecutor<ordered, materializeType>>;
  using Fetcher = typename Base::Fetcher;
  using Infos = typename Base::Infos;

  IResearchViewExecutor(IResearchViewExecutor&&) = default;
  IResearchViewExecutor(Fetcher& fetcher, Infos&);

 private:
  friend Base;

  using ReadContext = typename Base::ReadContext;
  using IndexReadBufferEntry = typename Base::IndexReadBufferEntry;

  size_t skip(size_t toSkip);
  size_t skipAll();

  void saveCollection();

  void evaluateScores(ReadContext const& ctx);

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
  LogicalCollection const* _collection{};

  // case ordered only:
  irs::score const* _scr;
  size_t _numScores;
};  // IResearchViewExecutor

template <bool ordered, iresearch::MaterializeType materializeType>
struct IResearchViewExecutorTraits<IResearchViewExecutor<ordered, materializeType>> {
  using IndexBufferValueType = LocalDocumentId;
  static constexpr bool Ordered = ordered;
  static constexpr iresearch::MaterializeType MaterializeType = materializeType;
};

template <bool ordered, iresearch::MaterializeType materializeType>
class IResearchViewMergeExecutor
    : public IResearchViewExecutorBase<IResearchViewMergeExecutor<ordered, materializeType>> {
 public:
  using Base = IResearchViewExecutorBase<IResearchViewMergeExecutor<ordered, materializeType>>;
  using Fetcher = typename Base::Fetcher;
  using Infos = typename Base::Infos;

  static constexpr bool Ordered = ordered;

  IResearchViewMergeExecutor(IResearchViewMergeExecutor&&) = default;
  IResearchViewMergeExecutor(Fetcher& fetcher, Infos&);

 private:
  friend Base;

  using ReadContext = typename Base::ReadContext;
  using IndexReadBufferEntry = typename Base::IndexReadBufferEntry;

  struct Segment {
    Segment(irs::doc_iterator::ptr&& docs, irs::document const& doc,
            irs::score const& score, size_t numScores,
            LogicalCollection const& collection,
            irs::doc_iterator::ptr&& pkReader,
            size_t storedValuesIndex,
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
    arangodb::LogicalCollection const* collection{};  // collecton associated with a segment
    ColumnIterator pkReader;    // primary key reader
    size_t storedValuesIndex;  // first stored values index
    irs::doc_iterator* sortReaderRef; // pointer to sort column reader
    irs::payload const* sortValue;    // sort column value
    irs::doc_iterator::ptr sortReader; // sort column reader
  };

  class MinHeapContext {
   public:
    MinHeapContext(iresearch::IResearchViewSort const& sort, size_t sortBuckets,
                   std::vector<Segment>& segments) noexcept;

    // advance
    bool operator()(size_t i) const;

    // compare
    bool operator()(size_t lhs, size_t rhs) const;

    iresearch::VPackComparer _less;
    std::vector<Segment>* _segments;
  };

  // reads local document id from a specified segment
  LocalDocumentId readPK(Segment const& segment);

  void evaluateScores(ReadContext const& ctx, irs::score const& score, size_t numScores);

  void fillBuffer(ReadContext& ctx);

  bool writeRow(ReadContext& ctx, IndexReadBufferEntry bufferEntry);

  void reset();
  size_t skip(size_t toSkip);
  size_t skipAll();

 private:
  std::vector<Segment> _segments;
  irs::external_heap_iterator<MinHeapContext> _heap_it;
};  // IResearchViewMergeExecutor

template <bool ordered, iresearch::MaterializeType materializeType>
struct IResearchViewExecutorTraits<IResearchViewMergeExecutor<ordered, materializeType>> {
  using IndexBufferValueType = std::pair<LocalDocumentId, LogicalCollection const*>;
  static constexpr bool Ordered = ordered;
  static constexpr iresearch::MaterializeType MaterializeType = materializeType;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H
