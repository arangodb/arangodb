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

// forward declaration
template <class T>
class IdExecutor;

// (empty) implementation of IdExecutor<void>
template <>
class IdExecutor<void> {};

// implementation of ExecutionBlockImpl<IdExecutor<void>>
template <>
class ExecutionBlockImpl<IdExecutor<void>> : public ExecutionBlock {
 public:
  ExecutionBlockImpl(ExecutionEngine* engine, ExecutionNode const* node,
                     RegisterId outputRegister, bool doCount)
      : ExecutionBlock(engine, node),
        _currentDependency(0),
        _outputRegister(outputRegister),
        _doCount(doCount) {
    // already insert ourselves into the statistics results
    if (_profile >= PROFILE_LEVEL_BLOCKS) {
      _engine->_stats.nodes.emplace(node->id(), ExecutionStats::Node());
    }
  }

  ~ExecutionBlockImpl() override = default;

  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSome(size_t atMost) override {
    traceGetSomeBegin(atMost);
    if (isDone()) {
      return traceGetSomeEnd(ExecutionState::DONE, nullptr);
    }

    ExecutionState state;
    SharedAqlItemBlockPtr block;
    std::tie(state, block) = currentDependency().getSome(atMost);

    countStats(block);

    if (state == ExecutionState::DONE) {
      nextDependency();
    }

    return traceGetSomeEnd(state, block);
  }

  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override {
    traceSkipSomeBegin(atMost);
    if (isDone()) {
      return traceSkipSomeEnd(ExecutionState::DONE, 0);
    }

    ExecutionState state;
    size_t skipped;
    std::tie(state, skipped) = currentDependency().skipSome(atMost);

    if (state == ExecutionState::DONE) {
      nextDependency();
    }

    return traceSkipSomeEnd(state, skipped);
  }

  RegisterId getOutputRegisterId() const noexcept { return _outputRegister; }

 private:
  bool isDone() const noexcept {
    // I'd like to assert this in the constructor, but the dependencies are
    // added after construction.
    TRI_ASSERT(!_dependencies.empty());
    return _currentDependency >= _dependencies.size();
  }

  ExecutionBlock& currentDependency() const {
    TRI_ASSERT(_currentDependency < _dependencies.size());
    TRI_ASSERT(_dependencies[_currentDependency] != nullptr);
    return *_dependencies[_currentDependency];
  }

  void nextDependency() noexcept { ++_currentDependency; }

  bool doCount() const noexcept { return _doCount; }

  void countStats(SharedAqlItemBlockPtr& block) {
    if (doCount() && block != nullptr) {
      CountStats stats;
      stats.setCounted(block->size());
      _engine->_stats += stats;
    }
  }

 private:
  size_t _currentDependency;
  RegisterId const _outputRegister;
  bool const _doCount;
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

  inline std::tuple<ExecutionState, Stats, SharedAqlItemBlockPtr> fetchBlockForPassthrough(size_t atMost) {
    auto rv = _fetcher.fetchBlockForPassthrough(atMost);
    return {rv.first, {}, std::move(rv.second)};
  }

 private:
  Fetcher& _fetcher;
};
}  // namespace aql
}  // namespace arangodb

#endif
