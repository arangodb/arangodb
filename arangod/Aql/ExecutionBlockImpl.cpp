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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "ExecutionBlockImpl.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlCallStack.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/CalculationExecutor.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ConstrainedSortExecutor.h"
#include "Aql/CountCollectExecutor.h"
#include "Aql/DistinctCollectExecutor.h"
#include "Aql/EnumerateCollectionExecutor.h"
#include "Aql/EnumerateListExecutor.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/FilterExecutor.h"
#include "Aql/HashedCollectExecutor.h"
#include "Aql/IResearchViewExecutor.h"
#include "Aql/IdExecutor.h"
#include "Aql/IndexExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/KShortestPathsExecutor.h"
#include "Aql/LimitExecutor.h"
#include "Aql/MaterializeExecutor.h"
#include "Aql/ModificationExecutor.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/NoResultsExecutor.h"
#include "Aql/ParallelUnsortedGatherExecutor.h"
#include "Aql/Query.h"
#include "Aql/QueryOptions.h"
#include "Aql/ReturnExecutor.h"
#include "Aql/ShadowAqlItemRow.h"
#include "Aql/ShortestPathExecutor.h"
#include "Aql/SimpleModifier.h"
#include "Aql/SingleRemoteModificationExecutor.h"
#include "Aql/SortExecutor.h"
#include "Aql/SortRegister.h"
#include "Aql/SortedCollectExecutor.h"
#include "Aql/SortingGatherExecutor.h"
#include "Aql/SubqueryEndExecutor.h"
#include "Aql/SubqueryExecutor.h"
#include "Aql/SubqueryStartExecutor.h"
#include "Aql/TraversalExecutor.h"
#include "Aql/UnsortedGatherExecutor.h"
#include "Aql/UpsertModifier.h"
#include "Basics/system-functions.h"
#include "Transaction/Context.h"

#include <velocypack/Dumper.h>
#include <velocypack/velocypack-aliases.h>

#include <type_traits>

using namespace arangodb;
using namespace arangodb::aql;

namespace {

std::string const doneString = "DONE";
std::string const hasDataRowString = "HASMORE";
std::string const waitingString = "WAITING";
std::string const unknownString = "UNKNOWN";

std::string const& stateToString(aql::ExecutionState state) {
  switch (state) {
    case aql::ExecutionState::DONE:
      return doneString;
    case aql::ExecutionState::HASMORE:
      return hasDataRowString;
    case aql::ExecutionState::WAITING:
      return waitingString;
    default:
      // just to suppress a warning ..
      return unknownString;
  }
}

}  // namespace

/*
 * Creates a metafunction `checkName` that tests whether a class has a method
 * named `methodName`, used like this:
 *
 * CREATE_HAS_MEMBER_CHECK(someMethod, hasSomeMethod);
 * ...
 * constexpr bool someClassHasSomeMethod = hasSomeMethod<SomeClass>::value;
 */

#define CREATE_HAS_MEMBER_CHECK(methodName, checkName)               \
  template <typename T>                                              \
  class checkName {                                                  \
    template <typename C>                                            \
    static std::true_type test(decltype(&C::methodName));            \
    template <typename C>                                            \
    static std::true_type test(decltype(&C::template methodName<>)); \
    template <typename>                                              \
    static std::false_type test(...);                                \
                                                                     \
   public:                                                           \
    static constexpr bool value = decltype(test<T>(0))::value;       \
  }

CREATE_HAS_MEMBER_CHECK(initializeCursor, hasInitializeCursor);
CREATE_HAS_MEMBER_CHECK(skipRows, hasSkipRows);
CREATE_HAS_MEMBER_CHECK(fetchBlockForPassthrough, hasFetchBlockForPassthrough);
CREATE_HAS_MEMBER_CHECK(expectedNumberOfRows, hasExpectedNumberOfRows);
CREATE_HAS_MEMBER_CHECK(skipRowsRange, hasSkipRowsRange);

/*
 * Determine whether we execute new style or old style skips, i.e. pre or post shadow row introduction
 * TODO: This should be removed once all executors and fetchers are ported to the new style.
 */
template <class Executor>
static bool constexpr isNewStyleExecutor() {
  return std::is_same<Executor, FilterExecutor>::value;
}

template <class Executor>
ExecutionBlockImpl<Executor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                 ExecutionNode const* node,
                                                 typename Executor::Infos&& infos)
    : ExecutionBlock(engine, node),
      _dependencyProxy(_dependencies, engine->itemBlockManager(),
                       infos.getInputRegisters(),
                       infos.numberOfInputRegisters(), trxVpackOptions()),
      _rowFetcher(_dependencyProxy),
      _infos(std::move(infos)),
      _executor(_rowFetcher, _infos),
      _outputItemRow(),
      _query(*engine->getQuery()),
      _state(InternalState::FETCH_DATA),
      _lastRange{ExecutorState::HASMORE} {
  // already insert ourselves into the statistics results
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    _engine->_stats.nodes.try_emplace(node->id(), ExecutionStats::Node());
  }
}

template <class Executor>
ExecutionBlockImpl<Executor>::~ExecutionBlockImpl() = default;

template <class Executor>
std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<Executor>::getSome(size_t atMost) {
  if constexpr (isNewStyleExecutor<Executor>()) {
    AqlCallStack stack{AqlCall::SimulateGetSome(atMost)};
    auto const [state, skipped, block] = execute(stack);
    return {state, block};
  } else {
    traceGetSomeBegin(atMost);
    auto result = getSomeWithoutTrace(atMost);
    return traceGetSomeEnd(result.first, std::move(result.second));
  }
}

