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

//dirty trick to access private member is standard conform way
template <typename Tag, typename Tag::type member>
struct createAccess {
  friend typename Tag::type get(Tag) { return member; } // ADL function get in namespace
};

struct RowTag {
      using type =  size_t OutputAqlItemRow::*;
      friend type get(RowTag); // finds the get functions via ADL
};

template struct createAccess<RowTag, &OutputAqlItemRow::_baseIndex>; // in template instantiation not access check is done

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
      : _trx(trx), _lhsBuffer(nullptr), _rhsBuffer(nullptr), _sortRegisters(sortRegisters) {}

  void setBuffers(arangodb::aql::AqlItemBlock* lhsBuffer,
                  arangodb::aql::AqlItemBlock* rhsBuffer) {
    _lhsBuffer = lhsBuffer;
    _rhsBuffer = rhsBuffer;
  }

  bool operator()(uint32_t const& a, uint32_t const& b) const {
    TRI_ASSERT(_lhsBuffer);
    TRI_ASSERT(_rhsBuffer);

    for (auto const& reg : _sortRegisters) {
      auto const& lhs = _lhsBuffer->getValueReference(a, reg.reg);
      auto const& rhs = _rhsBuffer->getValueReference(b, reg.reg);

      int const cmp = arangodb::aql::AqlValue::Compare(_trx, lhs, rhs, true);

      if (cmp < 0) {
        return reg.asc;
      } else if (cmp > 0) {
        return !reg.asc;
      }
    }

    return false;
  }

 private:
  arangodb::transaction::Methods* _trx;
  arangodb::aql::AqlItemBlock* _lhsBuffer;
  arangodb::aql::AqlItemBlock* _rhsBuffer;
  std::vector<arangodb::aql::SortRegister>& _sortRegisters;
};  // ConstrainedLessThan

arangodb::Result ConstrainedSortExecutor::pushRow(InputAqlItemRow& input) {
  LOG_DEVEL << "enter push row";
  using arangodb::aql::AqlItemBlock;
  using arangodb::aql::AqlValue;
  using arangodb::aql::RegisterId;

  size_t dRow = _rowsPushed;

  if (_rowsPushed >= _infos._limit) {
    // pop an entry first
    std::pop_heap(_rows.begin(), _rows.end(), *_cmpHeap.get());
    dRow = _rows.back();
     eraseRow(*_heapBuffer, dRow);
    _rows.pop_back();
  }
  TRI_ASSERT(dRow < _infos._limit);

  TRI_IF_FAILURE("SortBlock::doSortingInner") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  _heapOutPutRow.*get(RowTag()) = dRow;
  _heapOutPutRow.copyRow(input, true);
  _heapOutPutRow.advanceRow();

  _rows.emplace_back(dRow); // add to heap vector
  LOG_DEVEL << "increase rows pushed";
  ++_rowsPushed;

  // now restore heap condition
  std::push_heap(_rows.begin(), _rows.end(), *_cmpHeap.get());
  LOG_DEVEL << "exit push row" << _rowsPushed;

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
      _returnNext(0),
      _outputPrepared(false),
      _rowsPushed(0),
      _heapBuffer(std::make_shared<AqlItemBlockShell>(_infos._manager, std::unique_ptr<AqlItemBlock>(_infos._manager.requestBlock(_infos._limit, _infos.numberOfOutputRegisters())))),
      _cmpHeap(std::make_unique<ConstrainedLessThan>(_infos.trx(), _infos.sortRegisters())),
      _heapOutPutRow{_heapBuffer, infos.getOutputRegisters(), infos.registersToKeep()},
      _done(false)
  {
  TRI_ASSERT(_infos._limit > 0);
  _rows.reserve(infos._limit);
  _cmpHeap->setBuffers(&_heapBuffer->block(), &_heapBuffer->block());
  LOG_DEVEL << "####################### CREATED EXECUTOR ################################";
};

ConstrainedSortExecutor::~ConstrainedSortExecutor() = default;

std::pair<ExecutionState, NoStats> ConstrainedSortExecutor::produceRow(OutputAqlItemRow& output) {
  if(_done){
    return {ExecutionState::DONE, NoStats{}};
  }

  ExecutionState state = ExecutionState::HASMORE;

  if (!_outputPrepared) {
    // build-up heap by pulling form all rows in the singlerow fetcher
    state = doSorting();
  }

  if (state != ExecutionState::HASMORE) {
    TRI_ASSERT(state != ExecutionState::WAITING || state == ExecutionState::DONE);
    // assert heap empty
    return {state, NoStats{}};
  }

  TRI_ASSERT( state == ExecutionState::HASMORE && _outputPrepared);

  std::size_t heapRowPosition = _rows[_returnNext++];
  LOG_DEVEL << "heapRow: " << heapRowPosition
            << " next: " << _returnNext
            << " limit: " << _infos._limit
            << " rows->size(): " << _rows.size()
            << " pushed " << _rowsPushed
              ;

  if (_returnNext > _rows.size()) {
    LOG_DEVEL << "DONE";
    _done = true;
    return {ExecutionState::DONE, NoStats{}};
  } else {
    InputAqlItemRow heapRow(_heapBuffer, heapRowPosition);
    TRI_ASSERT(heapRowPosition < _rowsPushed);
    output.copyRow(heapRow);
    return {ExecutionState::HASMORE, NoStats{}};
  }
}

ExecutionState ConstrainedSortExecutor::doSorting() {
  // pull row one by one and add to heap if fitting
  ExecutionState state = ExecutionState::HASMORE;

  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  do {
    InputAqlItemRow input(CreateInvalidInputRowHint{});
    std::tie(state, input) = _fetcher.fetchRow();

    if (input.isInitialized() &&
        ( state == ExecutionState::HASMORE || state == ExecutionState::DONE )
       ) {
      TRI_ASSERT(input.isInitialized());
      if (_rowsPushed >= _infos._limit && compareInput(_rows.front(), input)) {
        LOG_DEVEL << "skipping because of compare -- limit " << _infos._limit << " - pushed; " << _rowsPushed  ;
        // skip row, already too low in sort order to make it past limit
        continue;
      }
      // there is something to add to the heap
      LOG_DEVEL << "pushing " << _rowsPushed;
      pushRow(input);
    }

  } while (state == ExecutionState::HASMORE);

  if (state == ExecutionState::DONE) {
    _outputPrepared = true;  // we do not need to re-enter this function
    if (_rowsPushed){
      std::sort(_rows.begin(), _rows.end(), *_cmpHeap);
      return ExecutionState::HASMORE;
    }
    return ExecutionState::DONE;
  }

  TRI_ASSERT(state == ExecutionState::WAITING);

  return state;
}
