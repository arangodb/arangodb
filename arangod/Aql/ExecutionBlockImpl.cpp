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
#include "Aql/SkipResult.h"
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
CREATE_HAS_MEMBER_CHECK(expectedNumberOfRowsNew, hasExpectedNumberOfRowsNew);

#ifdef ARANGODB_USE_GOOGLE_TESTS
// Forward declaration of Test Executors.
// only used as long as isNewStyleExecutor is required.
namespace arangodb::aql {
class TestLambdaExecutor;

class TestLambdaSkipExecutor;
}  // namespace arangodb::aql
#endif

/*
 *  Determine whether an executor cannot bypass subquery skips.
 *  This is if exection of this Executor does have side-effects
 *  other then it's own result.
 */

template <typename Executor>
constexpr bool executorHasSideEffects =
    is_one_of_v<Executor, ModificationExecutor<AllRowsFetcher, InsertModifier>,
                ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, InsertModifier>,
                ModificationExecutor<AllRowsFetcher, RemoveModifier>,
                ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, RemoveModifier>,
                ModificationExecutor<AllRowsFetcher, UpdateReplaceModifier>,
                ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, UpdateReplaceModifier>,
                ModificationExecutor<AllRowsFetcher, UpsertModifier>,
                ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, UpsertModifier>>;

/*
 * Determine whether we execute new style or old style skips, i.e. pre or post shadow row introduction
 * TODO: This should be removed once all executors and fetchers are ported to the new style.
 */
template <typename Executor>
constexpr bool isNewStyleExecutor = is_one_of_v<
    Executor, FilterExecutor, SortedCollectExecutor, IdExecutor<ConstFetcher>, ParallelUnsortedGatherExecutor,
    IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>, ReturnExecutor, DistinctCollectExecutor, IndexExecutor,
    EnumerateCollectionExecutor, SubqueryExecutor<true>, SubqueryExecutor<false>, CountCollectExecutor,
    CalculationExecutor<CalculationType::Condition>, CalculationExecutor<CalculationType::Reference>,
    CalculationExecutor<CalculationType::V8Condition>, HashedCollectExecutor, ConstrainedSortExecutor,
#ifdef ARANGODB_USE_GOOGLE_TESTS
    TestLambdaExecutor,
    TestLambdaSkipExecutor,  // we need one after these to avoid compile errors in non-test mode
