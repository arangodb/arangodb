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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/QueryOptions.h"
#include "Aql/RegisterInfos.h"

#include <cstddef>
#include <memory>

namespace arangodb {
struct ResourceMonitor;
class TemporaryStorageFeature;

namespace transaction {
class Methods;
}

namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
class AqlItemBlockManager;
class RegisterInfos;
class NoStats;
class OutputAqlItemRow;
template<BlockPassthrough>
class SingleRowFetcher;
struct SortRegister;
class SortedRowsStorageBackend;

class SortExecutorInfos {
 public:
  SortExecutorInfos(RegisterCount nrInputRegisters,
                    RegisterCount nrOutputRegisters,
                    RegIdFlatSet const& registersToClear,
                    std::vector<SortRegister> sortRegisters, std::size_t limit,
                    AqlItemBlockManager& manager,
                    TemporaryStorageFeature& tempStorage,
                    velocypack::Options const* options,
                    arangodb::ResourceMonitor& resourceMonitor,
                    size_t spillOverThresholdNumRows,
                    size_t spillOverThresholdMemoryUsage, bool stable);

  SortExecutorInfos() = delete;
  SortExecutorInfos(SortExecutorInfos&&) = default;
  SortExecutorInfos(SortExecutorInfos const&) = delete;
  ~SortExecutorInfos() = default;

  [[nodiscard]] RegisterCount numberOfInputRegisters() const;

  [[nodiscard]] RegisterCount numberOfOutputRegisters() const;

  [[nodiscard]] RegIdFlatSet const& registersToClear() const;

  [[nodiscard]] velocypack::Options const* vpackOptions() const noexcept;

  [[nodiscard]] std::vector<SortRegister> const& sortRegisters() const noexcept;

  [[nodiscard]] arangodb::ResourceMonitor& getResourceMonitor() const;

  [[nodiscard]] bool stable() const;

  [[nodiscard]] size_t limit() const noexcept;

  [[nodiscard]] size_t spillOverThresholdNumRows() const noexcept;

  [[nodiscard]] size_t spillOverThresholdMemoryUsage() const noexcept;

  [[nodiscard]] AqlItemBlockManager& itemBlockManager() noexcept;

  [[nodiscard]] TemporaryStorageFeature& getTemporaryStorageFeature() noexcept;

 private:
  RegisterCount _numInRegs;
  RegisterCount _numOutRegs;
  RegIdFlatSet _registersToClear;
  std::size_t _limit;
  AqlItemBlockManager& _manager;
  TemporaryStorageFeature& _tempStorage;
  velocypack::Options const* _vpackOptions;
  arangodb::ResourceMonitor& _resourceMonitor;
  std::vector<SortRegister> _sortRegisters;
  size_t _spillOverThresholdNumRows;
  size_t _spillOverThresholdMemoryUsage;
  bool _stable;
};

/**
 * @brief Implementation of Sort Node
 */
class SortExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = SortExecutorInfos;
  using Stats = NoStats;

  SortExecutor(Fetcher&, Infos& infos);
  ~SortExecutor();

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be sent to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be sent to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

 private:
  bool _inputReady = false;
  Infos& _infos;

  std::unique_ptr<SortedRowsStorageBackend> _storageBackend;
};
}  // namespace aql
}  // namespace arangodb
