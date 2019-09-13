////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SORT_EXECUTOR_H
#define ARANGOD_AQL_SORT_EXECUTOR_H

#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Indexes/IndexIterator.h"


#include <memory>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class AllRowsFetcher;
class ExecutorInfos;
class NoStats;
class OutputAqlItemRow;
class AqlItemBlockManager;
struct SortRegister;

class SortExecutorInfos : public ExecutorInfos {
 public:
  SortExecutorInfos(std::vector<SortRegister> sortRegisters, std::size_t limit,
                    AqlItemBlockManager& manager, RegisterId nrInputRegisters,
                    RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
                    std::unordered_set<RegisterId> registersToKeep,
                    transaction::Methods* trx, bool stable,
                    std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters = nullptr);

  SortExecutorInfos() = delete;
  SortExecutorInfos(SortExecutorInfos&&) = default;
  SortExecutorInfos(SortExecutorInfos const&) = delete;
  ~SortExecutorInfos() = default;

  arangodb::transaction::Methods* trx() const;

  std::vector<SortRegister>& sortRegisters();

  bool stable() const;

  std::size_t _limit;
  AqlItemBlockManager& _manager;

 private:
  arangodb::transaction::Methods* _trx;
  std::vector<SortRegister> _sortRegisters;
  bool _stable;
};

class SortMaterializingExecutorInfos : public SortExecutorInfos {
 public:
  SortMaterializingExecutorInfos(std::vector<SortRegister> sortRegisters, std::size_t limit,
    AqlItemBlockManager& manager, RegisterId nrInputRegisters,
    RegisterId nrOutputRegisters, const std::unordered_set<RegisterId>& registersToClear,
    const std::unordered_set<RegisterId>& registersToKeep,
    transaction::Methods* trx, bool stable,
    RegisterId inNonMaterializedColRegId,
    RegisterId inNonMaterializedDocRegId, RegisterId outMaterializedDocumentRegId);

  SortMaterializingExecutorInfos() = delete;
  SortMaterializingExecutorInfos(SortMaterializingExecutorInfos&&) = default;
  SortMaterializingExecutorInfos(SortMaterializingExecutorInfos const&) = delete;
  ~SortMaterializingExecutorInfos() = default;


  inline RegisterId inputNonMaterializedDocRegId() const noexcept {
    return _inNonMaterializedDocRegId;
  }

  inline RegisterId inputNonMaterializedColRegId() const noexcept {
    return _inNonMaterializedColRegId;
  }

  inline RegisterId outputMaterializedDocumentRegId() const noexcept {
    return _outMaterializedDocumentRegId;
  }

 private:
  /// @brief register to store raw collection pointer
  RegisterId const _inNonMaterializedColRegId;
  /// @brief register to store local document id
  RegisterId const _inNonMaterializedDocRegId;
  /// @brief register to store materialized document
  RegisterId const _outMaterializedDocumentRegId;
};

class CopyRowProducer {
 public:
  using Infos = SortExecutorInfos;
  static constexpr bool needSkipRows = false;

  explicit CopyRowProducer(Infos&) {}
  void outputRow(const InputAqlItemRow& input, OutputAqlItemRow& output);
};

class MaterializerProducer  {
 public:
  using Infos = SortMaterializingExecutorInfos;
  static constexpr bool needSkipRows = true;

  explicit MaterializerProducer(Infos& infos) :
    _readDocumentContext(infos) {}

  void outputRow(const InputAqlItemRow& input, OutputAqlItemRow& output);

 protected:
  class ReadContext {
   public:
    explicit ReadContext(Infos& infos)
      : _infos(&infos),
      _inputRow(nullptr),
      _outputRow(nullptr),
      _callback(copyDocumentCallback(*this)) {}

    const Infos* _infos;
    const arangodb::aql::InputAqlItemRow* _inputRow;
    arangodb::aql::OutputAqlItemRow* _outputRow;
    arangodb::IndexIterator::DocumentCallback const _callback;

   private:
    static arangodb::IndexIterator::DocumentCallback copyDocumentCallback(ReadContext& ctx);
  };
  ReadContext _readDocumentContext;
};

/**
 * @brief Implementation of Sort Node
 */
template<typename OutputRowImpl>
class SortExecutor  {
 public:
  struct Properties {
    static const bool preservesOrder = false;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = AllRowsFetcher;
  using Infos = typename OutputRowImpl::Infos;
  using Stats = NoStats;

  SortExecutor(Fetcher& fetcher, Infos& infos);
  ~SortExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t) const;

  std::tuple<ExecutionState, Stats, size_t> skipRows(size_t toSkip);

 private:
  std::pair<ExecutionState, NoStats> fetchAllRowsFromUpstream();
  void doSorting();

 private:
  Infos& _infos;

  Fetcher& _fetcher;

  AqlItemMatrix const* _input;

  std::vector<AqlItemMatrix::RowIndex> _sortedIndexes;

  size_t _returnNext;

  OutputRowImpl _outputImpl;
};

}  // namespace aql
}  // namespace arangodb

#endif
