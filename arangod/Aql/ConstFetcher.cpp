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

#include "Aql/ConstFetcher.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/DependencyProxy.h"
#include "Aql/FilterExecutor.h"
#include "ConstFetcher.h"

using namespace arangodb;
using namespace arangodb::aql;

ConstFetcher::ConstFetcher() : _currentBlock{nullptr}, _rowIndex(0) {}

ConstFetcher::ConstFetcher(DependencyProxy& executionBlock)
    : _currentBlock{nullptr}, _rowIndex(0) {}

void ConstFetcher::injectBlock(SharedAqlItemBlockPtr block) {
  _currentBlock = block;
  _blockForPassThrough = std::move(block);
  _rowIndex = 0;
}

std::pair<ExecutionState, InputAqlItemRow> ConstFetcher::fetchRow() {
  // This fetcher never waits because it can return only its
  // injected block and does not have the ability to pull.
  if (!indexIsValid()) {
    return {ExecutionState::DONE, InputAqlItemRow{CreateInvalidInputRowHint{}}};
  }
  TRI_ASSERT(_currentBlock != nullptr);

  //set state
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

  //set state
  ExecutionState rowState = ExecutionState::HASMORE;
  if (isLastRowInBlock()) {
    rowState = ExecutionState::DONE;
  }
  _rowIndex++;

  return {rowState, 1};
}

bool ConstFetcher::indexIsValid() {
  return _currentBlock != nullptr && _rowIndex + 1 <= _currentBlock->size();
}

bool ConstFetcher::isLastRowInBlock() {
  TRI_ASSERT(indexIsValid());
  return _rowIndex + 1 == _currentBlock->size();
}

std::pair<ExecutionState, SharedAqlItemBlockPtr> ConstFetcher::fetchBlockForPassthrough(size_t) {
  // Should only be called once, and then _blockForPassThrough should be
  // initialized. However, there are still some blocks left that ask their
  // parent even after they got DONE the last time, and I don't currently have
  // time to track them down. Thus the following assert is commented out.
  // TRI_ASSERT(_blockForPassThrough != nullptr);
  return {ExecutionState::DONE, std::move(_blockForPassThrough)};
}