template <class Executor>
std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<Executor>::getSomeWithoutTrace(size_t atMost) {
  TRI_ASSERT(atMost <= ExecutionBlock::DefaultBatchSize());
  // silence tests -- we need to introduce new failure tests for fetchers
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome1") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome2") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  TRI_IF_FAILURE("ExecutionBlock::getOrSkipSome3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (getQuery().killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }

  if (_state == InternalState::DONE) {
    // We are done, so we stay done
    return {ExecutionState::DONE, nullptr};
  }

  if (!_outputItemRow) {
    ExecutionState state;
    SharedAqlItemBlockPtr newBlock;
    std::tie(state, newBlock) =
        requestWrappedBlock(atMost, _infos.numberOfOutputRegisters());
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(newBlock == nullptr);
      return {state, nullptr};
    }
    if (newBlock == nullptr) {
      TRI_ASSERT(state == ExecutionState::DONE);
      _state = InternalState::DONE;
      // _rowFetcher must be DONE now already
      return {state, nullptr};
    }
    TRI_ASSERT(newBlock != nullptr);
    TRI_ASSERT(newBlock->size() > 0);
    // We cannot hold this assertion, if we are on a pass-through
    // block and the upstream uses execute already.
    // TRI_ASSERT(newBlock->size() <= atMost);
    _outputItemRow = createOutputRow(newBlock, AqlCall{});
  }

  ExecutionState state = ExecutionState::HASMORE;
  ExecutorStats executorStats{};

  TRI_ASSERT(atMost > 0);

  if (isInSplicedSubquery()) {
    // The loop has to be entered at least once!
    TRI_ASSERT(!_outputItemRow->isFull());
    while (!_outputItemRow->isFull() && _state != InternalState::DONE) {
      // Assert that write-head is always pointing to a free row
      TRI_ASSERT(!_outputItemRow->produced());
      switch (_state) {
        case InternalState::FETCH_DATA: {
          std::tie(state, executorStats) = _executor.produceRows(*_outputItemRow);
          // Count global but executor-specific statistics, like number of
          // filtered rows.
          _engine->_stats += executorStats;
          if (_outputItemRow->produced()) {
            _outputItemRow->advanceRow();
          }

          if (state == ExecutionState::WAITING) {
            return {state, nullptr};
          }

          if (state == ExecutionState::DONE) {
            _state = InternalState::FETCH_SHADOWROWS;
          }
          break;
        }
        case InternalState::FETCH_SHADOWROWS: {
          state = fetchShadowRowInternal();
          if (state == ExecutionState::WAITING) {
            return {state, nullptr};
          }
          break;
        }
        case InternalState::DONE: {
          TRI_ASSERT(false);  // Invalid state
        }
      }
    }
    // Modify the return state.
    // As long as we do still have ShadowRows
    // We need to return HASMORE!
    if (_state == InternalState::DONE) {
      state = ExecutionState::DONE;
    } else {
      state = ExecutionState::HASMORE;
    }
  } else {
    // The loop has to be entered at least once!
    TRI_ASSERT(!_outputItemRow->isFull());
    while (!_outputItemRow->isFull()) {
      std::tie(state, executorStats) = _executor.produceRows(*_outputItemRow);
      // Count global but executor-specific statistics, like number of filtered
      // rows.
      _engine->_stats += executorStats;
      if (_outputItemRow->produced()) {
        _outputItemRow->advanceRow();
      }

      if (state == ExecutionState::WAITING) {
        return {state, nullptr};
      }

      if (state == ExecutionState::DONE) {
        auto outputBlock = _outputItemRow->stealBlock();
        // This is not strictly necessary here, as we shouldn't be called again
        // after DONE.
        _outputItemRow.reset();
        return {state, std::move(outputBlock)};
      }
    }

    TRI_ASSERT(state == ExecutionState::HASMORE);
    TRI_ASSERT(_outputItemRow->isFull());
  }

  auto outputBlock = _outputItemRow->stealBlock();
  // we guarantee that we do return a valid pointer in the HASMORE case.
  TRI_ASSERT(outputBlock != nullptr || _state == InternalState::DONE);
  _outputItemRow.reset();
  return {state, std::move(outputBlock)};
}

template <class Executor>
std::unique_ptr<OutputAqlItemRow> ExecutionBlockImpl<Executor>::createOutputRow(
    SharedAqlItemBlockPtr& newBlock, AqlCall&& call) {
  if /* constexpr */ (Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable) {
    return std::make_unique<OutputAqlItemRow>(newBlock, infos().getOutputRegisters(),
                                              infos().registersToKeep(),
                                              infos().registersToClear(), std::move(call),
                                              OutputAqlItemRow::CopyRowBehavior::DoNotCopyInputRows);
  } else {
    return std::make_unique<OutputAqlItemRow>(newBlock, infos().getOutputRegisters(),
                                              infos().registersToKeep(),
                                              infos().registersToClear(),
                                              std::move(call));
  }
}

template <class Executor>
Executor& ExecutionBlockImpl<Executor>::executor() {
  return _executor;
}

template <class Executor>
Query const& ExecutionBlockImpl<Executor>::getQuery() const {
  return _query;
}

template <class Executor>
typename ExecutionBlockImpl<Executor>::Infos const& ExecutionBlockImpl<Executor>::infos() const {
  return _infos;
}

