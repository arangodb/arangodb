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

#include "Basics/Common.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
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
  ConstrainedLessThan(arangodb::transaction::Methods* trx,
                      std::vector<arangodb::aql::SortRegister>& sortRegisters) noexcept
      : _trx(trx), _heapBuffer(nullptr), _sortRegisters(sortRegisters) {}

  void setBuffer(arangodb::aql::AqlItemBlock* heap) { _heapBuffer = heap; }

  bool operator()(size_t const& a, size_t const& b) const {
    TRI_ASSERT(_heapBuffer);

    for (auto const& sortReg : _sortRegisters) {
      auto const& lhs = _heapBuffer->getValueReference(a, sortReg.reg);
      auto const& rhs = _heapBuffer->getValueReference(b, sortReg.reg);

      int const cmp = arangodb::aql::AqlValue::Compare(_trx, lhs, rhs, true);

      if (cmp < 0) {
        return sortReg.asc;
      } else if (cmp > 0) {
        return !sortReg.asc;
      }
    }

    return false;
  }

 private:
  arangodb::transaction::Methods* _trx;
  arangodb::aql::AqlItemBlock* _heapBuffer;
  std::vector<arangodb::aql::SortRegister>& _sortRegisters;
};  // ConstrainedLessThan

arangodb::Result ConstrainedSortExecutor::pushRow(InputAqlItemRow& input) {
  using arangodb::aql::AqlItemBlock;
  using arangodb::aql::AqlValue;
  using arangodb::aql::RegisterId;

  size_t dRow = _rowsPushed;

  if (dRow >= _infos._limit) {
    // pop an entry first
    std::pop_heap(_rows.begin(), _rows.end(), *_cmpHeap.get());
    dRow = _rows.back();
    eraseRow(_heapBuffer, dRow);
  } else {
    _rows.emplace_back(dRow);  // add to heap vector
  }

  TRI_ASSERT(dRow < _infos._limit);
  TRI_IF_FAILURE("SortBlock::doSortingInner") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  _heapOutputRow.setBaseIndex(dRow);
  _heapOutputRow.copyRow(input);
  _heapOutputRow.advanceRow();

  ++_rowsPushed;

  // now restore heap condition
  std::push_heap(_rows.begin(), _rows.end(), *_cmpHeap.get());

  return TRI_ERROR_NO_ERROR;
}

