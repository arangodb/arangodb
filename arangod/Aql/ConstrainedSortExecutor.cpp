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
/// @author Daniel Larkin
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "ConstrainedSortExecutor.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortExecutor.h"
#include "Aql/SortRegister.h"
#include "Aql/Stats.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

void eraseRow(SharedAqlItemBlockPtr& block, size_t row) {
  arangodb::aql::RegisterId const nrRegs = block->getNrRegs();
  for (arangodb::aql::RegisterId i = 0; i < nrRegs; i++) {
    block->destroyValue(row, i);
  }
}

}  // namespace

/// @brief OurLessThan
class arangodb::aql::ConstrainedLessThan {
 public:
  ConstrainedLessThan(velocypack::Options const* options,
                      std::vector<arangodb::aql::SortRegister> const& sortRegisters) noexcept
      : _vpackOptions(options), _heapBuffer(nullptr), _sortRegisters(sortRegisters) {}

  void setBuffer(arangodb::aql::AqlItemBlock* heap) { _heapBuffer = heap; }

  bool operator()(size_t const& a, size_t const& b) const {
    TRI_ASSERT(_heapBuffer);

    for (auto const& sortReg : _sortRegisters) {
      auto const& lhs = _heapBuffer->getValueReference(a, sortReg.reg);
      auto const& rhs = _heapBuffer->getValueReference(b, sortReg.reg);

      int const cmp = arangodb::aql::AqlValue::Compare(_vpackOptions, lhs, rhs, true);

      if (cmp < 0) {
        return sortReg.asc;
      } else if (cmp > 0) {
        return !sortReg.asc;
      }
    }

    return false;
  }

 private:
  velocypack::Options const* const _vpackOptions;
  arangodb::aql::AqlItemBlock* _heapBuffer;
  std::vector<arangodb::aql::SortRegister> const& _sortRegisters;
};  // ConstrainedLessThan

arangodb::Result ConstrainedSortExecutor::pushRow(InputAqlItemRow const& input) {
  using arangodb::aql::AqlItemBlock;
  using arangodb::aql::AqlValue;
  using arangodb::aql::RegisterId;

  size_t dRow = _rowsPushed;

  if (dRow >= _infos.limit()) {
    // pop an entry first
    std::pop_heap(_rows.begin(), _rows.end(), *_cmpHeap);
    dRow = _rows.back();
    eraseRow(_heapBuffer, dRow);
  } else {
    _rows.emplace_back(dRow);  // add to heap vector
  }

  TRI_ASSERT(dRow < _infos.limit());
  TRI_IF_FAILURE("SortBlock::doSortingInner") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  _heapOutputRow.setBaseIndex(dRow);
  _heapOutputRow.copyRow(input);
  _heapOutputRow.advanceRow();

  ++_rowsPushed;

  // now restore heap condition
  std::push_heap(_rows.begin(), _rows.end(), *_cmpHeap);

  return TRI_ERROR_NO_ERROR;
}

bool ConstrainedSortExecutor::compareInput(size_t const& rowPos,
                                           InputAqlItemRow const& row) const {
  for (auto const& reg : _infos.sortRegisters()) {
    auto const& lhs = _heapBuffer->getValueReference(rowPos, reg.reg);
    auto const& rhs = row.getValue(reg.reg);

    int const cmp =
        arangodb::aql::AqlValue::Compare(_infos.vpackOptions(), lhs, rhs, true);

    if (cmp < 0) {
      return reg.asc;
    } else if (cmp > 0) {
      return !reg.asc;
    }
  }
  return false;
}

