////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "SortedRowsStorageBackendMemory.h"

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/Executor/SortExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SortRegister.h"
#include "Basics/Exceptions.h"
#include "Basics/ResourceUsage.h"
#include "Basics/debugging.h"

#include <algorithm>

namespace {
using namespace arangodb;
using namespace arangodb::aql;

// custom AqlValue-aware comparator for sorting
class OurLessThan {
 public:
  OurLessThan(
      velocypack::Options const* options,
      std::vector<SharedAqlItemBlockPtr> const& input,  // from _inputBlocks
      std::vector<SortRegister> const&
          sortRegisters) noexcept  // from _infos.sortRegisters()
      : _vpackOptions(options), _input(input), _sortRegisters(sortRegisters) {}

  bool operator()(SortedRowsStorageBackendMemory::RowIndex const& a,
                  SortedRowsStorageBackendMemory::RowIndex const& b) const {
    auto const& left = _input[a.first].get();   // row
    auto const& right = _input[b.first].get();  // row
    for (auto const& reg : _sortRegisters) {
      AqlValue const& lhs = left->getValueReference(
          a.second, reg.reg);  // value of row at register
      AqlValue const& rhs = right->getValueReference(
          b.second, reg.reg);  // value of row at register
      int const cmp = AqlValue::Compare(_vpackOptions, lhs, rhs, true);

      if (cmp < 0) {
        return reg.asc;
      } else if (cmp > 0) {
        return !reg.asc;
      }
    }

    return false;
  }

 private:
  velocypack::Options const* _vpackOptions;
  std::vector<SharedAqlItemBlockPtr> const& _input;
  std::vector<SortRegister> const& _sortRegisters;
};  // OurLessThan

}  // namespace

namespace arangodb::aql {

SortedRowsStorageBackendMemory::SortedRowsStorageBackendMemory(
    SortExecutorInfos& infos)
    : _infos(infos), _returnNext(0), _memoryUsageForInputBlocks(0) {
  _infos.getResourceMonitor().increaseMemoryUsage(currentMemoryUsage());
}

SortedRowsStorageBackendMemory::~SortedRowsStorageBackendMemory() {
  _infos.getResourceMonitor().decreaseMemoryUsage(currentMemoryUsage());
}

GroupedValues SortedRowsStorageBackendMemory::groupedValuesForRow(
    RowIndex const& rowId) {
  auto row = _inputBlocks[rowId.first].get();
  std::vector<AqlValue> groupedValues;
  for (auto const& registerId : _infos.groupedRegisters()) {
    groupedValues.push_back(row->getValueReference(rowId.second, registerId));
  }
  return GroupedValues{_infos.vpackOptions(), groupedValues};
}

void SortedRowsStorageBackendMemory::consumeInputRange(
    AqlItemBlockInputRange& inputRange) {
  if (inputRange.upstreamState() == ExecutorState::DONE) {
    // TODO only do this when _current is not empty
    startNewGroup({});
    return;
  }
  auto firstRow = false;

  auto inputBlock = inputRange.getBlock();
  if (inputBlock != nullptr &&
      (_inputBlocks.empty() || _inputBlocks.back() != inputBlock)) {
    if (_inputBlocks.empty()) {
      firstRow = true;
    }
    _inputBlocks.emplace_back(inputBlock);
    _memoryUsageForInputBlocks += inputBlock->getMemoryUsage();
  }

  ResourceUsageScope guard(_infos.getResourceMonitor());

  size_t numDataRows = inputRange.countDataRows();
  if (_currentGroup.capacity() < _currentGroup.size() + numDataRows) {
    size_t newCapacity = _currentGroup.size() + numDataRows;

    // may throw
    guard.increase((newCapacity - _currentGroup.capacity()) * sizeof(RowIndex));

    _currentGroup.reserve(newCapacity);
  }

  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  // make sure that previous group was already fully consumed
  TRI_ASSERT(!hasMore());

  while (inputRange.hasDataRow()) {
    auto rowId =
        std::make_pair(static_cast<std::uint32_t>(_inputBlocks.size() - 1),
                       static_cast<std::uint32_t>(inputRange.getRowIndex()));

    auto grouped_values = groupedValuesForRow(rowId);

    if (grouped_values != _groupedValuesOfPreviousRow && !firstRow) {
      // finish group and return
      startNewGroup({rowId});
      _groupedValuesOfPreviousRow = grouped_values;
      auto [state, input] =
          inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
      TRI_ASSERT(input.isInitialized());
      guard.steal();
      return;
    }

    firstRow = false;
    _groupedValuesOfPreviousRow = grouped_values;
    _currentGroup.emplace_back(rowId);

    auto [state, input] =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());

    if (inputRange.upstreamState() == ExecutorState::DONE) {
      startNewGroup({});
      guard.steal();
      return;
    }
  }

