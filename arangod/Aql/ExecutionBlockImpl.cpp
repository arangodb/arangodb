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

#define LOG_QUERY(logId, level)            \
  LOG_TOPIC(logId, level, Logger::QUERIES) \
      << "[query#" << this->_engine->getQuery()->id() << "] "

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
    is_one_of_v<Executor, FilterExecutor, SortedCollectExecutor, IdExecutor<ConstFetcher>,
                IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>, ReturnExecutor, IndexExecutor, EnumerateCollectionExecutor,
                /*
                                CalculationExecutor<CalculationType::Condition>, CalculationExecutor<CalculationType::Reference>,
                                CalculationExecutor<CalculationType::V8Condition>,*/
                HashedCollectExecutor,
#ifdef ARANGODB_USE_GOOGLE_TESTS
                TestLambdaExecutor,
                TestLambdaSkipExecutor,  // we need one after these to avoid compile errors in non-test mode
#endif
                SubqueryStartExecutor, SubqueryEndExecutor, TraversalExecutor, KShortestPathsExecutor,
                ShortestPathExecutor, EnumerateListExecutor, LimitExecutor, SortExecutor>;

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
  if constexpr (isNewStyleExecutor<Executor>) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
  } else {
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
        // Count global but executor-specific statistics, like number of filtered rows.
        _engine->_stats += executorStats;
        if (_outputItemRow->produced()) {
          _outputItemRow->advanceRow();
        }

        if (state == ExecutionState::WAITING) {
          return {state, nullptr};
        }

        if (state == ExecutionState::DONE) {
          auto outputBlock = _outputItemRow->stealBlock();
          // This is not strictly necessary here, as we shouldn't be called again after DONE.
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
  static_assert(!isNewStyleExecutor<Executor>);
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
  if constexpr (isNewStyleExecutor<Executor>) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
  } else {
    constexpr SkipVariants customSkipType = skipType<Executor>();

    if constexpr (customSkipType == SkipVariants::GET_SOME) {
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
  _hasUsedDataRangeBlock = false;

  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_dependencyProxy);

  TRI_ASSERT(_skipped == 0);
  _skipped = 0;
  TRI_ASSERT(_state == InternalState::DONE || _state == InternalState::FETCH_DATA);
  _state = InternalState::FETCH_DATA;

  resetExecutor();

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

    auto res = executeWithoutTrace(stack);
    traceExecuteEnd(res);
    return res;
  }

  // Fall back to getSome/skipSome
  auto myCall = stack.popCall();

  TRI_ASSERT(AqlCall::IsSkipSomeCall(myCall) || AqlCall::IsGetSomeCall(myCall) ||
             AqlCall::IsFullCountCall(myCall) || AqlCall::IsFastForwardCall(myCall));
  _rowFetcher.useStack(stack);

  if (AqlCall::IsSkipSomeCall(myCall)) {
    auto const [state, skipped] = skipSome(myCall.getOffset());
    if (state != ExecutionState::WAITING) {
      myCall.didSkip(skipped);
    }
    return {state, skipped, nullptr};
  } else if (AqlCall::IsGetSomeCall(myCall)) {
    auto const [state, block] = getSome(myCall.getLimit());
    // We do not need to count as softLimit will be overwritten, and hard cannot be set.
    if (stack.empty() && myCall.hasHardLimit() && !myCall.needsFullCount() && block != nullptr) {
      // However we can do a short-cut here to report DONE on hardLimit if we are on the top-level query.
      myCall.didProduce(block->size());
      if (myCall.getLimit() == 0) {
        return {ExecutionState::DONE, 0, block};
      }
    }

    return {state, 0, block};
  } else if (AqlCall::IsFullCountCall(myCall)) {
    auto const [state, skipped] = skipSome(ExecutionBlock::SkipAllSize());
    if (state != ExecutionState::WAITING) {
      myCall.didSkip(skipped);
    }
    return {state, skipped, nullptr};
  } else if (AqlCall::IsFastForwardCall(myCall)) {
    // No idea if DONE is correct here...
    return {ExecutionState::DONE, 0, nullptr};
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
template <>
auto ExecutionBlockImpl<IdExecutor<ConstFetcher>>::injectConstantBlock<IdExecutor<ConstFetcher>>(SharedAqlItemBlockPtr block)
    -> void {
  // reinitialize the DependencyProxy
  _dependencyProxy.reset();

  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_dependencyProxy);

  TRI_ASSERT(_skipped == 0);
  _skipped = 0;
  TRI_ASSERT(_state == InternalState::DONE || _state == InternalState::FETCH_DATA);
  _state = InternalState::FETCH_DATA;

  // Reset state of execute
  _lastRange = AqlItemBlockInputRange{ExecutorState::HASMORE};
  _hasUsedDataRangeBlock = false;
  _upstreamState = ExecutionState::HASMORE;

  _rowFetcher.injectBlock(block);

  resetExecutor();
}

// TODO -- remove this specialization when cpp 17 becomes available
template <>
std::pair<ExecutionState, Result> ExecutionBlockImpl<IdExecutor<ConstFetcher>>::initializeCursor(
    InputAqlItemRow const& input) {
  SharedAqlItemBlockPtr block =
      input.cloneToBlock(_engine->itemBlockManager(), *(infos().registersToKeep()),
                         infos().numberOfOutputRegisters());

  injectConstantBlock(block);

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
      isNewStyleExecutor<Executor>
          ? RequestWrappedBlockVariant::DEFAULT
          : Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable
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
  if (_executorReturnedDone) {
    return ExecState::FASTFORWARD;
  }
  if (call.getOffset() > 0) {
    // First skip
    return ExecState::SKIP;
  }
  if (call.getLimit() > 0) {
    // Then produce
    return ExecState::PRODUCE;
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
// SUBQUERY_START:
// SUBQUERY_END:
//
enum class SkipRowsRangeVariant {
  FETCHER,
  EXECUTOR,
  SUBQUERY_START,
  SUBQUERY_END
};

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

  static_assert(
      useExecutor ==
          (is_one_of_v<Executor, FilterExecutor, ShortestPathExecutor, ReturnExecutor, KShortestPathsExecutor,
                       IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>,
                       IdExecutor<ConstFetcher>, HashedCollectExecutor, IndexExecutor, EnumerateCollectionExecutor,
#ifdef ARANGODB_USE_GOOGLE_TESTS
                       TestLambdaSkipExecutor,
#endif
                       TraversalExecutor, EnumerateListExecutor, SubqueryStartExecutor, SubqueryEndExecutor,
                       SortedCollectExecutor, LimitExecutor, SortExecutor>),
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
auto ExecutionBlockImpl<Executor>::executeSkipRowsRange(typename Fetcher::DataRange& inputRange,
                                                        AqlCall& call)
    -> std::tuple<ExecutorState, typename Executor::Stats, size_t, AqlCall> {
  if constexpr (isNewStyleExecutor<Executor>) {
    call.skippedRows = 0;
    if constexpr (skipRowsType<Executor>() == SkipRowsRangeVariant::EXECUTOR) {
      // If the executor has a method skipRowsRange, to skip outputs.
      // Every non-passthrough executor needs to implement this.
      // TRI_ASSERT(!_executorReturnedDone); // TODO: We're running into issues when we're activating that assert. Why that?!
      auto res =  _executor.skipRowsRange(inputRange, call);
      LOG_DEVEL << "skip returned state is: " << std::get<ExecutorState>(res);
      _executorReturnedDone = std::get<ExecutorState>(res) == ExecutorState::DONE;
      return res;
    } else if constexpr (skipRowsType<Executor>() == SkipRowsRangeVariant::FETCHER) {
      // If we know that every input row produces exactly one output row (this
      // is a property of the executor), then we can just let the fetcher skip
      // the number of rows that we would like to skip.
      // Returning this will trigger to end in upstream state now, with the
      // call that was handed it.
      static_assert(
          std::is_same_v<typename Executor::Stats, NoStats>,
          "Executors with custom statistics must implement skipRowsRange.");
      // TODO Set _executorReturnedDone?
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

template <>
auto ExecutionBlockImpl<SubqueryStartExecutor>::shadowRowForwarding() -> ExecState {
  TRI_ASSERT(_outputItemRow);
  TRI_ASSERT(_outputItemRow->isInitialized());
  TRI_ASSERT(!_outputItemRow->allRowsUsed());
  if (_lastRange.hasDataRow()) {
    // If we have a dataRow, the executor needs to write it's output.
    // If we get woken up by a dataRow during forwarding of ShadowRows
    // This will return false, and if so we need to call produce instead.
    auto didWrite = _executor.produceShadowRow(_lastRange, *_outputItemRow);
    if (didWrite) {
      if (_lastRange.hasShadowRow()) {
        // Forward the ShadowRows
        return ExecState::SHADOWROWS;
      }
      // If we have more input,
      // For now we need to return
      // here and cannot start another subquery.
      // We do not know what to do with the next DataRow.
      return ExecState::DONE;
    } else {
      // Woken up after shadowRow forwarding
      // Need to call the Executor
      return ExecState::CHECKCALL;
    }
  } else {
    // Need to forward the ShadowRows
    auto const& [state, shadowRow] = _lastRange.nextShadowRow();
    TRI_ASSERT(shadowRow.isInitialized());
    _outputItemRow->increaseShadowRowDepth(shadowRow);
    TRI_ASSERT(_outputItemRow->produced());
    _outputItemRow->advanceRow();
    if (_lastRange.hasShadowRow()) {
      return ExecState::SHADOWROWS;
    }
    // If we do not have more shadowRows
    // we need to return.
    return ExecState::DONE;
  }
}

template <>
auto ExecutionBlockImpl<SubqueryEndExecutor>::shadowRowForwarding() -> ExecState {
  TRI_ASSERT(_outputItemRow);
  TRI_ASSERT(_outputItemRow->isInitialized());
  TRI_ASSERT(!_outputItemRow->allRowsUsed());
  if (!_lastRange.hasShadowRow()) {
    // We got back without a ShadowRow in the LastRange
    // Let client call again
    return ExecState::DONE;
  }
  auto const& [state, shadowRow] = _lastRange.nextShadowRow();
  TRI_ASSERT(shadowRow.isInitialized());
  bool didConsume = false;
  if (shadowRow.isRelevant()) {
    // We need to consume the row, and write the Aggregate to it.
    _executor.consumeShadowRow(shadowRow, *_outputItemRow);
    didConsume = true;
  } else {
    _outputItemRow->decreaseShadowRowDepth(shadowRow);
  }

  TRI_ASSERT(_outputItemRow->produced());
  _outputItemRow->advanceRow();

  if (state == ExecutorState::DONE) {
    // We have consumed everything, we are
    // Done with this query
    return ExecState::DONE;
  } else if (_lastRange.hasDataRow()) {
    // Multiple concatenated Subqueries
    // This case is disallowed for now, as we do not know the
    // look-ahead call
    TRI_ASSERT(false);
    // If we would know we could now go into a continue with next subquery
    // state.
    return ExecState::DONE;
  } else if (_lastRange.hasShadowRow()) {
    // We still have shadowRows, we
    // need to forward them
    return ExecState::SHADOWROWS;
  } else {
    if (didConsume) {
      // We did only consume the input
      // ask upstream
      return ExecState::CHECKCALL;
    }
    // End of input, we are done for now
    // Need to call again
    return ExecState::DONE;
  }
}

template <class Executor>
auto ExecutionBlockImpl<Executor>::shadowRowForwarding() -> ExecState {
  TRI_ASSERT(_outputItemRow);
  TRI_ASSERT(_outputItemRow->isInitialized());
  TRI_ASSERT(!_outputItemRow->allRowsUsed());
  if (!_lastRange.hasShadowRow()) {
    // We got back without a ShadowRow in the LastRange
    // Let client call again
    return ExecState::DONE;
  }

  auto const& [state, shadowRow] = _lastRange.nextShadowRow();
  TRI_ASSERT(shadowRow.isInitialized());

  _outputItemRow->copyRow(shadowRow);

  if (shadowRow.isRelevant()) {
    LOG_QUERY("6d337", DEBUG) << printTypeInfo() << " init executor.";
    // We found a relevant shadow Row.
    // We need to reset the Executor
    resetExecutor();
  }

  TRI_ASSERT(_outputItemRow->produced());
  _outputItemRow->advanceRow();

  if (state == ExecutorState::DONE) {
    // We have consumed everything, we are
    // Done with this query
    return ExecState::DONE;
  } else if (_lastRange.hasDataRow()) {
    // Multiple concatenated Subqueries
    // This case is disallowed for now, as we do not know the
    // look-ahead call
    TRI_ASSERT(false);
    // If we would know we could now go into a continue with next subquery
    // state.
    return ExecState::DONE;
  } else if (_lastRange.hasShadowRow()) {
    // We still have shadowRows, we
    // need to forward them
    return ExecState::SHADOWROWS;
  } else {
    // End of input, we are done for now
    // Need to call again
    return ExecState::DONE;
  }
}

/**
 * @brief Define the variant of FastForward behaviour
 *
 * FULLCOUNT => Call executeSkipRowsRange and report what has been skipped.
 * EXECUTOR => Call executeSkipRowsRange, but do not report what has been skipped.
 *             (This instance is used to make sure Modifications are performed, or stats are correct)
 * FETCHER => Do not bother the Executor, drop all from input, without further reporting
 */
enum class FastForwardVariant { FULLCOUNT, EXECUTOR, FETCHER };

template <class Executor>
static auto fastForwardType(AqlCall const& call, Executor const& e) -> FastForwardVariant {
  if (call.needsFullCount() && call.getOffset() == 0 && call.getLimit() == 0) {
    // Only start fullCount after the original call is fulfilled. Otherwise
    // do fast-forward variant
    TRI_ASSERT(call.hasHardLimit());
    return FastForwardVariant::FULLCOUNT;
  }
  // TODO: We only need to do this is the executor actually require to call.
  // e.g. Modifications will always need to be called. Limit only if it needs to report fullCount
  if constexpr (is_one_of_v<Executor, LimitExecutor>) {
    return FastForwardVariant::EXECUTOR;
  }
  return FastForwardVariant::FETCHER;
}

template <class Executor>
auto ExecutionBlockImpl<Executor>::executeFastForward(typename Fetcher::DataRange& inputRange,
                                                      AqlCall& clientCall)
    -> std::tuple<ExecutorState, typename Executor::Stats, size_t, AqlCall> {
  TRI_ASSERT(isNewStyleExecutor<Executor>);
  if constexpr (std::is_same_v<Executor, SubqueryStartExecutor>) {
    if (clientCall.needsFullCount() && clientCall.getOffset() == 0 &&
        clientCall.getLimit() == 0) {
      // We can savely call skipRows.
      // It will not report anything if the row is already consumed
      return executeSkipRowsRange(_lastRange, clientCall);
    }
    // Do not fastForward anything, the Subquery start will handle it by itself
    return {ExecutorState::DONE, NoStats{}, 0, AqlCall{}};
  }
  auto type = fastForwardType(clientCall, _executor);
  switch (type) {
    case FastForwardVariant::FULLCOUNT:
    case FastForwardVariant::EXECUTOR: {
      LOG_QUERY("cb135", DEBUG) << printTypeInfo() << " apply full count.";
      auto [state, stats, skippedLocal, call] = executeSkipRowsRange(_lastRange, clientCall);
      if (type == FastForwardVariant::EXECUTOR) {
        // We do not report the skip
        skippedLocal = 0;
      }
      return {state, stats, skippedLocal, call};
    }
    case FastForwardVariant::FETCHER: {
      LOG_QUERY("fa327", DEBUG) << printTypeInfo() << " bypass unused rows.";
      inputRange.skipAllRemainingDataRows();
      AqlCall call{};
      call.hardLimit = 0;
      return {inputRange.upstreamState(), typename Executor::Stats{}, 0, call};
    }
  }
  // Unreachable
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
}

/**
 * @brief This is the central function of an executor, and it acts like a
 * coroutine: It can be called multiple times and keeps state across
 * calls.
 *
 * The intended behaviour of this function is best described in terms of
 * a state machine; the possible states are the ExecStates
 * SKIP, PRODUCE, FASTFORWARD, UPSTREAM, SHADOWROWS, DONE
 *
 * SKIP       skipping rows. How rows are skipped is determined by
 *            the Executor that is used. See SkipVariants
 * PRODUCE    calls produceRows of the executor
 * FASTFORWARD again skipping rows, will count skipped rows, if fullCount is requested.
 * UPSTREAM   fetches rows from the upstream executor(s) to be processed by
 *            our executor.
 * SHADOWROWS process any shadow rows
 * DONE       processing of one output is done. We did handle offset / limit / fullCount without crossing BatchSize limits.
 *            This state does not indicate that we are DONE with all input, we are just done with one walk through this statemachine.
 *
 * We progress within the states in the following way:
 *   There is a nextState method that determines the next state based on the call, it can only lead to:
 *   SKIP, PRODUCE, FASTFORWAD, DONE
 *
 *   On the first call we will use nextState to get to our starting point.
 *   After any of SKIP, PRODUCE,, FASTFORWAD, DONE We either go to
 *   1. FASTFORWARD (if executor is done)
 *   2. DONE (if output is full)
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
      LOG_QUERY("bf029", DEBUG) << "subquery bypassing executor " << printBlockInfo();
      // We are bypassing subqueries.
      // This executor is not allowed to perform actions
      // However we need to maintain the upstream state.
      size_t skippedLocal = 0;
      typename Fetcher::DataRange bypassedRange{ExecutorState::HASMORE};
      std::tie(_upstreamState, skippedLocal, bypassedRange) = _rowFetcher.execute(stack);
      return {_upstreamState, skippedLocal, bypassedRange.getBlock()};
    }

    AqlCall clientCall = stack.popCall();
    ExecutorState localExecutorState = ExecutorState::DONE;

    TRI_ASSERT(!(clientCall.getOffset() == 0 && clientCall.softLimit == AqlCall::Limit{0}));
    TRI_ASSERT(!(clientCall.hasSoftLimit() && clientCall.fullCount));
    TRI_ASSERT(!(clientCall.hasSoftLimit() && clientCall.hasHardLimit()));

    // We can only have returned the following internal states
    TRI_ASSERT(_execState == ExecState::CHECKCALL || _execState == ExecState::SHADOWROWS ||
               _execState == ExecState::UPSTREAM);
    // Skip can only be > 0 if we are in upstream cases.
    TRI_ASSERT(_skipped == 0 || _execState == ExecState::UPSTREAM);

    if constexpr (std::is_same_v<Executor, SubqueryEndExecutor>) {
      // TODO: implement forwarding of SKIP properly:
      // We need to modify the execute API to instead return a vector of skipped
      // values.
      // Then we can simply push a skip on the Stack here and let it forward.
      // In case of a modifaction we need to NOT forward a skip, but instead do
      // a limit := limit + offset call and a hardLimit 0 call on top of the stack.
      TRI_ASSERT(!clientCall.needSkipMore());

      // In subqeryEndExecutor we actually manage two calls.
      // The clientClient is defined of what will go into the Executor.
      // on SubqueryEnd this call is generated based on the call from downstream
      stack.pushCall(std::move(clientCall));
      // TODO: Implement different kind of calls we need to inject into Executor
      // based on modification, or on forwarding.
      // FOr now use a fetchUnlimited Call always
      clientCall = AqlCall{};
    }
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

    auto returnToState = ExecState::CHECKCALL;

    LOG_QUERY("007ac", DEBUG) << "starting statemachine of executor " << printBlockInfo();
    while (_execState != ExecState::DONE) {
      switch (_execState) {
        case ExecState::CHECKCALL: {
          LOG_QUERY("cfe46", DEBUG)
              << printTypeInfo() << " determine next action on call " << clientCall;
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
          LOG_QUERY("1f786", DEBUG) << printTypeInfo() << " call skipRows " << clientCall;
          auto [state, stats, skippedLocal, call] =
              executeSkipRowsRange(_lastRange, clientCall);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          // Assertion: We did skip 'skippedLocal' documents here.
          // This means that they have to be removed from clientCall.getOffset()
          // This has to be done by the Executor calling call.didSkip()
          // accordingly.
          // The LIMIT executor with a LIMIT of 0 can also bypass fullCount
          // here, even if callLimit > 0
          if (canPassFullcount || std::is_same_v<Executor, LimitExecutor>) {
            // In this case we can first skip. But straight after continue with fullCount, so we might skip more
            TRI_ASSERT(clientCall.getOffset() + skippedLocal >= offsetBefore);
            if (clientCall.getOffset() + skippedLocal > offsetBefore) {
              // First need to count down offset.
              TRI_ASSERT(clientCall.getOffset() == 0);
            }
          } else {
            TRI_ASSERT(clientCall.getOffset() + skippedLocal == offsetBefore);
          }
#endif
          localExecutorState = state;
          _skipped += skippedLocal;
          _engine->_stats += stats;
          // The execute might have modified the client call.
          if (state == ExecutorState::DONE) {
            _execState = ExecState::FASTFORWARD;
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

          LOG_QUERY("1f786", DEBUG) << printTypeInfo() << " call produceRows " << clientCall;
          if (outputIsFull()) {
            // We need to be able to write data
            // But maybe the existing block is full here
            // Then we need to wake up again.
            // However the client might decide on a different
            // call, so we do not record this position
            _execState = ExecState::DONE;
            break;
          }
          if constexpr (std::is_same_v<Executor, SubqueryEndExecutor>) {
            TRI_ASSERT(!stack.empty());
            AqlCall const& subqueryCall = stack.peek();
            AqlCall copyCall = subqueryCall;
            ensureOutputBlock(std::move(copyCall));
          } else {
            ensureOutputBlock(std::move(clientCall));
          }
          TRI_ASSERT(_outputItemRow);
          TRI_ASSERT(!_executorReturnedDone);

          // Execute getSome
          auto const [state, stats, call] =
              _executor.produceRows(_lastRange, *_outputItemRow);
          _executorReturnedDone = state == ExecutorState::DONE;
          LOG_DEVEL << "state?: " << state;
          LOG_DEVEL << "done?: " << _executorReturnedDone;
          _engine->_stats += stats;
          localExecutorState = state;

          if constexpr (!std::is_same_v<Executor, SubqueryEndExecutor>) {
            // Produce might have modified the clientCall
            // But only do this if we are not subquery.
            clientCall = _outputItemRow->getClientCall();
          }

          if (state == ExecutorState::DONE) {
            _execState = ExecState::FASTFORWARD;
          } else if ((Executor::Properties::allowsBlockPassthrough == BlockPassthrough::Enable ||
                      clientCall.getLimit() > 0) &&
                     outputIsFull()) {
            // In pass through variant we need to stop whenever the block is full.
            // In all other branches only if the client Still needs more data.
            _execState = ExecState::DONE;
            break;
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
          LOG_QUERY("96e2c", DEBUG)
              << printTypeInfo() << " all produced, fast forward to end up (sub-)query.";
          auto [state, stats, skippedLocal, call] =
              executeFastForward(_lastRange, clientCall);

          _skipped += skippedLocal;
          _engine->_stats += stats;
          localExecutorState = state;

          if (state == ExecutorState::DONE) {
            if (_outputItemRow && _outputItemRow->isInitialized() &&
                _outputItemRow->allRowsUsed()) {
              // We have a block with data, but no more place for a shadow row.
              _execState = ExecState::DONE;
            } else if (!_lastRange.hasShadowRow() && !_lastRange.hasDataRow()) {
              _execState = ExecState::DONE;
            } else {
              _execState = ExecState::SHADOWROWS;
            }
          } else {
            // We need to request more
            _upstreamRequest = call;
            _execState = ExecState::UPSTREAM;
          }
          break;
        }
        case ExecState::UPSTREAM: {
          LOG_QUERY("488de", DEBUG)
              << printTypeInfo() << " request dependency " << _upstreamRequest;
          // If this triggers the executors produceRows function has returned
          // HASMORE even if it knew that upstream has no further rows.
          TRI_ASSERT(_upstreamState != ExecutionState::DONE);
          // We need to make sure _lastRange is all used
          TRI_ASSERT(!_lastRange.hasDataRow());
          TRI_ASSERT(!_lastRange.hasShadowRow());
          size_t skippedLocal = 0;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          size_t subqueryLevelBefore = stack.subqueryLevel();
#endif
          // If we are SubqueryStart, we remove the top element of the stack
          // which belongs to the subquery enclosed by this
          // SubqueryStart and the partnered SubqueryEnd by *not*
          // pushing the upstream request.
          if constexpr (!std::is_same_v<Executor, SubqueryStartExecutor>) {
            auto callCopy = _upstreamRequest;
            stack.pushCall(std::move(callCopy));
          }

          std::tie(_upstreamState, skippedLocal, _lastRange) = _rowFetcher.execute(stack);

          if constexpr (std::is_same_v<Executor, SubqueryStartExecutor>) {
            // Do not pop the call, we did not put it on.
            // However we need it for accounting later.
          } else {
            // As the stack is copied into the fetcher, we need to pop off our call again.
            // If we use other datastructures or moving we may hand over ownership of the stack here
            // instead and no popCall is necessary.
            stack.popCall();
          }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          TRI_ASSERT(subqueryLevelBefore == stack.subqueryLevel());
#endif
          if (_upstreamState == ExecutionState::WAITING) {
            // We need to persist the old call before we return.
            // We might have some local accounting to this call.
            _clientRequest = clientCall;
            // We do not return anything in WAITING state, also NOT skipped.
            return {_upstreamState, 0, nullptr};
          }
          if constexpr (Executor::Properties::allowsBlockPassthrough ==
                        BlockPassthrough::Enable) {
            // We have a new range, passthrough can use this range.
            _hasUsedDataRangeBlock = false;
          }
          if constexpr (skipRowsType<Executor>() == SkipRowsRangeVariant::FETCHER) {
            _skipped += skippedLocal;
            // We skipped through passthrough, so count that a skip was solved.
            clientCall.didSkip(skippedLocal);
          }
          if (_lastRange.hasShadowRow() && !_lastRange.peekShadowRow().isRelevant()) {
            _execState = ExecState::SHADOWROWS;
          } else {
            _execState = ExecState::CHECKCALL;
          }
          break;
        }
        case ExecState::SHADOWROWS: {
          // We only get Called with something in the input.
          TRI_ASSERT(_lastRange.hasShadowRow() || _lastRange.hasDataRow());
          LOG_QUERY("7c63c", DEBUG)
              << printTypeInfo() << " (sub-)query completed. Move ShadowRows.";

          // TODO: Check if we can have the situation that we are between two shadow rows here.
          // E.g. LastRow is relevant shadowRow. NextRow is non-relevant shadowRow.
          // NOTE: I do not think this is an issue, as the Executor will always say that it cannot do anything with
          // an empty input. Only exception might be COLLECT COUNT.

          if (outputIsFull()) {
            // We need to be able to write data
            // But maybe the existing block is full here
            // Then we need to wake up again here.
            returnToState = ExecState::SHADOWROWS;
            _execState = ExecState::DONE;
            break;
          }
          if constexpr (std::is_same_v<Executor, SubqueryEndExecutor>) {
            TRI_ASSERT(!stack.empty());
            AqlCall const& subqueryCall = stack.peek();
            AqlCall copyCall = subqueryCall;
            ensureOutputBlock(std::move(copyCall));
          } else {
            ensureOutputBlock(std::move(clientCall));
          }

          TRI_ASSERT(!_outputItemRow->allRowsUsed());

          // This may write one or more rows.
          _execState = shadowRowForwarding();
          if constexpr (!std::is_same_v<Executor, SubqueryEndExecutor>) {
            // Produce might have modified the clientCall
            // But only do this if we are not subquery.
            clientCall = _outputItemRow->getClientCall();
          }
          break;
        }
        default:
          // unreachable
          TRI_ASSERT(false);
          THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
      }
    }
    LOG_QUERY("80c24", DEBUG) << printBlockInfo() << " local statemachine done. Return now.";
    // If we do not have an output, we simply return a nullptr here.
    auto outputBlock = _outputItemRow != nullptr ? _outputItemRow->stealBlock()
                                                 : SharedAqlItemBlockPtr{nullptr};
    // We are locally done with our output.
    // Next time we need to check the client call again
    _execState = returnToState;
    // This is not strictly necessary here, as we shouldn't be called again
    // after DONE.
    _outputItemRow.reset();

    // We return skipped here, reset member
    size_t skipped = _skipped;
    _skipped = 0;
    if (localExecutorState == ExecutorState::HASMORE ||
        _lastRange.hasDataRow() || _lastRange.hasShadowRow()) {
      // We have skipped or/and return data, otherwise we cannot return HASMORE
      /*
      LOG_DEVEL << " == IMPL == ";
      LOG_DEVEL << "Local executor state is : " << localExecutorState;
      LOG_DEVEL << "lastRange hasDataRow: " << _lastRange.hasDataRow();
      LOG_DEVEL << "lastRange hasShadowRow: " << _lastRange.hasShadowRow();
      LOG_DEVEL << "Skipped is : " << skipped;
      LOG_DEVEL << "nullptr? " << std::boolalpha << (outputBlock == nullptr);
      if (outputBlock != nullptr) {
        LOG_DEVEL << "rows: " << outputBlock->size();
      }
      LOG_DEVEL << " == IMPL == ";
       */
      TRI_ASSERT(skipped > 0 || (outputBlock != nullptr && outputBlock->numEntries() > 0));
      return {ExecutionState::HASMORE, skipped, std::move(outputBlock)};
    }
    // We must return skipped and/or data when reporting HASMORE
    TRI_ASSERT(_upstreamState != ExecutionState::HASMORE ||
               (skipped > 0 || (outputBlock != nullptr && outputBlock->numEntries() > 0)));
    return {_upstreamState, skipped, std::move(outputBlock)};
  } else {
    // TODO this branch must never be taken with an executor that has not been
    //      converted yet
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}

template <class Executor>
void ExecutionBlockImpl<Executor>::resetExecutor() {
  LOG_DEVEL << "did reset the executor";
  // cppcheck-suppress unreadVariable
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
  _executorReturnedDone = false;
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
      resetExecutor();
    }
  }
  return state;
}

template <class Executor>
auto ExecutionBlockImpl<Executor>::outputIsFull() const noexcept -> bool {
  return _outputItemRow != nullptr && _outputItemRow->isInitialized() &&
         _outputItemRow->allRowsUsed();
}

template <>
template <>
RegisterId ExecutionBlockImpl<IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>::getOutputRegisterId() const
    noexcept {
  return _infos.getOutputRegister();
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
