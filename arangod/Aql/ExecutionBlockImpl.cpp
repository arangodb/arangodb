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

#include "Basics/Common.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"

#include "Aql/CalculationExecutor.h"
#include "Aql/ConstrainedSortExecutor.h"
#include "Aql/CountCollectExecutor.h"
#include "Aql/DistinctCollectExecutor.h"
#include "Aql/EnumerateCollectionExecutor.h"
#include "Aql/EnumerateListExecutor.h"
#include "Aql/FilterExecutor.h"
#include "Aql/HashedCollectExecutor.h"
#include "Aql/IResearchViewExecutor.h"
#include "Aql/IdExecutor.h"
#include "Aql/IndexExecutor.h"
#include "Aql/KShortestPathsExecutor.h"
#include "Aql/LimitExecutor.h"
#include "Aql/ModificationExecutor.h"
#include "Aql/ModificationExecutorTraits.h"
#include "Aql/NoResultsExecutor.h"
#include "Aql/ReturnExecutor.h"
#include "Aql/ShortestPathExecutor.h"
#include "Aql/SingleRemoteModificationExecutor.h"
#include "Aql/SortExecutor.h"
#include "Aql/SortRegister.h"
#include "Aql/SortedCollectExecutor.h"
#include "Aql/SortingGatherExecutor.h"
#include "Aql/SubqueryExecutor.h"
#include "Aql/TraversalExecutor.h"

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

#define CREATE_HAS_MEMBER_CHECK(methodName, checkName)  \
  template <typename T>                                 \
  class checkName {                                     \
    typedef char yes[1];                                \
    typedef char no[2];                                 \
                                                        \
    template <typename C>                               \
    static yes& test(decltype(&C::methodName));         \
    template <typename>                                 \
    static no& test(...);                               \
                                                        \
   public:                                              \
    enum { value = sizeof(test<T>(0)) == sizeof(yes) }; \
  }

CREATE_HAS_MEMBER_CHECK(initializeCursor, hasInitializeCursor);
CREATE_HAS_MEMBER_CHECK(skipRows, hasSkipRows);
CREATE_HAS_MEMBER_CHECK(fetchBlockForPassthrough, hasFetchBlockForPassthrough);
CREATE_HAS_MEMBER_CHECK(expectedNumberOfRows, hasExpectedNumberOfRows);

template <class Executor>
ExecutionBlockImpl<Executor>::ExecutionBlockImpl(ExecutionEngine* engine,
                                                 ExecutionNode const* node,
                                                 typename Executor::Infos&& infos)
    : ExecutionBlock(engine, node),
      _dependencyProxy(_dependencies, engine->itemBlockManager(),
                       infos.getInputRegisters(), infos.numberOfInputRegisters()),
      _rowFetcher(_dependencyProxy),
      _infos(std::move(infos)),
      _executor(_rowFetcher, _infos),
      _outputItemRow(),
      _query(*engine->getQuery()) {
  // already insert ourselves into the statistics results
  if (_profile >= PROFILE_LEVEL_BLOCKS) {
    _engine->_stats.nodes.emplace(node->id(), ExecutionStats::Node());
  }
}

template <class Executor>
ExecutionBlockImpl<Executor>::~ExecutionBlockImpl() = default;

template <class Executor>
std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<Executor>::getSome(size_t atMost) {
  traceGetSomeBegin(atMost);
  auto result = getSomeWithoutTrace(atMost);
  return traceGetSomeEnd(result.first, std::move(result.second));
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
      // _rowFetcher must be DONE now already
      return {state, nullptr};
    }
    TRI_ASSERT(newBlock != nullptr);
    TRI_ASSERT(newBlock->size() > 0);
    TRI_ASSERT(newBlock->size() <= atMost);
    _outputItemRow = createOutputRow(newBlock);
  }

  ExecutionState state = ExecutionState::HASMORE;
  ExecutorStats executorStats{};

  TRI_ASSERT(atMost > 0);

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
  // When we're passing blocks through we have no control over the size of the
  // output block.
  if /* constexpr */ (!Executor::Properties::allowsBlockPassthrough) {
    TRI_ASSERT(_outputItemRow->numRowsWritten() == atMost);
  }

  auto outputBlock = _outputItemRow->stealBlock();
  // we guarantee that we do return a valid pointer in the HASMORE case.
  TRI_ASSERT(outputBlock != nullptr);
  _outputItemRow.reset();
  return {state, std::move(outputBlock)};
}