  guard.steal();
}

void SortedRowsStorageBackendMemory::startNewGroup(
    std::vector<RowIndex>&& newGroup) {
  ResourceUsageScope guard(_infos.getResourceMonitor());
  // we overwrite finished group, therefore decrease by its memory usage (before
  // updating it)
  _infos.getResourceMonitor().decreaseMemoryUsage(_finishedGroup.capacity() *
                                                  sizeof(RowIndex));
  _finishedGroup = std::move(_currentGroup);
  sortFinishedGroup();

  _infos.getResourceMonitor().increaseMemoryUsage(newGroup.capacity() *
                                                  sizeof(RowIndex));
  _currentGroup = std::move(newGroup);
  _returnNext = 0;
}

bool SortedRowsStorageBackendMemory::hasReachedCapacityLimit() const noexcept {
  return ((_currentGroup.size() + _finishedGroup.size()) >
              _infos.spillOverThresholdNumRows() ||
          _memoryUsageForInputBlocks > _infos.spillOverThresholdMemoryUsage());
}

bool SortedRowsStorageBackendMemory::hasMore() const {
  return _returnNext < _finishedGroup.size();
}

void SortedRowsStorageBackendMemory::produceOutputRow(
    OutputAqlItemRow& output) {
  TRI_ASSERT(hasMore());
  InputAqlItemRow inRow(_inputBlocks[_finishedGroup[_returnNext].first],
                        _finishedGroup[_returnNext].second);
  output.copyRow(inRow);
  output.advanceRow();
  ++_returnNext;
}

void SortedRowsStorageBackendMemory::skipOutputRow() noexcept {
  TRI_ASSERT(hasMore());
  ++_returnNext;
}

bool SortedRowsStorageBackendMemory::spillOver(
    SortedRowsStorageBackend& other) {
  // if we have a grouped sort, we could have already produced rows and / or
  // have additional (sorted) data in _finishedGroup instead of only in
  // _currentGroup
  if (_infos.groupedRegisters().size() > 0) {
    return false;
  }

  if (_currentGroup.empty()) {
    return true;
  }

  std::uint32_t lastBlockId = _currentGroup[0].first;
  std::uint32_t startRow = _currentGroup[0].second;

  for (size_t i = 0; i < _currentGroup.size(); ++i) {
    // if row i starts new block or is the very last row
    if (_currentGroup[i].first != lastBlockId ||
        i == _currentGroup.size() - 1) {
      // emit block from lastBlockId to startRow, endRow
      aql::AqlItemBlockInputRange inputRange(
          aql::MainQueryState::HASMORE, 0, _inputBlocks[lastBlockId], startRow);

      // TODO make sure the full inputRange is consumed
      other.consumeInputRange(inputRange);

      lastBlockId = _currentGroup[i].first;
      startRow = _currentGroup[i].second;
    }
  }

  // reset our own state, so we can give back memory
  _infos.getResourceMonitor().decreaseMemoryUsage(currentMemoryUsage());
  _inputBlocks = {};
  _currentGroup.clear();
  _currentGroup.shrink_to_fit();
  TRI_ASSERT(currentMemoryUsage() == 0);
  _memoryUsageForInputBlocks = 0;
  return true;
}

void SortedRowsStorageBackendMemory::sortFinishedGroup() {
  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // comparison function
  OurLessThan ourLessThan(_infos.vpackOptions(), _inputBlocks,
                          _infos.sortRegisters());
  if (_infos.stable()) {
    std::stable_sort(_finishedGroup.begin(), _finishedGroup.end(), ourLessThan);
  } else {
    std::sort(_finishedGroup.begin(), _finishedGroup.end(), ourLessThan);
  }
}

size_t SortedRowsStorageBackendMemory::currentMemoryUsage() const noexcept {
  return (_currentGroup.capacity() + _finishedGroup.capacity()) *
         sizeof(RowIndex);
}

}  // namespace arangodb::aql
