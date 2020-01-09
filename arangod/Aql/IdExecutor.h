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

// There are currently three variants of IdExecutor in use:
//
// - IdExecutor<void>
//     This is a variant of the ReturnBlock. It can optionally count and holds
//     an output register id.
// - IdExecutor<ConstFetcher>
//     This is the SingletonBlock.
// - IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>
//     This is the DistributeConsumerBlock. It holds a distributeId and honors
//     isResponsibleForInitializeCursor.
//
// The last variant using the SingleRowFetcher could be replaced by the (faster)
// void variant. It only has to learn distributeId and
// isResponsibleForInitializeCursor for that.

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {
class ExecutionEngine;
class ExecutionNode;
class ExecutorInfos;
class NoStats;
class OutputAqlItemRow;

class IdExecutorInfos : public ExecutorInfos {
 public:
  IdExecutorInfos(RegisterId nrInOutRegisters, std::unordered_set<RegisterId> registersToKeep,
                  std::unordered_set<RegisterId> registersToClear,
                  std::string distributeId = {""},
                  bool isResponsibleForInitializeCursor = true);

  IdExecutorInfos() = delete;
  IdExecutorInfos(IdExecutorInfos&&) = default;
  IdExecutorInfos(IdExecutorInfos const&) = delete;
  ~IdExecutorInfos() = default;

  [[nodiscard]] std::string const& distributeId();

  [[nodiscard]] bool isResponsibleForInitializeCursor() const;

 private:
  std::string const _distributeId;

  bool const _isResponsibleForInitializeCursor;
};

// forward declaration
template <class Fetcher>
class IdExecutor;

// (empty) implementation of IdExecutor<void>
template <>
class IdExecutor<void> {};

// implementation of ExecutionBlockImpl<IdExecutor<void>>
template <>
class ExecutionBlockImpl<IdExecutor<void>> : public ExecutionBlock {
 public:
  ExecutionBlockImpl(ExecutionEngine* engine, ExecutionNode const* node,
                     RegisterId outputRegister, bool doCount);

  ~ExecutionBlockImpl() override = default;

  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSome(size_t atMost) override;

  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override;

  [[nodiscard]] RegisterId getOutputRegisterId() const noexcept;

  std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr> execute(AqlCallStack stack) override;

 private:
  [[nodiscard]] bool isDone() const noexcept;

  [[nodiscard]] ExecutionBlock& currentDependency() const;

  void nextDependency() noexcept;

  [[nodiscard]] bool doCount() const noexcept;

  void countStats(SharedAqlItemBlockPtr& block);

 private:
  size_t _currentDependency;
  RegisterId const _outputRegister;
  bool const _doCount;
};

template <class UsedFetcher>
// cppcheck-suppress noConstructor
class IdExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Enable;
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

  std::tuple<ExecutionState, Stats, SharedAqlItemBlockPtr> fetchBlockForPassthrough(size_t atMost);

 private:
  Fetcher& _fetcher;
};
}  // namespace aql
}  // namespace arangodb

#endif
