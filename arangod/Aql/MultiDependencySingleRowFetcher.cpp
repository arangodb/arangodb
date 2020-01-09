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
  atMost = (std::min)(atMost, ExecutionBlock::DefaultBatchSize());
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

std::pair<ExecutionState, size_t> MultiDependencySingleRowFetcher::skipSomeForDependency(
    size_t const dependency, size_t const atMost) {
  TRI_ASSERT(!_dependencyInfos.empty());
  TRI_ASSERT(dependency < _dependencyInfos.size());
  auto& depInfo = _dependencyInfos[dependency];
  TRI_ASSERT(depInfo._upstreamState != ExecutionState::DONE);

  // There are still some blocks left that ask their parent even after they got
  // DONE the last time, and I don't currently have time to track them down.
  // Thus the following assert is commented out.
  // TRI_ASSERT(_upstreamState != ExecutionState::DONE);
  auto res = _dependencyProxy->skipSomeForDependency(dependency, atMost);
  depInfo._upstreamState = res.first;

  return res;
}

std::pair<ExecutionState, ShadowAqlItemRow> MultiDependencySingleRowFetcher::fetchShadowRow(size_t const atMost) {
  // If any dependency is in an invalid state, but not done, refetch it
  for (size_t dependency = 0; dependency < _dependencyInfos.size(); ++dependency) {
    // TODO We should never fetch from upstream here, as we can't know atMost - only the executor does.
    if (!fetchBlockIfNecessary(dependency, atMost)) {
      return {ExecutionState::WAITING, ShadowAqlItemRow{CreateInvalidShadowRowHint{}}};
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    auto const begin = _dependencyInfos.cbegin();
    auto const end = _dependencyInfos.cend();
    // The previous loop assures that all dependencies must either have a valid
    // index, or be both done and have no valid index.
    TRI_ASSERT(std::all_of(begin, end, [this](auto const& dep) {
      return isDone(dep) || indexIsValid(dep);
    }));

    bool const anyDone = std::any_of(begin, end, [this](auto const& dep) {
      return !indexIsValid(dep);
    });
    bool const anyShadow = std::any_of(begin, end, [this](auto const& dep) {
      return indexIsValid(dep) && isAtShadowRow(dep);
    });
    // We must not both have a dependency that's completely done, and a shadow row in another dependency. Otherwise it indicates an error upstream, because each shadow row must have been inserted in all dependencies by the distribute/scatter block.
    TRI_ASSERT(!(anyShadow && anyDone));
  }
#endif

  bool allDone = true;
  bool allShadow = true;
  auto row = ShadowAqlItemRow{CreateInvalidShadowRowHint{}};
  for (auto const& dep : _dependencyInfos) {
    if (!indexIsValid(dep)) {
      allShadow = false;
    } else {
      allDone = false;
      if (!isAtShadowRow(dep)) {
        // We have one dependency that's not done and not a shadow row.
        return {ExecutionState::HASMORE, ShadowAqlItemRow{CreateInvalidShadowRowHint{}}};
      } else if (!row.isInitialized()) {
        // Save the first shadow row we encounter
        row = ShadowAqlItemRow{dep._currentBlock, dep._rowIndex};
      } else {
        TRI_ASSERT(row.isInitialized());
        // All shadow rows must be equal!
        auto const options = _dependencyProxy->velocypackOptions();
        TRI_ASSERT(row.equates(ShadowAqlItemRow{dep._currentBlock, dep._rowIndex}, options));
      }
    }
  }
  // Obviously, at most one of those can be true.
  TRI_ASSERT(!(allDone && allShadow));
  // If we've encountered any shadow row, no dependency may be done.
  // And if we've encountered any non-shadow row, we must have returned in the
  // loop immediately.
  TRI_ASSERT(allShadow == row.isInitialized());

  if (allShadow) {
    TRI_ASSERT(row.isInitialized());
    for (auto& dep : _dependencyInfos) {
      if (isLastRowInBlock(dep) && isDone(dep)) {
        dep._currentBlock = nullptr;
        dep._rowIndex = 0;
      } else {
        ++dep._rowIndex;
      }
    }
  }

  ExecutionState const state = allDone ? ExecutionState::DONE : ExecutionState::HASMORE;

  return {state, row};
}

MultiDependencySingleRowFetcher::MultiDependencySingleRowFetcher()
    : _dependencyProxy(nullptr) {}

RegisterId MultiDependencySingleRowFetcher::getNrInputRegisters() const {
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
}

size_t MultiDependencySingleRowFetcher::numberDependencies() {
  if (_dependencyInfos.empty()) {
    initDependencies();
  }
  return _dependencyInfos.size();
}

std::pair<ExecutionState, size_t> MultiDependencySingleRowFetcher::preFetchNumberOfRows(size_t atMost) {
  ExecutionState state = ExecutionState::DONE;
  size_t available = 0;
  for (size_t i = 0; i < numberDependencies(); ++i) {
    auto res = preFetchNumberOfRowsForDependency(i, atMost);
    if (res.first == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, 0};
    }
    available += res.second;
    if (res.first == ExecutionState::HASMORE) {
      state = ExecutionState::HASMORE;
    }
  }
  return {state, available};
}

