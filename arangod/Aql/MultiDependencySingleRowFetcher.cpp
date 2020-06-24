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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "MultiDependencySingleRowFetcher.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/DependencyProxy.h"
#include "Aql/ShadowAqlItemRow.h"
#include "Logger/LogMacros.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

MultiDependencySingleRowFetcher::DependencyInfo::DependencyInfo()
    : _upstreamState{ExecutionState::HASMORE}, _currentBlock{nullptr}, _rowIndex{0} {}

MultiDependencySingleRowFetcher::MultiDependencySingleRowFetcher(
    DependencyProxy<BlockPassthrough::Disable>& executionBlock)
    : _dependencyProxy(&executionBlock) {}

std::pair<ExecutionState, SharedAqlItemBlockPtr> MultiDependencySingleRowFetcher::fetchBlockForDependency(
    size_t dependency, size_t atMost) {
  TRI_ASSERT(!_dependencyInfos.empty());
  atMost = (std::min)(atMost, ExecutionBlock::DefaultBatchSize);
  TRI_ASSERT(dependency < _dependencyInfos.size());

  auto& depInfo = _dependencyInfos[dependency];
  TRI_ASSERT(depInfo._upstreamState != ExecutionState::DONE);

  // There are still some blocks left that ask their parent even after they got
  // DONE the last time, and I don't currently have time to track them down.
  // Thus the following assert is commented out.
  // TRI_ASSERT(_upstreamState != ExecutionState::DONE);
  auto res = _dependencyProxy->fetchBlockForDependency(dependency, atMost);
  depInfo._upstreamState = res.first;

  return res;
}

MultiDependencySingleRowFetcher::MultiDependencySingleRowFetcher()
    : _dependencyProxy(nullptr) {}

RegisterCount MultiDependencySingleRowFetcher::getNrInputRegisters() const {
  return _dependencyProxy->getNrInputRegisters();
}

void MultiDependencySingleRowFetcher::initDependencies() {
  // Need to setup the dependencies, they are injected lazily.
  TRI_ASSERT(_dependencyProxy->numberDependencies() > 0);
  TRI_ASSERT(_dependencyInfos.empty());
  _dependencyInfos.reserve(_dependencyProxy->numberDependencies());
  for (size_t i = 0; i < _dependencyProxy->numberDependencies(); ++i) {
    _dependencyInfos.emplace_back(DependencyInfo{});
  }
  _dependencyStates.reserve(_dependencyProxy->numberDependencies());
  for (size_t i = 0; i < _dependencyProxy->numberDependencies(); ++i) {
    _dependencyStates.emplace_back(ExecutionState::HASMORE);
  }
}

size_t MultiDependencySingleRowFetcher::numberDependencies() {
  return _dependencyInfos.size();
}

void MultiDependencySingleRowFetcher::init() {
  TRI_ASSERT(_dependencyInfos.empty());
  initDependencies();
  _callsInFlight.resize(numberDependencies());
}

bool MultiDependencySingleRowFetcher::indexIsValid(
    const MultiDependencySingleRowFetcher::DependencyInfo& info) const {
  return info._currentBlock != nullptr && info._rowIndex < info._currentBlock->size();
}

bool MultiDependencySingleRowFetcher::isDone(
    const MultiDependencySingleRowFetcher::DependencyInfo& info) const {
  return info._upstreamState == ExecutionState::DONE;
}

bool MultiDependencySingleRowFetcher::isLastRowInBlock(
    const MultiDependencySingleRowFetcher::DependencyInfo& info) const {
  TRI_ASSERT(indexIsValid(info));
  return info._rowIndex + 1 == info._currentBlock->size();
}

bool MultiDependencySingleRowFetcher::noMoreDataRows(
    const MultiDependencySingleRowFetcher::DependencyInfo& info) const {
  return (isDone(info) && !indexIsValid(info)) ||
         (indexIsValid(info) && isAtShadowRow(info));
}

bool MultiDependencySingleRowFetcher::isAtShadowRow(DependencyInfo const& depInfo) const {
  TRI_ASSERT(indexIsValid(depInfo));
  return depInfo._currentBlock->isShadowRow(depInfo._rowIndex);
}

bool MultiDependencySingleRowFetcher::fetchBlockIfNecessary(size_t const dependency,
                                                            size_t const atMost) {
  MultiDependencySingleRowFetcher::DependencyInfo& depInfo = _dependencyInfos[dependency];
  if (!indexIsValid(depInfo) && !isDone(depInfo)) {
    // This returns the AqlItemBlock to the ItemBlockManager before fetching a
    // new one, so we might reuse it immediately!
    depInfo._currentBlock = nullptr;

    ExecutionState state;
    SharedAqlItemBlockPtr newBlock;
    std::tie(state, newBlock) = fetchBlockForDependency(dependency, atMost);
    if (state == ExecutionState::WAITING) {
      return false;
    }

    depInfo._currentBlock = std::move(newBlock);
    depInfo._rowIndex = 0;
  }
  return true;
}

//@deprecated
auto MultiDependencySingleRowFetcher::useStack(AqlCallStack const& stack) -> void {
  _dependencyProxy->useStack(stack);
}

