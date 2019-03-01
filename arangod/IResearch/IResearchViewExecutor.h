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

namespace arangodb {
namespace aql {

template <bool>
class SingleRowFetcher;

class IResearchViewExecutorInfos : public ExecutorInfos {
 public:
  explicit IResearchViewExecutorInfos(ExecutorInfos&& infos,
                                      std::shared_ptr<iresearch::IResearchView::Snapshot const> reader,
                                      RegisterId outputRegister, Query& query,
                                      iresearch::IResearchViewNode const& node);

  RegisterId getOutputRegister() const;

  std::shared_ptr<iresearch::IResearchView::Snapshot const> getReader() const;

  Query& getQuery() const noexcept;

  iresearch::IResearchViewNode const& getNode() const;

 private:
  RegisterId _outputRegister;
  std::shared_ptr<iresearch::IResearchView::Snapshot const> _reader;
  Query& _query;
  // TODO Remove this member! Instead, pass all its members that are needed
  // separately.
  iresearch::IResearchViewNode const& _node;
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
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = IResearchViewExecutorInfos;
  using Stats = IResearchViewStats;

  IResearchViewExecutor() = delete;
  IResearchViewExecutor(IResearchViewExecutor&&) noexcept = default;
  IResearchViewExecutor(IResearchViewExecutor const&) = delete;
  IResearchViewExecutor(Fetcher& fetcher, Infos&);
  ~IResearchViewExecutor() = default;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 private:
  class ReadContext {
   private:
    static IndexIterator::DocumentCallback copyDocumentCallback(ReadContext& ctx);

   public:
    explicit ReadContext(aql::RegisterId curRegs, InputAqlItemRow& inputRow, OutputAqlItemRow& outputRow)
        : curRegs(curRegs), inputRow(inputRow), outputRow(outputRow), callback(copyDocumentCallback(*this)) {}

    aql::RegisterId const curRegs;
    size_t pos{};
    InputAqlItemRow& inputRow;
    OutputAqlItemRow& outputRow;
    IndexIterator::DocumentCallback const callback;
  };  // ReadContext

 private:
  Infos const& infos() const noexcept;

  // Copied from IResearchViewUnorderedBlock.
  // TODO Should be removed later, as it does not fit the pattern.
  std::pair<bool, size_t> next(ReadContext &ctx, size_t limit);

  bool resetIterator();

  void reset();

 private:
  Infos const& _infos;
  Fetcher& _fetcher;

  InputAqlItemRow _inputRow;

  // IResearchViewBlockBase members:
  std::vector<LocalDocumentId> _keys;  // buffer for primary keys
  irs::attribute_view _filterCtx;      // filter context
  iresearch::ViewExpressionContext _ctx;
  std::shared_ptr<iresearch::IResearchView::Snapshot const> _reader;
  irs::filter::prepared::ptr _filter;
  irs::order::prepared _order;
  iresearch::ExpressionExecutionContext _execCtx;  // expression execution context
  size_t _inflight;  // The number of documents inflight if we hit a WAITING state.
  bool _hasMore;
  bool _volatileSort;
  bool _volatileFilter;

  // IResearchViewUnorderedBlock members:
  irs::columnstore_reader::values_reader_f _pkReader;  // current primary key reader
  irs::doc_iterator::ptr _itr;
  size_t _readerOffset;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_EXECUTOR_H