ConstrainedSortExecutor::ConstrainedSortExecutor(Fetcher& fetcher, SortExecutorInfos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _state(ExecutionState::HASMORE),
      _returnNext(0),
      _rowsPushed(0),
      _rowsRead(0),
      _skippedAfter(0),
      _heapBuffer(_infos.itemBlockManager().requestBlock(_infos.limit(),
                                                         _infos.numberOfOutputRegisters())),
      _cmpHeap(std::make_unique<ConstrainedLessThan>(_infos.vpackOptions(),
                                                     _infos.sortRegisters())),
      _heapOutputRow{_heapBuffer, std::vector<RegisterId>(),
                     ExecutorInfos::makeRegisterVector(_infos.numberOfOutputRegisters()),
                     _infos.registersToClear()} {
  TRI_ASSERT(_infos.limit() > 0);
  _rows.reserve(infos.limit());
  _cmpHeap->setBuffer(_heapBuffer.get());
}

ConstrainedSortExecutor::~ConstrainedSortExecutor() = default;

bool ConstrainedSortExecutor::doneProducing() const noexcept {
  // must not get strictly larger
  TRI_ASSERT(_returnNext <= _rows.size());
  return _returnNext >= _rows.size();
}

bool ConstrainedSortExecutor::doneSkipping() const noexcept {
  // must not get strictly larger
  TRI_ASSERT(_returnNext + _skippedAfter <= _rowsRead);
  return _returnNext + _skippedAfter >= _rowsRead;
}

ExecutorState ConstrainedSortExecutor::consumeInput(AqlItemBlockInputRange& inputRange) {
  while (inputRange.hasDataRow()) {
    TRI_IF_FAILURE("SortBlock::doSorting") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    auto const& [state, input] = inputRange.nextDataRow();
    // Otherwise we would have left the loop
    TRI_ASSERT(input.isInitialized());
    ++_rowsRead;
    if (_rowsPushed < _infos.limit() || !compareInput(_rows.front(), input)) {
      // Push this row into the heap
      pushRow(input);
    }
  }
  if (inputRange.upstreamState() == ExecutorState::DONE) {
    if (_returnNext == 0) {
      // Only once sort the rows again, s.t. the
      // contained list of elements is in the right ordering.
      std::sort(_rows.begin(), _rows.end(), *_cmpHeap);
    }
  }
  return inputRange.upstreamState();
}

auto ConstrainedSortExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  if (consumeInput(input) == ExecutorState::HASMORE) {
    // Input could not be fully consumed, executor is more hungry!
    // Get more.
    AqlCall upstreamCall{};
    // We need to fetch everything form upstream.
    // Unlimited, no offset call.
    return {ExecutorState::HASMORE, NoStats{}, upstreamCall};
  };

  while (!output.isFull() && !doneProducing()) {
    // Now our heap is full and sorted, we just need to return it line by line
    TRI_ASSERT(_returnNext < _rows.size());
    auto const heapRowPosition = _rows[_returnNext];
    ++_returnNext;
    InputAqlItemRow heapRow(_heapBuffer, heapRowPosition);
    TRI_ASSERT(heapRow.isInitialized());
    TRI_ASSERT(heapRowPosition < _rowsPushed);
    output.copyRow(heapRow);
    output.advanceRow();
  }
  if (doneProducing()) {
    return {ExecutorState::DONE, NoStats{}, AqlCall{}};
  }
  return {ExecutorState::HASMORE, NoStats{}, AqlCall{}};
}

auto ConstrainedSortExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  if (consumeInput(inputRange) == ExecutorState::HASMORE) {
    // Input could not be fully consumed, executor is more hungry!
    // Get more.
    AqlCall upstreamCall{};
    // We need to fetch everything form upstream.
    // Unlimited, no offset call.
    return {ExecutorState::HASMORE, NoStats{}, 0, upstreamCall};
  };

  while (!doneProducing()) {
    if (call.getOffset() > 0) {
      size_t available = _rows.size() - _returnNext;
      size_t toSkip = std::min(available, call.getOffset());
      _returnNext += toSkip;
      call.didSkip(toSkip);
    } else if (call.needSkipMore()) {
      // We are in fullcount case, simply skip all!
      // I think this is actually invalid, as it would case LIMIT
      // to underfetch.
      // However will work like this.
      size_t available = _rows.size() - _returnNext;
      call.didSkip(available);
      _returnNext = _rows.size();
    } else {
      // We still have something, but cannot continue to skip.
      return {ExecutorState::HASMORE, NoStats{}, call.getSkipCount(), AqlCall{}};
    }
  }

  while (call.needSkipMore() && !doneSkipping()) {
    // unlikely, but for backwardscompatibility.
    if (call.getOffset() > 0) {
      auto const rowsLeftToSkip = _rowsRead - (_rows.size() + _skippedAfter);
      auto const skipNum = (std::min)(call.getOffset(), rowsLeftToSkip);
      call.didSkip(skipNum);
      _skippedAfter += skipNum;
    } else {
      // Fullcount
      auto const rowsLeftToSkip = _rowsRead - (_rows.size() + _skippedAfter);
      call.didSkip(rowsLeftToSkip);
      _skippedAfter += rowsLeftToSkip;
      TRI_ASSERT(doneSkipping());
    }
  }

  auto const state = doneSkipping() ? ExecutorState::DONE : ExecutorState::HASMORE;

  return {state, NoStats{}, call.getSkipCount(), AqlCall{}};
}

std::pair<ExecutionState, size_t> ConstrainedSortExecutor::expectedNumberOfRows(size_t) const {
  // This block cannot support atMost
  size_t rowsLeft = 0;
  if (_state != ExecutionState::DONE) {
    ExecutionState state;
    size_t expectedRows;
    std::tie(state, expectedRows) =
        _fetcher.preFetchNumberOfRows(ExecutionBlock::DefaultBatchSize);
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(expectedRows == 0);
      return {state, 0};
    }
    // Return the minimum of upstream + limit
    rowsLeft = (std::min)(expectedRows, _infos.limit());
  } else {
    // We have exactly the following rows available:
    rowsLeft = _rows.size() - _returnNext;
  }

  if (rowsLeft == 0) {
    if (doneSkipping()) {
      return {ExecutionState::DONE, rowsLeft};
    }
    // We always report at least 1 row here, for a possible LIMIT block with fullCount to work.
    // However, we should never have to do this if the LIMIT block doesn't overfetch with getSome.
    rowsLeft = 1;
  }

  return {ExecutionState::HASMORE, rowsLeft};
}