template <class Executor>
std::unique_ptr<OutputAqlItemRow> ExecutionBlockImpl<Executor>::createOutputRow(
    SharedAqlItemBlockPtr& newBlock) const {
  if /* constexpr */ (Executor::Properties::allowsBlockPassthrough) {
    return std::make_unique<OutputAqlItemRow>(newBlock, infos().getOutputRegisters(),
                                              infos().registersToKeep(),
                                              infos().registersToClear(),
                                              OutputAqlItemRow::CopyRowBehaviour::DoNotCopyInputRows);
  } else {
    return std::make_unique<OutputAqlItemRow>(newBlock, infos().getOutputRegisters(),
                                              infos().registersToKeep(),
                                              infos().registersToClear());
  }
}

namespace arangodb {
namespace aql {

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
  bool constexpr useFetcher = Executor::Properties::allowsBlockPassthrough &&
                              !std::is_same<Executor, SubqueryExecutor<true>>::value;

  bool constexpr useExecutor = hasSkipRows<Executor>::value;

  // ConstFetcher and SingleRowFetcher<true> can skip, but it may not be done
  // for modification subqueries.
  static_assert(useFetcher ==
                    (std::is_same<typename Executor::Fetcher, ConstFetcher>::value ||
                     (std::is_same<typename Executor::Fetcher, SingleRowFetcher<true>>::value &&
                      !std::is_same<Executor, SubqueryExecutor<true>>::value)),
                "Unexpected fetcher for SkipVariants::FETCHER");

  static_assert(!useFetcher || hasSkipRows<typename Executor::Fetcher>::value,
                "Fetcher is chosen for skipping, but has not skipRows method!");

  static_assert(useExecutor ==
                    (std::is_same<Executor, IndexExecutor>::value ||
                     std::is_same<Executor, IResearchViewExecutor<false>>::value ||
                     std::is_same<Executor, IResearchViewExecutor<true>>::value ||
                     std::is_same<Executor, IResearchViewMergeExecutor<false>>::value ||
                     std::is_same<Executor, IResearchViewMergeExecutor<true>>::value ||
                     std::is_same<Executor, EnumerateCollectionExecutor>::value ||
                     std::is_same<Executor, LimitExecutor>::value),
                "Unexpected executor for SkipVariants::EXECUTOR");

  // The LimitExecutor will not work correctly with SkipVariants::FETCHER!
  static_assert(!std::is_same<Executor, LimitExecutor>::value || useFetcher,
      "LimitExecutor needs to implement skipRows() to work correctly");

  if (useExecutor) {
    return SkipVariants::EXECUTOR;
  } else if (useFetcher) {
    return SkipVariants::FETCHER;
  } else {
    return SkipVariants::GET_SOME;
  }
}

}  // namespace aql
}  // namespace arangodb

template <class Executor>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<Executor>::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);

  constexpr SkipVariants customSkipType = skipType<Executor>();

  if (customSkipType == SkipVariants::GET_SOME) {
    atMost = std::min(atMost, DefaultBatchSize());
    auto res = getSomeWithoutTrace(atMost);

    size_t skipped = 0;
    if (res.second != nullptr) {
      skipped = res.second->size();
    }
    TRI_ASSERT(skipped <= atMost);

    return traceSkipSomeEnd({res.first, skipped});
  }

  ExecutionState state;
  typename Executor::Stats stats;
  size_t skipped;
  std::tie(state, stats, skipped) =
      ExecuteSkipVariant<customSkipType>::executeSkip(_executor, _rowFetcher, atMost);
  _engine->_stats += stats;
  TRI_ASSERT(skipped <= atMost);

  return traceSkipSomeEnd(state, skipped);
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

  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_dependencyProxy);

  constexpr bool customInit = hasInitializeCursor<Executor>::value;
  // IndexExecutor and EnumerateCollectionExecutor have initializeCursor
  // implemented, so assert this implementation is used.
  static_assert(!std::is_same<Executor, EnumerateCollectionExecutor>::value || customInit,
                "EnumerateCollectionExecutor is expected to implement a custom "
                "initializeCursor method!");
  static_assert(!std::is_same<Executor, IndexExecutor>::value || customInit,
                "IndexExecutor is expected to implement a custom "
                "initializeCursor method!");
  InitializeCursor<customInit>::init(_executor, _rowFetcher, _infos);

  // // use this with c++17 instead of specialisation below
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

