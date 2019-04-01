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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H
#define ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H

#include "Aql/ExecutionStats.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchExpressionContext.h"
#include "IResearch/IResearchView.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/LocalDocumentId.h"

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
      ExecutorInfos&& infos, std::shared_ptr<iresearch::IResearchView::Snapshot const> reader,
      RegisterId firstOutputRegister, RegisterId numScoreRegisters, Query& query,
      std::vector<iresearch::Scorer> const& scorers, ExecutionPlan const& plan,
      Variable const& outVariable, aql::AstNode const& filterCondition,
      std::pair<bool, bool> volatility, VarInfoMap const& varInfoMap, int depth);

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

 private:
  RegisterId const _outputRegister;
  RegisterId const _numScoreRegisters;
  std::shared_ptr<iresearch::IResearchView::Snapshot const> const _reader;
  Query& _query;

  std::vector<iresearch::Scorer> const& _scorers;
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

template <bool ordered>
class IResearchViewExecutor {
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

  IResearchViewExecutor() = delete;
  IResearchViewExecutor(IResearchViewExecutor&&) = default;
  IResearchViewExecutor(IResearchViewExecutor const&) = delete;
  IResearchViewExecutor(Fetcher& fetcher, Infos&);
  ~IResearchViewExecutor() = default;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

  // not implemented!
  inline size_t numberOfRowsInFlight() const;

 private:
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
    size_t pos{};
    InputAqlItemRow& inputRow;
    OutputAqlItemRow& outputRow;
    IndexIterator::DocumentCallback const callback;
  };  // ReadContext

  struct IndexResult {
    LocalDocumentId id;
    TRI_voc_cid_t cid;
    std::vector<AqlValue> scores;
  };

 private:
  Infos const& infos() const noexcept;

  bool next(ReadContext& ctx);

  void fillBuffer(ReadContext& ctx);

  bool writeRow(ReadContext& ctx, IndexResult& result);

  bool resetIterator();

  void reset();

  // Currently used only in the ordered case.
  bool readDocument(LogicalCollection const& collection, irs::doc_id_t docId,
                    irs::columnstore_reader::values_reader_f const& pkValues,
                    IndexIterator::DocumentCallback const& callback);

 private:
  Infos const& _infos;
  Fetcher& _fetcher;

  InputAqlItemRow _inputRow;

  ExecutionState _upstreamState;

  std::deque<IndexResult> _indexResultBuffer;

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

  // IResearchViewUnorderedBlock members:
  irs::columnstore_reader::values_reader_f _pkReader;  // current primary key reader
  irs::doc_iterator::ptr _itr;
  size_t _readerOffset;

  // IResearchViewBlock members (i.e., case ordered only):
  irs::score const* _scr;
  irs::bytes_ref _scrVal;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H
