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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H
#define ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/IResearchViewNode.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchExpressionContext.h"
#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchView.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/LocalDocumentId.h"

#include <formats/formats.hpp>
#include <index/heap_iterator.hpp>

#include <utility>

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
struct ExecutionStats;
class OutputAqlItemRow;
template <BlockPassthrough>
class SingleRowFetcher;

class IResearchViewExecutorInfos : public ExecutorInfos {
 public:
  using VarInfoMap = std::unordered_map<aql::VariableId, aql::VarInfo>;

  IResearchViewExecutorInfos(
      ExecutorInfos&& infos, std::shared_ptr<iresearch::IResearchView::Snapshot const> reader,
      RegisterId firstOutputRegister, RegisterId numScoreRegisters,
      Query& query, std::vector<iresearch::Scorer> const& scorers,
      std::pair<iresearch::IResearchViewSort const*, size_t> const& sort,
      iresearch::IResearchViewStoredValues const& storedValues,
      ExecutionPlan const& plan,
      Variable const& outVariable,
      aql::AstNode const& filterCondition,
      std::pair<bool, bool> volatility,
      VarInfoMap const& varInfoMap,
      int depth, iresearch::IResearchViewNode::ViewValuesRegisters&& outNonMaterializedViewRegs);

  RegisterId getOutputRegister() const noexcept;
  RegisterId getFirstScoreRegister() const noexcept;
  RegisterId getNumScoreRegisters() const noexcept;
  iresearch::IResearchViewNode::ViewValuesRegisters const& getOutNonMaterializedViewRegs() const noexcept;
  std::shared_ptr<iresearch::IResearchView::Snapshot const> getReader() const noexcept;
  Query& getQuery() const noexcept;
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

  bool isScoreReg(RegisterId reg) const noexcept;

 private:
  RegisterId const _firstOutputRegister;
  RegisterId const _numScoreRegisters;
  std::shared_ptr<iresearch::IResearchView::Snapshot const> const _reader;
  Query& _query;
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

  std::size_t getScanned() const noexcept;

 private:
  std::size_t _scannedIndex;
};  // IResearchViewStats

ExecutionStats& operator+=(ExecutionStats& executionStats,
                           IResearchViewStats const& iResearchViewStats) noexcept;

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
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::tuple<ExecutionState, Stats, size_t> skipRows(size_t toSkip);
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

 protected:
  class ReadContext {
   private:
    static IndexIterator::DocumentCallback copyDocumentCallback(ReadContext& ctx);

   public:
    explicit ReadContext(aql::RegisterId docOutReg, InputAqlItemRow& inputRow,
                         OutputAqlItemRow& outputRow);

    aql::RegisterId const docOutReg;
    InputAqlItemRow& inputRow;
    OutputAqlItemRow& outputRow;
    IndexIterator::DocumentCallback const callback;

    aql::RegisterId getNmColPtrOutReg() const noexcept {
      return docOutReg;
    }

    aql::RegisterId getNmDocIdOutReg() const noexcept {
      return docOutReg + 1;
    }
  };  // ReadContext

  template <typename ValueType>
  class IndexReadBuffer;

  class IndexReadBufferEntry {
   public:
    IndexReadBufferEntry() = delete;

   private:
    template <typename ValueType>
    friend class IndexReadBuffer;

    explicit inline IndexReadBufferEntry(std::size_t keyIdx) noexcept;

   protected:
    std::size_t _keyIdx;
  };

  // Holds and encapsulates the data read from the iresearch index.
  template <typename ValueType>
  class IndexReadBuffer {
   public:
    class ScoreIterator {
     public:
      ScoreIterator() = delete;
      ScoreIterator(std::vector<AqlValue>& scoreBuffer, std::size_t keyIdx,
                    std::size_t numScores) noexcept;

      std::vector<AqlValue>::iterator begin() noexcept;

      std::vector<AqlValue>::iterator end() noexcept;

     private:
      std::vector<AqlValue>& _scoreBuffer;
      std::size_t _scoreBaseIdx;
      std::size_t _numScores;
    };

   public:
    IndexReadBuffer() = delete;
    explicit IndexReadBuffer(std::size_t numScoreRegisters);

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

    std::size_t size() const noexcept;

    bool empty() const noexcept;

    IndexReadBufferEntry pop_front() noexcept;

    // This is violated while documents and scores are pushed, but must hold
    // before and after.
    void assertSizeCoherence() const noexcept;

    void pushStoredValue(std::vector<irs::bytes_ref>&& storedValue);

    std::vector<irs::bytes_ref> const& getStoredValue(IndexReadBufferEntry bufferEntry) const noexcept;

   private:
    // _keyBuffer, _scoreBuffer, _sortValueBuffer together hold all the
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
    std::vector<std::vector<irs::bytes_ref>> _storedValueBuffer;
    std::size_t _numScoreRegisters;
    std::size_t _keyBaseIdx;
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

  bool writeLocalDocumentId(ReadContext& ctx,
                LocalDocumentId const& documentId,
                LogicalCollection const& collection);

  void reset();

  bool writeStoredValue(ReadContext& ctx, std::vector<irs::bytes_ref> const& storedValues, size_t columnNum,
                        std::map<size_t, RegisterId> const& fieldsRegs);

  void getStoredValue(irs::document const& doc, std::vector<irs::bytes_ref>& storedValue, size_t index,
                      std::vector<irs::columnstore_reader::values_reader_f> const& storedValuesReaders);

  void pushStoredValues(irs::document const& doc, std::vector<irs::columnstore_reader::values_reader_f> const& storedValuesReaders);

  bool getStoredValuesReaders(irs::sub_reader const& segmentReader, std::vector<irs::columnstore_reader::values_reader_f>& storedValuesReaders);

 private:
  bool next(ReadContext& ctx);

 protected:
  Infos const& _infos;
  Fetcher& _fetcher;
  InputAqlItemRow _inputRow;
  ExecutionState _upstreamState;
  IndexReadBuffer<typename Traits::IndexBufferValueType> _indexReadBuffer;
  irs::bytes_ref _pk;  // temporary store for pk buffer before decoding it
  irs::attribute_view _filterCtx;  // filter context
  iresearch::ViewExpressionContext _ctx;
  std::shared_ptr<iresearch::IResearchView::Snapshot const> _reader;
  irs::filter::prepared::ptr _filter;
  irs::order::prepared _order;
  iresearch::ExpressionExecutionContext _execCtx;  // expression execution context
  size_t _inflight;  // The number of documents inflight if we hit a WAITING state.
  bool _hasMore;
  bool _isInitialized;
};  // IResearchViewExecutorBase

