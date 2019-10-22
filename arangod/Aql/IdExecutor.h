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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_ID_EXECUTOR_H
#define ARANGOD_AQL_ID_EXECUTOR_H

#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/SharedAqlItemBlockPtr.h"

#include <tuple>
#include <utility>

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {
class ExecutionEngine;
class ExecutionNode;
class ConstFetcher;
class AqlItemMatrix;
class ExecutorInfos;
class NoStats;
class OutputAqlItemRow;
struct SortRegister;

class IdExecutorInfos : public ExecutorInfos {
 public:
  IdExecutorInfos(RegisterId nrInOutRegisters, std::unordered_set<RegisterId> registersToKeep,
                  std::unordered_set<RegisterId> registersToClear,
                  std::string const& distributeId = "",
                  bool isResponsibleForInitializeCursor = true);

  IdExecutorInfos() = delete;
  IdExecutorInfos(IdExecutorInfos&&) = default;
  IdExecutorInfos(IdExecutorInfos const&) = delete;
  ~IdExecutorInfos() = default;

  std::string const& distributeId() { return _distributeId; }

  bool isResponsibleForInitializeCursor() const {
    return _isResponsibleForInitializeCursor;
  }

 private:
  std::string const _distributeId;

  bool const _isResponsibleForInitializeCursor;
};

// forward declaration
template <BlockPassthrough usePassThrough, class T>
class IdExecutor;

// (empty) implementation of IdExecutor<void>
template <>
class IdExecutor<BlockPassthrough::Enable, void> {};

// implementation of ExecutionBlockImpl<IdExecutor<void>>
template <>
class ExecutionBlockImpl<IdExecutor<BlockPassthrough::Enable, void>> : public ExecutionBlock {
 public:
  ExecutionBlockImpl(ExecutionEngine* engine, ExecutionNode const* node,
                     RegisterId outputRegister, bool doCount);

  ~ExecutionBlockImpl() override = default;

  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSome(size_t atMost) override;

  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override;

  RegisterId getOutputRegisterId() const noexcept;

  std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr> execute(AqlCallStack stack) override;

 private:
  bool isDone() const noexcept;

  ExecutionBlock& currentDependency() const;

  void nextDependency() noexcept;

  bool doCount() const noexcept;

  void countStats(SharedAqlItemBlockPtr& block);

 private:
  size_t _currentDependency;
  RegisterId const _outputRegister;
  bool const _doCount;
};

template <BlockPassthrough usePassThrough, class UsedFetcher>
// cppcheck-suppress noConstructor
class IdExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = usePassThrough;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  // Only Supports SingleRowFetcher and ConstFetcher
  using Fetcher = UsedFetcher;
  using Infos = IdExecutorInfos;
  using Stats = NoStats;

  IdExecutor(Fetcher& fetcher, IdExecutorInfos&);
  ~IdExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  template <BlockPassthrough allowPass = usePassThrough, typename = std::enable_if_t<allowPass == BlockPassthrough::Enable>>
  std::tuple<ExecutionState, Stats, SharedAqlItemBlockPtr> fetchBlockForPassthrough(size_t atMost);

  template <BlockPassthrough allowPass = usePassThrough, typename = std::enable_if_t<allowPass == BlockPassthrough::Disable>>
  std::tuple<ExecutionState, NoStats, size_t> skipRows(size_t atMost);

 private:
  Fetcher& _fetcher;
};  // namespace aql
}  // namespace aql
}  // namespace arangodb

#endif
