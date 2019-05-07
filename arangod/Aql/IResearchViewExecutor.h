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
#include "Aql/ExecutionStats.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchExpressionContext.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchVPackComparer.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/LocalDocumentId.h"

#include "index/heap_iterator.hpp"

namespace iresearch {
class score;
}

namespace arangodb {

namespace iresearch {
struct Scorer;
}

namespace aql {

template <bool>
class SingleRowFetcher;

class IResearchViewExecutorInfos : public ExecutorInfos {
 public:
  using VarInfoMap = std::unordered_map<aql::VariableId, aql::ExecutionNode::VarInfo>;

  IResearchViewExecutorInfos(
      ExecutorInfos&& infos,
      std::shared_ptr<iresearch::IResearchView::Snapshot const> reader,
      RegisterId firstOutputRegister,
      RegisterId numScoreRegisters,
      Query& query,
      std::vector<iresearch::Scorer> const& scorers,
      iresearch::IResearchViewSort const* sort,
      ExecutionPlan const& plan,
      Variable const& outVariable,
      aql::AstNode const& filterCondition,
      std::pair<bool, bool> volatility,
      VarInfoMap const& varInfoMap,
      int depth);

  RegisterId getOutputRegister() const;

  RegisterId getNumScoreRegisters() const;

  std::shared_ptr<iresearch::IResearchView::Snapshot const> getReader() const;

  Query& getQuery() const noexcept;

  std::vector<iresearch::Scorer> const& scorers() const noexcept;
  ExecutionPlan const& plan() const noexcept;
  Variable const& outVariable() const noexcept;
  aql::AstNode const& filterCondition() const noexcept;
  VarInfoMap const& varInfoMap() const noexcept;
  int getDepth() const noexcept;
  bool volatileSort() const noexcept;
  bool volatileFilter() const noexcept;

  bool isScoreReg(RegisterId reg) const;

  iresearch::IResearchViewSort const* sort() {
    return _sort;
  }

 private:
  RegisterId const _outputRegister;
  RegisterId const _numScoreRegisters;
  std::shared_ptr<iresearch::IResearchView::Snapshot const> const _reader;
  Query& _query;

  std::vector<iresearch::Scorer> const& _scorers;
  iresearch::IResearchViewSort const* _sort{};
  ExecutionPlan const& _plan;
  Variable const& _outVariable;
  aql::AstNode const& _filterCondition;
  bool const _volatileSort;
  bool const _volatileFilter;
  VarInfoMap const& _varInfoMap;
  int const _depth;
};

class IResearchViewStats {
 public:
  IResearchViewStats() noexcept : _scannedIndex(0) {}

  void incrScanned() noexcept { _scannedIndex++; }
  void incrScanned(size_t value) noexcept {
    _scannedIndex = _scannedIndex + value;
  }

  std::size_t getScanned() const noexcept { return _scannedIndex; }

 private:
  std::size_t _scannedIndex;
};

inline ExecutionStats& operator+=(ExecutionStats& executionStats,
                                  IResearchViewStats const& iResearchViewStats) noexcept {
  executionStats.scannedIndex += iResearchViewStats.getScanned();
  return executionStats;
}

template<typename Impl>
struct IResearchViewExecutorTraits;

template<typename Impl, typename Traits = IResearchViewExecutorTraits<Impl>>
class IResearchViewExecutorBase {
 public:
  struct Properties {
    // even with "ordered = true", this block preserves the order; it just
    // writes scorer information in additional register for a following sort
    // block to use.
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = IResearchViewExecutorInfos;
  using Stats = IResearchViewStats;

  inline std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Logic_error, prefetching number fo rows not supported");
  }

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
                         OutputAqlItemRow& outputRow)
        : docOutReg(docOutReg),
          inputRow(inputRow),
          outputRow(outputRow),
          callback(copyDocumentCallback(*this)) {}

    aql::RegisterId const docOutReg;
    InputAqlItemRow& inputRow;
    OutputAqlItemRow& outputRow;
    IndexIterator::DocumentCallback const callback;
  };  // ReadContext

  template<typename ValueType>
  class IndexReadBuffer;

  class IndexReadBufferEntry {
    template<typename ValueType>
    friend class IndexReadBuffer;

    IndexReadBufferEntry() = delete;
    inline IndexReadBufferEntry(std::size_t keyIdx) : _keyIdx(keyIdx) {}

   protected:
    std::size_t _keyIdx;
  };

  // Holds and encapsulates the data read from the iresearch index.
  template<typename ValueType>
  class IndexReadBuffer {
   public:
    class ScoreIterator {
     public:
      ScoreIterator() = delete;
      inline ScoreIterator(std::vector<AqlValue>& scoreBuffer,
                           std::size_t keyIdx, std::size_t numScores)
          : _scoreBuffer(scoreBuffer),
            _scoreBaseIdx(keyIdx * numScores),
            _numScores(numScores) {
        TRI_ASSERT(_scoreBaseIdx + _numScores <= _scoreBuffer.size());
      }