#endif
    ModificationExecutor<AllRowsFetcher, InsertModifier>,
    ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, InsertModifier>,
    ModificationExecutor<AllRowsFetcher, RemoveModifier>,
    ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, RemoveModifier>,
    ModificationExecutor<AllRowsFetcher, UpdateReplaceModifier>,
    ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, UpdateReplaceModifier>,
    ModificationExecutor<AllRowsFetcher, UpsertModifier>,
    ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, UpsertModifier>, SubqueryStartExecutor,
    UnsortedGatherExecutor, SortingGatherExecutor, SubqueryEndExecutor, TraversalExecutor,
    KShortestPathsExecutor, ShortestPathExecutor, EnumerateListExecutor, LimitExecutor, SortExecutor,
    IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::NotMaterialize>,
    IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::LateMaterialize>,
    IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::Materialize>,
    IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
    IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
    IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::NotMaterialize>,
    IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::LateMaterialize>,
    IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::Materialize>,
    IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
    IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
    IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::NotMaterialize>,
    IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::LateMaterialize>,
    IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::Materialize>,
    IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
    IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
    IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::NotMaterialize>,
    IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::LateMaterialize>,
    IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::Materialize>,
    IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
    IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
    SubqueryStartExecutor, SubqueryEndExecutor, TraversalExecutor, KShortestPathsExecutor,
    NoResultsExecutor, ShortestPathExecutor, EnumerateListExecutor, LimitExecutor,
    SingleRemoteModificationExecutor<IndexTag>, SingleRemoteModificationExecutor<Insert>,
    SingleRemoteModificationExecutor<Remove>, SingleRemoteModificationExecutor<Update>,
    SingleRemoteModificationExecutor<Replace>, SingleRemoteModificationExecutor<Upsert>,
    MaterializeExecutor<RegisterId>, MaterializeExecutor<std::string const&>>;

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
    AqlCallStack stack{AqlCallList{AqlCall::SimulateGetSome(atMost)}};
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
  AqlCallStack stack{AqlCallList{AqlCall::SimulateSkipSome(atMost)}};
  auto const [state, skipped, block] = execute(stack);

  // execute returns ExecutionState::DONE here, which stops execution after simulating a skip.
  // If we indiscriminately return ExecutionState::HASMORE, then we end up in an infinite loop
  //
  // luckily we can dispose of this kludge once executors have been ported.
  if (skipped.getSkipCount() < atMost && state == ExecutionState::DONE) {
    return {ExecutionState::DONE, skipped.getSkipCount()};
  } else {
    return {ExecutionState::HASMORE, skipped.getSkipCount()};
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
  _hasUsedDataRangeBlock = false;
  initOnce();
  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_dependencyProxy);

  if constexpr (isMultiDepExecutor<Executor>) {
    _lastRange.reset();
    _rowFetcher.init();
  } else {
    _lastRange = DataRange(ExecutorState::HASMORE);
  }

  TRI_ASSERT(_skipped.nothingSkipped());
  _skipped.reset();
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
std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr>
ExecutionBlockImpl<Executor>::execute(AqlCallStack stack) {
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
    initOnce();

    auto res = executeWithoutTrace(stack);
    return traceExecuteEnd(res);
  }

  // Fall back to getSome/skipSome
  auto myCallList = stack.popCall();
  auto myCall = myCallList.popNextCall();

  TRI_ASSERT(AqlCall::IsSkipSomeCall(myCall) || AqlCall::IsGetSomeCall(myCall) ||
             AqlCall::IsFullCountCall(myCall) || AqlCall::IsFastForwardCall(myCall));
  _rowFetcher.useStack(stack);

  if (AqlCall::IsSkipSomeCall(myCall)) {
    auto const [state, skipped] = skipSome(myCall.getOffset());
    if (state != ExecutionState::WAITING) {
      myCall.didSkip(skipped);
    }
    SkipResult skipRes{};
    skipRes.didSkip(skipped);
    return {state, skipRes, nullptr};
  } else if (AqlCall::IsGetSomeCall(myCall)) {
    auto const [state, block] = getSome(myCall.getLimit());
    // We do not need to count as softLimit will be overwritten, and hard cannot be set.
    if (stack.empty() && myCall.hasHardLimit() && !myCall.needsFullCount() && block != nullptr) {
      // However we can do a short-cut here to report DONE on hardLimit if we are on the top-level query.
      myCall.didProduce(block->size());
      if (myCall.getLimit() == 0) {
        return {ExecutionState::DONE, SkipResult{}, block};
      }
    }

    return {state, SkipResult{}, block};
  } else if (AqlCall::IsFullCountCall(myCall)) {
    auto const [state, skipped] = skipSome(ExecutionBlock::SkipAllSize());
    if (state != ExecutionState::WAITING) {
      myCall.didSkip(skipped);
    }
    SkipResult skipRes{};
    skipRes.didSkip(skipped);
    return {state, skipRes, nullptr};
  } else if (AqlCall::IsFastForwardCall(myCall)) {
    // No idea if DONE is correct here...
    return {ExecutionState::DONE, SkipResult{}, nullptr};
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
auto ExecutionBlockImpl<IdExecutor<ConstFetcher>>::injectConstantBlock<IdExecutor<ConstFetcher>>(
    SharedAqlItemBlockPtr block, SkipResult skipped) -> void {
  // reinitialize the DependencyProxy
  _dependencyProxy.reset();

  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_dependencyProxy);

  TRI_ASSERT(_skipped.nothingSkipped());

  // Local skipped is either fresh (depth == 1)
  // Or exactly of the size handed in
  TRI_ASSERT(_skipped.subqueryDepth() == 1 ||
             _skipped.subqueryDepth() == skipped.subqueryDepth());
  _skipped = skipped;

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
  TRI_ASSERT(_skipped.nothingSkipped());
  _skipped.reset();
  // We inject an empty copy of our skipped here,
  // This is resettet, but will maintain the size
  injectConstantBlock(block, _skipped);

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
    static_assert(
        Executor::Properties::inputSizeRestrictsOutputSize ==
            hasExpectedNumberOfRows<Executor>::value,
        "Executors should implement the method expectedNumberOfRows() iff "
        "Properties::inputSizeRestrictsOutputSize is true");
  }

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
auto ExecutionBlockImpl<Executor>::allocateOutputBlock(AqlCall&& call, DataRange const& inputRange)
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
    if constexpr (isMultiDepExecutor<Executor>) {
      // MultiDepExecutor would require dependency handling.
      // We do not have it here.
      if (!inputRange.hasShadowRow() && !inputRange.hasDataRow()) {
        // On empty input do not yet create output.
        // We are going to ask again later
        SharedAqlItemBlockPtr newBlock{nullptr};
        return createOutputRow(newBlock, std::move(call));
      }
    } else {
      if (!inputRange.hasShadowRow() && !inputRange.hasDataRow() &&
          inputRange.upstreamState() == ExecutorState::HASMORE) {
        // On empty input do not yet create output.
        // We are going to ask again later
        SharedAqlItemBlockPtr newBlock{nullptr};
        return createOutputRow(newBlock, std::move(call));
      }
    }

    // Non-Passthrough variant, we need to allocate the block ourselfs
    size_t blockSize = ExecutionBlock::DefaultBatchSize;
    if constexpr (hasExpectedNumberOfRowsNew<Executor>::value) {
      // Only limit the output size if there will be no more
      // data from upstream. Or if we have ordered a SOFT LIMIT.
      // Otherwise we will overallocate here.
      // In production it is now very unlikely in the non-softlimit case
      // that the upstream is no block using less than batchSize many rows, but returns HASMORE.
      if (inputRange.upstreamState() == ExecutorState::DONE || call.hasSoftLimit()) {
        blockSize = _executor.expectedNumberOfRowsNew(inputRange, call);
        // The executor cannot expect to produce more then the limit!
        if constexpr (!std::is_same_v<Executor, SubqueryStartExecutor>) {
          // Except the subqueryStartExecutor, it's limit differs
          // from it's output (it needs to count the new ShadowRows in addition)
          TRI_ASSERT(blockSize <= call.getLimit());
        }

        blockSize += inputRange.countShadowRows();
        // We have an upper bound by DefaultBatchSize;
        blockSize = std::min(ExecutionBlock::DefaultBatchSize, blockSize);
      }
    }

    if (blockSize == 0) {
      // There is no data to be produced
      SharedAqlItemBlockPtr newBlock{nullptr};
      return createOutputRow(newBlock, std::move(call));
    }
    SharedAqlItemBlockPtr newBlock =
        _engine->itemBlockManager().requestBlock(blockSize, _infos.numberOfOutputRegisters());
    return createOutputRow(newBlock, std::move(call));
  }
}

