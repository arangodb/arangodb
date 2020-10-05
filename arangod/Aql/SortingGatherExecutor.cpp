////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "SortingGatherExecutor.h"

#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SortRegister.h"
#include "Aql/Stats.h"
#include "Aql/QueryContext.h"
#include "Transaction/Methods.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

/// @brief OurLessThan: comparison method for elements of SortingGatherBlock
class OurLessThan {
 public:
  OurLessThan(arangodb::aql::QueryContext& query, std::vector<SortRegister>& sortRegisters) noexcept
      : _resolver(query.resolver()), _vpackOptions(query.vpackOptions()),
        _sortRegisters(sortRegisters) {}

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
        cmp = AqlValue::Compare(&_vpackOptions, lhs, rhs, true);
      } else {
        // Take attributePath into consideration:
        bool mustDestroyA;
        AqlValue aa = lhs.get(_resolver, attributePath, mustDestroyA, false);
        AqlValueGuard guardA(aa, mustDestroyA);
        bool mustDestroyB;
        AqlValue bb = rhs.get(_resolver, attributePath, mustDestroyB, false);
        AqlValueGuard guardB(bb, mustDestroyB);
        cmp = AqlValue::Compare(&_vpackOptions, aa, bb, true);
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
  arangodb::CollectionNameResolver const& _resolver;
  arangodb::velocypack::Options const& _vpackOptions;
  std::vector<SortRegister>& _sortRegisters;
};  // OurLessThan

////////////////////////////////////////////////////////////////////////////////
/// @class HeapSorting
/// @brief "Heap" sorting strategy
////////////////////////////////////////////////////////////////////////////////
class HeapSorting final : public SortingGatherExecutor::SortingStrategy, private OurLessThan {
 public:
  HeapSorting(arangodb::aql::QueryContext& query, std::vector<SortRegister>& sortRegisters) noexcept
      : OurLessThan(query, sortRegisters) {}

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

  // The STL heap (regarding push_heap, pop_heap, make_heap) is a max heap, but
  // we want a min heap!
  bool operator()(SortingGatherExecutor::ValueType const& left,
                  SortingGatherExecutor::ValueType const& right) const {
    // Note that right and left are swapped!
    return OurLessThan::operator()(right, left);
  }

 private:
  std::vector<std::reference_wrapper<SortingGatherExecutor::ValueType>> _heap;
};  // HeapSorting

////////////////////////////////////////////////////////////////////////////////
/// @class MinElementSorting
/// @brief "MinElement" sorting strategy
////////////////////////////////////////////////////////////////////////////////
class MinElementSorting final : public SortingGatherExecutor::SortingStrategy,
                                private OurLessThan {
 public:
  MinElementSorting(arangodb::aql::QueryContext& query,
                    std::vector<SortRegister>& sortRegisters) noexcept
      : OurLessThan(query, sortRegisters), _blockPos(nullptr) {}

  virtual SortingGatherExecutor::ValueType nextValue() override {
    TRI_ASSERT(_blockPos);
    return *(std::min_element(_blockPos->begin(), _blockPos->end(), *this));
  }

  virtual void prepare(std::vector<SortingGatherExecutor::ValueType>& blockPos) override {
    _blockPos = &blockPos;
  }

  virtual void reset() noexcept override { _blockPos = nullptr; }

  using OurLessThan::operator();

 private:
  std::vector<SortingGatherExecutor::ValueType> const* _blockPos;
};

}  // namespace
SortingGatherExecutor::ValueType::ValueType(size_t index)
    : dependencyIndex{index}, row{CreateInvalidInputRowHint()}, state{ExecutorState::HASMORE} {}

SortingGatherExecutor::ValueType::ValueType(size_t index, InputAqlItemRow prow, ExecutorState pstate)
    : dependencyIndex{index}, row{std::move(prow)}, state{pstate} {}

SortingGatherExecutorInfos::SortingGatherExecutorInfos(
    std::vector<SortRegister>&& sortRegister, arangodb::aql::QueryContext& query,
    GatherNode::SortMode sortMode, size_t limit, GatherNode::Parallelism p)
    : _sortRegister(std::move(sortRegister)),
      _query(query),
      _sortMode(sortMode),
      _parallelism(p),
      _limit(limit) {}

SortingGatherExecutor::SortingGatherExecutor(Fetcher& fetcher, Infos& infos)
    : _numberDependencies(0),
      _inputRows(),
      _limit(infos.limit()),
      _rowsReturned(0),
      _strategy(nullptr),
      _fetchParallel(infos.parallelism() == GatherNode::Parallelism::Parallel) {
  switch (infos.sortMode()) {
    case GatherNode::SortMode::MinElement:
      _strategy = std::make_unique<MinElementSorting>(infos.query(), infos.sortRegister());
      break;
    case GatherNode::SortMode::Heap:
    case GatherNode::SortMode::Default:  // use heap by default
      _strategy = std::make_unique<HeapSorting>(infos.query(), infos.sortRegister());
      break;
    default:
      TRI_ASSERT(false);
      break;
  }
  TRI_ASSERT(_strategy);
}

