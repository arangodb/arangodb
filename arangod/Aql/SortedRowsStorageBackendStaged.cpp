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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "SortedRowsStorageBackendStaged.h"

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Exceptions.h"

#include <algorithm>

using namespace arangodb;

namespace arangodb::aql {

SortedRowsStorageBackendStaged::SortedRowsStorageBackendStaged(
    std::unique_ptr<SortedRowsStorageBackend> backend1,
    std::unique_ptr<SortedRowsStorageBackend> backend2)
    : _currentBackend(0) {
  _backends.reserve(2);
  _backends.emplace_back(std::move(backend1));
  _backends.emplace_back(std::move(backend2));
}

SortedRowsStorageBackendStaged::~SortedRowsStorageBackendStaged() = default;

ExecutorState SortedRowsStorageBackendStaged::consumeInputRange(
    AqlItemBlockInputRange& inputRange) {
  if (_backends[_currentBackend]->hasReachedCapacityLimit()) {
    if (_currentBackend >= _backends.size() + 1) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_RESOURCE_LIMIT,
          "reached capacity limit for storing intermediate results");
    }

    _backends[_currentBackend]->spillOver(*_backends[_currentBackend + 1]);
    ++_currentBackend;
  }

  ExecutorState state =
      _backends[_currentBackend]->consumeInputRange(inputRange);

  return state;
}

bool SortedRowsStorageBackendStaged::hasReachedCapacityLimit() const noexcept {
  return _backends[_currentBackend]->hasReachedCapacityLimit();
}

bool SortedRowsStorageBackendStaged::hasMore() const {
  return _backends[_currentBackend]->hasMore();
}

void SortedRowsStorageBackendStaged::produceOutputRow(
    OutputAqlItemRow& output) {
  _backends[_currentBackend]->produceOutputRow(output);
}

void SortedRowsStorageBackendStaged::skipOutputRow() noexcept {
  _backends[_currentBackend]->skipOutputRow();
}

void SortedRowsStorageBackendStaged::seal() {
  _backends[_currentBackend]->seal();
}

void SortedRowsStorageBackendStaged::spillOver(
    SortedRowsStorageBackend& other) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "unexpected call to SortedRowsStorageBackendStaged::spillOver");
}

}  // namespace arangodb::aql