template <class Executor>
void ExecutionBlockImpl<Executor>::ensureOutputBlock(AqlCall&& call,
                                                     DataRange const& inputRange) {
  if (_outputItemRow == nullptr || !_outputItemRow->isInitialized()) {
    _outputItemRow = allocateOutputBlock(std::move(call), inputRange);
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
      !std::is_same_v<Executor, SubqueryExecutor<true>>;

  bool constexpr useExecutor = hasSkipRowsRange<Executor>::value;

  static_assert(!std::is_same_v<Executor, SubqueryExecutor<true>> ||
                hasSkipRowsRange<Executor>::value);

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
          (is_one_of_v<
              Executor, FilterExecutor, ShortestPathExecutor, ReturnExecutor, KShortestPathsExecutor, ParallelUnsortedGatherExecutor,
              IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>, IdExecutor<ConstFetcher>,
              HashedCollectExecutor, IndexExecutor, EnumerateCollectionExecutor, DistinctCollectExecutor,
              ConstrainedSortExecutor, CountCollectExecutor, SubqueryExecutor<true>,
#ifdef ARANGODB_USE_GOOGLE_TESTS
              TestLambdaSkipExecutor,
#endif
              ModificationExecutor<AllRowsFetcher, InsertModifier>,
              ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, InsertModifier>,
              ModificationExecutor<AllRowsFetcher, RemoveModifier>,
              ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, RemoveModifier>,
              ModificationExecutor<AllRowsFetcher, UpdateReplaceModifier>,
              ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, UpdateReplaceModifier>,
              ModificationExecutor<AllRowsFetcher, UpsertModifier>,
              ModificationExecutor<SingleRowFetcher<BlockPassthrough::Disable>, UpsertModifier>, TraversalExecutor,
              EnumerateListExecutor, SubqueryStartExecutor, SubqueryEndExecutor, SortedCollectExecutor,
              LimitExecutor, UnsortedGatherExecutor, SortingGatherExecutor, SortExecutor,
              IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::NotMaterialize>,
              IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::LateMaterialize>,
              IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::Materialize>,
              IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
              IResearchViewExecutor<false, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
              IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::NotMaterialize>,
              IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::LateMaterialize>,
              IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::Materialize>,
              IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
              IResearchViewExecutor<true, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
              IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::NotMaterialize>,
              IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::LateMaterialize>,
              IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::Materialize>,
              IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
              IResearchViewMergeExecutor<false, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
              IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::NotMaterialize>,
              IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::LateMaterialize>,
              IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::Materialize>,
              IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::NotMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
              IResearchViewMergeExecutor<true, arangodb::iresearch::MaterializeType::LateMaterialize | arangodb::iresearch::MaterializeType::UseStoredValues>,
              TraversalExecutor, EnumerateListExecutor, SubqueryStartExecutor, SubqueryEndExecutor, SortedCollectExecutor,
              LimitExecutor, NoResultsExecutor, SingleRemoteModificationExecutor<IndexTag>, SingleRemoteModificationExecutor<Insert>,
              SingleRemoteModificationExecutor<Remove>, SingleRemoteModificationExecutor<Update>,
              SingleRemoteModificationExecutor<Replace>, SingleRemoteModificationExecutor<Upsert>,
              MaterializeExecutor<RegisterId>, MaterializeExecutor<std::string const&>>),
      "Unexpected executor for SkipVariants::EXECUTOR");

  // The LimitExecutor will not work correctly with SkipVariants::FETCHER!
  static_assert(
      !std::is_same<Executor, LimitExecutor>::value || useFetcher,
      "LimitExecutor needs to implement skipRows() to work correctly");

  if constexpr (useExecutor) {
    return SkipRowsRangeVariant::EXECUTOR;
  } else {
    static_assert(useFetcher);
    return SkipRowsRangeVariant::FETCHER;
  }
}

// Let's do it the C++ way.
template <class T>
struct dependent_false : std::false_type {};

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
  // TODO: We only need to do this if the executor is required to call.
  // e.g. Modifications and SubqueryStart will always need to be called. Limit only if it needs to report fullCount
  if constexpr (is_one_of_v<Executor, LimitExecutor, SubqueryStartExecutor> ||
                executorHasSideEffects<Executor>) {
    return FastForwardVariant::EXECUTOR;
  }
  return FastForwardVariant::FETCHER;
}