      inline std::vector<AqlValue>::iterator begin() noexcept {
        return _scoreBuffer.begin() + _scoreBaseIdx;
      }
      inline std::vector<AqlValue>::iterator end() noexcept {
        return _scoreBuffer.begin() + _scoreBaseIdx + _numScores;
      }

     private:
      std::vector<AqlValue>& _scoreBuffer;
      std::size_t _scoreBaseIdx;
      std::size_t _numScores;
    };

   public:
    IndexReadBuffer() = delete;
    explicit IndexReadBuffer(std::size_t const numScoreRegisters)
        : _keyBuffer(),
          _scoreBuffer(),
          _numScoreRegisters(numScoreRegisters),
          _keyBaseIdx(0) {}

    inline ValueType const& getValue(IndexReadBufferEntry const bufferEntry) const noexcept {
      assertSizeCoherence();
      TRI_ASSERT(bufferEntry._keyIdx < _keyBuffer.size());
      return _keyBuffer[bufferEntry._keyIdx];
    }

    inline ScoreIterator getScores(IndexReadBufferEntry const bufferEntry) noexcept {
      assertSizeCoherence();
      return ScoreIterator{_scoreBuffer, bufferEntry._keyIdx, _numScoreRegisters};
    }

    template<typename... Args>
    inline void pushValue(Args&&... args) {
      _keyBuffer.emplace_back(std::forward<Args>(args)...);
    }

    // A note on the scores: instead of saving an array of AqlValues, we could
    // save an array of floats plus a bitfield noting which entries should be
    // None.

    inline void pushScore(float_t const scoreValue) {
      _scoreBuffer.emplace_back(AqlValueHintDouble{scoreValue});
    }

    inline void pushScoreNone() { _scoreBuffer.emplace_back(); }

    inline void reset() noexcept {
      // Should only be called after everything was consumed
      TRI_ASSERT(empty());
      _keyBaseIdx = 0;
      _keyBuffer.clear();
      _scoreBuffer.clear();
    }

    inline std::size_t size() const noexcept {
      assertSizeCoherence();
      return _keyBuffer.size() - _keyBaseIdx;
    }

    inline bool empty() const noexcept { return size() == 0; }

    inline IndexReadBufferEntry pop_front() noexcept {
      TRI_ASSERT(!empty());
      TRI_ASSERT(_keyBaseIdx < _keyBuffer.size());
      assertSizeCoherence();
      IndexReadBufferEntry entry{_keyBaseIdx};
      ++_keyBaseIdx;
      return entry;
    }

    // This is violated while documents and scores are pushed, but must hold
    // before and after.
    inline void assertSizeCoherence() const {
      TRI_ASSERT(_scoreBuffer.size() == _keyBuffer.size() * _numScoreRegisters);
    };

   private:
    // _keyBuffer, _scoreBuffer together hold all the
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
    std::size_t _numScoreRegisters;
    std::size_t _keyBaseIdx;
  };

  IResearchViewExecutorBase() = delete;
  IResearchViewExecutorBase(IResearchViewExecutorBase&&) = default;
  IResearchViewExecutorBase(IResearchViewExecutorBase const&) = delete;
  IResearchViewExecutorBase(Fetcher& fetcher, Infos&);
  ~IResearchViewExecutorBase() = default;

  Infos const& infos() const noexcept {
    return _infos;
  }

  void fillScores(ReadContext const& ctx,
                  float_t const* begin,
                  float_t const* end);

  bool writeRow(ReadContext& ctx,
                IndexReadBufferEntry bufferEntry,
                LocalDocumentId const& documentId,
                LogicalCollection const& collection);

  void reset();

 private:
  bool next(ReadContext& ctx);

 protected:
  Infos const& _infos;
  Fetcher& _fetcher;
  InputAqlItemRow _inputRow;
  ExecutionState _upstreamState;
  IndexReadBuffer<typename Traits::IndexBufferValueType> _indexReadBuffer;

  // IResearchViewBlockBase members:
  irs::attribute_view _filterCtx;  // filter context
  iresearch::ViewExpressionContext _ctx;
  std::shared_ptr<iresearch::IResearchView::Snapshot const> _reader;
  irs::filter::prepared::ptr _filter;
  irs::order::prepared _order;
  iresearch::ExpressionExecutionContext _execCtx;  // expression execution context
  size_t _inflight;  // The number of documents inflight if we hit a WAITING state.
  bool _hasMore;
  bool _isInitialized;
}; // IResearchViewExecutorBase

