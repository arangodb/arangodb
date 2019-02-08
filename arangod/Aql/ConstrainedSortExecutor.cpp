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

/// @brief OurLessThan
class ConstrainedLessThan {
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

// class OurLessThan {
// public:
//  OurLessThan(arangodb::transaction::Methods* trx, AqlItemMatrix const& input,
//              std::vector<SortRegister> const& sortRegisters) noexcept
//      : _trx(trx), _input(input), _sortRegisters(sortRegisters) {}
//
//  bool operator()(size_t const& a, size_t const& b) const {
//    InputAqlItemRow left = _input.getRow(a);
//    InputAqlItemRow right = _input.getRow(b);
//    for (auto const& reg : _sortRegisters) {
//      AqlValue const& lhs = left.getValue(reg.reg);
//      AqlValue const& rhs = right.getValue(reg.reg);
//
//#if 0  // #ifdef USE_IRESEARCH
//      TRI_ASSERT(reg.comparator);
//      int const cmp = (*reg.comparator)(reg.scorer.get(), _trx, lhs, rhs);
//#else
//      int const cmp = AqlValue::Compare(_trx, lhs, rhs, true);
//#endif
//
//      if (cmp < 0) {
//        return reg.asc;
//      } else if (cmp > 0) {
//        return !reg.asc;
//      }
//    }
//
//    return false;
//  }
//
// private:
//  arangodb::transaction::Methods* _trx;
//  AqlItemMatrix const& _input;
//  std::vector<SortRegister> const& _sortRegisters;
//};  // OurLessThan

}  // namespace

static std::shared_ptr<std::unordered_set<RegisterId>> mapSortRegistersToRegisterIds(
    std::vector<SortRegister> const& sortRegisters) {
  auto set = make_shared_unordered_set();
  std::transform(sortRegisters.begin(), sortRegisters.end(),
                 std::inserter(*set, set->begin()),
                 [](SortRegister const& sortReg) { return sortReg.reg; });
  return set;
}

ConstrainedSortExecutor::ConstrainedSortExecutor(Fetcher& fetcher, SortExecutorInfos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      //_input(nullptr),
      _returnNext(0),
      _cmpHeap(std::make_unique<ConstrainedLessThan>(_infos.trx(), _infos.sortRegisters())),
      _cmpInput(std::make_unique<ConstrainedLessThan>(_infos.trx(), _infos.sortRegisters())) {
  TRI_ASSERT(_infos._limit > 0);
  _rows.reserve(infos._limit);
};

ConstrainedSortExecutor::~ConstrainedSortExecutor() = default;

std::pair<ExecutionState, NoStats> ConstrainedSortExecutor::produceRow(OutputAqlItemRow& output) {
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

  // get next row from cache
  // output.copyRow() //from heap

  if (true /*cursor expired*/) {
    return {ExecutionState::DONE, NoStats{}};
  } else {
    return {ExecutionState::HASMORE, NoStats{}};
  }
}


// TRI_ASSERT(_sortedIndexes.size() == _input->size());
// if (_returnNext >= _sortedIndexes.size()) {
//   // Bail out if called too often,
//   // Bail out on no elements
//   return {ExecutionState::DONE, NoStats{}};
// }
// InputAqlItemRow inRow = _input->getRow(_sortedIndexes[_returnNext]);
// output.copyRow(inRow);
// _returnNext++;
// if (_returnNext >= _sortedIndexes.size()) {
//   return {ExecutionState::DONE, NoStats{}};
// }
// return {ExecutionState::HASMORE, NoStats{}};
//}

ExecutionState ConstrainedSortExecutor::doSorting() {
  // pull row one by one and add to heap if fitting
  ExecutionState state = ExecutionState::HASMORE;

  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  do {
    InputAqlItemRow input(CreateInvalidInputRowHint{});
    std::tie(state, input) = _fetcher.fetchRow();
    if (state == ExecutionState::HASMORE ||
        (state == ExecutionState::DONE && input.isInitialized())) {
      //there is something to add to the heap
    }

  } while (state == ExecutionState::HASMORE);

  if (state == ExecutionState::DONE) {
    _outputPrepared = true; // we do not need to re-enter this function
  }

  return state;
}
