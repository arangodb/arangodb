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
#include "Aql/AqlItemBlockShell.h"
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

void eraseRow(AqlItemBlockShell& shell, size_t row) {
  arangodb::aql::RegisterId const nrRegs = shell.block().getNrRegs();
  for (size_t i = 0; i < nrRegs; i++) {
    shell.block().destroyValue(row, i);
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

  bool operator()(uint32_t const& a, uint32_t const& b) const {
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
    eraseRow(*_heapBuffer, dRow);
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

bool ConstrainedSortExecutor::compareInput(uint32_t const& rowPos, InputAqlItemRow& row) const {
  for (auto const& reg : _infos.sortRegisters()) {
    auto const& lhs = _heapBuffer->block().getValueReference(rowPos, reg.reg);
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
      _heapBuffer(std::make_shared<AqlItemBlockShell>(
          _infos._manager, std::unique_ptr<AqlItemBlock>(_infos._manager.requestBlock(
                               _infos._limit, _infos.numberOfOutputRegisters())))),
      _cmpHeap(std::make_unique<ConstrainedLessThan>(_infos.trx(), _infos.sortRegisters())),
      _heapOutputRow{_heapBuffer, make_shared_unordered_set(),
                     make_shared_unordered_set(_infos.numberOfOutputRegisters()),
                     _infos.registersToClear()} {
  TRI_ASSERT(_infos._limit > 0);
  _rows.reserve(infos._limit);
  _cmpHeap->setBuffer(&_heapBuffer->block());
};

ConstrainedSortExecutor::~ConstrainedSortExecutor() = default;

std::pair<ExecutionState, NoStats> ConstrainedSortExecutor::produceRow(OutputAqlItemRow& output) {
  while (_state != ExecutionState::DONE) {
    TRI_IF_FAILURE("SortBlock::doSorting") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    // We need to pull rows from above, and insert them into the heap
    InputAqlItemRow input(CreateInvalidInputRowHint{});

    std::tie(_state, input) = _fetcher.fetchRow();
    if (_state == ExecutionState::WAITING) {
      return {_state, NoStats{}};
    }
    if (!input.isInitialized()) {
      TRI_ASSERT(_state == ExecutionState::DONE);
    } else {
      if (_rowsPushed < _infos._limit || !compareInput(_rows.front(), input)) {
        // Push this row into the heap
        pushRow(input);
      }
    }
  }
  if (_returnNext >= _rows.size()) {
    // Happens if, we either have no upstream e.g. _rows is empty
    // Or if dependency is pulling too often (should not happen)
    return {ExecutionState::DONE, NoStats{}};
  }
  if (_returnNext == 0) {
    // Only once sort the rows again, s.t. the
    // contained list of elements is in the right ordering.
    std::sort(_rows.begin(), _rows.end(), *_cmpHeap);
  }
  // Now our heap is full and sorted, we just need to return it line by line
  TRI_ASSERT(_returnNext < _rows.size());
  std::size_t heapRowPosition = _rows[_returnNext++];
  InputAqlItemRow heapRow(_heapBuffer, heapRowPosition);
  TRI_ASSERT(heapRow.isInitialized());
  TRI_ASSERT(heapRowPosition < _rowsPushed);
  output.copyRow(heapRow);
  if (_returnNext == _rows.size()) {
    return {ExecutionState::DONE, NoStats{}};
  }
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
  if (rowsLeft > 0) {
    return {ExecutionState::HASMORE, rowsLeft};
  }
  return {ExecutionState::DONE, rowsLeft};
}