namespace arangodb::aql {

enum class SkipVariants { FETCHER, EXECUTOR, GET_SOME };

// Specifying the namespace here is important to MSVC.
template <enum arangodb::aql::SkipVariants>
struct ExecuteSkipVariant {};

template <>
struct ExecuteSkipVariant<SkipVariants::FETCHER> {
  template <class Executor>
  static std::tuple<ExecutionState, typename Executor::Stats, size_t> executeSkip(
      Executor& executor, typename Executor::Fetcher& fetcher, size_t toSkip) {
    auto res = fetcher.skipRows(toSkip);
    return std::make_tuple(res.first, typename Executor::Stats{}, res.second);  // tuple, cannot use initializer list due to build failure
  }
};

template <>
struct ExecuteSkipVariant<SkipVariants::EXECUTOR> {
  template <class Executor>
  static std::tuple<ExecutionState, typename Executor::Stats, size_t> executeSkip(
      Executor& executor, typename Executor::Fetcher& fetcher, size_t toSkip) {
    return executor.skipRows(toSkip);
  }
};

template <>
struct ExecuteSkipVariant<SkipVariants::GET_SOME> {
  template <class Executor>
  static std::tuple<ExecutionState, typename Executor::Stats, size_t> executeSkip(
      Executor& executor, typename Executor::Fetcher& fetcher, size_t toSkip) {
    // this function should never be executed
    TRI_ASSERT(false);
    // Make MSVC happy:
    return std::make_tuple(ExecutionState::DONE, typename Executor::Stats{}, 0);  // tuple, cannot use initializer list due to build failure
  }
};

template <class Executor>
static SkipVariants constexpr skipType() {
  bool constexpr useFetcher =
      Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable &&
      !std::is_same<Executor, SubqueryExecutor<true>>::value;

  bool constexpr useExecutor = hasSkipRows<Executor>::value;

  // ConstFetcher and SingleRowFetcher<BlockPassthrough::Enable> can skip, but
  // it may not be done for modification subqueries.
  static_assert(useFetcher ==
                    (std::is_same<typename Executor::Fetcher, ConstFetcher>::value ||
                     (std::is_same<typename Executor::Fetcher, SingleRowFetcher<BlockPassthrough::Enable>>::value &&
                      !std::is_same<Executor, SubqueryExecutor<true>>::value)),
                "Unexpected fetcher for SkipVariants::FETCHER");

  static_assert(!useFetcher || hasSkipRows<typename Executor::Fetcher>::value,
                "Fetcher is chosen for skipping, but has not skipRows method!");

  static_assert(
      useExecutor ==
          (std::is_same<Executor, IndexExecutor>::value ||
           std::is_same<Executor, IResearchViewExecutor<false, iresearch::MaterializeType::NotMaterialize>>::value ||
           std::is_same<Executor, IResearchViewExecutor<false, iresearch::MaterializeType::LateMaterialize>>::value ||
           std::is_same<Executor, IResearchViewExecutor<false, iresearch::MaterializeType::Materialize>>::value ||
           std::is_same<Executor, IResearchViewExecutor<false, iresearch::MaterializeType::NotMaterialize | iresearch::MaterializeType::UseStoredValues>>::value ||
           std::is_same<Executor, IResearchViewExecutor<false, iresearch::MaterializeType::LateMaterialize | iresearch::MaterializeType::UseStoredValues>>::value ||
           std::is_same<Executor, IResearchViewExecutor<true, iresearch::MaterializeType::NotMaterialize>>::value ||
           std::is_same<Executor, IResearchViewExecutor<true, iresearch::MaterializeType::LateMaterialize>>::value ||
           std::is_same<Executor, IResearchViewExecutor<true, iresearch::MaterializeType::Materialize>>::value ||
           std::is_same<Executor, IResearchViewExecutor<true, iresearch::MaterializeType::NotMaterialize | iresearch::MaterializeType::UseStoredValues>>::value ||
           std::is_same<Executor, IResearchViewExecutor<true, iresearch::MaterializeType::LateMaterialize | iresearch::MaterializeType::UseStoredValues>>::value ||
           std::is_same<Executor, IResearchViewMergeExecutor<false, iresearch::MaterializeType::NotMaterialize>>::value ||
           std::is_same<Executor, IResearchViewMergeExecutor<false, iresearch::MaterializeType::LateMaterialize>>::value ||
           std::is_same<Executor, IResearchViewMergeExecutor<false, iresearch::MaterializeType::Materialize>>::value ||
           std::is_same<Executor, IResearchViewMergeExecutor<false, iresearch::MaterializeType::NotMaterialize | iresearch::MaterializeType::UseStoredValues>>::value ||
           std::is_same<Executor, IResearchViewMergeExecutor<false, iresearch::MaterializeType::LateMaterialize | iresearch::MaterializeType::UseStoredValues>>::value ||
           std::is_same<Executor, IResearchViewMergeExecutor<true, iresearch::MaterializeType::NotMaterialize>>::value ||
           std::is_same<Executor, IResearchViewMergeExecutor<true, iresearch::MaterializeType::LateMaterialize>>::value ||
           std::is_same<Executor, IResearchViewMergeExecutor<true, iresearch::MaterializeType::Materialize>>::value ||
           std::is_same<Executor, IResearchViewMergeExecutor<true, iresearch::MaterializeType::NotMaterialize | iresearch::MaterializeType::UseStoredValues>>::value ||
           std::is_same<Executor, IResearchViewMergeExecutor<true, iresearch::MaterializeType::LateMaterialize | iresearch::MaterializeType::UseStoredValues>>::value ||
           std::is_same<Executor, EnumerateCollectionExecutor>::value ||
           std::is_same<Executor, LimitExecutor>::value ||
           std::is_same<Executor, ConstrainedSortExecutor>::value ||
           std::is_same<Executor, SortingGatherExecutor>::value ||
           std::is_same<Executor, UnsortedGatherExecutor>::value ||
           std::is_same<Executor, ParallelUnsortedGatherExecutor>::value ||
           std::is_same<Executor, MaterializeExecutor<RegisterId>>::value ||
           std::is_same<Executor, MaterializeExecutor<std::string const&>>::value),
      "Unexpected executor for SkipVariants::EXECUTOR");

  // The LimitExecutor will not work correctly with SkipVariants::FETCHER!
  static_assert(
      !std::is_same<Executor, LimitExecutor>::value || useFetcher,
      "LimitExecutor needs to implement skipRows() to work correctly");

  if (useExecutor) {
    return SkipVariants::EXECUTOR;
  } else if (useFetcher) {
    return SkipVariants::FETCHER;
  } else {
    return SkipVariants::GET_SOME;
  }
}

}  // namespace arangodb::aql

template <class Executor>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<Executor>::skipSome(size_t const atMost) {
  if constexpr (isNewStyleExecutor<Executor>()) {
    AqlCallStack stack{AqlCall::SimulateSkipSome(atMost)};
    auto const [state, skipped, block] = execute(stack);

    // execute returns ExecutionState::DONE here, which stops execution after simulating a skip.
    // If we indiscriminately return ExecutionState::HASMORE, then we end up in an infinite loop
    //
    // luckily we can dispose of this kludge once executors have been ported.
    if (skipped < atMost && state == ExecutionState::DONE) {
      return {ExecutionState::DONE, skipped};
    } else {
      return {ExecutionState::HASMORE, skipped};
    }
  } else {
    traceSkipSomeBegin(atMost);
    auto state = ExecutionState::HASMORE;

    while (state == ExecutionState::HASMORE && _skipped < atMost) {
      auto res = skipSomeOnceWithoutTrace(atMost - _skipped);
      TRI_ASSERT(state != ExecutionState::WAITING || res.second == 0);
      state = res.first;
      _skipped += res.second;
      TRI_ASSERT(_skipped <= atMost);
    }

    size_t skipped = 0;
    if (state != ExecutionState::WAITING) {
      std::swap(skipped, _skipped);
    }

    TRI_ASSERT(skipped <= atMost);
    return traceSkipSomeEnd(state, skipped);
  }
}

template <class Executor>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<Executor>::skipSomeOnceWithoutTrace(size_t atMost) {
  constexpr SkipVariants customSkipType = skipType<Executor>();

  if (customSkipType == SkipVariants::GET_SOME) {
    atMost = std::min(atMost, DefaultBatchSize());
    auto res = getSomeWithoutTrace(atMost);

    size_t skipped = 0;
    if (res.second != nullptr) {
      skipped = res.second->size();
    }
    TRI_ASSERT(skipped <= atMost);

    return {res.first, skipped};
  }

  ExecutionState state;
  typename Executor::Stats stats;
  size_t skipped;
  std::tie(state, stats, skipped) =
      ExecuteSkipVariant<customSkipType>::executeSkip(_executor, _rowFetcher, atMost);
  _engine->_stats += stats;
  TRI_ASSERT(skipped <= atMost);

  return {state, skipped};
}

template <bool customInit>
struct InitializeCursor {};

