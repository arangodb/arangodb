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

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/Stats.h"
#include "Aql/Variable.h"

#include "Aql/ExecutionBlockImpl.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

class ConstFetcher;
class AqlItemMatrix;
class ExecutorInfos;
class NoStats;
class OutputAqlItemRow;
struct SortRegister;

class IdExecutorInfos : public ExecutorInfos {
 public:
  IdExecutorInfos(RegisterId nrInOutRegisters, std::unordered_set<RegisterId> registersToKeep,
                  std::unordered_set<RegisterId> registersToClear);

  IdExecutorInfos() = delete;
  IdExecutorInfos(IdExecutorInfos&&) = default;
  IdExecutorInfos(IdExecutorInfos const&) = delete;
  ~IdExecutorInfos() = default;
};

template <class T>
class IdExecutor;

class JustPassThrough;

template <>
class IdExecutor<JustPassThrough> {};

template <>
class ExecutionBlockImpl<IdExecutor<JustPassThrough>> : public ExecutionBlock {
 public:
  ExecutionBlockImpl(ExecutionEngine* engine, ExecutionNode const* node, ExecutorInfos&& infos, RegisterId outputRegister)
      : ExecutionBlock(engine, node),
        _currentDependency(0),
        _outputRegister(outputRegister) {
    TRI_ASSERT(infos.numberOfInputRegisters() == infos.numberOfOutputRegisters());
    TRI_ASSERT(infos.numberOfInputRegisters() == infos.registersToKeep()->size() + infos.registersToClear()->size());
    for(auto const& it : *infos.registersToKeep()) {
      TRI_ASSERT(it < infos.numberOfInputRegisters());
      TRI_ASSERT(infos.registersToClear()->find(it) == infos.registersToClear()->end());
    }
    for(auto const& it : *infos.registersToClear()) {
      TRI_ASSERT(it < infos.numberOfInputRegisters());
      TRI_ASSERT(infos.registersToKeep()->find(it) == infos.registersToKeep()->end());
    }
  }

  ~ExecutionBlockImpl() override = default;

  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSome(size_t atMost) override {
    if (isDone()) {
      return {ExecutionState::DONE, nullptr};
    }

    ExecutionState state;
    SharedAqlItemBlockPtr block;
    std::tie(state, block) = currentDependency().getSome(atMost);

    if (state == ExecutionState::DONE) {
      nextDependency();
    }

    return {state, block};
  }

  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override {
    if (isDone()) {
      return {ExecutionState::DONE, 0};
    }

    ExecutionState state;
    size_t skipped;
    std::tie(state, skipped) = currentDependency().skipSome(atMost);

    if (state == ExecutionState::DONE) {
      nextDependency();
    }

    return {state, skipped};
  }

  RegisterId getOutputRegisterId() const noexcept {
    return _outputRegister;
  }

 private:
  bool isDone() const noexcept {
    // I'd like to assert this in the constructor, but the dependencies are added
    // after construction.
    TRI_ASSERT(!_dependencies.empty());
    return _currentDependency >= _dependencies.size();
  }

  ExecutionBlock& currentDependency() {
    TRI_ASSERT(_currentDependency < _dependencies.size());
    TRI_ASSERT(_dependencies[_currentDependency] != nullptr);
    return *_dependencies[_currentDependency];
  }

  void nextDependency() noexcept {
    ++_currentDependency;
  }

 private:
  size_t _currentDependency;
  RegisterId const _outputRegister;
};

template <class UsedFetcher>
class IdExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = true;
    static const bool inputSizeRestrictsOutputSize = false;
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

  inline std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const {
    // This is passthrough!
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Logic_error, prefetching number fo rows not supported");
  }

 private:
  Fetcher& _fetcher;
};
}  // namespace aql
}  // namespace arangodb

#endif
