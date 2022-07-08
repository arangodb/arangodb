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

#include "SortedRowsStorageBackendMemory.h"

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SortExecutor.h"
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
  OurLessThan(velocypack::Options const* options,
              std::vector<SharedAqlItemBlockPtr> const& input,
              std::vector<SortRegister> const& sortRegisters) noexcept
      : _vpackOptions(options), _input(input), _sortRegisters(sortRegisters) {}

  bool operator()(AqlItemMatrix::RowIndex const& a,
                  AqlItemMatrix::RowIndex const& b) const {
    auto const& left = _input[a.first].get();
    auto const& right = _input[b.first].get();
    for (auto const& reg : _sortRegisters) {
      AqlValue const& lhs = left->getValueReference(a.second, reg.reg);
      AqlValue const& rhs = right->getValueReference(b.second, reg.reg);
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
    : _infos(infos),
      _memoryUsageForRowIndexes(0),
      _returnNext(0),
      _sealed(false) {}

SortedRowsStorageBackendMemory::~SortedRowsStorageBackendMemory() {
  _infos.getResourceMonitor().decreaseMemoryUsage(_memoryUsageForRowIndexes);
}

ExecutorState SortedRowsStorageBackendMemory::consumeInputRange(
    AqlItemBlockInputRange& inputRange) {
  TRI_ASSERT(!_sealed);

  ExecutorState state = ExecutorState::HASMORE;

  auto inputBlock = inputRange.getBlock();
  if (inputBlock != nullptr) {
    _inputBlocks.emplace_back(inputBlock);
  }
  size_t numDataRows = inputRange.countDataRows();
  size_t memoryUsageForRowIndexes =
      numDataRows * sizeof(AqlItemMatrix::RowIndex);

  ResourceUsageScope guard(_infos.getResourceMonitor(),
                           memoryUsageForRowIndexes);

  _rowIndexes.reserve(_rowIndexes.size() + numDataRows);

  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  while (inputRange.hasDataRow()) {
    // This executor is passthrough. it has enough place to write.
    _rowIndexes.emplace_back(
        std::make_pair(static_cast<std::uint32_t>(_inputBlocks.size() - 1),
                       static_cast<std::uint32_t>(inputRange.getRowIndex())));
    std::tie(state, input) =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());
  }

  guard.steal();
  _memoryUsageForRowIndexes += memoryUsageForRowIndexes;

  return state;
}

bool SortedRowsStorageBackendMemory::hasMore() const {
  TRI_ASSERT(_sealed);
  return _returnNext < _rowIndexes.size();
}

void SortedRowsStorageBackendMemory::produceOutputRow(
    OutputAqlItemRow& output) {
  TRI_ASSERT(hasMore());
  InputAqlItemRow inRow(_inputBlocks[_rowIndexes[_returnNext].first],
                        _rowIndexes[_returnNext].second);
  output.copyRow(inRow);
  output.advanceRow();
  ++_returnNext;
}

void SortedRowsStorageBackendMemory::skipOutputRow() noexcept {
  TRI_ASSERT(hasMore());
  ++_returnNext;
}

void SortedRowsStorageBackendMemory::seal() {
  TRI_ASSERT(!_sealed);
  doSorting();
  _sealed = true;
}

void SortedRowsStorageBackendMemory::doSorting() {
  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // comparison function
  OurLessThan ourLessThan(_infos.vpackOptions(), _inputBlocks,
                          _infos.sortRegisters());
  if (_infos.stable()) {
    std::stable_sort(_rowIndexes.begin(), _rowIndexes.end(), ourLessThan);
  } else {
    std::sort(_rowIndexes.begin(), _rowIndexes.end(), ourLessThan);
  }
}

}  // namespace arangodb::aql