bool ConstrainedSortExecutor::compareInput(size_t const& rowPos, InputAqlItemRow& row) const {
  for (auto const& reg : _infos.sortRegisters()) {
    auto const& lhs = _heapBuffer->getValueReference(rowPos, reg.reg);
    auto const& rhs = row.getValue(reg.reg);

    int const cmp = arangodb::aql::AqlValue::Compare(_infos.trx(), lhs, rhs, true);

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
      _heapBuffer(_infos._manager.requestBlock(_infos._limit,
                                               _infos.numberOfOutputRegisters())),
      _cmpHeap(std::make_unique<ConstrainedLessThan>(_infos.trx(), _infos.sortRegisters())),
      _heapOutputRow{_heapBuffer, make_shared_unordered_set(),
                     make_shared_unordered_set(_infos.numberOfOutputRegisters()),
                     _infos.registersToClear()} {
  TRI_ASSERT(_infos._limit > 0);
  _rows.reserve(infos._limit);
  _cmpHeap->setBuffer(_heapBuffer.get());
}

ConstrainedSortExecutor::~ConstrainedSortExecutor() = default;

bool ConstrainedSortExecutor::doneProducing() const noexcept {
  // must not get strictly larger
  TRI_ASSERT(_returnNext <= _rows.size());
  return _state == ExecutionState::DONE && _returnNext >= _rows.size();
}

bool ConstrainedSortExecutor::doneSkipping() const noexcept {
  // must not get strictly larger
  TRI_ASSERT(_returnNext + _skippedAfter <= _rowsRead);
  return _state == ExecutionState::DONE && _returnNext + _skippedAfter >= _rowsRead;
}

ExecutionState ConstrainedSortExecutor::consumeInput() {
  while (_state != ExecutionState::DONE) {
    TRI_IF_FAILURE("SortBlock::doSorting") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    // We need to pull rows from above, and insert them into the heap
    InputAqlItemRow input(CreateInvalidInputRowHint{});

    std::tie(_state, input) = _fetcher.fetchRow();
    if (_state == ExecutionState::WAITING) {
      return _state;
    }
    if (!input.isInitialized()) {
      TRI_ASSERT(_state == ExecutionState::DONE);
    } else {
      ++_rowsRead;
      if (_rowsPushed < _infos._limit || !compareInput(_rows.front(), input)) {
        // Push this row into the heap
        pushRow(input);
      }
    }
  }

  TRI_ASSERT(_state == ExecutionState::DONE);

  return _state;
}

std::pair<ExecutionState, NoStats> ConstrainedSortExecutor::produceRows(OutputAqlItemRow& output) {
  {
    ExecutionState state = consumeInput();
    TRI_ASSERT(state == _state);
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, NoStats{}};
    }
    TRI_ASSERT(state == ExecutionState::DONE);
  }

  if (doneProducing()) {
    if (doneSkipping()) {
      // No we're really done
      return {ExecutionState::DONE, NoStats{}};
    }
    // We should never get here, as the following LIMIT block should never fetch
    // more than our limit. It may only skip after that.
    // But note that this means that this block breaks with usual AQL behaviour!
    // From this point on (i.e. doneProducing()), this block may only skip, not produce.
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL_AQL,
        "Overfetch during constrained heap sort. Please report this error! Try "
        "turning off the sort-limit optimizer rule to get your query working.");
  }

  if (_returnNext == 0) {
    // Only once sort the rows again, s.t. the
    // contained list of elements is in the right ordering.
    std::sort(_rows.begin(), _rows.end(), *_cmpHeap);
  }

  // Now our heap is full and sorted, we just need to return it line by line
  TRI_ASSERT(_returnNext < _rows.size());
  auto const heapRowPosition = _rows[_returnNext];
  ++_returnNext;
  InputAqlItemRow heapRow(_heapBuffer, heapRowPosition);
  TRI_ASSERT(heapRow.isInitialized());
  TRI_ASSERT(heapRowPosition < _rowsPushed);
  output.copyRow(heapRow);

  // Lie, we may have a possible LIMIT block with fullCount to work.
  // We emitted at least one row at this point, so this is fine.
  return {ExecutionState::HASMORE, NoStats{}};
}

std::pair<ExecutionState, size_t> ConstrainedSortExecutor::expectedNumberOfRows(size_t) const {
  // This block cannot support atMost
  size_t rowsLeft = 0;
  if (_state != ExecutionState::DONE) {
    ExecutionState state;
    size_t expectedRows;
    std::tie(state, expectedRows) =
        _fetcher.preFetchNumberOfRows(ExecutionBlock::DefaultBatchSize());
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(expectedRows == 0);
      return {state, 0};
    }
    // Return the minimum of upstream + limit
    rowsLeft = (std::min)(expectedRows, _infos._limit);
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

std::tuple<ExecutionState, NoStats, size_t> ConstrainedSortExecutor::skipRows(size_t toSkipRequested) {
  {
    ExecutionState state = consumeInput();
    TRI_ASSERT(state == _state);
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, NoStats{}, 0};
    }
    TRI_ASSERT(state == ExecutionState::DONE);
  }

  if (_returnNext == 0) {
    // Only once sort the rows again, s.t. the
    // contained list of elements is in the right ordering.
    std::sort(_rows.begin(), _rows.end(), *_cmpHeap);
  }

  size_t skipped = 0;

  // Skip rows in the heap
  if (!doneProducing()) {
    TRI_ASSERT(_rows.size() >= _returnNext);
    auto const rowsLeftInHeap = _rows.size() - _returnNext;
    auto const skipNum = (std::min)(toSkipRequested, rowsLeftInHeap);
    _returnNext += skipNum;
    skipped += skipNum;
  }

  // Skip rows we've dropped
  if (skipped < toSkipRequested && !doneSkipping()) {
    TRI_ASSERT(doneProducing());
    auto const rowsLeftToSkip = _rowsRead - (_rows.size() + _skippedAfter);
    auto const skipNum = (std::min)(toSkipRequested, rowsLeftToSkip);
    _skippedAfter += skipNum;
    skipped += skipNum;
  }

  TRI_ASSERT(skipped <= toSkipRequested);
  auto const state = doneSkipping() ? ExecutionState::DONE : ExecutionState::HASMORE;

  return {state, NoStats{}, skipped};
}