SortingGatherExecutor::~SortingGatherExecutor() = default;

auto SortingGatherExecutor::initialize(typename Fetcher::DataRange const& inputRange,
                                       AqlCall const& clientCall) -> AqlCallSet {
  if (!_initialized) {
    // We cannot modify the number of dependencies, so we start
    // with 0 dependencies, and will increase to whatever inputRange gives us.
    TRI_ASSERT(_numberDependencies == 0 ||
               _numberDependencies == inputRange.numberDependencies());
    _numberDependencies = inputRange.numberDependencies();
    // If we have collected all ranges once, we can prepare the local data-structure copy
    _inputRows.reserve(_numberDependencies);
    if (_inputRows.empty()) {
      for (size_t dep = 0; dep < _numberDependencies; ++dep) {
        _inputRows.emplace_back(dep);
      }
    }

    auto callSet = AqlCallSet{};
    for (size_t dep = 0; dep < _numberDependencies; ++dep) {
      auto const [state, row] = inputRange.peekDataRow(dep);
      _inputRows[dep] = {dep, row, state};
      if (!row && state != ExecutorState::DONE) {
        // This dependency requires input
        callSet.calls.emplace_back(
            AqlCallSet::DepCallPair{dep, calculateUpstreamCall(clientCall)});
        if (!_fetchParallel) {
          break;
        }
      }
    }
    if (!callSet.empty()) {
      return callSet;
    }
    _strategy->prepare(_inputRows);
    _initialized = true;
  }
  return {};
}

auto SortingGatherExecutor::requiresMoreInput(typename Fetcher::DataRange const& inputRange,
                                              AqlCall const& clientCall) -> AqlCallSet {
  auto callSet = AqlCallSet{};

  if (clientCall.hasSoftLimit()) {
    // This is the case if we finished the current call. In some cases
    // our dependency is consumed as well. If we do not exit here,
    // we would create a call with softlimit = 0.
    // The next call with softlimit != 0 will update the dependency.
    if (clientCall.getOffset() == 0 && clientCall.getLimit() == 0) {
      return callSet;
    }
  }

  if (_depToUpdate.has_value()) {
    auto const dependency = _depToUpdate.value();
    auto const& [state, row] = inputRange.peekDataRow(dependency);
    auto const needMoreInput = !row && state != ExecutorState::DONE;
    if (needMoreInput) {
      // Still waiting for input
      // TODO: This call requires limits
      callSet.calls.emplace_back(
          AqlCallSet::DepCallPair{dependency, calculateUpstreamCall(clientCall)});
    } else {
      // We got an answer, save it
      ValueType& localDep = _inputRows[dependency];
      localDep.row = row;
      localDep.state = state;
      // We don't need to update this dep anymore
      _depToUpdate = std::nullopt;
    }
  }

  return callSet;
}