template <>
struct InitializeCursor<false> {
  template <class Executor>
  static void init(Executor& executor, typename Executor::Fetcher& rowFetcher,
                   typename Executor::Infos& infos) {
    // destroy and re-create the Executor
    executor.~Executor();
    new (&executor) Executor(rowFetcher, infos);
  }
};

template <>
struct InitializeCursor<true> {
  template <class Executor>
  static void init(Executor& executor, typename Executor::Fetcher&,
                   typename Executor::Infos&) {
    // re-initialize the Executor
    executor.initializeCursor();
  }
};

template <class Executor>
std::pair<ExecutionState, Result> ExecutionBlockImpl<Executor>::initializeCursor(InputAqlItemRow const& input) {
  // reinitialize the DependencyProxy
  _dependencyProxy.reset();
  _lastRange = DataRange(ExecutorState::HASMORE);

  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_dependencyProxy);

  TRI_ASSERT(_skipped == 0);
  _skipped = 0;
  TRI_ASSERT(_state == InternalState::DONE || _state == InternalState::FETCH_DATA);
  _state = InternalState::FETCH_DATA;

  constexpr bool customInit = hasInitializeCursor<Executor>::value;
  // IndexExecutor and EnumerateCollectionExecutor have initializeCursor
  // implemented, so assert this implementation is used.
  static_assert(!std::is_same<Executor, EnumerateCollectionExecutor>::value || customInit,
                "EnumerateCollectionExecutor is expected to implement a custom "
                "initializeCursor method!");
  static_assert(!std::is_same<Executor, IndexExecutor>::value || customInit,
                "IndexExecutor is expected to implement a custom "
                "initializeCursor method!");
  static_assert(!std::is_same<Executor, DistinctCollectExecutor>::value || customInit,
                "DistinctCollectExecutor is expected to implement a custom "
                "initializeCursor method!");
  InitializeCursor<customInit>::init(_executor, _rowFetcher, _infos);

  // // use this with c++17 instead of specialization below
  // if constexpr (std::is_same_v<Executor, IdExecutor>) {
  //   if (items != nullptr) {
  //     _executor._inputRegisterValues.reset(
  //         items->slice(pos, *(_executor._infos.registersToKeep())));
  //   }
  // }

  return ExecutionBlock::initializeCursor(input);
}

template <class Executor>
std::pair<ExecutionState, Result> ExecutionBlockImpl<Executor>::shutdown(int errorCode) {
  return ExecutionBlock::shutdown(errorCode);
}

template <class Executor>
std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr> ExecutionBlockImpl<Executor>::execute(AqlCallStack stack) {
  // TODO remove this IF
  // These are new style executors
  if constexpr (isNewStyleExecutor<Executor>()) {
    // Only this executor is fully implemented
    traceExecuteBegin(stack);
    auto res = executeWithoutTrace(stack);
    traceExecuteEnd(res);
    return res;
  }

  // Fall back to getSome/skipSome
  auto myCall = stack.popCall();
  TRI_ASSERT(AqlCall::IsSkipSomeCall(myCall) || AqlCall::IsGetSomeCall(myCall));
  if (AqlCall::IsSkipSomeCall(myCall)) {
    auto const [state, skipped] = skipSome(myCall.getOffset());
    if (state != ExecutionState::WAITING) {
      myCall.didSkip(skipped);
    }
    return {state, skipped, nullptr};
  } else if (AqlCall::IsGetSomeCall(myCall)) {
    auto const [state, block] = getSome(myCall.getLimit());
    // We do not need to count as softLimit will be overwritten, and hard cannot be set.
    return {state, 0, block};
  }
  // Should never get here!
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

template <class Executor>
void ExecutionBlockImpl<Executor>::traceExecuteBegin(AqlCallStack const& stack) {
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    if (_getSomeBegin <= 0.0) {
      _getSomeBegin = TRI_microtime();
    }
    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      auto const node = getPlanNode();
      auto const queryId = this->_engine->getQuery()->id();
      // TODO make sure this works also if stack is non relevant, e.g. passed through by outer subquery.
      auto const& call = stack.peek();
      LOG_TOPIC("1e717", INFO, Logger::QUERIES)
          << "[query#" << queryId << "] "
          << "execute type=" << node->getTypeString()
          << " offset=" << call.getOffset() << " limit= " << call.getLimit()
          << " this=" << (uintptr_t)this << " id=" << node->id();
    }
  }
}

template <class Executor>
void ExecutionBlockImpl<Executor>::traceExecuteEnd(
    std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr> const& result) {
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    auto const& [state, skipped, block] = result;
    auto const items = block != nullptr ? block->size() : 0;
    ExecutionNode const* en = getPlanNode();
    ExecutionStats::Node stats;
    stats.calls = 1;
    stats.items = skipped + items;
    if (state != ExecutionState::WAITING) {
      stats.runtime = TRI_microtime() - _getSomeBegin;
      _getSomeBegin = 0.0;
    }

    auto it = _engine->_stats.nodes.find(en->id());
    if (it != _engine->_stats.nodes.end()) {
      it->second += stats;
    } else {
      _engine->_stats.nodes.emplace(en->id(), stats);
    }

    if (_profile >= PROFILE_LEVEL_TRACE_1) {
      ExecutionNode const* node = getPlanNode();
      auto const queryId = this->_engine->getQuery()->id();
      LOG_TOPIC("60bbc", INFO, Logger::QUERIES)
          << "[query#" << queryId << "] "
          << "execute done type=" << node->getTypeString() << " this=" << (uintptr_t)this
          << " id=" << node->id() << " state=" << stateToString(state)
          << " skipped=" << skipped << " produced=" << items;

      if (_profile >= PROFILE_LEVEL_TRACE_2) {
        if (block == nullptr) {
          LOG_TOPIC("9b3f4", INFO, Logger::QUERIES)
              << "[query#" << queryId << "] "
              << "execute type=" << node->getTypeString() << " result: nullptr";
        } else {
          VPackBuilder builder;
          auto const options = trxVpackOptions();
          block->toSimpleVPack(options, builder);
          LOG_TOPIC("f12f9", INFO, Logger::QUERIES)
              << "[query#" << queryId << "] "
              << "execute type=" << node->getTypeString()
              << " result: " << VPackDumper::toString(builder.slice(), options);
        }
      }
    }
  }
}

// Work around GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56480
// Without the namespaces it fails with
// error: specialization of 'template<class Executor> std::pair<arangodb::aql::ExecutionState, arangodb::Result> arangodb::aql::ExecutionBlockImpl<Executor>::initializeCursor(arangodb::aql::AqlItemBlock*, size_t)' in different namespace
namespace arangodb::aql {
// TODO -- remove this specialization when cpp 17 becomes available
template <>
std::pair<ExecutionState, Result> ExecutionBlockImpl<IdExecutor<ConstFetcher>>::initializeCursor(
    InputAqlItemRow const& input) {
  // reinitialize the DependencyProxy
  _dependencyProxy.reset();

  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_dependencyProxy);

