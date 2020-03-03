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
    : dependencyIndex{index}, row{CreateInvalidInputRowHint()}, state{ExecutorState::HASMORE} {}

SortingGatherExecutor::ValueType::ValueType(size_t index, InputAqlItemRow prow, ExecutorState pstate)
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
    : _initialized(false),
      _numberDependencies(0),
      _inputRows(),
      _limit(infos.limit()),
      _rowsReturned(0),
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

auto SortingGatherExecutor::initialize(typename Fetcher::DataRange const& inputRange)
    -> std::optional<std::tuple<AqlCall, size_t>> {
  if (!_initialized) {
    // We cannot modify the number of dependencies, so we start
    // with 0 dependencies, and will increase to whatever inputRange gives us.
    TRI_ASSERT(_numberDependencies == 0 ||
               _numberDependencies == inputRange.numberDependencies());
    _numberDependencies = inputRange.numberDependencies();
    // If we have collected all ranges once, we can prepare the local data-structure copy
    _inputRows.reserve(_numberDependencies);
    for (size_t dep = 0; dep < _numberDependencies; ++dep) {
      auto const [state, row] = inputRange.peekDataRow(dep);
      _inputRows.emplace_back(dep, row, state);
    }
    auto call = requiresMoreInput(inputRange);
    if (call.has_value()) {
      return call;
    }
    _strategy->prepare(_inputRows);
    _initialized = true;
  }
  return {};
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
    auto const& [state, row] = input.peekDataRow(dep);
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

auto SortingGatherExecutor::produceRows(typename Fetcher::DataRange& input,
                                        OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall, size_t> {
  {
    // First initialize
    auto maybeCall = initialize(input);
    if (maybeCall.has_value()) {
      auto const& [request, dep] = maybeCall.value();
      return {ExecutorState::HASMORE, NoStats{}, request, dep};
    }
  }

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
  {
    // First initialize
    auto maybeCall = initialize(input);
    if (maybeCall.has_value()) {
      auto const& [request, dep] = maybeCall.value();
      return {ExecutorState::HASMORE, NoStats{}, 0, request, dep};
    }
  }

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

std::pair<ExecutionState, size_t> SortingGatherExecutor::expectedNumberOfRows(size_t const atMost) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
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

auto SortingGatherExecutor::rowsLeftToWrite() const noexcept -> size_t {
  TRI_ASSERT(constrainedSort());
  TRI_ASSERT(_limit >= _rowsReturned);
  return _limit - std::min(_limit, _rowsReturned);
}
