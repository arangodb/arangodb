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
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"

#include <cstddef>
#include <memory>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

struct AqlCall;
class AqlItemBlockInputMatrix;
class AllRowsFetcher;
class RegisterInfos;
class NoStats;
class OutputAqlItemRow;
class AqlItemBlockManager;
struct SortRegister;

class SortExecutorInfos {
 public:
  SortExecutorInfos(RegisterCount nrInputRegisters, RegisterCount nrOutputRegisters,
                    RegIdFlatSet const& registersToClear,
                    std::vector<SortRegister> sortRegisters, std::size_t limit,
                    AqlItemBlockManager& manager,
                    velocypack::Options const* options, bool stable);

  SortExecutorInfos() = delete;
  SortExecutorInfos(SortExecutorInfos&&) = default;
  SortExecutorInfos(SortExecutorInfos const&) = delete;
  ~SortExecutorInfos() = default;

  [[nodiscard]] RegisterCount numberOfInputRegisters() const;

  [[nodiscard]] RegisterCount numberOfOutputRegisters() const;

  [[nodiscard]] RegIdFlatSet const& registersToClear() const;

  [[nodiscard]] velocypack::Options const* vpackOptions() const noexcept;

  [[nodiscard]] std::vector<SortRegister> const& sortRegisters() const noexcept;

  [[nodiscard]] bool stable() const;

  [[nodiscard]] size_t limit() const noexcept;

  [[nodiscard]] AqlItemBlockManager& itemBlockManager() noexcept;

 private:
  RegisterCount _numInRegs;
  RegisterCount _numOutRegs;
  RegIdFlatSet _registersToClear;
  std::size_t _limit;
  AqlItemBlockManager& _manager;
  velocypack::Options const* _vpackOptions;
  std::vector<SortRegister> _sortRegisters;
  bool _stable;
};

/**
 * @brief Implementation of Sort Node
 */
class SortExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = AllRowsFetcher;
  using Infos = SortExecutorInfos;
  using Stats = NoStats;

  SortExecutor(Fetcher&, Infos& infos);
  ~SortExecutor();

  void initializeInputMatrix(AqlItemBlockInputMatrix& inputMatrix);

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputMatrix& inputMatrix, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputMatrix& inputMatrix, AqlCall& call);

  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputMatrix const& input,
                                             AqlCall const& call) const noexcept -> size_t;

 private:
  void doSorting();

 private:
  Infos& _infos;

  AqlItemMatrix const* _input;
  InputAqlItemRow _currentRow;

  std::vector<AqlItemMatrix::RowIndex> _sortedIndexes;

  size_t _returnNext;
};
}  // namespace aql
}  // namespace arangodb

#endif