  TRI_ASSERT(_skipped == 0);
  _skipped = 0;
  TRI_ASSERT(_state == InternalState::DONE || _state == InternalState::FETCH_DATA);
  _state = InternalState::FETCH_DATA;

  SharedAqlItemBlockPtr block =
      input.cloneToBlock(_engine->itemBlockManager(), *(infos().registersToKeep()),
                         infos().numberOfOutputRegisters());

  _rowFetcher.injectBlock(block);

  // cppcheck-suppress unreadVariable
  constexpr bool customInit = hasInitializeCursor<decltype(_executor)>::value;
  InitializeCursor<customInit>::init(_executor, _rowFetcher, _infos);

  // end of default initializeCursor
  return ExecutionBlock::initializeCursor(input);
}

// TODO the shutdown specializations shall be unified!

template <>
std::pair<ExecutionState, Result> ExecutionBlockImpl<TraversalExecutor>::shutdown(int errorCode) {
  ExecutionState state;
  Result result;

  std::tie(state, result) = ExecutionBlock::shutdown(errorCode);

  if (state == ExecutionState::WAITING) {
    return {state, result};
  }
  return this->executor().shutdown(errorCode);
}

template <>
std::pair<ExecutionState, Result> ExecutionBlockImpl<ShortestPathExecutor>::shutdown(int errorCode) {
  ExecutionState state;
  Result result;

  std::tie(state, result) = ExecutionBlock::shutdown(errorCode);
  if (state == ExecutionState::WAITING) {
    return {state, result};
  }
  return this->executor().shutdown(errorCode);
}

template <>
std::pair<ExecutionState, Result> ExecutionBlockImpl<KShortestPathsExecutor>::shutdown(int errorCode) {
  ExecutionState state;
  Result result;

  std::tie(state, result) = ExecutionBlock::shutdown(errorCode);
  if (state == ExecutionState::WAITING) {
    return {state, result};
  }
  return this->executor().shutdown(errorCode);
}

template <>
std::pair<ExecutionState, Result> ExecutionBlockImpl<SubqueryExecutor<true>>::shutdown(int errorCode) {
  ExecutionState state;
  Result subqueryResult;
  // shutdown is repeatable
  std::tie(state, subqueryResult) = this->executor().shutdown(errorCode);
  if (state == ExecutionState::WAITING) {
    return {ExecutionState::WAITING, subqueryResult};
  }
  Result result;

  std::tie(state, result) = ExecutionBlock::shutdown(errorCode);
  if (state == ExecutionState::WAITING) {
    return {state, result};
  }
  if (result.fail()) {
    return {state, result};
  }
  return {state, subqueryResult};
}

template <>
std::pair<ExecutionState, Result> ExecutionBlockImpl<SubqueryExecutor<false>>::shutdown(int errorCode) {
  ExecutionState state;
  Result subqueryResult;
  // shutdown is repeatable
  std::tie(state, subqueryResult) = this->executor().shutdown(errorCode);
  if (state == ExecutionState::WAITING) {
    return {ExecutionState::WAITING, subqueryResult};
  }
  Result result;

  std::tie(state, result) = ExecutionBlock::shutdown(errorCode);
  if (state == ExecutionState::WAITING) {
    return {state, result};
  }
  if (result.fail()) {
    return {state, result};
  }
  return {state, subqueryResult};
}

template <>
std::pair<ExecutionState, Result>
ExecutionBlockImpl<IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>::shutdown(int errorCode) {
  if (this->infos().isResponsibleForInitializeCursor()) {
    return ExecutionBlock::shutdown(errorCode);
  }
  return {ExecutionState::DONE, {errorCode}};
}

}  // namespace arangodb::aql

namespace arangodb::aql {

// The constant "PASSTHROUGH" is somehow reserved with MSVC.
enum class RequestWrappedBlockVariant {
  DEFAULT,
  PASS_THROUGH,
  INPUTRESTRICTED
};

// Specifying the namespace here is important to MSVC.
template <enum arangodb::aql::RequestWrappedBlockVariant>
struct RequestWrappedBlock {};

template <>
struct RequestWrappedBlock<RequestWrappedBlockVariant::DEFAULT> {
  /**
   * @brief Default requestWrappedBlock() implementation. Just get a new block
   *        from the AqlItemBlockManager.
   */
  template <class Executor>
  static std::pair<ExecutionState, SharedAqlItemBlockPtr> run(
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      typename Executor::Infos const&,
#endif
      Executor& executor, ExecutionEngine& engine, size_t nrItems, RegisterCount nrRegs) {
    return {ExecutionState::HASMORE,
            engine.itemBlockManager().requestBlock(nrItems, nrRegs)};
  }
};

template <>
struct RequestWrappedBlock<RequestWrappedBlockVariant::PASS_THROUGH> {
  /**
   * @brief If blocks can be passed through, we do not create new blocks.
   *        Instead, we take the input blocks and reuse them.
   */
  template <class Executor>
  static std::pair<ExecutionState, SharedAqlItemBlockPtr> run(
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      typename Executor::Infos const& infos,
#endif
      Executor& executor, ExecutionEngine& engine, size_t nrItems, RegisterCount nrRegs) {
    static_assert(Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable,
                  "This function can only be used with executors supporting "
                  "`allowsBlockPassthrough`");
    static_assert(hasFetchBlockForPassthrough<Executor>::value,
                  "An Executor with allowsBlockPassthrough must implement "
                  "fetchBlockForPassthrough");

    SharedAqlItemBlockPtr block;

    ExecutionState state;
    typename Executor::Stats executorStats;
    std::tie(state, executorStats, block) = executor.fetchBlockForPassthrough(nrItems);
    engine._stats += executorStats;

    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(block == nullptr);
      return {state, nullptr};
    }
    if (block == nullptr) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, nullptr};
    }

    // Now we must have a block.
    TRI_ASSERT(block != nullptr);
    // Assert that the block has enough registers. This must be guaranteed by
    // the register planning.
    TRI_ASSERT(block->getNrRegs() == nrRegs);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // Check that all output registers are empty.
    for (auto const& reg : *infos.getOutputRegisters()) {
      for (size_t row = 0; row < block->size(); row++) {
        AqlValue const& val = block->getValueReference(row, reg);
        TRI_ASSERT(val.isEmpty());
      }
    }
#endif

    return {ExecutionState::HASMORE, block};
  }
};