auto SortingGatherExecutor::nextRow(MultiAqlItemBlockInputRange& input) -> InputAqlItemRow {
  if (input.isDone()) {
    // No rows, there is a chance we get into this.
    // If we requested data from upstream, but all if it is done.
    return InputAqlItemRow{CreateInvalidInputRowHint{}};
  }
  TRI_ASSERT(!_depToUpdate.has_value());
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
  TRI_ASSERT(nextVal.row);
  _rowsReturned++;
  {
    // Consume the row, and set it to next input
    auto const dependency = nextVal.dependencyIndex;
    std::ignore = input.nextDataRow(dependency);
    auto const& [state, row] = input.peekDataRow(dependency);
    _inputRows[dependency].state = state;
    _inputRows[dependency].row = row;

    auto const needMoreInput = !row && state != ExecutorState::DONE;
    if (needMoreInput) {
      _depToUpdate = dependency;
    }
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
    -> std::tuple<ExecutorState, Stats, AqlCallSet> {
  {
    // First initialize
    auto const callSet = initialize(input, output.getClientCall());
    if (!callSet.empty()) {
      return {ExecutorState::HASMORE, NoStats{}, callSet};
    }
  }

  {
    auto const callSet = requiresMoreInput(input, output.getClientCall());
    if (!callSet.empty()) {
      return {ExecutorState::HASMORE, NoStats{}, callSet};
    }
  }

  // produceRows should not be called again when the limit is reached;
  // the downstream limit should see to that.
  TRI_ASSERT(!limitReached());

  while (!input.isDone() && !output.isFull() && !limitReached()) {
    TRI_ASSERT(!maySkip());
    auto row = nextRow(input);
    if (row) {
      output.copyRow(row);
      output.advanceRow();
    }

    auto const callSet = requiresMoreInput(input, output.getClientCall());
    if (!callSet.empty()) {
      return {ExecutorState::HASMORE, NoStats{}, callSet};
    }
  }

  auto const state = std::invoke([&]() {
    if (input.isDone()) {
      return ExecutorState::DONE;
    } else if (limitReached() && !output.getClientCall().needsFullCount()) {
      return ExecutorState::DONE;
    } else {
      return ExecutorState::HASMORE;
    }
  });

  return {state, NoStats{}, AqlCallSet{}};
}

auto SortingGatherExecutor::skipRowsRange(typename Fetcher::DataRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCallSet> {
  {
    // First initialize
    auto const callSet = initialize(input, call);
    if (!callSet.empty()) {
      return {ExecutorState::HASMORE, NoStats{}, 0, callSet};
    }
  }

  {
    auto const callSet = requiresMoreInput(input, call);
    if (!callSet.empty()) {
      return {ExecutorState::HASMORE, NoStats{}, 0, callSet};
    }
  }

  // skip offset
  while (!input.isDone() && call.getOffset() > 0) {
    // During offset phase we have the guarntee
    // that the rows we need to skip have been fetched
    // We will fetch rows as data from upstream for
    // all rows we need to skip here.
    TRI_ASSERT(!maySkip());
    // We need to sort still
    // And account the row in the limit
    auto row = nextRow(input);
    TRI_ASSERT(row.isInitialized() || input.isDone());
    if (row) {
      call.didSkip(1);
    }
    auto const callSet = requiresMoreInput(input, call);
    if (!callSet.empty()) {
      return {ExecutorState::HASMORE, NoStats{}, call.getSkipCount(), callSet};
    }
  }

  TRI_ASSERT(input.isDone() || call.getOffset() == 0);

  auto callSet = AqlCallSet{};
  if (call.needSkipMore() && call.getOffset() == 0) {
    // We can only skip more if the offset is reached.
    // Otherwise we would have looped more above
    TRI_ASSERT(call.getOffset() == 0);
    TRI_ASSERT(call.hasHardLimit());

    // We are only called with fullcount.
    // or if the input is done.
    // sorting does not matter.
    // Start simply skip all from upstream.
    for (size_t dep = 0; dep < input.numberDependencies(); ++dep) {
      auto& range = input.rangeForDependency(dep);
      while (range.hasDataRow()) {
        // Consume the row and count it as skipped
        std::ignore = input.nextDataRow(dep);
        call.didSkip(1);
      }
      // Skip all rows in flight
      call.didSkip(range.skipAll());

      if (range.upstreamState() == ExecutorState::HASMORE) {
        TRI_ASSERT(!input.hasDataRow(dep));
        TRI_ASSERT(input.skippedInFlight(dep) == 0);
        // We need to fetch more data, but can fullCount now
        AqlCallList request{AqlCall{0, true, 0, AqlCall::LimitType::HARD}};
        callSet.calls.emplace_back(AqlCallSet::DepCallPair{dep, request});
        if (!_fetchParallel) {
          break;
        }
      }
    }
  }

  TRI_ASSERT(!input.isDone() || callSet.empty());

  return {input.state(), NoStats{}, call.getSkipCount(), callSet};
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

auto SortingGatherExecutor::limitReached() const noexcept -> bool {
  return constrainedSort() && rowsLeftToWrite() == 0;
}

[[nodiscard]] auto SortingGatherExecutor::calculateUpstreamCall(AqlCall const& clientCall) const
    noexcept -> AqlCallList {
  auto upstreamCall = AqlCall{};
  if (constrainedSort()) {
    if (clientCall.hasSoftLimit()) {
      // We do not know if we are going to be asked again to do a fullcount
      // So we can only request a softLimit bounded by our internal limit to upstream

      upstreamCall.softLimit = clientCall.offset + clientCall.softLimit;
      if (rowsLeftToWrite() < upstreamCall.softLimit) {
        // Do not overfetch
        // NOTE: We cannnot use std::min as the numbers have different types ;(
        upstreamCall.softLimit = rowsLeftToWrite();
      }

      // We need at least 1 to not violate API. It seems we have nothing to
      // produce and are called with a softLimit.
      TRI_ASSERT(0 < upstreamCall.softLimit);
    } else {
      if (rowsLeftToWrite() < upstreamCall.hardLimit) {
        // Do not overfetch
        // NOTE: We cannnot use std::min as the numbers have different types ;(
        upstreamCall.hardLimit = rowsLeftToWrite();
      }
      // In case the client needs a fullCount we do it as well, for all rows
      // after the above limits
      upstreamCall.fullCount = clientCall.fullCount;
      TRI_ASSERT(0 < upstreamCall.hardLimit || upstreamCall.needsFullCount());
    }
  } else {
    // Increase the clientCall limit by it's offset and forward.
    upstreamCall.softLimit = clientCall.softLimit + clientCall.offset;
    upstreamCall.hardLimit = clientCall.hardLimit + clientCall.offset;
    // In case the client needs a fullCount we do it as well, for all rows
    // after the above limits
    upstreamCall.fullCount = clientCall.fullCount;
  }

  // We do never send a skip to upstream here.
  // We need to look at every relevant line ourselves
  TRI_ASSERT(upstreamCall.offset == 0);
  return AqlCallList{upstreamCall};
}