// Work around GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56480
// Without the namespaces it fails with
// error: specialization of 'template<class Executor> std::pair<arangodb::aql::ExecutionState, arangodb::Result> arangodb::aql::ExecutionBlockImpl<Executor>::initializeCursor(arangodb::aql::AqlItemBlock*, size_t)' in different namespace
namespace arangodb {
namespace aql {
// TODO -- remove this specialization when cpp 17 becomes available
template <>
std::pair<ExecutionState, Result> ExecutionBlockImpl<IdExecutor<ConstFetcher>>::initializeCursor(
    InputAqlItemRow const& input) {
  // reinitialize the DependencyProxy
  _dependencyProxy.reset();

  // destroy and re-create the Fetcher
  _rowFetcher.~Fetcher();
  new (&_rowFetcher) Fetcher(_dependencyProxy);

  SharedAqlItemBlockPtr block =
      input.cloneToBlock(_engine->itemBlockManager(), *(infos().registersToKeep()),
                         infos().numberOfOutputRegisters());

  _rowFetcher.injectBlock(block);

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

}  // namespace aql
}  // namespace arangodb

namespace arangodb {
namespace aql {

// The constant "PASSTHROUGH" is somehow reserved with MSVC.
enum class RequestWrappedBlockVariant { DEFAULT , PASS_THROUGH , INPUTRESTRICTED };

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
    static_assert(
        Executor::Properties::allowsBlockPassthrough,
        "This function can only be used with executors supporting `allowsBlockPassthrough`");
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
    static_assert(
        Executor::Properties::inputSizeRestrictsOutputSize,
        "This function can only be used with executors supporting `inputSizeRestrictsOutputSize`");
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

}  // namespace aql
}  // namespace arangodb

template <class Executor>
std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<Executor>::requestWrappedBlock(
    size_t nrItems, RegisterCount nrRegs) {
  static_assert(!Executor::Properties::allowsBlockPassthrough ||
                    !Executor::Properties::inputSizeRestrictsOutputSize,
                "At most one of Properties::allowsBlockPassthrough or "
                "Properties::inputSizeRestrictsOutputSize should be true for "
                "each Executor");
  static_assert(
      Executor::Properties::allowsBlockPassthrough ==
          hasFetchBlockForPassthrough<Executor>::value,
      "Executors should implement the method fetchBlockForPassthrough() iff "
      "Properties::allowsBlockPassthrough is true");
  static_assert(
      Executor::Properties::inputSizeRestrictsOutputSize ==
          hasExpectedNumberOfRows<Executor>::value,
      "Executors should implement the method expectedNumberOfRows() iff "
      "Properties::inputSizeRestrictsOutputSize is true");

  constexpr RequestWrappedBlockVariant variant =
      Executor::Properties::allowsBlockPassthrough
          ? RequestWrappedBlockVariant::PASS_THROUGH
          : Executor::Properties::inputSizeRestrictsOutputSize
                ? RequestWrappedBlockVariant::INPUTRESTRICTED
                : RequestWrappedBlockVariant::DEFAULT;

  return RequestWrappedBlock<variant>::run(
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      infos(),
#endif
      executor(), *_engine, nrItems, nrRegs);
}

/// @brief request an AqlItemBlock from the memory manager
template <class Executor>
SharedAqlItemBlockPtr ExecutionBlockImpl<Executor>::requestBlock(size_t nrItems,
                                                                 RegisterId nrRegs) {
  return _engine->itemBlockManager().requestBlock(nrItems, nrRegs);
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
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewExecutor<false>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewExecutor<true>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<false>>;
template class ::arangodb::aql::ExecutionBlockImpl<IResearchViewMergeExecutor<true>>;
template class ::arangodb::aql::ExecutionBlockImpl<IdExecutor<ConstFetcher>>;
template class ::arangodb::aql::ExecutionBlockImpl<IdExecutor<SingleRowFetcher<true>>>;
template class ::arangodb::aql::ExecutionBlockImpl<IndexExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<LimitExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<Insert, SingleBlockFetcher<false /*allowsBlockPassthrough */>>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<Insert, AllRowsFetcher>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<Remove, SingleBlockFetcher<false /*allowsBlockPassthrough */>>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<Remove, AllRowsFetcher>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<Replace, SingleBlockFetcher<false /*allowsBlockPassthrough */>>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<Replace, AllRowsFetcher>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<Update, SingleBlockFetcher<false /*allowsBlockPassthrough */>>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<Update, AllRowsFetcher>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<Upsert, SingleBlockFetcher<false /*allowsBlockPassthrough */>>>;
template class ::arangodb::aql::ExecutionBlockImpl<ModificationExecutor<Upsert, AllRowsFetcher>>;
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
template class ::arangodb::aql::ExecutionBlockImpl<SubqueryExecutor<true>>;
template class ::arangodb::aql::ExecutionBlockImpl<SubqueryExecutor<false>>;
template class ::arangodb::aql::ExecutionBlockImpl<TraversalExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SortingGatherExecutor>;