template <class Executor>
auto ExecutionBlockImpl<Executor>::executeFetcher(AqlCallStack& stack,
                                                  AqlCallType const& aqlCall,
                                                  ADB_IGNORE_UNUSED bool wasCalledWithContinueCall)
    -> std::tuple<ExecutionState, SkipResult, typename Fetcher::DataRange> {
  if constexpr (isNewStyleExecutor<Executor>) {
    // TODO The logic in the MultiDependencySingleRowFetcher branch should be
    //      moved into the MultiDependencySingleRowFetcher.
    static_assert(isMultiDepExecutor<Executor> ==
                  std::is_same_v<Fetcher, MultiDependencySingleRowFetcher>);
    if constexpr (std::is_same_v<Fetcher, MultiDependencySingleRowFetcher>) {
      static_assert(
          !executorHasSideEffects<Executor>,
          "there is a special implementation for side-effect executors to "
          "exchange the stack. For the MultiDependencyFetcher this special "
          "case is not implemented. There is no reason to disallow this "
          "case here however, it is just not needed thus far.");
      // Note the aqlCall is an AqlCallSet in this case:
      static_assert(std::is_same_v<AqlCallSet, std::decay_t<decltype(aqlCall)>>);
      TRI_ASSERT(_lastRange.numberDependencies() == _dependencies.size());
      auto const& [state, skipped, ranges] = _rowFetcher.execute(stack, aqlCall);
      for (auto const& [dependency, range] : ranges) {
        _lastRange.setDependency(dependency, range);
      }
      return {state, skipped, _lastRange};

    } else if constexpr (executorHasSideEffects<Executor>) {
      // If the executor has side effects, we cannot bypass any subqueries
      // by skipping them. So we need to fetch all shadow rows in order to
      // trigger this Executor with everthing from above.
      // NOTE: The Executor needs to discard shadowRows, and do the accouting.
      static_assert(std::is_same_v<AqlCall, std::decay_t<decltype(aqlCall)>>);
      auto fetchAllStack = stack.createEquivalentFetchAllShadowRowsStack();
      fetchAllStack.pushCall(createUpstreamCall(aqlCall, wasCalledWithContinueCall));
      auto res = _rowFetcher.execute(fetchAllStack);
      // Just make sure we did not Skip anything
      TRI_ASSERT(std::get<SkipResult>(res).nothingSkipped());
      return res;
    } else {
      // If we are SubqueryStart, we remove the top element of the stack
      // which belongs to the subquery enclosed by this
      // SubqueryStart and the partnered SubqueryEnd by *not*
      // pushing the upstream request.
      if constexpr (!std::is_same_v<Executor, SubqueryStartExecutor>) {
        stack.pushCall(createUpstreamCall(std::move(aqlCall), wasCalledWithContinueCall));
      }

      auto const result = _rowFetcher.execute(stack);

      if constexpr (!std::is_same_v<Executor, SubqueryStartExecutor>) {
        // As the stack is copied into the fetcher, we need to pop off our call
        // again. If we use other datastructures or moving we may hand over
        // ownership of the stack here instead and no popCall is necessary.
        std::ignore = stack.popCall();
      } else {
        // Do not pop the call, we did not put it on.
        // However we need it for accounting later.
      }

      return result;
    }
  } else {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}

template <class Executor>
auto ExecutionBlockImpl<Executor>::executeProduceRows(typename Fetcher::DataRange& input,
                                                      OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, typename Executor::Stats, AqlCallType> {
  if constexpr (isNewStyleExecutor<Executor>) {
    if constexpr (isMultiDepExecutor<Executor>) {
      TRI_ASSERT(input.numberDependencies() == _dependencies.size());
      return _executor.produceRows(input, output);
    } else if constexpr (is_one_of_v<Executor, SubqueryExecutor<true>, SubqueryExecutor<false>>) {
      // The SubqueryExecutor has it's own special handling outside.
      // SO this code is in fact not reachable
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
    } else {
      auto [state, stats, call] = _executor.produceRows(input, output);
      return {state, stats, call};
    }
  } else {
    return {ExecutorState::DONE, typename Executor::Stats{}, AqlCall{}};
  }
}

template <class Executor>
auto ExecutionBlockImpl<Executor>::executeSkipRowsRange(typename Fetcher::DataRange& inputRange,
                                                        AqlCall& call)
    -> std::tuple<ExecutorState, typename Executor::Stats, size_t, AqlCallType> {
  if constexpr (isNewStyleExecutor<Executor>) {
    call.skippedRows = 0;
    if constexpr (skipRowsType<Executor>() == SkipRowsRangeVariant::EXECUTOR) {
      if constexpr (isMultiDepExecutor<Executor>) {
        TRI_ASSERT(inputRange.numberDependencies() == _dependencies.size());
        // If the executor has a method skipRowsRange, to skip outputs.
        // Every non-passthrough executor needs to implement this.
        auto res = _executor.skipRowsRange(inputRange, call);
        _executorReturnedDone = std::get<ExecutorState>(res) == ExecutorState::DONE;
        return res;
      } else if constexpr (is_one_of_v<Executor, SubqueryExecutor<true>, SubqueryExecutor<false>>) {
        // The SubqueryExecutor has it's own special handling outside.
        // SO this code is in fact not reachable
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL_AQL);
      } else {
        auto [state, stats, skipped, localCall] =
            _executor.skipRowsRange(inputRange, call);
        _executorReturnedDone = state == ExecutorState::DONE;
        return {state, stats, skipped, localCall};
      }
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
      TRI_ASSERT(false);
    }
  } else {
    TRI_ASSERT(false);
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

template <>
auto ExecutionBlockImpl<SubqueryStartExecutor>::shadowRowForwarding(AqlCallStack& stack)
    -> ExecState {
  TRI_ASSERT(_outputItemRow);
  TRI_ASSERT(_outputItemRow->isInitialized());
  TRI_ASSERT(!_outputItemRow->allRowsUsed());
  if (_lastRange.hasDataRow()) {
    // If we have a dataRow, the executor needs to write it's output.
    // If we get woken up by a dataRow during forwarding of ShadowRows
    // This will return false, and if so we need to call produce instead.
    auto didWrite = _executor.produceShadowRow(_lastRange, *_outputItemRow);
    // The Subquery Start returns DONE after every row.
    // This needs to be resetted as soon as a shadowRow has been produced
    _executorReturnedDone = false;
    // Need to report that we have written a row in the call

    if (didWrite) {
      auto& subqueryCall = stack.modifyTopCall();
      subqueryCall.didProduce(1);
      if (_lastRange.hasShadowRow()) {
        // Forward the ShadowRows
        return ExecState::SHADOWROWS;
      }
      return ExecState::NEXTSUBQUERY;
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
auto ExecutionBlockImpl<SubqueryEndExecutor>::shadowRowForwarding(AqlCallStack& stack)
    -> ExecState {
  TRI_ASSERT(_outputItemRow);
  TRI_ASSERT(_outputItemRow->isInitialized());
  TRI_ASSERT(!_outputItemRow->allRowsUsed());
  if (!_lastRange.hasShadowRow()) {
    // We got back without a ShadowRow in the LastRange
    // Let client call again
    return ExecState::NEXTSUBQUERY;
  }
  auto const& [state, shadowRow] = _lastRange.nextShadowRow();
  TRI_ASSERT(shadowRow.isInitialized());
  bool didConsume = false;
  if (shadowRow.isRelevant()) {
    // We need to consume the row, and write the Aggregate to it.
    _executor.consumeShadowRow(shadowRow, *_outputItemRow);
    didConsume = true;
    // we need to reset the ExecutorHasReturnedDone, it will
    // return done after every subquery is fully collected.
    _executorReturnedDone = false;
  } else {
    _outputItemRow->decreaseShadowRowDepth(shadowRow);
  }

  TRI_ASSERT(_outputItemRow->produced());
  _outputItemRow->advanceRow();
  // we need to update the Top of the stack now
  auto& topCall = stack.modifyTopCall();
  topCall = _outputItemRow->getClientCall();

  if (state == ExecutorState::DONE) {
    // We have consumed everything, we are
    // Done with this query
    return ExecState::DONE;
  } else if (_lastRange.hasDataRow()) {
    // Multiple concatenated Subqueries
    return ExecState::NEXTSUBQUERY;
  } else if (_lastRange.hasShadowRow()) {
    // We still have shadowRows, we
    // need to forward them
    return ExecState::SHADOWROWS;
  } else if (_outputItemRow->isFull()) {
    // Fullfilled the call
    // Need to return!
    return ExecState::DONE;
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
auto ExecutionBlockImpl<Executor>::sideEffectShadowRowForwarding(AqlCallStack& stack,
                                                                 SkipResult& skipResult)
    -> ExecState {
  TRI_ASSERT(executorHasSideEffects<Executor>);
  if (!stack.needToCountSubquery()) {
    // We need to really produce things here
    // fall back to original version as any other executor.
    auto res = shadowRowForwarding(stack);
    return res;
  }
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
  uint64_t depthSkippingNow = static_cast<uint64_t>(stack.shadowRowDepthToSkip());
  uint64_t shadowDepth = shadowRow.getDepth();

  bool didWriteRow = false;
  if (shadowRow.isRelevant()) {
    LOG_QUERY("1b257", DEBUG) << printTypeInfo() << " init executor.";
    // We found a relevant shadow Row.
    // We need to reset the Executor
    resetExecutor();
  }

  if (depthSkippingNow > shadowDepth) {
    // We are skipping the outermost Subquery.
    // Simply drop this ShadowRow
  } else if (depthSkippingNow == shadowDepth) {
    // We are skipping on this subquery level.
    // Skip the row, but report skipped 1.
    AqlCall& shadowCall = stack.modifyCallAtDepth(shadowDepth);
    if (shadowCall.needSkipMore()) {
      shadowCall.didSkip(1);
      skipResult.didSkipSubquery(1, shadowDepth);
    } else if (shadowCall.getLimit() > 0) {
      TRI_ASSERT(!shadowCall.needSkipMore() && shadowCall.getLimit() > 0);
      _outputItemRow->copyRow(shadowRow);
      shadowCall.didProduce(1);
      TRI_ASSERT(_outputItemRow->produced());
      _outputItemRow->advanceRow();
      didWriteRow = true;
    } else {
      TRI_ASSERT(shadowCall.hardLimit == 0);
      // Simply drop this shadowRow!
    }
  } else {
    // We got a shadowRow of a subquery we are not skipping here.
    // Do proper reporting on it's call.
    AqlCall& shadowCall = stack.modifyCallAtDepth(shadowDepth);
    TRI_ASSERT(!shadowCall.needSkipMore() && shadowCall.getLimit() > 0);
    _outputItemRow->copyRow(shadowRow);
    shadowCall.didProduce(1);

    TRI_ASSERT(_outputItemRow->produced());
    _outputItemRow->advanceRow();
    didWriteRow = true;
  }
  if (state == ExecutorState::DONE) {
    // We have consumed everything, we are
    // Done with this query
    return ExecState::DONE;
  } else if (_lastRange.hasDataRow()) {
    // Multiple concatenated Subqueries
    return ExecState::NEXTSUBQUERY;
  } else if (_lastRange.hasShadowRow()) {
    // We still have shadowRows, we
    // need to forward them
    return ExecState::SHADOWROWS;
  } else if (didWriteRow) {
    // End of input, we are done for now
    // Need to call again
    return ExecState::DONE;
  } else {
    // Done with this subquery.
    // We did not write any output yet.
    // So we can continue with upstream.
    return ExecState::UPSTREAM;
  }
}

template <class Executor>
auto ExecutionBlockImpl<Executor>::shadowRowForwarding(AqlCallStack&) -> ExecState {
  TRI_ASSERT(_outputItemRow);
  TRI_ASSERT(_outputItemRow->isInitialized());
  TRI_ASSERT(!_outputItemRow->allRowsUsed());
  if (!_lastRange.hasShadowRow()) {
    // We got back without a ShadowRow in the LastRange
    // Let us continue with the next Subquery
    return ExecState::NEXTSUBQUERY;
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
    return ExecState::NEXTSUBQUERY;
  } else if (_lastRange.hasShadowRow()) {
    // We still have shadowRows.
    auto const& lookAheadRow = _lastRange.peekShadowRow();
    if (lookAheadRow.isRelevant()) {
      // We are starting the NextSubquery here.
      return ExecState::NEXTSUBQUERY;
    }
    // we need to forward them
    return ExecState::SHADOWROWS;
  } else {
    // End of input, we are done for now
    // Need to call again
    return ExecState::DONE;
  }
}

template <class Executor>
auto ExecutionBlockImpl<Executor>::executeFastForward(typename Fetcher::DataRange& inputRange,
                                                      AqlCall& clientCall)
    -> std::tuple<ExecutorState, typename Executor::Stats, size_t, AqlCallType> {
  TRI_ASSERT(isNewStyleExecutor<Executor>);
  auto type = fastForwardType(clientCall, _executor);

  switch (type) {
    case FastForwardVariant::FULLCOUNT: {
      LOG_QUERY("cb135", DEBUG) << printTypeInfo() << " apply full count.";
      auto [state, stats, skippedLocal, call] = executeSkipRowsRange(_lastRange, clientCall);

      if constexpr (is_one_of_v<DataRange, AqlItemBlockInputMatrix, MultiAqlItemBlockInputRange>) {
        // The executor will have used all Rows.
        // However we need to drop them from the input
        // here.
        inputRange.skipAllRemainingDataRows();
      }

      return {state, stats, skippedLocal, call};
    }
    case FastForwardVariant::EXECUTOR: {
      LOG_QUERY("2890e", DEBUG) << printTypeInfo() << " fast forward.";
      // We use a DUMMY Call to simulate fullCount.
      AqlCall dummy;
      dummy.hardLimit = 0;
      dummy.fullCount = true;
      auto [state, stats, skippedLocal, call] = executeSkipRowsRange(_lastRange, dummy);

      if constexpr (is_one_of_v<DataRange, AqlItemBlockInputMatrix, MultiAqlItemBlockInputRange>) {
        // The executor will have used all Rows.
        // However we need to drop them from the input
        // here.
        inputRange.skipAllRemainingDataRows();
      }

      return {state, stats, 0, call};
    }
    case FastForwardVariant::FETCHER: {
      LOG_QUERY("fa327", DEBUG) << printTypeInfo() << " bypass unused rows.";
      ADB_IGNORE_UNUSED auto const dependency = inputRange.skipAllRemainingDataRows();
      auto constexpr fastForwardCall = AqlCall{0, false, 0, AqlCall::LimitType::HARD};
      auto const call = std::invoke([&]() -> AqlCallType {
        if constexpr (std::is_same_v<AqlCallType, AqlCall>) {
          return fastForwardCall;
        } else {
#ifndef _WIN32
          // For some reason our Windows compiler complains about this static
          // assert in the cases that should be in the above constexpr path.
          // So simply not compile it in.
          static_assert(std::is_same_v<AqlCallType, AqlCallSet>);
#endif
          auto call = AqlCallSet{};
          call.calls.emplace_back(
              typename AqlCallSet::DepCallPair{dependency, AqlCallList{fastForwardCall}});
          return call;
        }
      });

      // TODO We have to ask all dependencies to go forward to the next shadow row
      auto const state = std::invoke(
          [&](auto) {
            if constexpr (std::is_same_v<DataRange, MultiAqlItemBlockInputRange>) {
              return inputRange.upstreamState(dependency);
            } else {
              return inputRange.upstreamState();
            }
          },
          0);

      return {state, typename Executor::Stats{}, 0, call};
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
std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr>
ExecutionBlockImpl<Executor>::executeWithoutTrace(AqlCallStack stack) {
  if constexpr (isNewStyleExecutor<Executor>) {
    AqlCallList clientCallList = stack.popCall();
    AqlCall clientCall;
    if constexpr (std::is_same_v<Executor, SubqueryEndExecutor>) {
      // In subqeryEndExecutor we actually manage two calls.
      // The clientCall defines what will go into the Executor.
      // on SubqueryEnd this call is generated based on the call from downstream

      if (_outputItemRow != nullptr && _outputItemRow->isInitialized()) {
        // If we return with a waiting state, we need to report it to the
        // subquery callList, but not pull it of.
        auto& subQueryCall = clientCallList.modifyNextCall();
        // Overwrite with old state.
        subQueryCall = _outputItemRow->getClientCall();
      }
      stack.pushCall(std::move(clientCallList));
      clientCallList = AqlCallList{AqlCall{}, AqlCall{}};
      clientCall = clientCallList.popNextCall();
    } else {
      clientCall = clientCallList.popNextCall();
    }

    ExecutorState localExecutorState = ExecutorState::DONE;

    TRI_ASSERT(!(clientCall.getOffset() == 0 && clientCall.softLimit == AqlCall::Limit{0}));
    TRI_ASSERT(!(clientCall.hasSoftLimit() && clientCall.fullCount));
    TRI_ASSERT(!(clientCall.hasSoftLimit() && clientCall.hasHardLimit()));
    if constexpr (is_one_of_v<Executor, SubqueryExecutor<true>, SubqueryExecutor<false>>) {
      // The old subquery executor can in-fact return waiting on produce call.
      // if it needs to wait for the subquery.
      // So we need to allow the return state here as well.
      TRI_ASSERT(_execState == ExecState::CHECKCALL ||
                 _execState == ExecState::SHADOWROWS || _execState == ExecState::UPSTREAM ||
                 _execState == ExecState::PRODUCE || _execState == ExecState::SKIP);
    } else {
      // We can only have returned the following internal states
      TRI_ASSERT(_execState == ExecState::CHECKCALL || _execState == ExecState::SHADOWROWS ||
                 _execState == ExecState::UPSTREAM);
    }

    // In some executors we may write something into the output, but then return waiting.
    // In this case we are not allowed to lose the call we have been working on, we have
    // noted down created or skipped rows in there.
    // The client is disallowed to change her mind anyways
    // so we simply continue to work on the call we already have
    // The guarantee is, if we have returned the block, and modified
    // our local call, then the outputItemRow is not initialized
    if constexpr (!std::is_same_v<Executor, SubqueryEndExecutor>) {
      // The subqueryEndeExecutor has handled it above
      if (_outputItemRow != nullptr && _outputItemRow->isInitialized()) {
        clientCall = _outputItemRow->getClientCall();
      }
    }

    // Skip can only be > 0 if we are in upstream cases, or if we got injected a block
    TRI_ASSERT(_skipped.nothingSkipped() || _execState == ExecState::UPSTREAM ||
               (std::is_same_v<Executor, IdExecutor<ConstFetcher>>));

    if constexpr (executorHasSideEffects<Executor>) {
      if (!_skipped.nothingSkipped()) {
        // We get woken up on upstream, but we have not reported our
        // local skip value to downstream
        // In the sideEffect executor we need to apply the skip values on the
        // incomming stack, which has not been modified yet.
        // NOTE: We only apply the skipping on subquery level.
        TRI_ASSERT(_skipped.subqueryDepth() == stack.subqueryLevel() + 1);
        for (size_t i = 0; i < stack.subqueryLevel(); ++i) {
          auto skippedSub = _skipped.getSkipOnSubqueryLevel(i);
          if (skippedSub > 0) {
            auto& call = stack.modifyCallAtDepth(i);
            call.didSkip(skippedSub);
          }
        }
      }
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

          if constexpr (executorHasSideEffects<Executor>) {
            // If the executor has sideEffects, and we need to skip the results we would
            // produce here because we actually skip the subquery, we instead do a
            // hardLimit 0 (aka FastForward) call instead to the local Executor
            if (stack.needToSkipSubquery()) {
              _execState = ExecState::FASTFORWARD;
              break;
            }
          }
          _execState = nextState(clientCall);
          break;
        }
        case ExecState::SKIP: {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          auto const offsetBefore = clientCall.getOffset();
          TRI_ASSERT(offsetBefore > 0);
          bool const canPassFullcount =
              clientCall.getLimit() == 0 && clientCall.needsFullCount();
#endif
          LOG_QUERY("1f786", DEBUG) << printTypeInfo() << " call skipRows " << clientCall;

          ExecutorState state = ExecutorState::HASMORE;
          typename Executor::Stats stats;
          size_t skippedLocal = 0;
          AqlCallType call{};
          if constexpr (is_one_of_v<Executor, SubqueryExecutor<true>>) {
            // NOTE: The subquery Executor will by itself call EXECUTE on it's
            // subquery. This can return waiting => we can get a WAITING state
            // here. We can only get the waiting state for Subquery executors.
            ExecutionState subqueryState = ExecutionState::HASMORE;
            std::tie(subqueryState, stats, skippedLocal, call) =
                _executor.skipRowsRange(_lastRange, clientCall);
            if (subqueryState == ExecutionState::WAITING) {
              TRI_ASSERT(skippedLocal == 0);
              return {subqueryState, SkipResult{}, nullptr};
            } else if (subqueryState == ExecutionState::DONE) {
              state = ExecutorState::DONE;
            } else {
              state = ExecutorState::HASMORE;
            }
          } else {
            // Execute skipSome
            std::tie(state, stats, skippedLocal, call) =
                executeSkipRowsRange(_lastRange, clientCall);
          }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          // Assertion: We did skip 'skippedLocal' documents here.
          // This means that they have to be removed from
          // clientCall.getOffset() This has to be done by the Executor
          // calling call.didSkip() accordingly. The LIMIT executor with a
          // LIMIT of 0 can also bypass fullCount here, even if callLimit > 0
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
          _skipped.didSkip(skippedLocal);
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
            ensureOutputBlock(std::move(copyCall), _lastRange);
          } else {
            ensureOutputBlock(std::move(clientCall), _lastRange);
          }
          TRI_ASSERT(_outputItemRow);
          TRI_ASSERT(!_executorReturnedDone);
          ExecutorState state = ExecutorState::HASMORE;
          typename Executor::Stats stats;
          auto call = AqlCallType{};
          if constexpr (is_one_of_v<Executor, SubqueryExecutor<true>, SubqueryExecutor<false>>) {
            // NOTE: The subquery Executor will by itself call EXECUTE on it's
            // subquery. This can return waiting => we can get a WAITING state
            // here. We can only get the waiting state for SUbquery executors.
            ExecutionState subqueryState = ExecutionState::HASMORE;
            std::tie(subqueryState, stats, call) =
                _executor.produceRows(_lastRange, *_outputItemRow);
            if (subqueryState == ExecutionState::WAITING) {
              return {subqueryState, SkipResult{}, nullptr};
            } else if (subqueryState == ExecutionState::DONE) {
              state = ExecutorState::DONE;
            } else {
              state = ExecutorState::HASMORE;
            }
          } else {
            // Execute getSome
            std::tie(state, stats, call) = executeProduceRows(_lastRange, *_outputItemRow);
          }
          _executorReturnedDone = state == ExecutorState::DONE;
          _engine->_stats += stats;
          localExecutorState = state;

          if constexpr (!std::is_same_v<Executor, SubqueryEndExecutor>) {
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
          } else if (clientCall.getLimit() > 0 && executorNeedsCall(call)) {
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

          AqlCall callCopy = clientCall;
          if constexpr (executorHasSideEffects<Executor>) {
            if (stack.needToSkipSubquery()) {
              // Fast Forward call.
              callCopy = AqlCall{0, false, 0, AqlCall::LimitType::HARD};
            }
          }

          auto [state, stats, skippedLocal, call] =
              executeFastForward(_lastRange, callCopy);

          if constexpr (executorHasSideEffects<Executor>) {
            if (!stack.needToSkipSubquery()) {
              // We need to modify the original call.
              clientCall = callCopy;
            }
            // else: We are bypassing the results.
            // Do not count them here.
          } else {
            clientCall = callCopy;
          }

          _skipped.didSkip(skippedLocal);
          _engine->_stats += stats;
          localExecutorState = state;

          if (state == ExecutorState::DONE) {
            if (!_lastRange.hasShadowRow() && !_lastRange.hasDataRow()) {
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
          // We need to make sure _lastRange is all used for single-dependency
          // executors.
          TRI_ASSERT(isMultiDepExecutor<Executor> || !lastRangeHasDataRow());
          TRI_ASSERT(!_lastRange.hasShadowRow());
          SkipResult skippedLocal;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          auto subqueryLevelBefore = stack.subqueryLevel();
#endif
          std::tie(_upstreamState, skippedLocal, _lastRange) =
              executeFetcher(stack, _upstreamRequest, clientCallList.hasMoreCalls());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          TRI_ASSERT(subqueryLevelBefore == stack.subqueryLevel());
#endif

          if (_upstreamState == ExecutionState::WAITING) {
            // We need to persist the old call before we return.
            // We might have some local accounting to this call.
            _clientRequest = clientCall;
            // We do not return anything in WAITING state, also NOT skipped.
            return {_upstreamState, SkipResult{}, nullptr};
          }
          if constexpr (Executor::Properties::allowsBlockPassthrough ==
                        BlockPassthrough::Enable) {
            // We have a new range, passthrough can use this range.
            _hasUsedDataRangeBlock = false;
          }

          if constexpr (std::is_same_v<Executor, SubqueryEndExecutor>) {
            // We need to pop the last subquery from the returned skip
            // We have not asked for a subquery skip.
            TRI_ASSERT(skippedLocal.getSkipCount() == 0);
            skippedLocal.decrementSubquery();
          }
          if constexpr (skipRowsType<Executor>() == SkipRowsRangeVariant::FETCHER) {
            // We skipped through passthrough, so count that a skip was solved.
            _skipped.merge(skippedLocal, false);
            clientCall.didSkip(skippedLocal.getSkipCount());
          } else if constexpr (is_one_of_v<Executor, SubqueryStartExecutor, SubqueryEndExecutor>) {
            // Subquery needs to include the topLevel Skip.
            // But does not need to apply the count to clientCall.
            _skipped.merge(skippedLocal, false);
            // This is what has been asked for by the SubqueryEnd

            auto& subqueryCall = stack.modifyTopCall();
            subqueryCall.didSkip(skippedLocal.getSkipCount());
          } else {
            _skipped.merge(skippedLocal, true);
          }
          if constexpr (std::is_same_v<Executor, SubqueryStartExecutor>) {
            // For the subqueryStart, we need to increment the SkipLevel by one
            // as we may trigger this multiple times, check if we need to do it.
            while (_skipped.subqueryDepth() < stack.subqueryLevel() + 1) {
              // In fact, we only need to increase by 1
              TRI_ASSERT(_skipped.subqueryDepth() == stack.subqueryLevel());
              _skipped.incrementSubquery();
            }
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
            ensureOutputBlock(std::move(copyCall), _lastRange);
          } else {
            ensureOutputBlock(std::move(clientCall), _lastRange);
          }

          TRI_ASSERT(!_outputItemRow->allRowsUsed());
          if constexpr (executorHasSideEffects<Executor>) {
            _execState = sideEffectShadowRowForwarding(stack, _skipped);
          } else {
            // This may write one or more rows.
            _execState = shadowRowForwarding(stack);
          }
          if constexpr (!std::is_same_v<Executor, SubqueryEndExecutor>) {
            // Produce might have modified the clientCall
            // But only do this if we are not subquery.
            clientCall = _outputItemRow->getClientCall();
          }
          break;
        }
        case ExecState::NEXTSUBQUERY: {
          // This state will continue with the next run in the current subquery.
          // For this executor the input of the next run will be injected and it can continue to work.
          LOG_QUERY("0ca35", DEBUG)
              << printTypeInfo() << " ShadowRows moved, continue with next subquery.";
          if constexpr (std::is_same_v<Executor, SubqueryStartExecutor>) {
            auto currentSubqueryCall = stack.peek();
            if (currentSubqueryCall.getLimit() == 0 && currentSubqueryCall.hasSoftLimit()) {
              // SoftLimitReached.
              // We cannot continue.
              _execState = ExecState::DONE;
              break;
            }
            // Otherwise just check like the other blocks
          }
          if (clientCallList.hasMoreCalls()) {
            // Update to next call and start all over.
            clientCall = clientCallList.popNextCall();
            _execState = ExecState::CHECKCALL;
          } else {
            // We cannot continue, so we are done
            _execState = ExecState::DONE;
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
    SkipResult skipped = _skipped;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    if (!stack.is36Compatible()) {
      if constexpr (std::is_same_v<Executor, SubqueryEndExecutor>) {
        TRI_ASSERT(skipped.subqueryDepth() == stack.subqueryLevel() /*we inected a call*/);
      } else {
        TRI_ASSERT(skipped.subqueryDepth() == stack.subqueryLevel() + 1 /*we took our call*/);
      }
    }
#endif

    _skipped.reset();
    if (localExecutorState == ExecutorState::HASMORE ||
        _lastRange.hasDataRow() || _lastRange.hasShadowRow()) {
      // We have skipped or/and return data, otherwise we cannot return HASMORE
      TRI_ASSERT(!skipped.nothingSkipped() ||
                 (outputBlock != nullptr && outputBlock->numEntries() > 0));
      return {ExecutionState::HASMORE, skipped, std::move(outputBlock)};
    }
    // We must return skipped and/or data when reporting HASMORE
    TRI_ASSERT(_upstreamState != ExecutionState::HASMORE ||
               (!skipped.nothingSkipped() ||
                (outputBlock != nullptr && outputBlock->numEntries() > 0)));
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

template <class Executor>
auto ExecutionBlockImpl<Executor>::lastRangeHasDataRow() const noexcept -> bool {
  return _lastRange.hasDataRow();
}

template <>
template <>
RegisterId ExecutionBlockImpl<IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>>::getOutputRegisterId() const
    noexcept {
  return _infos.getOutputRegister();
}

template <class Executor>
void ExecutionBlockImpl<Executor>::init() {
  TRI_ASSERT(!_initialized);
  if constexpr (isMultiDepExecutor<Executor>) {
    _lastRange.resizeOnce(ExecutorState::HASMORE, 0, _dependencies.size());
    _rowFetcher.init();
  }
}

template <class Executor>
void ExecutionBlockImpl<Executor>::initOnce() {
  if (!_initialized) {
    init();
    _initialized = true;
  }
}

template <class Executor>
auto ExecutionBlockImpl<Executor>::executorNeedsCall(AqlCallType& call) const
    noexcept -> bool {
  if constexpr (isMultiDepExecutor<Executor>) {
    // call is an AqlCallSet. We need to call upstream if it's not empty.
    return !call.empty();
  } else {
    // call is an AqlCall, unconditionally. The current convention is
    // to call upstream when there is no input left.
    // This could be made unnecessary by returning an optional AqlCall
    // for single-dependency executors.
    return !lastRangeHasDataRow();
  }
};

template <class Executor>
auto ExecutionBlockImpl<Executor>::memoizeCall(AqlCall const& call,
                                               bool wasCalledWithContinueCall) noexcept
    -> void {
  if (!_hasMemoizedCall) {
    if constexpr (!isMultiDepExecutor<Executor>) {
      // We can only try to memoize the first call ever send.
      // Otherwise the call might be influenced by state
      // inside the Executor
      if (wasCalledWithContinueCall && call.getOffset() == 0 && !call.needsFullCount()) {
        // First draft, we only memoize non-skipping calls
        _defaultUpstreamRequest = call;
      }
    }
    _hasMemoizedCall = true;
  }
}

template <class Executor>
auto ExecutionBlockImpl<Executor>::createUpstreamCall(AqlCall const& call, bool wasCalledWithContinueCall)
    -> AqlCallList {
  // We can only memoize the first call
  memoizeCall(call, wasCalledWithContinueCall);
  TRI_ASSERT(_hasMemoizedCall);
  if constexpr (!isMultiDepExecutor<Executor>) {
    if (_defaultUpstreamRequest.has_value()) {
      // We have memoized a default call.
      // So we can use it.
      return AqlCallList{call, _defaultUpstreamRequest.value()};
    }
  }
  return AqlCallList{call};
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
