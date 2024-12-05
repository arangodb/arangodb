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
    : _infos(infos), _returnNext(0), _memoryUsageForInputBlocks(0) {}

SortedRowsStorageBackendMemory::~SortedRowsStorageBackendMemory() {
  _infos.getResourceMonitor().decreaseMemoryUsage(currentMemoryUsage());
}

GroupedValues SortedRowsStorageBackendMemory::groupedValuesForRow(
    RowIndex const& rowId) {
  auto row = _inputBlocks[rowId.first].get();
  std::vector<AqlValue> groupedValues;
  size_t count = 0;
  for (auto const& reg : _infos.sortRegisters()) {
    if (count >= _infos.numberOfTopGroupedElements()) {
      break;
    }
    groupedValues.push_back(row->getValueReference(rowId.second, reg.reg));
    count++;
  }
  return GroupedValues{_infos.vpackOptions(), groupedValues};
}

ExecutorState SortedRowsStorageBackendMemory::consumeInputRange(
    AqlItemBlockInputRange& inputRange) {
  ExecutorState state = ExecutorState::HASMORE;
  if (inputRange.upstreamState() == ExecutorState::DONE) {
    return ExecutorState::DONE;
  }

  auto inputBlock = inputRange.getBlock();
  if (inputBlock != nullptr &&
      (_inputBlocks.empty() || _inputBlocks.back() != inputBlock)) {
    _inputBlocks.emplace_back(inputBlock);
    _memoryUsageForInputBlocks += inputBlock->getMemoryUsage();
  }

  ResourceUsageScope guard(_infos.getResourceMonitor());

  size_t numDataRows = inputRange.countDataRows();
  if (_currentGroup.capacity() < _currentGroup.size() + numDataRows) {
    size_t newCapacity = _currentGroup.size() + numDataRows;

    // may throw
    guard.increase((newCapacity - _currentGroup.capacity()) *
                   sizeof(SortedRowsStorageBackendMemory::RowIndex));

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

    if (grouped_values == _groupedValuesOfPreviousRow) {
      _currentGroup.emplace_back(rowId);

      std::tie(state, input) =
          inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
      TRI_ASSERT(input.isInitialized());

      // if this was the last inputRange
      if (state == ExecutorState::DONE) {
        updateFinishedGroup();
        guard.steal();
        return state;
      }

    } else {
      // finish group and return
      updateFinishedGroup();
      guard.decrease((_currentGroup.capacity() - 1) * sizeof(RowIndex));
      _currentGroup = std::vector<RowIndex>{rowId};
      _groupedValuesOfPreviousRow = grouped_values;
      std::tie(state, input) =
          inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
      TRI_ASSERT(input.isInitialized());
      guard.steal();
      return state;
    }
  }

  guard.steal();

  return state;
}

void SortedRowsStorageBackendMemory::updateFinishedGroup() {
  ResourceUsageScope guard(_infos.getResourceMonitor());
  _infos.getResourceMonitor().decreaseMemoryUsage(_finishedGroup.capacity() *
                                                  sizeof(RowIndex));
  _finishedGroup = std::move(_currentGroup);
  sortFinishedGroup();
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

void SortedRowsStorageBackendMemory::seal() {}

void SortedRowsStorageBackendMemory::spillOver(
    SortedRowsStorageBackend& other) {
  // TODO
  TRI_ASSERT(false);
  // if (_rowIndexes.empty()) {
  //   return;
  // }

  // std::uint32_t lastBlockId = _rowIndexes[0].first;
  // std::uint32_t startRow = _rowIndexes[0].second;

  // for (size_t i = 0; i < _rowIndexes.size(); ++i) {
  //   // if row i starts new block or is the very last row
  //   if (_rowIndexes[i].first != lastBlockId || i == _rowIndexes.size() - 1) {
  //     // emit block from lastBlockId to startRow, endRow
  //     aql::AqlItemBlockInputRange inputRange(
  //         aql::MainQueryState::HASMORE, 0, _inputBlocks[lastBlockId],
  //         startRow);

  //     // TODO make sure the full inputRange is consumed
  //     other.consumeInputRange(inputRange);

  //     lastBlockId = _rowIndexes[i].first;
  //     startRow = _rowIndexes[i].second;
  //   }
  // }

  // // reset our own state, so we can give back memory
  // _infos.getResourceMonitor().decreaseMemoryUsage(currentMemoryUsage());
  // _inputBlocks = {};
  // _rowIndexes.clear();
  // _rowIndexes.shrink_to_fit();
  // TRI_ASSERT(currentMemoryUsage() == 0);
  // _memoryUsageForInputBlocks = 0;
}

void SortedRowsStorageBackendMemory::sortFinishedGroup() {
  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // comparison function
  OurLessThan ourLessThan(_infos.vpackOptions(), _inputBlocks,
                          _infos.sortRegisters());
  if (_infos.stable()) {
    // TODO need to sort only by non-grouped registers
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