std::pair<ExecutionState, InputAqlItemRow> MultiDependencySingleRowFetcher::fetchRowForDependency(
    size_t const dependency, size_t atMost) {
  TRI_ASSERT(dependency < _dependencyInfos.size());
  auto& depInfo = _dependencyInfos[dependency];
  // Fetch a new block iff necessary
  if (!fetchBlockIfNecessary(dependency, atMost)) {
    return {ExecutionState::WAITING, InputAqlItemRow{CreateInvalidInputRowHint{}}};
  }

  TRI_ASSERT(depInfo._currentBlock == nullptr ||
             depInfo._rowIndex < depInfo._currentBlock->size());

  ExecutionState rowState;
  InputAqlItemRow row = InputAqlItemRow{CreateInvalidInputRowHint{}};
  if (depInfo._currentBlock == nullptr) {
    TRI_ASSERT(depInfo._upstreamState == ExecutionState::DONE);
    rowState = ExecutionState::DONE;
  } else if (!isAtShadowRow(depInfo)) {
    TRI_ASSERT(depInfo._currentBlock != nullptr);
    row = InputAqlItemRow{depInfo._currentBlock, depInfo._rowIndex};
    TRI_ASSERT(depInfo._upstreamState != ExecutionState::WAITING);
    ++depInfo._rowIndex;
    if (noMoreDataRows(depInfo)) {
      rowState = ExecutionState::DONE;
    } else {
      rowState = ExecutionState::HASMORE;
    }
    if (isDone(depInfo) && !indexIsValid(depInfo)) {
      // Free block
      depInfo._currentBlock = nullptr;
      depInfo._rowIndex = 0;
    }
  } else {
    ShadowAqlItemRow shadowAqlItemRow{depInfo._currentBlock, depInfo._rowIndex};
    TRI_ASSERT(shadowAqlItemRow.isRelevant());
    rowState = ExecutionState::DONE;
  }

  return {rowState, row};
}

std::pair<ExecutionState, size_t> MultiDependencySingleRowFetcher::skipRowsForDependency(
    size_t const dependency, size_t const atMost) {
  TRI_ASSERT(dependency < _dependencyInfos.size());
  auto& depInfo = _dependencyInfos[dependency];

  TRI_ASSERT((!indexIsValid(depInfo) || !isAtShadowRow(depInfo) ||
              ShadowAqlItemRow{depInfo._currentBlock, depInfo._rowIndex}.isRelevant()));

  size_t skip = 0;
  while (indexIsValid(depInfo) && !isAtShadowRow(depInfo) && skip < atMost) {
    ++skip;
    ++depInfo._rowIndex;
  }

  if (skip > 0) {
    ExecutionState const state =
        noMoreDataRows(depInfo) ? ExecutionState::DONE : ExecutionState::HASMORE;
    return {state, skip};
  }

  TRI_ASSERT(!indexIsValid(depInfo) || isAtShadowRow(depInfo));
  TRI_ASSERT(skip == 0);

  if (noMoreDataRows(depInfo) || isDone(depInfo)) {
    return {ExecutionState::DONE, 0};
  }

  TRI_ASSERT(!indexIsValid(depInfo));
  ExecutionState state;
  size_t skipped;
  std::tie(state, skipped) = skipSomeForDependency(dependency, atMost);
  if (state == ExecutionState::HASMORE && skipped < atMost) {
    state = ExecutionState::DONE;
  }
  return {state, skipped};
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
  return (isDone(info) && !indexIsValid(info)) || (indexIsValid(info) && isAtShadowRow(info));
}

std::pair<ExecutionState, size_t> MultiDependencySingleRowFetcher::preFetchNumberOfRowsForDependency(
    size_t dependency, size_t atMost) {
  TRI_ASSERT(dependency < _dependencyInfos.size());
  auto& depInfo = _dependencyInfos[dependency];
  // Fetch a new block iff necessary
  if (!indexIsValid(depInfo) && !isDone(depInfo)) {
    // This returns the AqlItemBlock to the ItemBlockManager before fetching a
    // new one, so we might reuse it immediately!
    depInfo._currentBlock = nullptr;

    ExecutionState state;
    SharedAqlItemBlockPtr newBlock;
    std::tie(state, newBlock) = fetchBlockForDependency(dependency, atMost);
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, 0};
    }

    depInfo._currentBlock = std::move(newBlock);
    depInfo._rowIndex = 0;
  }

  if (!indexIsValid(depInfo)) {
    TRI_ASSERT(depInfo._upstreamState == ExecutionState::DONE);
    return {ExecutionState::DONE, 0};
  } else {
    if (isDone(depInfo)) {
      TRI_ASSERT(depInfo._currentBlock != nullptr);
      TRI_ASSERT(depInfo._currentBlock->size() > depInfo._rowIndex);
      return {depInfo._upstreamState, depInfo._currentBlock->size() - depInfo._rowIndex};
    }
    // In the HAS_MORE case we do not know exactly how many rows there are.
    // So we need to return an uppter bound (atMost) here.
    return {depInfo._upstreamState, atMost};
  }
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
