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

#include "SortingGatherExecutor.h"

#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SortRegister.h"
#include "Aql/Stats.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {

/// @brief OurLessThan: comparison method for elements of SortingGatherBlock
class OurLessThan {
 public:
  OurLessThan(arangodb::transaction::Methods* trx, std::vector<SortRegister>& sortRegisters) noexcept
      : _trx(trx), _sortRegisters(sortRegisters) {}

  bool operator()(SortingGatherExecutor::ValueType const& a,
                  SortingGatherExecutor::ValueType const& b) const {
    // nothing in the buffer is maximum!
    if (!a.row) {
      return false;
    }

    if (!b.row) {
      return true;
    }

    TRI_ASSERT(a.row);
    TRI_ASSERT(b.row);

    for (auto const& reg : _sortRegisters) {
      auto const& lhs = a.row.getValue(reg.reg);
      auto const& rhs = b.row.getValue(reg.reg);
      auto const& attributePath = reg.attributePath;

      // Fast path if there is no attributePath:
      int cmp;

      if (attributePath.empty()) {
        cmp = AqlValue::Compare(_trx, lhs, rhs, true);
      } else {
        // Take attributePath into consideration:
        bool mustDestroyA;
        auto resolver = _trx->resolver();
        TRI_ASSERT(resolver != nullptr);
        AqlValue aa = lhs.get(*resolver, attributePath, mustDestroyA, false);
        AqlValueGuard guardA(aa, mustDestroyA);
        bool mustDestroyB;
        AqlValue bb = rhs.get(*resolver, attributePath, mustDestroyB, false);
        AqlValueGuard guardB(bb, mustDestroyB);
        cmp = AqlValue::Compare(_trx, aa, bb, true);
      }

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
  std::vector<SortRegister>& _sortRegisters;
};  // OurLessThan

////////////////////////////////////////////////////////////////////////////////
/// @class HeapSorting
/// @brief "Heap" sorting strategy
////////////////////////////////////////////////////////////////////////////////
class HeapSorting final : public SortingGatherExecutor::SortingStrategy, private OurLessThan {
 public:
  HeapSorting(arangodb::transaction::Methods* trx, std::vector<SortRegister>& sortRegisters) noexcept
      : OurLessThan(trx, sortRegisters) {}

  virtual SortingGatherExecutor::ValueType nextValue() override {
    TRI_ASSERT(!_heap.empty());
    std::push_heap(_heap.begin(), _heap.end(), *this);  // re-insert element
    std::pop_heap(_heap.begin(), _heap.end(),
                  *this);  // remove element from _heap but not from vector
    return _heap.back();
  }

  virtual void prepare(std::vector<SortingGatherExecutor::ValueType>& blockPos) override {
    TRI_ASSERT(!blockPos.empty());

    if (_heap.size() == blockPos.size()) {
      return;
    }

    _heap.clear();
    std::copy(blockPos.begin(), blockPos.end(), std::back_inserter(_heap));
    std::make_heap(_heap.begin(), _heap.end() - 1,
                   *this);  // remain last element out of heap to maintain invariant
    TRI_ASSERT(!_heap.empty());
  }

  virtual void reset() noexcept override { _heap.clear(); }

  bool operator()(SortingGatherExecutor::ValueType const& lhs,
                  SortingGatherExecutor::ValueType const& rhs) const {
    return OurLessThan::operator()(rhs, lhs);
  }

 private:
  std::vector<std::reference_wrapper<SortingGatherExecutor::ValueType>> _heap;
};  // HeapSorting

////////////////////////////////////////////////////////////////////////////////
/// @class MinElementSorting
/// @brief "MinElement" sorting strategy
////////////////////////////////////////////////////////////////////////////////
class MinElementSorting final : public SortingGatherExecutor::SortingStrategy,
                                public OurLessThan {
 public:
  MinElementSorting(arangodb::transaction::Methods* trx,
                    std::vector<SortRegister>& sortRegisters) noexcept
      : OurLessThan(trx, sortRegisters), _blockPos(nullptr) {}

  virtual SortingGatherExecutor::ValueType nextValue() override {
    TRI_ASSERT(_blockPos);
    return *(std::min_element(_blockPos->begin(), _blockPos->end(), *this));
  }

  virtual void prepare(std::vector<SortingGatherExecutor::ValueType>& blockPos) override {
    _blockPos = &blockPos;
  }

  virtual void reset() noexcept override { _blockPos = nullptr; }

 private:
  std::vector<SortingGatherExecutor::ValueType> const* _blockPos;
};

}  // namespace
SortingGatherExecutor::ValueType::ValueType(size_t index)
    : dependencyIndex{index}, row{CreateInvalidInputRowHint()}, state{ExecutionState::HASMORE} {}

SortingGatherExecutorInfos::SortingGatherExecutorInfos(
    std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters, RegisterId nrInputRegisters,
    RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep, std::vector<SortRegister>&& sortRegister,
    arangodb::transaction::Methods* trx, GatherNode::SortMode sortMode)
    : ExecutorInfos(std::move(inputRegisters), std::move(outputRegisters),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _sortRegister(std::move(sortRegister)),
      _trx(trx),
      _sortMode(sortMode) {}

SortingGatherExecutorInfos::SortingGatherExecutorInfos(SortingGatherExecutorInfos&&) = default;
SortingGatherExecutorInfos::~SortingGatherExecutorInfos() = default;

SortingGatherExecutor::SortingGatherExecutor(Fetcher& fetcher, Infos& infos)
    : _fetcher(fetcher),
      _initialized(false),
      _numberDependencies(0),
      _dependencyToFetch(0),
      _nrDone() {
  switch (infos.sortMode()) {
    case GatherNode::SortMode::MinElement:
      _strategy = std::make_unique<MinElementSorting>(infos.trx(), infos.sortRegister());
      break;
    case GatherNode::SortMode::Heap:
    case GatherNode::SortMode::Default:  // use heap by default
      _strategy = std::make_unique<HeapSorting>(infos.trx(), infos.sortRegister());
      break;
    default:
      TRI_ASSERT(false);
      break;
  }
  TRI_ASSERT(_strategy);
}

SortingGatherExecutor::~SortingGatherExecutor() = default;

////////////////////////////////////////////////////////////////////////////////
/// @brief Guarantees requiredby this this block:
///        1) For every dependency the input is sorted, according to the same strategy.
///
///        What this block does:
///        InitPhase:
///          Fetch 1 Block for every dependency.
///        ExecPhase:
///          Fetch row of scheduled block.
///          Pick the next (sorted) element (by strategy)
///          Schedule this block to fetch Row
///
////////////////////////////////////////////////////////////////////////////////

std::pair<ExecutionState, NoStats> SortingGatherExecutor::produceRows(OutputAqlItemRow& output) {
  TRI_ASSERT(_strategy != nullptr);
  if (!_initialized) {
    ExecutionState state = init();
    if (state != ExecutionState::HASMORE) {
      // Can be DONE(unlikely, no input) of WAITING
      return {state, NoStats{}};
    }
  } else {
    // Activate this assert as soon as all blocks follow the done == no call api
    // TRI_ASSERT(_nrDone < _numberDependencies);
    if (_inputRows[_dependencyToFetch].state == ExecutionState::DONE) {
      _inputRows[_dependencyToFetch].row = InputAqlItemRow{CreateInvalidInputRowHint()};
    } else {
      // This is executed on every produceRows, and will replace the row that we have returned last time
      std::tie(_inputRows[_dependencyToFetch].state,
               _inputRows[_dependencyToFetch].row) =
          _fetcher.fetchRowForDependency(_dependencyToFetch);
      if (_inputRows[_dependencyToFetch].state == ExecutionState::WAITING) {
        return {ExecutionState::WAITING, NoStats{}};
      }
      if (!_inputRows[_dependencyToFetch].row) {
        TRI_ASSERT(_inputRows[_dependencyToFetch].state == ExecutionState::DONE);
        adjustNrDone(_dependencyToFetch);
      }
    }
  }
  if (_nrDone >= _numberDependencies) {
    // We cannot return a row, because all are done
    return {ExecutionState::DONE, NoStats{}};
  }
// if we get here, we have a valid row for every not done dependency.
// And we have atLeast 1 valid row left
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool oneWithContent = false;
  for (auto const& inPair : _inputRows) {
    // Waiting needs to bail out at fetch state
    TRI_ASSERT(inPair.state != ExecutionState::WAITING);
    // row.invalid => dependency is done
    TRI_ASSERT(inPair.row || inPair.state == ExecutionState::DONE);
    if (inPair.row) {
      oneWithContent = true;
    }
  }
  // We have at least one row to sort.
  TRI_ASSERT(oneWithContent);
#endif
  // get the index of the next best value.
  ValueType val = _strategy->nextValue();
  _dependencyToFetch = val.dependencyIndex;
  // We can never pick an invalid row!
  TRI_ASSERT(val.row);
  // NOTE: The original gatherBlock did referencing
  // inside the outputblock by identical AQL values.
  // This optimization is not in use anymore.
  output.copyRow(val.row);
  adjustNrDone(_dependencyToFetch);
  if (_nrDone >= _numberDependencies) {
    return {ExecutionState::DONE, NoStats{}};
  }
  return {ExecutionState::HASMORE, NoStats{}};
}

void SortingGatherExecutor::adjustNrDone(size_t dependency) {
  auto const& dep = _inputRows[dependency];
  if (dep.state == ExecutionState::DONE) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_flaggedAsDone[dependency] == false);
    _flaggedAsDone[dependency] = true;
#endif
    ++_nrDone;
  }
}

