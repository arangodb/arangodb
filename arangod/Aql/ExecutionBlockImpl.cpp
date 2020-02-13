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

#include <boost/core/demangle.hpp>

#include <type_traits>

using namespace arangodb;
using namespace arangodb::aql;

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

#ifdef ARANGODB_USE_GOOGLE_TESTS
// Forward declaration of Test Executors.
// only used as long as isNewStyleExecutor is required.
namespace arangodb {
namespace aql {
class TestLambdaExecutor;

class TestLambdaSkipExecutor;
}  // namespace aql
}  // namespace arangodb
#endif

template <typename T, typename... Es>
constexpr bool is_one_of_v = (std::is_same_v<T, Es> || ...);

/*
 * Determine whether we execute new style or old style skips, i.e. pre or post shadow row introduction
 * TODO: This should be removed once all executors and fetchers are ported to the new style.
 */
template <typename Executor>
constexpr bool isNewStyleExecutor =
    is_one_of_v<Executor, FilterExecutor, SortedCollectExecutor, ReturnExecutor,
#ifdef ARANGODB_USE_GOOGLE_TESTS
                TestLambdaExecutor, TestLambdaSkipExecutor,  // we need one after these to avoid compile errors in non-test mode
#endif
                ShortestPathExecutor>;

template <class Executor>
ExecutionBlockImpl<Executor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                 ExecutionNode const* node,
                                                 typename Executor::Infos infos)
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
      _lastRange{ExecutorState::HASMORE},
      _execState{ExecState::CHECKCALL},
      _upstreamRequest{},
      _clientRequest{},
      _hasUsedDataRangeBlock{false} {
  // already insert ourselves into the statistics results
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    _engine->_stats.nodes.try_emplace(node->id(), ExecutionStats::Node());
  }
}

template <class Executor>
ExecutionBlockImpl<Executor>::~ExecutionBlockImpl() = default;

