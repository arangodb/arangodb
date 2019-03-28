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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "SortExecutor.h"

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
class OurLessThan {
 public:
  OurLessThan(arangodb::transaction::Methods* trx, AqlItemMatrix const& input,
              std::vector<SortRegister> const& sortRegisters) noexcept
      : _trx(trx), _input(input), _sortRegisters(sortRegisters) {}

  bool operator()(size_t const& a, size_t const& b) const {
    InputAqlItemRow left = _input.getRow(a);
    InputAqlItemRow right = _input.getRow(b);
    for (auto const& reg : _sortRegisters) {
      AqlValue const& lhs = left.getValue(reg.reg);
      AqlValue const& rhs = right.getValue(reg.reg);

      int const cmp = AqlValue::Compare(_trx, lhs, rhs, true);

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
  AqlItemMatrix const& _input;
  std::vector<SortRegister> const& _sortRegisters;
};  // OurLessThan

}  // namespace

static std::shared_ptr<std::unordered_set<RegisterId>> mapSortRegistersToRegisterIds(
    std::vector<SortRegister> const& sortRegisters) {
  auto set = make_shared_unordered_set();
  std::transform(sortRegisters.begin(), sortRegisters.end(),
                 std::inserter(*set, set->begin()),
                 [](SortRegister const& sortReg) { return sortReg.reg; });
  return set;
}

SortExecutorInfos::SortExecutorInfos(
    // cppcheck-suppress passedByValue
    std::vector<SortRegister> sortRegisters, std::size_t limit,
    AqlItemBlockManager& manager, RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToClear,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToKeep, transaction::Methods* trx, bool stable)
    : ExecutorInfos(mapSortRegistersToRegisterIds(sortRegisters), nullptr,
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _limit(limit),
      _manager(manager),
      _trx(trx),
      _sortRegisters(std::move(sortRegisters)),
      _stable(stable) {
  TRI_ASSERT(trx != nullptr);
  TRI_ASSERT(!_sortRegisters.empty());
}

transaction::Methods* SortExecutorInfos::trx() const { return _trx; }

std::vector<SortRegister>& SortExecutorInfos::sortRegisters() {
  return _sortRegisters;
}

bool SortExecutorInfos::stable() const { return _stable; }

SortExecutor::SortExecutor(Fetcher& fetcher, SortExecutorInfos& infos)
    : _infos(infos), _fetcher(fetcher), _input(nullptr), _returnNext(0){};
SortExecutor::~SortExecutor() = default;

std::pair<ExecutionState, NoStats> SortExecutor::produceRow(OutputAqlItemRow& output) {
  ExecutionState state;
  if (_input == nullptr) {
    // We need to get data
    std::tie(state, _input) = _fetcher.fetchAllRows();
    if (state == ExecutionState::WAITING) {
      return {state, NoStats{}};
    }
    // If the execution state was not waiting it is guaranteed that we get a
    // matrix. Maybe empty still
    TRI_ASSERT(_input != nullptr);
    if (_input == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    // After allRows the dependency has to be done
    TRI_ASSERT(state == ExecutionState::DONE);

    // Execute the sort
    doSorting();
  }
  // If we get here we have an input matrix
  // And we have a list of sorted indexes.
  TRI_ASSERT(_input != nullptr);
  TRI_ASSERT(_sortedIndexes.size() == _input->size());
  if (_returnNext >= _sortedIndexes.size()) {
    // Bail out if called too often,
    // Bail out on no elements
    return {ExecutionState::DONE, NoStats{}};
  }
  InputAqlItemRow inRow = _input->getRow(_sortedIndexes[_returnNext]);
  output.copyRow(inRow);
  _returnNext++;
  if (_returnNext >= _sortedIndexes.size()) {
    return {ExecutionState::DONE, NoStats{}};
  }
  return {ExecutionState::HASMORE, NoStats{}};
}

void SortExecutor::doSorting() {
  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_ASSERT(_input != nullptr);
  _sortedIndexes.reserve(_input->size());
  for (size_t i = 0; i < _input->size(); ++i) {
    _sortedIndexes.emplace_back(i);
  }
  // comparison function
  OurLessThan ourLessThan(_infos.trx(), *_input, _infos.sortRegisters());
  if (_infos.stable()) {
    std::stable_sort(_sortedIndexes.begin(), _sortedIndexes.end(), ourLessThan);
  } else {
    std::sort(_sortedIndexes.begin(), _sortedIndexes.end(), ourLessThan);
  }
}

std::pair<ExecutionState, size_t> SortExecutor::expectedNumberOfRows(size_t atMost) const {
  if (_input == nullptr) {
    // This executor does not know anything yet.
    // Just take whatever is presented from upstream.
    // This will return WAITING a couple of times
    return _fetcher.preFetchNumberOfRows(atMost);
  }
  TRI_ASSERT(_returnNext <= _sortedIndexes.size());
  size_t rowsLeft = _sortedIndexes.size() - _returnNext;
  if (rowsLeft > 0) {
    return {ExecutionState::HASMORE, rowsLeft};
  }
  return {ExecutionState::DONE, rowsLeft};
}