template <bool ordered>
class IResearchViewExecutor : public IResearchViewExecutorBase<IResearchViewExecutor<ordered>> {
 public:
  using Base = IResearchViewExecutorBase<IResearchViewExecutor<ordered>>;
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

  inline bool writeRow(ReadContext& ctx, IndexReadBufferEntry bufferEntry) {
    TRI_ASSERT(_collection);

    return Base::writeRow(ctx,
                          bufferEntry,
                          this->_indexReadBuffer.getValue(bufferEntry),
                          *_collection);
  }

  bool resetIterator();

  void reset();

 private:
  irs::columnstore_reader::values_reader_f _pkReader;  // current primary key reader
  irs::doc_iterator::ptr _itr;
  size_t _readerOffset;
  LogicalCollection const* _collection{};

  // case ordered only:
  irs::score const* _scr;
  irs::bytes_ref _scrVal;
}; // IResearchViewExecutor

template<bool ordered>
struct IResearchViewExecutorTraits<IResearchViewExecutor<ordered>> {
  using IndexBufferValueType = LocalDocumentId;
  static constexpr const bool Ordered = ordered;
};

template <bool ordered>
class IResearchViewMergeExecutor : public IResearchViewExecutorBase<IResearchViewMergeExecutor<ordered>> {
 public:
  using Base = IResearchViewExecutorBase<IResearchViewMergeExecutor<ordered>>;
  using Fetcher = typename Base::Fetcher;
  using Infos = typename Base::Infos;

  static constexpr const bool Ordered = ordered;

  IResearchViewMergeExecutor(IResearchViewMergeExecutor&&) = default;
  IResearchViewMergeExecutor(Fetcher& fetcher, Infos&);

 private:
  friend Base;

  using ReadContext = typename Base::ReadContext;
  using IndexReadBufferEntry = typename Base::IndexReadBufferEntry;

  struct Segment {
    Segment(irs::doc_iterator::ptr&& docs,
            irs::score const& score,
            LogicalCollection const& collection,
            irs::columnstore_reader::values_reader_f&& sortReader,
            irs::columnstore_reader::values_reader_f&& pkReader) noexcept
      : docs(std::move(docs)),
        score(&score),
        collection(&collection),
        sortReader(std::move(sortReader)),
        pkReader(std::move(pkReader)) {
      TRI_ASSERT(this->docs);
      TRI_ASSERT(this->score);
      TRI_ASSERT(this->collection);
      TRI_ASSERT(this->pkReader);
    }
    Segment(Segment const&) = delete;
    Segment(Segment&&) = default;
    Segment& operator=(Segment const&) = delete;
    Segment& operator=(Segment&&) = default;

    irs::doc_iterator::ptr docs;
    irs::score const* score{};
    arangodb::LogicalCollection const* collection{}; // collecton associated with a segment
    irs::bytes_ref sortValue{ irs::bytes_ref::NIL }; // sort column value
    irs::columnstore_reader::values_reader_f sortReader; // sort column reader
    irs::columnstore_reader::values_reader_f pkReader;  // primary key reader
  };

  class MinHeapContext {
   public:
    MinHeapContext(
        iresearch::IResearchViewSort const& sort,
        std::vector<Segment>& segments) noexcept
      : _less(sort),
        _segments(&segments) {
    }

    // advance
    bool operator()(const size_t i) const {
      assert(i < (*_segments).size());
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

    // compare
    bool operator()(const size_t lhs, const size_t rhs) const {
      assert(lhs < (*_segments).size());
      assert(rhs < (*_segments).size());
      return _less((*_segments)[rhs].sortValue, (*_segments)[lhs].sortValue);
    }

    iresearch::VPackComparer _less;
    std::vector<Segment>* _segments;
  };

  void evaluateScores(ReadContext const& ctx, irs::score const& score);

  void fillBuffer(ReadContext& ctx);

  inline bool writeRow(ReadContext& ctx, IndexReadBufferEntry bufferEntry) {
    auto const& id = this->_indexReadBuffer.getValue(bufferEntry);
    LocalDocumentId const& documentId = id.first;
    TRI_ASSERT(documentId.isSet());
    LogicalCollection const* collection = id.second;
    TRI_ASSERT(collection);

    return Base::writeRow(ctx, bufferEntry, documentId, *collection);
  }

  void reset();
  size_t skip(size_t toSkip);

 private:
  std::vector<Segment> _segments;
  irs::external_heap_iterator<MinHeapContext> _heap_it;
}; // IResearchViewMergeExecutor

template<bool ordered>
struct IResearchViewExecutorTraits<IResearchViewMergeExecutor<ordered>> {
  using IndexBufferValueType = std::pair<LocalDocumentId, LogicalCollection const*>;
  static constexpr const bool Ordered = ordered;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H