ExecutionState SortingGatherExecutor::init() {
  if (_numberDependencies == 0) {
    // We need to initialize the dependencies once, they are injected
    // after the fetcher is created.
    _numberDependencies = _fetcher.numberDependencies();
    TRI_ASSERT(_numberDependencies > 0);
    _inputRows.reserve(_numberDependencies);
    for (size_t index = 0; index < _numberDependencies; ++index) {
      _inputRows.emplace_back(ValueType{index});
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _flaggedAsDone.emplace_back(false);
#endif
    }
  }

  while (_dependencyToFetch < _numberDependencies) {
    std::tie(_inputRows[_dependencyToFetch].state,
             _inputRows[_dependencyToFetch].row) =
        _fetcher.fetchRowForDependency(_dependencyToFetch);
    if (_inputRows[_dependencyToFetch].state == ExecutionState::WAITING) {
      return ExecutionState::WAITING;
    }
    if (!_inputRows[_dependencyToFetch].row) {
      TRI_ASSERT(_inputRows[_dependencyToFetch].state == ExecutionState::DONE);
      adjustNrDone(_dependencyToFetch);
    }
    ++_dependencyToFetch;
  }
  _initialized = true;
  if (_nrDone >= _numberDependencies) {
    return ExecutionState::DONE;
  }
  _strategy->prepare(_inputRows);
  return ExecutionState::HASMORE;
}

std::pair<ExecutionState, size_t> SortingGatherExecutor::expectedNumberOfRows(size_t atMost) const {
  ExecutionState state;
  size_t expectedNumberOfRows;
  std::tie(state, expectedNumberOfRows) = _fetcher.preFetchNumberOfRows(atMost);
  if (state == ExecutionState::WAITING) {
    return {state, 0};
  }
  if (expectedNumberOfRows >= atMost) {
    // We do not care, we have more than atMost anyways.
    return {state, expectedNumberOfRows};
  }
  // Now we need to figure out a more precise state
  for (auto const& inRow : _inputRows) {
    if (inRow.state == ExecutionState::HASMORE) {
      // This block is not fully fetched, we do NOT know how many rows
      // will be in the next batch, overestimate!
      return {ExecutionState::HASMORE, atMost};
    }
    if (inRow.row.isInitialized()) {
      // This dependency is in owned by this Executor
      expectedNumberOfRows++;
    }
  }
  if (expectedNumberOfRows == 0) {
    return {ExecutionState::DONE, 0};
  }
  return {ExecutionState::HASMORE, expectedNumberOfRows};
}
