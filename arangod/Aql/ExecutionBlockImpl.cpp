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
    TRI_ASSERT(newBlock->size() > 0);
    TRI_ASSERT(newBlock != nullptr);
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

enum class SkipVariants { SKIPROW, SKIPROWS, EXECUTOR, DEFAULT };

template <enum SkipVariants>
struct ExecuteSkipVariant {};

template <>
struct ExecuteSkipVariant<SkipVariants::SKIPROW> {
  template <class Executor>
  static std::pair<ExecutionState, size_t> executeSkip(Executor& executor,
                                                       typename Executor::Fetcher& fetcher,
                                                       size_t toSkip) {
    return fetcher.skipRow();
  }
};

template <>
struct ExecuteSkipVariant<SkipVariants::SKIPROWS> {
  template <class Executor>
  static std::pair<ExecutionState, size_t> executeSkip(Executor& executor,
                                                       typename Executor::Fetcher& fetcher,
                                                       size_t toSkip) {
    return fetcher.skipRows(toSkip);
  }
};

template <>
struct ExecuteSkipVariant<SkipVariants::EXECUTOR> {
  template <class Executor>
  static std::pair<ExecutionState, size_t> executeSkip(Executor& executor,
                                                       typename Executor::Fetcher& fetcher,
                                                       size_t toSkip) {
    return executor().skipRows(toSkip);
  }
};

template <>
struct ExecuteSkipVariant<SkipVariants::DEFAULT> {
  template <class Executor>
  static std::pair<ExecutionState, size_t> executeSkip(Executor& executor,
                                                       typename Executor::Fetcher& fetcher,
                                                       size_t toSkip) {
    // this function should never be executed
    TRI_ASSERT(false);
  }
};

template <class Executor>
static SkipVariants constexpr skipType() {
  // TODO: Add subquery modification check - still missing
  if /* constexpr */ (Executor::Properties::allowsBlockPassthrough &&
                      !std::is_same<Executor, SubqueryExecutor<true>>::value) {
    if (std::is_same<typename Executor::Fetcher, SingleRowFetcher<true>>::value) {
      return SkipVariants::SKIPROWS;
    } else if (std::is_same<typename Executor::Fetcher, ConstFetcher>::value) {
      return SkipVariants::SKIPROW;
    } else {
      return SkipVariants::DEFAULT;
    }
  } else if (std::is_same<Executor, EnumerateCollectionExecutor>::value) {
    return SkipVariants::EXECUTOR;
  } else if (std::is_same<Executor, IResearchViewExecutor<true>>::value) {
    return SkipVariants::DEFAULT;
  } else if (std::is_same<Executor, IResearchViewExecutor<false>>::value) {
    return SkipVariants::EXECUTOR;
  } else {
    return SkipVariants::DEFAULT;
  }
}

}  // namespace aql
}  // namespace arangodb

template <class Executor>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<Executor>::skipSome(size_t atMost) {
  traceSkipSomeBegin(atMost);

  constexpr SkipVariants customSkipType = skipType<Executor>();

  // ConstFetcher has skipRow implemented,
  // so assert this implementation is used.
  static_assert(!std::is_same<Fetcher, ConstFetcher>::value ||
                    customSkipType == SkipVariants::SKIPROW,
                "ConstFetcher is expected to implement a custom "
                "skipRow method!");
  /*static_assert(!std::is_same<Fetcher, SingleRowFetcher<true>>::value ||
     customSkipType == SkipVariants::SKIPROWS, "SingleRowFetcher with enabled
     PassThrough is expected to implement a custom " "skipRows method!");*/ // TODO RE ADD

  if (customSkipType == SkipVariants::DEFAULT) {
    auto res = getSomeWithoutTrace(atMost);

    size_t skipped = 0;
    if (res.second != nullptr) {
      skipped = res.second->size();
    }

    return traceSkipSomeEnd({res.first, skipped});
  }

  return traceSkipSomeEnd(ExecuteSkipVariant<customSkipType>::executeSkip(_executor, _rowFetcher, atMost));
}

namespace arangodb {
namespace aql {
template <>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<EnumerateCollectionExecutor>::skipSome(size_t atMost) {
  LOG_DEVEL << " SKIP ENUM COLL Special case";
  return this->executor().skipRows(atMost);
}

template <>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<IResearchViewExecutor<true>>::skipSome(size_t atMost) {
  return this->executor().skipRows(atMost);
}

template <>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<IResearchViewExecutor<false>>::skipSome(size_t atMost) {
  return this->executor().skipRows(atMost);
}

template <>
std::pair<ExecutionState, size_t> ExecutionBlockImpl<IndexExecutor>::skipSome(size_t atMost) {
  return this->executor().skipRows(atMost);
}

}  // namespace aql
}  // namespace arangodb

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

template <class Executor>
std::pair<ExecutionState, SharedAqlItemBlockPtr> ExecutionBlockImpl<Executor>::requestWrappedBlock(
    size_t nrItems, RegisterId nrRegs) {
  SharedAqlItemBlockPtr block;
  if /* constexpr */ (Executor::Properties::allowsBlockPassthrough) {
    // If blocks can be passed through, we do not create new blocks.
    // Instead, we take the input blocks from the fetcher and reuse them.

    ExecutionState state;
    std::tie(state, block) = _rowFetcher.fetchBlockForPassthrough(nrItems);

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
    if (!std::is_same<Executor, ReturnExecutor<true>>::value) {
      for (auto const& reg : *infos().getOutputRegisters()) {
        for (size_t row = 0; row < block->size(); row++) {
          AqlValue const& val = block->getValueReference(row, reg);
          TRI_ASSERT(val.isEmpty());
        }
      }
    }
#endif
  } else if /* constexpr */ (Executor::Properties::inputSizeRestrictsOutputSize) {
    // The SortExecutor should refetch a block to save memory in case if only few elements to sort
    ExecutionState state;
    size_t expectedRows = 0;
    // Note: this might trigger a prefetch on the rowFetcher!
    std::tie(state, expectedRows) = _executor.expectedNumberOfRows(nrItems);
    if (state == ExecutionState::WAITING) {
      return {state, 0};
    }
    nrItems = (std::min)(expectedRows, nrItems);
    if (nrItems == 0) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, nullptr};
    }
    block = requestBlock(nrItems, nrRegs);
  } else {
    block = requestBlock(nrItems, nrRegs);
  }

  return {ExecutionState::HASMORE, std::move(block)};
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
template class ::arangodb::aql::ExecutionBlockImpl<ReturnExecutor<false>>;
template class ::arangodb::aql::ExecutionBlockImpl<ReturnExecutor<true>>;
template class ::arangodb::aql::ExecutionBlockImpl<ShortestPathExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<KShortestPathsExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SortedCollectExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SortExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SubqueryExecutor<true>>;
template class ::arangodb::aql::ExecutionBlockImpl<SubqueryExecutor<false>>;
template class ::arangodb::aql::ExecutionBlockImpl<TraversalExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<SortingGatherExecutor>;
