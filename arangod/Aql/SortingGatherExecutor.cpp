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

SortingGatherExecutor::ValueType::ValueType(size_t index, InputAqlItemRow prow,
                                            ExecutionState pstate)
    : dependencyIndex{index}, row{prow}, state{pstate} {}

SortingGatherExecutorInfos::SortingGatherExecutorInfos(
    std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters, RegisterId nrInputRegisters,
    RegisterId nrOutputRegisters, std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep,
    std::vector<SortRegister>&& sortRegister, arangodb::transaction::Methods* trx,
    GatherNode::SortMode sortMode, size_t limit, GatherNode::Parallelism p)
    : ExecutorInfos(std::move(inputRegisters), std::move(outputRegisters),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _sortRegister(std::move(sortRegister)),
      _trx(trx),
      _sortMode(sortMode),
      _parallelism(p),
      _limit(limit) {}

SortingGatherExecutorInfos::SortingGatherExecutorInfos(SortingGatherExecutorInfos&&) = default;
SortingGatherExecutorInfos::~SortingGatherExecutorInfos() = default;

SortingGatherExecutor::SortingGatherExecutor(Fetcher& fetcher, Infos& infos)
    : _fetcher(fetcher),
      _initialized(false),
      _numberDependencies(0),
      _dependencyToFetch(0),
      _inputRows(),
      _nrDone(0),
      _limit(infos.limit()),
      _rowsReturned(0),
      _heapCounted(false),
      _rowsLeftInHeap(0),
      _skipped(0),
      _strategy(nullptr),
      _fetchParallel(infos.parallelism() == GatherNode::Parallelism::Parallel) {
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
  size_t const atMost = constrainedSort() ? output.numRowsLeft()
                                          : ExecutionBlock::DefaultBatchSize;
  ExecutionState state;
  InputAqlItemRow row{CreateInvalidInputRowHint{}};
  std::tie(state, row) = produceNextRow(atMost);

  // HASMORE => row has to be initialized
  TRI_ASSERT(state != ExecutionState::HASMORE || row.isInitialized());
  // WAITING => row may not be initialized
  TRI_ASSERT(state != ExecutionState::WAITING || !row.isInitialized());

  if (row) {
    // NOTE: The original gatherBlock did referencing
    // inside the outputblock by identical AQL values.
    // This optimization is not in use anymore.
    output.copyRow(row);
  }

  return {state, NoStats{}};
}

auto SortingGatherExecutor::initialize(typename Fetcher::DataRange const& inputRange)
    -> std::optional<std::tuple<AqlCall, size_t>> {
  if (!_initialized) {
    // We cannot modify the number of dependencies, so we start
    // with 0 dependencies, and will increase to whatever inputRange gives us.
    TRI_ASSERT(_numberDependencies == 0 ||
               _numberDependencies == inputRange.numberDependencies());
    _numberDependencies = inputRange.numberDependencies();
    auto call = requiresMoreInput(inputRange);
    if (call.has_value) {
      return call;
    }
    // If we have collected all ranges once, we can prepare the local data-structure copy
    _inputRows.reserve(_numberDependencies);
    for (size_t dep = 0; dep < _numberDependencies; ++dep) {
      auto const [state, row] = inputRange.peekDataRow(dep);
      _inputRows.emplace_back(dep, row, state);
    }
    _strategy->prepare(_inputRows);
    _initialized = true;
    _numberDependencies = inputRange.numberDependencies();
  }
}

auto SortingGatherExecutor::requiresMoreInput(typename Fetcher::DataRange const& inputRange)
    -> std::optional<std::tuple<AqlCall, size_t>> {
  for (size_t dep = 0; dep < _numberDependencies; ++dep) {
    auto const& [state, input] = inputRange.peekDataRow(dep);
    // Update the local copy, just to be sure it is up to date
    // We might do too many copies here, but most likely this
    // will not be a performance bottleneck.
    ValueType& localDep = _inputRows[dep];
    localDep.row = input;
    localDep.state = state;
    if (!input && state != ExecutorState::DONE) {
      // This dependency requires input
      // TODO: This call requires limits
      return std::tuple<AqlCall, size_t>{AqlCall{}, dep};
    }
  }
  // No call required
  return {};
}

auto SortingGatherExecutor::isDone(typename Fetcher::DataRange const& input) const -> bool {
  // TODO: Include contrained sort
  return input.isDone();
}

auto SortingGatherExecutor::nextRow(MultiAqlItemBlockInputRange& input) -> InputAqlItemRow {
  if (isDone(input)) {
    // No rows, there is a chance we get into this.
    // If we requested data from upstream, but all if it is done.
    return InputAqlItemRow{CreateInvalidInputRowHint{}};
  }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool oneWithContent = false;
  for (size_t dep = 0; dep < _numberDependencies; ++dep) {
    auto const& [state, row] = input.peekDataRow(i);
    if (row) {
      oneWithContent = true;
    }
  }
  TRI_ASSERT(oneWithContent);
#endif
  auto nextVal = _strategy->nextValue();
  _rowsReturned++;
  {
    // Consume the row, and set it to next input
    std::ignore = input.nextDataRow(nextVal.dependencyIndex);
    auto const& [state, row] = input.peekDataRow(nextVal.dependencyIndex);
    _inputRows[nextVal.dependencyIndex].state = state;
    _inputRows[nextVal.dependencyIndex].row = row;

    // TODO we might do some short-cuts here to maintain a list of requests
    // to send in order to improve requires input
  }

  return nextVal.row;
}

auto SortingGatherExecutor::produceRows(typename Fetcher::DataRange& input,
                                        OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall, size_t> {
  while (!isDone(input) && !output.isFull()) {
    TRI_ASSERT(!maySkip());
    auto maybeCall = requiresMoreInput(input);
    if (maybeCall.has_value()) {
      auto const& [request, dep] = maybeCall.value();
      return {ExecutorState::HASMORE, NoStats{}, request, dep};
    }
    auto row = nextRow(input);
    TRI_ASSERT(row.isInitialized() || isDone(input));
    if (row) {
      output.copyRow(row);
      output.advanceRow();
    }
  }

  // Call and dependency unused, so we return a too large dependency
  // in order to trigger asserts if it is used.
  if (isDone(input)) {
    return {ExecutorState::DONE, NoStats{}, AqlCall{}, _numberDependencies + 1};
  }
  return {ExecutorState::HASMORE, NoStats{}, AqlCall{}, _numberDependencies + 1};
}

auto SortingGatherExecutor::skipRowsRange(typename Fetcher::DataRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall, size_t> {
  while (!isDone(input) && call.needSkipMore()) {
    auto maybeCall = requiresMoreInput(input);
    if (maybeCall.has_value()) {
      auto const& [request, dep] = maybeCall.value();
      return {ExecutorState::HASMORE, NoStats{}, call.getSkipCount(), request, dep};
    }
    if (call.getOffset() > 0) {
      TRI_ASSERT(!maySkip());
      // We need to sort still
      // And account the row in the limit
      auto row = nextRow(input);
      TRI_ASSERT(row.isInitialized() || isDone(input));
      if (row) {
        call.didSkip(1);
      }
    } else {
      // We are only called with fullcount.
      // sorting does not matter.
      // Start simply skip all from upstream.
      for (size_t dep = 0; dep < input.numberDependencies(); ++dep) {
        ExecutorState state = ExecutorState::HASMORE;
        InputAqlItemRow row{CreateInvalidInputRowHint{}};
        while (state == ExecutorState::HASMORE) {
          std::tie(state, row) = input.nextDataRow(dep);
          if (row) {
            call.didSkip(1);
          } else {
            // We have consumed all overfetched rows.
            // We may still have a skip counter within the range.
            call.didSkip(input.skipAll(dep));
            if (state == ExecutorState::HASMORE) {
              // We need to fetch more data, but can fullCount now
              AqlCall request{0, true, 0, AqlCall::LimitType::HARD};
              return {ExecutorState::HASMORE, NoStats{}, call.getSkipCount(), request, dep};
            }
          }
        }
      }
    }
  }

  // Call and dependency unused, so we return a too large dependency
  // in order to trigger asserts if it is used.
  if (isDone(input)) {
    return {ExecutorState::DONE, NoStats{}, call.getSkipCount(), AqlCall{},
            _numberDependencies + 1};
  }
  return {ExecutorState::HASMORE, NoStats{}, call.getSkipCount(), AqlCall{},
          _numberDependencies + 1};
}

std::pair<ExecutionState, InputAqlItemRow> SortingGatherExecutor::produceNextRow(size_t const atMost) {
  TRI_ASSERT(_strategy != nullptr);
  assertConstrainedDoesntOverfetch(atMost);
  // We shouldn't be asked for more rows when we are allowed to skip
  TRI_ASSERT(!maySkip());
  if (!_initialized) {
    ExecutionState state = init(atMost);
    if (state != ExecutionState::HASMORE) {
      // Can be DONE(unlikely, no input) of WAITING
      return {state, InputAqlItemRow{CreateInvalidInputRowHint{}}};
    }
  } else {
    // Activate this assert as soon as all blocks follow the done == no call
    // api TRI_ASSERT(_nrDone < _numberDependencies);
    if (_inputRows[_dependencyToFetch].state == ExecutionState::DONE) {
      _inputRows[_dependencyToFetch].row = InputAqlItemRow{CreateInvalidInputRowHint()};
    } else {
      // This is executed on every produceRows, and will replace the row that we have returned last time
      std::tie(_inputRows[_dependencyToFetch].state,
               _inputRows[_dependencyToFetch].row) =
          _fetcher.fetchRowForDependency(_dependencyToFetch, atMost);
      if (_inputRows[_dependencyToFetch].state == ExecutionState::WAITING) {
        return {ExecutionState::WAITING, InputAqlItemRow{CreateInvalidInputRowHint{}}};
      }
      if (!_inputRows[_dependencyToFetch].row) {
        TRI_ASSERT(_inputRows[_dependencyToFetch].state == ExecutionState::DONE);
        adjustNrDone(_dependencyToFetch);
      }
    }
  }
  if (_nrDone >= _numberDependencies) {
    // We cannot return a row, because all are done
    return {ExecutionState::DONE, InputAqlItemRow{CreateInvalidInputRowHint{}}};
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
  ++_rowsReturned;
  adjustNrDone(_dependencyToFetch);
  if (_nrDone >= _numberDependencies) {
    return {ExecutionState::DONE, val.row};
  }
  return {ExecutionState::HASMORE, val.row};
}

void SortingGatherExecutor::adjustNrDone(size_t const dependency) {
  auto const& dep = _inputRows[dependency];
  if (dep.state == ExecutionState::DONE) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_flaggedAsDone[dependency] == false);
    _flaggedAsDone[dependency] = true;
#endif
    ++_nrDone;
  }
}

void SortingGatherExecutor::initNumDepsIfNecessary() {
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
}

ExecutionState SortingGatherExecutor::init(size_t const atMost) {
  assertConstrainedDoesntOverfetch(atMost);
  initNumDepsIfNecessary();

  size_t numWaiting = 0;
  for (size_t i = 0; i < _numberDependencies; i++) {
    if (_inputRows[i].state == ExecutionState::DONE || _inputRows[i].row) {
      continue;
    }

    std::tie(_inputRows[i].state, _inputRows[i].row) =
        _fetcher.fetchRowForDependency(i, atMost);
    if (_inputRows[i].state == ExecutionState::WAITING) {
      if (!_fetchParallel) {
        return ExecutionState::WAITING;
      }
      numWaiting++;
    } else if (!_inputRows[i].row) {
      TRI_ASSERT(_inputRows[i].state == ExecutionState::DONE);
      adjustNrDone(i);
    }
  }

  if (numWaiting > 0) {
    return ExecutionState::WAITING;
  }

  TRI_ASSERT(_numberDependencies > 0);
  _dependencyToFetch = _numberDependencies - 1;
  _initialized = true;
  if (_nrDone >= _numberDependencies) {
    return ExecutionState::DONE;
  }
  _strategy->prepare(_inputRows);
  return ExecutionState::HASMORE;
}

std::pair<ExecutionState, size_t> SortingGatherExecutor::expectedNumberOfRows(size_t const atMost) const {
  assertConstrainedDoesntOverfetch(atMost);
  // We shouldn't be asked for more rows when we are allowed to skip
  TRI_ASSERT(!maySkip());
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

size_t SortingGatherExecutor::rowsLeftToWrite() const noexcept {
  TRI_ASSERT(constrainedSort());
  TRI_ASSERT(_limit >= _rowsReturned);
  return _limit - _rowsReturned;
}

bool SortingGatherExecutor::constrainedSort() const noexcept {
  return _limit > 0;
}

void SortingGatherExecutor::assertConstrainedDoesntOverfetch(size_t const atMost) const noexcept {
  // if we have a constrained sort, we should not be asked for more rows than
  // our limit.
  TRI_ASSERT(!constrainedSort() || atMost <= rowsLeftToWrite());
}

bool SortingGatherExecutor::maySkip() const noexcept {
  TRI_ASSERT(!constrainedSort() || _rowsReturned <= _limit);
  return constrainedSort() && _rowsReturned >= _limit;
}

std::tuple<ExecutionState, SortingGatherExecutor::Stats, size_t> SortingGatherExecutor::skipRows(
    size_t const atMost) {
  if (!maySkip()) {
    // Until our limit, we must produce rows, because we might be asked later
    // to produce rows, in which case all rows have to have been skipped in
    // order.
    return produceAndSkipRows(atMost);
  } else {
    // If we've reached our limit, we will never be asked to produce rows
    // again. So we can just skip without sorting.
    return reallySkipRows(atMost);
  }
}

std::tuple<ExecutionState, SortingGatherExecutor::Stats, size_t> SortingGatherExecutor::reallySkipRows(
    size_t const atMost) {
  // Once, count all rows that are left in the heap (and free them)
  if (!_heapCounted) {
    initNumDepsIfNecessary();

    // This row was just fetched:
    _inputRows[_dependencyToFetch].row = InputAqlItemRow{CreateInvalidInputRowHint{}};
    _rowsLeftInHeap = 0;
    for (auto& it : _inputRows) {
      if (it.row) {
        ++_rowsLeftInHeap;
        it.row = InputAqlItemRow{CreateInvalidInputRowHint{}};
      }
    }
    _heapCounted = true;

    // Now we will just skip through all dependencies, starting with the first.
    _dependencyToFetch = 0;
  }

  {  // Skip rows we had left in the heap first
    std::size_t const skip = std::min(atMost, _rowsLeftInHeap);
    _rowsLeftInHeap -= skip;
    _skipped += skip;
  }

  while (_dependencyToFetch < _numberDependencies && _skipped < atMost) {
    auto& state = _inputRows[_dependencyToFetch].state;
    while (state != ExecutionState::DONE && _skipped < atMost) {
      std::size_t skippedNow;
      std::tie(state, skippedNow) =
          _fetcher.skipRowsForDependency(_dependencyToFetch, atMost - _skipped);
      if (state == ExecutionState::WAITING) {
        TRI_ASSERT(skippedNow == 0);
        return {state, NoStats{}, 0};
      }
      _skipped += skippedNow;
    }
    if (state == ExecutionState::DONE) {
      ++_dependencyToFetch;
    }
  }

  // Skip dependencies which are DONE
  while (_dependencyToFetch < _numberDependencies &&
         _inputRows[_dependencyToFetch].state == ExecutionState::DONE) {
    ++_dependencyToFetch;
  }
  // The current dependency must now neither be DONE, nor WAITING.
  TRI_ASSERT(_dependencyToFetch >= _numberDependencies ||
             _inputRows[_dependencyToFetch].state == ExecutionState::HASMORE);

  ExecutionState const state = _dependencyToFetch < _numberDependencies
                                   ? ExecutionState::HASMORE
                                   : ExecutionState::DONE;

  TRI_ASSERT(_skipped <= atMost);
  std::size_t const skipped = _skipped;
  _skipped = 0;
  return {state, NoStats{}, skipped};
}

std::tuple<ExecutionState, SortingGatherExecutor::Stats, size_t> SortingGatherExecutor::produceAndSkipRows(
    size_t const atMost) {
  ExecutionState state = ExecutionState::HASMORE;
  InputAqlItemRow row{CreateInvalidInputRowHint{}};

  // We may not skip more rows in this method than we can produce!
  auto const ourAtMost = constrainedSort() ? std::min(atMost, rowsLeftToWrite()) : atMost;

  while (state == ExecutionState::HASMORE && _skipped < ourAtMost) {
    std::tie(state, row) = produceNextRow(ourAtMost - _skipped);
    // HASMORE => row has to be initialized
    TRI_ASSERT(state != ExecutionState::HASMORE || row.isInitialized());
    // WAITING => row may not be initialized
    TRI_ASSERT(state != ExecutionState::WAITING || !row.isInitialized());

    if (row.isInitialized()) {
      ++_skipped;
    }
  }

  if (state == ExecutionState::WAITING) {
    return {state, NoStats{}, 0};
  }

  // Note that _skipped *can* be larger than `ourAtMost`, due to WAITING, in
  // which case we might get a lower `ourAtMost` on the second call than
  // during the first.
  TRI_ASSERT(_skipped <= atMost);
  TRI_ASSERT(state != ExecutionState::HASMORE || _skipped > 0);
  TRI_ASSERT(state != ExecutionState::WAITING || _skipped == 0);

  std::size_t const skipped = _skipped;
  _skipped = 0;
  return {state, NoStats{}, skipped};
}