template <>
struct RequestWrappedBlock<RequestWrappedBlockVariant::INPUTRESTRICTED> {
  /**
   * @brief If the executor can set an upper bound on the output size knowing
   *        the input size, usually because size(input) >= size(output), let it
   *        prefetch an input block to give us this upper bound.
   *        Only then we allocate a new block with at most this upper bound.
   */
  template <class Executor>
  static std::pair<ExecutionState, SharedAqlItemBlockPtr> run(
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      typename Executor::Infos const&,
#endif
      Executor& executor, ExecutionEngine& engine, size_t nrItems, RegisterCount nrRegs) {
    static_assert(Executor::Properties::inputSizeRestrictsOutputSize,
                  "This function can only be used with executors supporting "
                  "`inputSizeRestrictsOutputSize`");
    static_assert(hasExpectedNumberOfRows<Executor>::value,
                  "An Executor with inputSizeRestrictsOutputSize must "
                  "implement expectedNumberOfRows");

    SharedAqlItemBlockPtr block;

    ExecutionState state;
    size_t expectedRows = 0;
    // Note: this might trigger a prefetch on the rowFetcher!
    std::tie(state, expectedRows) = executor.expectedNumberOfRows(nrItems);
    if (state == ExecutionState::WAITING) {
      return {state, nullptr};
    }
    nrItems = (std::min)(expectedRows, nrItems);
    if (nrItems == 0) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, nullptr};
    }
    block = engine.itemBlockManager().requestBlock(nrItems, nrRegs);

    return {ExecutionState::HASMORE, block};
  }
};

}  // namespace arangodb::aql

template <class Executor>
std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<Executor>::requestWrappedBlock(
    size_t nrItems, RegisterCount nrRegs) {
  static_assert(Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Disable ||
                    !Executor::Properties::inputSizeRestrictsOutputSize,
                "At most one of Properties::allowsBlockPassthrough or "
                "Properties::inputSizeRestrictsOutputSize should be true for "
                "each Executor");
  static_assert(
      (Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable) ==
          hasFetchBlockForPassthrough<Executor>::value,
      "Executors should implement the method fetchBlockForPassthrough() iff "
      "Properties::allowsBlockPassthrough is true");
  static_assert(
      Executor::Properties::inputSizeRestrictsOutputSize ==
          hasExpectedNumberOfRows<Executor>::value,
      "Executors should implement the method expectedNumberOfRows() iff "
      "Properties::inputSizeRestrictsOutputSize is true");

  constexpr RequestWrappedBlockVariant variant =
      Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable
          ? RequestWrappedBlockVariant::PASS_THROUGH
          : Executor::Properties::inputSizeRestrictsOutputSize
                ? RequestWrappedBlockVariant::INPUTRESTRICTED
                : RequestWrappedBlockVariant::DEFAULT;

  // Override for spliced subqueries, this optimization does not work there.
  if (isInSplicedSubquery() && variant == RequestWrappedBlockVariant::INPUTRESTRICTED) {
    return RequestWrappedBlock<RequestWrappedBlockVariant::DEFAULT>::run(
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        infos(),
#endif
        executor(), *_engine, nrItems, nrRegs);
  }

  return RequestWrappedBlock<variant>::run(
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      infos(),
#endif
      executor(), *_engine, nrItems, nrRegs);
}

// TODO: We need to define the size of this block based on Input / Executor / Subquery depth
template <class Executor>
auto ExecutionBlockImpl<Executor>::allocateOutputBlock(AqlCall&& call)
    -> std::unique_ptr<OutputAqlItemRow> {
  size_t blockSize = ExecutionBlock::DefaultBatchSize();
  SharedAqlItemBlockPtr newBlock =
      _engine->itemBlockManager().requestBlock(blockSize, _infos.numberOfOutputRegisters());
  return createOutputRow(newBlock, std::move(call));
}

template <class Executor>
void ExecutionBlockImpl<Executor>::ensureOutputBlock(AqlCall&& call) {
  if (_outputItemRow == nullptr || _outputItemRow->isFull()) {
    _outputItemRow = allocateOutputBlock(std::move(call));
  } else {
    _outputItemRow->setCall(std::move(call));
  }
}

/// @brief request an AqlItemBlock from the memory manager
template <class Executor>
SharedAqlItemBlockPtr ExecutionBlockImpl<Executor>::requestBlock(size_t nrItems,
                                                                 RegisterId nrRegs) {
  return _engine->itemBlockManager().requestBlock(nrItems, nrRegs);
}

// TODO move me up
enum ExecState { SKIP, PRODUCE, FULLCOUNT, UPSTREAM, SHADOWROWS, DONE };

// TODO clean me up
namespace {
// This cannot return upstream call or shadowrows.
ExecState NextState(AqlCall const& call) {
  if (call.getOffset() > 0) {
    // First skip
    return ExecState::SKIP;
  }
  if (call.getLimit() > 0) {
    // Then produce
    return ExecState::PRODUCE;
  }
  if (call.needsFullCount()) {
    // then fullcount
    return ExecState::FULLCOUNT;
  }
  // now we are done.
  return ExecState::DONE;
}

}  // namespace

//
// FETCHER:  if we have one output row per input row, we can skip
//           directly by just calling the fetcher and see whether
//           it produced any output.
//           With the new architecture we should be able to just skip
//           ahead on the input range, fetching new blocks when necessary
// EXECUTOR: the executor has a specialised skipRowsRange method
//           that will be called to skip
// GET_SOME: we just request rows from the executor and then discard
//           them
//
enum class SkipRowsRangeVariant { FETCHER, EXECUTOR, GET_SOME };

// This function is just copy&pasted from above to decide which variant of skip
// is used for which executor.
template <class Executor>
static SkipRowsRangeVariant constexpr skipRowsType() {
  bool constexpr useFetcher =
      Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable &&
      !std::is_same<Executor, SubqueryExecutor<true>>::value;

  bool constexpr useExecutor = hasSkipRowsRange<Executor>::value;

  // ConstFetcher and SingleRowFetcher<BlockPassthrough::Enable> can skip, but
  // it may not be done for modification subqueries.
  static_assert(useFetcher ==
                    (std::is_same<typename Executor::Fetcher, ConstFetcher>::value ||
                     (std::is_same<typename Executor::Fetcher, SingleRowFetcher<BlockPassthrough::Enable>>::value &&
                      !std::is_same<Executor, SubqueryExecutor<true>>::value)),
                "Unexpected fetcher for SkipVariants::FETCHER");

  static_assert(!useFetcher || hasSkipRows<typename Executor::Fetcher>::value,
                "Fetcher is chosen for skipping, but has not skipRows method!");

  static_assert(useExecutor == (std::is_same<Executor, FilterExecutor>::value),
                "Unexpected executor for SkipVariants::EXECUTOR");

  // The LimitExecutor will not work correctly with SkipVariants::FETCHER!
  static_assert(
      !std::is_same<Executor, LimitExecutor>::value || useFetcher,
      "LimitExecutor needs to implement skipRows() to work correctly");

  if (useExecutor) {
    return SkipRowsRangeVariant::EXECUTOR;
  } else if (useFetcher) {
    return SkipRowsRangeVariant::FETCHER;
  } else {
    return SkipRowsRangeVariant::GET_SOME;
  }
}