template <bool ordered, iresearch::MaterializeType materializeType>
class IResearchViewExecutor : public IResearchViewExecutorBase<IResearchViewExecutor<ordered, materializeType>> {
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

  irs::columnstore_reader::values_reader_f _pkReader;   // current primary key reader
  irs::doc_iterator::ptr _itr;
  irs::document const* _doc{};
  size_t _readerOffset;
  LogicalCollection const* _collection{};
  std::vector<irs::columnstore_reader::values_reader_f> _storedValuesReaders; // current stored values readers

  // case ordered only:
  irs::score const* _scr;
  irs::bytes_ref _scrVal;
};  // IResearchViewExecutor

template<bool ordered, iresearch::MaterializeType materializeType>
struct IResearchViewExecutorTraits<IResearchViewExecutor<ordered, materializeType>> {
  using IndexBufferValueType = LocalDocumentId;
  static constexpr bool Ordered = ordered;
  static constexpr iresearch::MaterializeType MaterializeType = materializeType;
};

template <bool ordered, iresearch::MaterializeType materializeType>
class IResearchViewMergeExecutor : public IResearchViewExecutorBase<IResearchViewMergeExecutor<ordered, materializeType>> {
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
            irs::score const& score, LogicalCollection const& collection,
            irs::columnstore_reader::values_reader_f&& pkReader,
            std::vector<irs::columnstore_reader::values_reader_f>&& storedValuesReaders) noexcept;
    Segment(Segment const&) = delete;
    Segment(Segment&&) = default;
    Segment& operator=(Segment const&) = delete;
    Segment& operator=(Segment&&) = delete;

    irs::doc_iterator::ptr docs;
    irs::document const* doc{};
    irs::score const* score{};
    arangodb::LogicalCollection const* collection{};  // collecton associated with a segment
    irs::bytes_ref sortValue{irs::bytes_ref::NIL};        // sort column value
    irs::columnstore_reader::values_reader_f pkReader;    // primary key reader
    std::vector<irs::columnstore_reader::values_reader_f> storedValuesReaders; // current stored values readers
    irs::columnstore_reader::values_reader_f& sortReader; // sort column reader
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

  void evaluateScores(ReadContext const& ctx, irs::score const& score);

  void fillBuffer(ReadContext& ctx);

  bool writeRow(ReadContext& ctx, IndexReadBufferEntry bufferEntry);

  void reset();
  size_t skip(size_t toSkip);

 private:
  std::vector<Segment> _segments;
  irs::external_heap_iterator<MinHeapContext> _heap_it;
};  // IResearchViewMergeExecutor

template<bool ordered, iresearch::MaterializeType materializeType>
struct IResearchViewExecutorTraits<IResearchViewMergeExecutor<ordered, materializeType>> {
  using IndexBufferValueType = std::pair<LocalDocumentId, LogicalCollection const*>;
  static constexpr bool Ordered = ordered;
  static constexpr iresearch::MaterializeType MaterializeType = materializeType;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H
