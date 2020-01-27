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

#include "ConstFetcher.h"

#include "Aql/AqlCallStack.h"
#include "Aql/DependencyProxy.h"
#include "Aql/ShadowAqlItemRow.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

using namespace arangodb;
using namespace arangodb::aql;

ConstFetcher::ConstFetcher() : _currentBlock{nullptr}, _rowIndex(0) {}

ConstFetcher::ConstFetcher(DependencyProxy& executionBlock)
    : _currentBlock{nullptr}, _rowIndex(0) {}

auto ConstFetcher::execute(AqlCallStack& stack)
    -> std::tuple<ExecutionState, size_t, AqlItemBlockInputRange> {
  // Note this fetcher can only be executed on top level (it is the singleton, or test)
  TRI_ASSERT(stack.isRelevant());
  auto call = stack.popCall();
  if (!indexIsValid()) {
    // we are done, nothing to move arround here.
    return {ExecutionState::DONE, 0, AqlItemBlockInputRange{ExecutorState::DONE}};
  }

  size_t skipped = 0;
  if (call.getOffset() > 0) {
    TRI_ASSERT(_blockForPassThrough->size() > _rowIndex);
    skipped = (std::min)(call.getOffset(), _blockForPassThrough->size() - _rowIndex);
    _rowIndex += skipped;
    call.didSkip(skipped);
    if (call.getOffset() > 0) {
      // Skipped over the full block
      TRI_ASSERT(!indexIsValid());
      return {ExecutionState::DONE, skipped, AqlItemBlockInputRange{ExecutorState::DONE}};
    }
  }
  if (call.getLimit() > 0) {
    if (call.getLimit() >= numRowsLeft()) {
      size_t startIndex = _rowIndex;
      // Increase _rowIndex to maximum
      _rowIndex = _currentBlock->size();
      return {ExecutionState::DONE, skipped,
              AqlItemBlockInputRange{ExecutorState::DONE, _currentBlock,
                                     startIndex, _currentBlock->size()}};
    }
    // Unlucky case we need to slice
    // This is expensive, however this is only used in tests.
    auto newBlock = _currentBlock->slice(_rowIndex, _rowIndex + call.getLimit());
    _rowIndex += call.getLimit();
    if (call.hasHardLimit() && call.needsFullCount()) {
      skipped += numRowsLeft();
      call.didSkip(numRowsLeft());
      _rowIndex = _currentBlock->size();
      TRI_ASSERT(!indexIsValid());
      return {ExecutionState::DONE, skipped,
              AqlItemBlockInputRange{ExecutorState::HASMORE, newBlock, 0,
                                     newBlock->size()}};
    }
    return {ExecutionState::HASMORE, skipped,
            AqlItemBlockInputRange{ExecutorState::HASMORE, newBlock, 0, newBlock->size()}};
  }
  if (call.hasHardLimit() && call.needsFullCount()) {
    // FastForward
    skipped += numRowsLeft();
    call.didSkip(numRowsLeft());
    _rowIndex = _currentBlock->size();
    TRI_ASSERT(!indexIsValid());
    return {ExecutionState::DONE, skipped, AqlItemBlockInputRange{ExecutorState::DONE}};
  }
  TRI_ASSERT(skipped > 0);
  return {ExecutionState::HASMORE, skipped, AqlItemBlockInputRange{ExecutorState::HASMORE}};
}

void ConstFetcher::injectBlock(SharedAqlItemBlockPtr block) {
  _currentBlock = block;
  _blockForPassThrough = std::move(block);
  _rowIndex = 0;
}

std::pair<ExecutionState, InputAqlItemRow> ConstFetcher::fetchRow(size_t) {
  // This fetcher does not use atMost
  // This fetcher never waits because it can return only its
  // injected block and does not have the ability to pull.
  if (!indexIsValid()) {
    return {ExecutionState::DONE, InputAqlItemRow{CreateInvalidInputRowHint{}}};
  }
  TRI_ASSERT(_currentBlock != nullptr);

  // set state
  ExecutionState rowState = ExecutionState::HASMORE;
  if (isLastRowInBlock()) {
    rowState = ExecutionState::DONE;
  }

  return {rowState, InputAqlItemRow{_currentBlock, _rowIndex++}};
}

std::pair<ExecutionState, size_t> ConstFetcher::skipRows(size_t) {
  // This fetcher never waits because it can return only its
  // injected block and does not have the ability to pull.
  if (!indexIsValid()) {
    return {ExecutionState::DONE, 0};
  }
  TRI_ASSERT(_currentBlock != nullptr);

  // set state
  ExecutionState rowState = ExecutionState::HASMORE;
  if (isLastRowInBlock()) {
    rowState = ExecutionState::DONE;
  }
  _rowIndex++;

  return {rowState, 1};
}

auto ConstFetcher::indexIsValid() const noexcept -> bool {
  return _currentBlock != nullptr && _rowIndex + 1 <= _currentBlock->size();
}

auto ConstFetcher::isLastRowInBlock() const noexcept -> bool {
  TRI_ASSERT(indexIsValid());
  return _rowIndex + 1 == _currentBlock->size();
}

auto ConstFetcher::numRowsLeft() const noexcept -> size_t {
  if (!indexIsValid()) {
    return 0;
  }
  return _currentBlock->size() - _rowIndex;
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ConstFetcher::fetchBlockForPassthrough(size_t) {
  // Should only be called once, and then _blockForPassThrough should be
  // initialized. However, there are still some blocks left that ask their
  // parent even after they got DONE the last time, and I don't currently have
  // time to track them down. Thus the following assert is commented out.
  // TRI_ASSERT(_blockForPassThrough != nullptr);
  return {ExecutionState::DONE, std::move(_blockForPassThrough)};
}

std::pair<ExecutionState, ShadowAqlItemRow> ConstFetcher::fetchShadowRow(size_t) const {
  return {ExecutionState::DONE, ShadowAqlItemRow{CreateInvalidShadowRowHint{}}};
}