// Let's do it the C++ way.
template <class T>
struct dependent_false : std::false_type {};

template <class Executor>
std::tuple<ExecutorState, size_t, AqlCall> ExecutionBlockImpl<Executor>::executeSkipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& call) {
  if constexpr (isNewStyleExecutor<Executor>()) {
    if constexpr (skipRowsType<Executor>() == SkipRowsRangeVariant::EXECUTOR) {
      // If the executor has a method skipRowsRange, to skip outputs more
      // efficiently than just producing them to subsequently discard them, then
      // we use it
      return _executor.skipRowsRange(inputRange, call);
    } else if constexpr (skipRowsType<Executor>() == SkipRowsRangeVariant::FETCHER) {
      // If we know that every input row produces exactly one output row (this
      // is a property of the executor), then we can just let the fetcher skip
      // the number of rows that we would like to skip.
      return _rowFetcher.execute(call);
    } else if constexpr (skipRowsType<Executor>() == SkipRowsRangeVariant::GET_SOME) {
      // In all other cases, we skip by letting the executor produce rows, and
      // then throw them away.

      size_t toSkip = std::min(call.getOffset(), DefaultBatchSize());
      AqlCall skipCall{};
      skipCall.softLimit = toSkip;
      skipCall.hardLimit = toSkip;
      skipCall.offset = 0;

      // we can't mess with _outputItemRow,
      auto skipOutput = allocateOutputBlock(std::move(skipCall));
      auto [state, stats, rescall] = _executor.produceRows(inputRange, *skipOutput);
      auto skipped = skipOutput->numRowsWritten();

      call.didSkip(skipped);
      return std::make_tuple(state, skipped, rescall);
    } else {
      static_assert(dependent_false<Executor>::value,
                    "This value of SkipRowsRangeVariant is not supported");
      return std::make_tuple(ExecutorState::DONE, 0, call);
    }
  } else {
    TRI_ASSERT(false);
    return std::make_tuple(ExecutorState::DONE, 0, call);
  }
  // Compiler is unhappy without this.
  return std::make_tuple(ExecutorState::DONE, 0, call);
}

// This is the central function of an executor, and it acts like a
// coroutine: It can be called multiple times and keeps state across
// calls.
//
// The intended behaviour of this function is best described in terms of
// a state machine; the possible states are the ExecStates
// SKIP, PRODUCE, FULLCOUNT, UPSTREAM, SHADOWROWS, DONE
//
// SKIP       skipping rows. How rows are skipped is determined by
//            the Executor that is used. See SkipVariants
// PRODUCE    calls produceRows of the executor
// FULLCOUNT
// UPSTREAM   fetches rows from the upstream executor(s) to be processed by
//            our executor.
// SHADOWROWS process any shadow rows
// DONE       processing is done
template <class Executor>
std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr>
ExecutionBlockImpl<Executor>::executeWithoutTrace(AqlCallStack stack) {
  if constexpr (isNewStyleExecutor<Executor>()) {
    // Make sure there's a block allocated and set
    // the call
    ensureOutputBlock(stack.popCall());

    auto skipped = size_t{0};

    TRI_ASSERT(_outputItemRow);

    auto execState = ::NextState(_outputItemRow->getClientCall());

    if (_lastRange.hasShadowRow()) {
      // We have not been able to move all shadowRows into the output last time.
      // Continue from there.
      // TODO test if this works with COUNT COLLECT
      execState = ExecState::SHADOWROWS;
    }

    AqlCall executorRequest;
    while (execState != ExecState::DONE && !_outputItemRow->allRowsUsed()) {
      switch (execState) {
        case ExecState::SKIP: {
          auto const& clientCall = _outputItemRow->getClientCall();
          auto [state, skippedLocal, call] =
              executeSkipRowsRange(_lastRange, _outputItemRow->getModifiableClientCall());
          skipped += skippedLocal;

          if (state == ExecutorState::DONE) {
            execState = ExecState::SHADOWROWS;
          } else if (clientCall.getOffset() > 0) {
            TRI_ASSERT(_upstreamState != ExecutionState::DONE);
            // We need to request more
            executorRequest = call;
            execState = ExecState::UPSTREAM;
          } else {
            // We are done with skipping. Skip is not allowed to request more
            execState = ::NextState(clientCall);
          }
          break;
        }
        case ExecState::PRODUCE: {
          auto const& clientCall = _outputItemRow->getClientCall();
          TRI_ASSERT(clientCall.getLimit() > 0);
          // Execute getSome
          auto const [state, stats, call] =
              _executor.produceRows(_lastRange, *_outputItemRow);
          _engine->_stats += stats;
          if (state == ExecutorState::DONE) {
            execState = ExecState::SHADOWROWS;
          } else if (clientCall.getLimit() > 0 && !_lastRange.hasDataRow()) {
            TRI_ASSERT(_upstreamState != ExecutionState::DONE);
            // We need to request more
            executorRequest = call;
            execState = ExecState::UPSTREAM;
          } else {
            // We are done with producing. Produce is not allowed to request more
            execState = ::NextState(clientCall);
          }
          break;
        }
        case ExecState::FULLCOUNT: {
          TRI_ASSERT(false);
          // TODO: wat.
        }
        case ExecState::UPSTREAM: {
          // If this triggers the executors produceRows function has returned
          // HASMORE even if it knew that upstream has no further rows.
          TRI_ASSERT(_upstreamState != ExecutionState::DONE);
          TRI_ASSERT(!_lastRange.hasDataRow());
          size_t skippedLocal = 0;
          stack.pushCall(std::move(executorRequest));
          std::tie(_upstreamState, skippedLocal, _lastRange) = _rowFetcher.execute(stack);
          if (_upstreamState == ExecutionState::WAITING) {
            // We do not return anything in WAITING state, also NOT skipped.
            // TODO: Check if we need to leverage this restriction.
            TRI_ASSERT(skipped == 0);
            return {_upstreamState, 0, nullptr};
          }
          skipped += skippedLocal;
          ensureOutputBlock(_outputItemRow->stealClientCall());
          // Do we need to call it?
          // clientCall.didSkip(skippedLocal);
          execState = ::NextState(_outputItemRow->getClientCall());
          break;
        }
        case ExecState::SHADOWROWS: {
          // TODO: Check if there is a situation where we are at this point, but at the end of a block
          // Or if we would not recognize this beforehand
          // TODO: Check if we can have the situation that we are between two shadow rows here.
          // E.g. LastRow is releveant shadowRow. NextRow is non-relevant shadowRow.
          // NOTE: I do not think this is an issue, as the Executor will always say that it cannot do anything with
          // an empty input. Only exception might be COLLECT COUNT.
          if (_lastRange.hasShadowRow()) {
            auto const& [state, shadowRow] = _lastRange.nextShadowRow();
            TRI_ASSERT(shadowRow.isInitialized());
            _outputItemRow->copyRow(shadowRow);
            if (shadowRow.isRelevant()) {
              // We found a relevant shadow Row.
              // We need to reset the Executor
              // TODO: call reset!
            }
            TRI_ASSERT(_outputItemRow->produced());
            _outputItemRow->advanceRow();
            if (state == ExecutorState::DONE) {
              if (_lastRange.hasDataRow()) {
                // TODO this state is invalid, and can just show up now if we exclude SKIP
                execState = ExecState::PRODUCE;
              } else {
                // Right now we cannot support to have more than one set of
                // ShadowRows inside of a Range.
                // We do not know how to continue with the above executor after a shadowrow.
                TRI_ASSERT(!_lastRange.hasDataRow());
                execState = ExecState::DONE;
              }
            }
          } else {
            execState = ExecState::DONE;
          }
          break;
        }
        default:
          // unreachable
          TRI_ASSERT(false);
      }
    }

    auto outputBlock = _outputItemRow->stealBlock();
    // This is not strictly necessary here, as we shouldn't be called again
    // after DONE.
    _outputItemRow.reset();
    if (_lastRange.hasDataRow() || _lastRange.hasShadowRow()) {
      // We have skipped or/and return data, otherwise we cannot return HASMORE
      TRI_ASSERT(skipped > 0 || (outputBlock != nullptr && outputBlock->numEntries() > 0));
      return {ExecutionState::HASMORE, skipped, std::move(outputBlock)};
    }
    return {_upstreamState, skipped, std::move(outputBlock)};
  } else {
    // TODO this branch must never be taken with an executor that has not been
    //      converted yet
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}

/// @brief reset all internal states after processing a shadow row.
template <class Executor>
void ExecutionBlockImpl<Executor>::resetAfterShadowRow() {
  // cppcheck-suppress unreadVariable
  constexpr bool customInit = hasInitializeCursor<decltype(_executor)>::value;
  InitializeCursor<customInit>::init(_executor, _rowFetcher, _infos);
}

template <class Executor>
ExecutionState ExecutionBlockImpl<Executor>::fetchShadowRowInternal() {
  TRI_ASSERT(_state == InternalState::FETCH_SHADOWROWS);
  TRI_ASSERT(!_outputItemRow->isFull());
  ExecutionState state = ExecutionState::HASMORE;
  ShadowAqlItemRow shadowRow{CreateInvalidShadowRowHint{}};
  // TODO: Add lazy evaluation in case of LIMIT "lying" on done
  std::tie(state, shadowRow) = _rowFetcher.fetchShadowRow();
  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(!shadowRow.isInitialized());
    return state;
  }

  if (state == ExecutionState::DONE) {
    _state = InternalState::DONE;
  }
  if (shadowRow.isInitialized()) {
    _outputItemRow->copyRow(shadowRow);
    TRI_ASSERT(_outputItemRow->produced());
    _outputItemRow->advanceRow();
  } else {
    if (_state != InternalState::DONE) {
      _state = InternalState::FETCH_DATA;
      resetAfterShadowRow();
    }
  }
  return state;
}