auto MultiDependencySingleRowFetcher::executeForDependency(size_t const dependency,
                                                           AqlCallStack& stack)
    -> std::tuple<ExecutionState, SkipResult, AqlItemBlockInputRange> {
  auto [state, skipped, block] = _dependencyProxy->executeForDependency(dependency, stack);

  if (state == ExecutionState::WAITING) {
    return {state, SkipResult{}, AqlItemBlockInputRange{ExecutorState::HASMORE}};
  }
  ExecutorState execState =
      state == ExecutionState::DONE ? ExecutorState::DONE : ExecutorState::HASMORE;

  _dependencyStates.at(dependency) = state;

  if (block == nullptr) {
    return {state, skipped, AqlItemBlockInputRange{execState, skipped.getSkipCount()}};
  }
  TRI_ASSERT(block != nullptr);
  auto [start, end] = block->getRelevantRange();
  return {state, skipped,
          AqlItemBlockInputRange{execState, skipped.getSkipCount(), block, start}};
}

auto MultiDependencySingleRowFetcher::execute(AqlCallStack const& stack,
                                              AqlCallSet const& aqlCallSet)
    -> std::tuple<ExecutionState, SkipResult, std::vector<std::pair<size_t, AqlItemBlockInputRange>>> {
  TRI_ASSERT(_callsInFlight.size() == numberDependencies());

  auto ranges = std::vector<std::pair<size_t, AqlItemBlockInputRange>>{};
  ranges.reserve(aqlCallSet.size());

  auto depCallIdx = size_t{0};
  auto allAskedDepsAreWaiting = true;
  auto askedAtLeastOneDep = false;
  auto skippedTotal = SkipResult{};
  // Iterate in parallel over `_callsInFlight` and `aqlCall.calls`.
  // _callsInFlight[i] corresponds to aqlCalls.calls[k] iff
  // aqlCalls.calls[k].dependency = i.
  // So there is not always a matching entry in aqlCall.calls.
  for (auto dependency = size_t{0}; dependency < _callsInFlight.size(); ++dependency) {
    auto& maybeCallInFlight = _callsInFlight[dependency];

    // See if there is an entry for `dependency` in `aqlCall.calls`
    if (depCallIdx < aqlCallSet.calls.size() &&
        aqlCallSet.calls[depCallIdx].dependency == dependency) {
      // If there is a call in flight, we *must not* change the call,
      // no matter what we got. Otherwise, we save the current call.
      if (!maybeCallInFlight.has_value()) {
        auto depStack = stack;
        depStack.pushCall(aqlCallSet.calls[depCallIdx].call);
        maybeCallInFlight = depStack;
      }
      ++depCallIdx;
      if (depCallIdx < aqlCallSet.calls.size()) {
        TRI_ASSERT(aqlCallSet.calls[depCallIdx - 1].dependency <
                   aqlCallSet.calls[depCallIdx].dependency);
      }
    }

    if (maybeCallInFlight.has_value()) {
      // We either need to make a new call, or check whether we got a result
      // for a call in flight.
      auto& callInFlight = maybeCallInFlight.value();
      auto [state, skipped, range] = executeForDependency(dependency, callInFlight);
      askedAtLeastOneDep = true;
      if (state != ExecutionState::WAITING) {
        // Got a result, call is no longer in flight
        maybeCallInFlight = std::nullopt;
        allAskedDepsAreWaiting = false;

        // NOTE:
        // in this fetcher case we do not have and do not want to have
        // any control of the order the upstream responses are entering.
        // Every of the upstream response will contain an identical skipped
        // stack on the subqueries.
        // We only need to forward the skipping of any one of those.
        // So we implemented the following logic to return the skip
        // information for the first on that arrives and all other
        // subquery skip informations will be discarded.
        if (!_didReturnSubquerySkips) {
          // We have nothing skipped locally.
          TRI_ASSERT(skippedTotal.subqueryDepth() == 1);
          TRI_ASSERT(skippedTotal.getSkipCount() == 0);

          // We forward the skip block as is.
          // This will also include the skips on subquery level
          skippedTotal = skipped;
          // Do this only once.
          // The first response will contain the amount of rows skipped
          // in subquery
          _didReturnSubquerySkips = true;
        } else {
          // We only need the skip amount on the top level.
          // Another dependency has forwarded the subquery level skips
          // already
          skippedTotal.mergeOnlyTopLevel(skipped);
        }

      } else {
        TRI_ASSERT(skipped.nothingSkipped());
      }

      ranges.emplace_back(dependency, range);
    }
  }

  auto const state = std::invoke([&]() {
    if (askedAtLeastOneDep && allAskedDepsAreWaiting) {
      TRI_ASSERT(skippedTotal.nothingSkipped());
      return ExecutionState::WAITING;
    } else {
      return upstreamState();
    }
  });

  return {state, skippedTotal, ranges};
}

auto MultiDependencySingleRowFetcher::upstreamState() const -> ExecutionState {
  if (std::any_of(std::begin(_dependencyStates), std::end(_dependencyStates),
                  [](ExecutionState const s) {
                    return s == ExecutionState::HASMORE;
                  })) {
    return ExecutionState::HASMORE;
  } else {
    return ExecutionState::DONE;
  }
}