template <class Executor>
std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<Executor>::getSome(size_t atMost) {
  if constexpr (isNewStyleExecutor<Executor>) {
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
  TRI_ASSERT(atMost <= ExecutionBlock::DefaultBatchSize);
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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (newBlock != nullptr) {
    // Assert that the block has enough registers. This must be guaranteed by
    // the register planning.
    TRI_ASSERT(newBlock->getNrRegs() == _infos.numberOfOutputRegisters());
    // Check that all output registers are empty.
    for (auto const& reg : *_infos.getOutputRegisters()) {
      for (size_t row = 0; row < newBlock->size(); row++) {
        AqlValue const& val = newBlock->getValueReference(row, reg);
        TRI_ASSERT(val.isEmpty());
      }
    }
  }
#endif

  if /* constexpr */ (Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable) {
    return std::make_unique<OutputAqlItemRow>(newBlock, infos().getOutputRegisters(),
                                              infos().registersToKeep(),
                                              infos().registersToClear(), call,
                                              OutputAqlItemRow::CopyRowBehavior::DoNotCopyInputRows);
  } else {
    return std::make_unique<OutputAqlItemRow>(newBlock, infos().getOutputRegisters(),
                                              infos().registersToKeep(),
                                              infos().registersToClear(), call);
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
  if constexpr (isNewStyleExecutor<Executor>) {
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
    atMost = std::min(atMost, DefaultBatchSize);
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
  if constexpr (isNewStyleExecutor<Executor>) {
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
      if (state != ExecutionState::DONE) {
        auto const executorName = boost::core::demangle(typeid(Executor).name());
        THROW_ARANGO_EXCEPTION_FORMAT(
            TRI_ERROR_INTERNAL_AQL,
            "Unexpected result of expectedNumberOfRows in %s", executorName.c_str());
      }
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
  if constexpr (!isNewStyleExecutor<Executor>) {
    static_assert(Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Disable ||
                      !Executor::Properties::inputSizeRestrictsOutputSize,
                  "At most one of Properties::allowsBlockPassthrough or "
                  "Properties::inputSizeRestrictsOutputSize should be true for "
                  "each Executor");
    static_assert((Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable) ==
                      hasFetchBlockForPassthrough<Executor>::value,
                  "Executors should implement the method "
                  "fetchBlockForPassthrough() iff "
                  "Properties::allowsBlockPassthrough is true");
  }
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
  if constexpr (Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable) {
    SharedAqlItemBlockPtr newBlock{nullptr};
    // Passthrough variant, re-use the block stored in InputRange
    if (!_hasUsedDataRangeBlock) {
      // In the pass through variant we have the contract that we work on a
      // block all or nothing, so if we have used the block once, we cannot use it again
      // however we cannot remove the _lastRange as it may contain additional information.
      newBlock = _lastRange.getBlock();
      _hasUsedDataRangeBlock = true;
    }

    return createOutputRow(newBlock, std::move(call));
  } else {
    // Non-Passthrough variant, we need to allocate the block ourselfs
    size_t blockSize = ExecutionBlock::DefaultBatchSize;
    SharedAqlItemBlockPtr newBlock =
        _engine->itemBlockManager().requestBlock(blockSize, _infos.numberOfOutputRegisters());
    return createOutputRow(newBlock, std::move(call));
  }
}

template <class Executor>
void ExecutionBlockImpl<Executor>::ensureOutputBlock(AqlCall&& call) {
  if (_outputItemRow == nullptr || !_outputItemRow->isInitialized()) {
    _outputItemRow = allocateOutputBlock(std::move(call));
  } else {
    _outputItemRow->setCall(std::move(call));
  }
}

// This cannot return upstream call or shadowrows.
template <class Executor>
auto ExecutionBlockImpl<Executor>::nextState(AqlCall const& call) const -> ExecState {
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
  if (call.hardLimit == 0) {
    // We reached hardLimit, fast forward
    return ExecState::FASTFORWARD;
  }
  // now we are done.
  return ExecState::DONE;
}

/// @brief request an AqlItemBlock from the memory manager
template <class Executor>
SharedAqlItemBlockPtr ExecutionBlockImpl<Executor>::requestBlock(size_t nrItems,
                                                                 RegisterId nrRegs) {
  return _engine->itemBlockManager().requestBlock(nrItems, nrRegs);
}

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
enum class SkipRowsRangeVariant { FETCHER, EXECUTOR };

// This function is just copy&pasted from above to decide which variant of
// skip is used for which executor.
template <class Executor>
static SkipRowsRangeVariant constexpr skipRowsType() {
  bool constexpr useFetcher =
      Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable &&
      !std::is_same<Executor, SubqueryExecutor<true>>::value;

  bool constexpr useExecutor = hasSkipRowsRange<Executor>::value;

  // ConstFetcher and SingleRowFetcher<BlockPassthrough::Enable> can skip, but
  // it may not be done for modification subqueries.
  static_assert(useFetcher ==
                    (std::is_same_v<typename Executor::Fetcher, ConstFetcher> ||
                     (std::is_same_v<typename Executor::Fetcher, SingleRowFetcher<BlockPassthrough::Enable>> &&
                      !std::is_same<Executor, SubqueryExecutor<true>>::value)),
                "Unexpected fetcher for SkipVariants::FETCHER");

  static_assert(!useFetcher || hasSkipRows<typename Executor::Fetcher>::value,
                "Fetcher is chosen for skipping, but has not skipRows method!");

  static_assert(useExecutor == (is_one_of_v<Executor, FilterExecutor, ShortestPathExecutor, ReturnExecutor,
#ifdef ARANGODB_USE_GOOGLE_TESTS
                                            TestLambdaSkipExecutor,
#endif
                                            SortedCollectExecutor>),
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
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}

// Let's do it the C++ way.
template <class T>
struct dependent_false : std::false_type {};

template <class Executor>
std::tuple<ExecutorState, typename Executor::Stats, size_t, AqlCall>
ExecutionBlockImpl<Executor>::executeSkipRowsRange(AqlItemBlockInputRange& inputRange,
                                                   AqlCall& call) {
  if constexpr (isNewStyleExecutor<Executor>) {
    call.skippedRows = 0;
    if constexpr (skipRowsType<Executor>() == SkipRowsRangeVariant::EXECUTOR) {
      // If the executor has a method skipRowsRange, to skip outputs.
      // Every non-passthrough executor needs to implement this.
      return _executor.skipRowsRange(inputRange, call);
    } else if constexpr (skipRowsType<Executor>() == SkipRowsRangeVariant::FETCHER) {
      // If we know that every input row produces exactly one output row (this
      // is a property of the executor), then we can just let the fetcher skip
      // the number of rows that we would like to skip.
      // Returning this will trigger to end in upstream state now, with the
      // call that was handed it.
      static_assert(
          std::is_same_v<typename Executor::Stats, NoStats>,
          "Executors with custom statistics must implement skipRowsRange.");
      return {inputRange.upstreamState(), NoStats{}, 0, call};
    } else {
      static_assert(dependent_false<Executor>::value,
                    "This value of SkipRowsRangeVariant is not supported");
      return std::make_tuple(ExecutorState::DONE, typename Executor::Stats{}, 0, call);
    }
  } else {
    TRI_ASSERT(false);
    return std::make_tuple(ExecutorState::DONE, typename Executor::Stats{}, 0, call);
  }
  // Compiler is unhappy without this.
  return std::make_tuple(ExecutorState::DONE, typename Executor::Stats{}, 0, call);
}

/**
 * @brief This is the central function of an executor, and it acts like a
 * coroutine: It can be called multiple times and keeps state across
 * calls.
 *
 * The intended behaviour of this function is best described in terms of
 * a state machine; the possible states are the ExecStates
 * SKIP, PRODUCE, FULLCOUNT, FASTFORWARD, UPSTREAM, SHADOWROWS, DONE
 *
 * SKIP       skipping rows. How rows are skipped is determined by
 *            the Executor that is used. See SkipVariants
 * PRODUCE    calls produceRows of the executor
 * FULLCOUNT  again skipping rows. like skip, but will skip all rows
 * FASTFORWARD like fullcount, but does not count skipped rows.
 * UPSTREAM   fetches rows from the upstream executor(s) to be processed by
 *            our executor.
 * SHADOWROWS process any shadow rows
 * DONE       processing of one output is done. We did handle offset / limit / fullCount without crossing BatchSize limits.
 *            This state does not indicate that we are DONE with all input, we are just done with one walk through this statemachine.
 *
 * We progress within the states in the following way:
 *   There is a nextState method that determines the next state based on the call, it can only lead to:
 *   SKIP, PRODUCE, FULLCOUNT, FASTFORWAD, DONE
 *
 *   On the first call we will use nextState to get to our starting point.
 *   After any of SKIP, PRODUCE, FULLCOUNT, FASTFORWAD, DONE We either go to
 *   1. DONE (if output is full)
 *   2. SHADOWROWS (if executor is done)
 *   3. UPSTREAM if executor has More, (Invariant: input fully consumed)
 *   4. NextState (if none of the above applies)
 *
 *   From SHADOWROWS we can only go to DONE
 *   From UPSTREAM we go to NextState.
 *
 * @tparam Executor The Executor that will implement the logic of what needs to happen to the data
 * @param stack The call stack of lower levels
 * @return std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr>
 *        ExecutionState: WAITING -> We wait for IO, secure state, return you will be called again
 *        ExecutionState: HASMORE -> We still have data
 *        ExecutionState: DONE -> We do not have any more data, do never call again
 *        size_t -> Amount of documents skipped within this one call. (contains offset and fullCount)
 *        SharedAqlItemBlockPtr -> The resulting data
 */
template <class Executor>
std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr>
ExecutionBlockImpl<Executor>::executeWithoutTrace(AqlCallStack stack) {
  if constexpr (isNewStyleExecutor<Executor>) {
    if (!stack.isRelevant()) {
      // We are bypassing subqueries.
      // This executor is not allowed to perform actions
      // However we need to maintain the upstream state.
      size_t skippedLocal = 0;
      typename Fetcher::DataRange bypassedRange{ExecutorState::HASMORE};
      std::tie(_upstreamState, skippedLocal, bypassedRange) = _rowFetcher.execute(stack);
      return {_upstreamState, skippedLocal, bypassedRange.getBlock()};
    }
    AqlCall clientCall = stack.popCall();

    // We can only have returned the following internal states
    TRI_ASSERT(_execState == ExecState::CHECKCALL || _execState == ExecState::SHADOWROWS ||
               _execState == ExecState::UPSTREAM);
    // Skip can only be > 0 if we are in upstream cases.
    TRI_ASSERT(_skipped == 0 || _execState == ExecState::UPSTREAM);
    if (_execState == ExecState::UPSTREAM) {
      // We have been in waiting state.
      // We may have local work on the original call.
      // The client does not have the right to change her
      // mind just because we told her to hold the line.

      // The client cannot request less data!
      TRI_ASSERT(_clientRequest.getOffset() <= clientCall.getOffset());
      TRI_ASSERT(_clientRequest.getLimit() <= clientCall.getLimit());
      TRI_ASSERT(_clientRequest.needsFullCount() == clientCall.needsFullCount());
      clientCall = _clientRequest;
    }

    while (_execState != ExecState::DONE) {
      switch (_execState) {
        case ExecState::CHECKCALL: {
          _execState = nextState(clientCall);
          break;
        }
        case ExecState::SKIP: {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          size_t offsetBefore = clientCall.getOffset();
          TRI_ASSERT(offsetBefore > 0);
          size_t canPassFullcount =
              clientCall.getLimit() == 0 && clientCall.needsFullCount();
#endif
          auto [state, stats, skippedLocal, call] =
              executeSkipRowsRange(_lastRange, clientCall);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          // Assertion: We did skip 'skippedLocal' documents here.
          // This means that they have to be removed from clientCall.getOffset()
          // This has to be done by the Executor calling call.didSkip()
          // accordingly.
          if (canPassFullcount) {
            // In htis case we can first skip. But straight after continue with fullCount, so we might skip more
            TRI_ASSERT(clientCall.getOffset() + skippedLocal >= offsetBefore);
            if (clientCall.getOffset() + skippedLocal > offsetBefore) {
              // First need to count down offset.
              TRI_ASSERT(clientCall.getOffset() == 0);
            }
          } else {
            TRI_ASSERT(clientCall.getOffset() + skippedLocal == offsetBefore);
          }
#endif
          _skipped += skippedLocal;
          _engine->_stats += stats;
          // The execute might have modified the client call.
          if (state == ExecutorState::DONE) {
            _execState = ExecState::SHADOWROWS;
          } else if (clientCall.getOffset() > 0) {
            TRI_ASSERT(_upstreamState != ExecutionState::DONE);
            // We need to request more
            _upstreamRequest = call;
            _execState = ExecState::UPSTREAM;
          } else {
            // We are done with skipping. Skip is not allowed to request more
            _execState = ExecState::CHECKCALL;
          }
          break;
        }
        case ExecState::PRODUCE: {
          // Make sure there's a block allocated and set
          // the call
          TRI_ASSERT(clientCall.getLimit() > 0);
          ensureOutputBlock(std::move(clientCall));
          TRI_ASSERT(_outputItemRow);

          // Execute getSome
          auto const [state, stats, call] =
              _executor.produceRows(_lastRange, *_outputItemRow);
          _engine->_stats += stats;

          // Produce might have modified the clientCall
          clientCall = _outputItemRow->getClientCall();

          if (_outputItemRow->isInitialized() && _outputItemRow->allRowsUsed()) {
            _execState = ExecState::DONE;
          } else if (state == ExecutorState::DONE) {
            _execState = ExecState::SHADOWROWS;
          } else if (clientCall.getLimit() > 0 && !_lastRange.hasDataRow()) {
            TRI_ASSERT(_upstreamState != ExecutionState::DONE);
            // We need to request more
            _upstreamRequest = call;
            _execState = ExecState::UPSTREAM;
          } else {
            // We are done with producing. Produce is not allowed to request more
            _execState = ExecState::CHECKCALL;
          }
          break;
        }
        case ExecState::FASTFORWARD: {
          // We can either do FASTFORWARD or FULLCOUNT, difference is that
          // fullcount counts what is produced now, FASTFORWARD simply drops
          TRI_ASSERT(!clientCall.needsFullCount());
          // We can drop all dataRows from upstream

          while (_lastRange.hasDataRow()) {
            auto [state, row] = _lastRange.nextDataRow();
            TRI_ASSERT(row.isInitialized());
          }
          if (_lastRange.upstreamState() == ExecutorState::DONE) {
            _execState = ExecState::SHADOWROWS;
          } else {
            // We need to request more, simply send hardLimit 0 upstream
            _upstreamRequest = AqlCall{};
            _upstreamRequest.hardLimit = 0;
            _execState = ExecState::UPSTREAM;
          }
          break;
        }
        case ExecState::FULLCOUNT: {
          auto [state, stats, skippedLocal, call] =
              executeSkipRowsRange(_lastRange, clientCall);
          _skipped += skippedLocal;
          _engine->_stats += stats;

          if (state == ExecutorState::DONE) {
            _execState = ExecState::SHADOWROWS;
          } else {
            // We need to request more
            _upstreamRequest = call;
            _execState = ExecState::UPSTREAM;
          }
          break;
        }
        case ExecState::UPSTREAM: {
          // If this triggers the executors produceRows function has returned
          // HASMORE even if it knew that upstream has no further rows.
          TRI_ASSERT(_upstreamState != ExecutionState::DONE);
          // We need to make sure _lastRange is all used
          TRI_ASSERT(!_lastRange.hasDataRow());
          TRI_ASSERT(!_lastRange.hasShadowRow());
          size_t skippedLocal = 0;
          auto callCopy = _upstreamRequest;
          stack.pushCall(std::move(callCopy));
          std::tie(_upstreamState, skippedLocal, _lastRange) = _rowFetcher.execute(stack);
          if (_upstreamState == ExecutionState::WAITING) {
            // We need to persist the old call before we return.
            // We might have some local accounting to this call.
            _clientRequest = clientCall;
            // We do not return anything in WAITING state, also NOT skipped.
            return {_upstreamState, 0, nullptr};
          }
          // We have a new range, passthrough can use this range.
          _hasUsedDataRangeBlock = false;
          _skipped += skippedLocal;
          // We skipped through passthroug, so count that a skip was solved.
          clientCall.didSkip(skippedLocal);
          _execState = ExecState::CHECKCALL;
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
            ensureOutputBlock(std::move(clientCall));
            TRI_ASSERT(_outputItemRow);
            TRI_ASSERT(_outputItemRow->isInitialized());

            _outputItemRow->copyRow(shadowRow);

            if (shadowRow.isRelevant()) {
              // We found a relevant shadow Row.
              // We need to reset the Executor
              // cppcheck-suppress unreadVariable
              constexpr bool customInit = hasInitializeCursor<decltype(_executor)>::value;
              InitializeCursor<customInit>::init(_executor, _rowFetcher, _infos);
            }
            TRI_ASSERT(_outputItemRow->produced());
            _outputItemRow->advanceRow();
            clientCall = _outputItemRow->getClientCall();
            if (_outputItemRow->allRowsUsed()) {
              _execState = ExecState::DONE;
            } else if (state == ExecutorState::DONE) {
              if (_lastRange.hasDataRow()) {
                // TODO this state is invalid, and can just show up now if we exclude SKIP
                _execState = ExecState::PRODUCE;
              } else {
                // Right now we cannot support to have more than one set of
                // ShadowRows inside of a Range.
                // We do not know how to continue with the above executor after a shadowrow.
                TRI_ASSERT(!_lastRange.hasDataRow());
                _execState = ExecState::DONE;
              }
            }
          } else {
            _execState = ExecState::DONE;
          }
          break;
        }
        default:
          // unreachable
          TRI_ASSERT(false);
      }
    }
    // If we do not have an output, we simply return a nullptr here.
    auto outputBlock = _outputItemRow != nullptr ? _outputItemRow->stealBlock()
                                                 : SharedAqlItemBlockPtr{nullptr};
    // We are locally done with our output.
    // Next time we need to check the client call again
    _execState = ExecState::CHECKCALL;
    // This is not strictly necessary here, as we shouldn't be called again
    // after DONE.
    _outputItemRow.reset();

    // We return skipped here, reset member
    size_t skipped = _skipped;
    _skipped = 0;
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