template class ::arangodb::aql::ExecutionBlockImpl<CalculationExecutor<CalculationType::Condition>>;
template class ::arangodb::aql::ExecutionBlockImpl<CalculationExecutor<CalculationType::Reference>>;
template class ::arangodb::aql::ExecutionBlockImpl<CalculationExecutor<CalculationType::V8Condition>>;
template class ::arangodb::aql::ExecutionBlockImpl<ConstrainedSortExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<CountCollectExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<DistinctCollectExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<EnumerateCollectionExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<EnumerateListExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<FilterExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<HashedCollectExecutor>;

template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::Materialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::Materialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::ExecutionBlockImpl<
    IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::Materialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<
    false, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<
    false, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::NotMaterialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::LateMaterialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::Materialize>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<
    true, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<
    true, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>>;

template class ::arangodb::aql::ExecutionBlockImpl<IdExecutor<ConstFetcher>>;
template class ::arangodb::aql::ExecutionBlockImpl<IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>;
template class ::arangodb::aql::ExecutionBlockImpl<IndexExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<LimitExecutor>;

// IndexTag, Insert, Remove, Update,Replace, Upsert are only tags for this one
template class ::arangodb::aql::ExecutionBlockImpl<SingleRemoteModificationExecutor<IndexTag>>;
template class ::arangodb::aql::ExecutionBlockImpl<SingleRemoteModificationExecutor<Insert>>;
template class ::arangodb::aql::ExecutionBlockImpl<SingleRemoteModificationExecutor<Remove>>;
template class ::arangodb::aql::ExecutionBlockImpl<SingleRemoteModificationExecutor<Update>>;
template class ::arangodb::aql::ExecutionBlockImpl<SingleRemoteModificationExecutor<Replace>>;
template class ::arangodb::aql::ExecutionBlockImpl<SingleRemoteModificationExecutor<Upsert>>;

template class ::arangodb::aql::ExecutionBlockImpl<NoResultsExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<ReturnExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<ShortestPathExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<KShortestPathsExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SortedCollectExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SortExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SubqueryEndExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SubqueryExecutor<true>>;
template class ::arangodb::aql::ExecutionBlockImpl<SubqueryExecutor<false>>;
template class ::arangodb::aql::ExecutionBlockImpl<SubqueryStartExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<TraversalExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SortingGatherExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<ParallelUnsortedGatherExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<UnsortedGatherExecutor>;

template class ::arangodb::aql::ExecutionBlockImpl<MaterializeExecutor<RegisterId>>;
template class ::arangodb::aql::ExecutionBlockImpl<MaterializeExecutor<std::string const&>>;

template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<AllRowsFetcher, InsertModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, InsertModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<AllRowsFetcher, RemoveModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, RemoveModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<AllRowsFetcher, UpdateReplaceModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, UpdateReplaceModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<AllRowsFetcher, UpsertModifier>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, UpsertModifier>>;
