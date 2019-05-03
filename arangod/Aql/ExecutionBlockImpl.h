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

#ifndef ARANGOD_AQL_EXECUTION_BLOCK_IMPL_H
#define ARANGOD_AQL_EXECUTION_BLOCK_IMPL_H 1

#include <functional>
#include <queue>

#include "Aql/AllRowsFetcher.h"
#include "Aql/ConstFetcher.h"
#include "Aql/DependencyProxy.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutionStats.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "OutputAqlItemRow.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;
class InputAqlItemRow;
class ExecutionNode;
class ExecutionEngine;

/**
 * @brief This is the implementation class of AqlExecutionBlocks.
 *        It is responsible to create AqlItemRows for subsequent
 *        Blocks, also it will fetch new AqlItemRows from preceeding
 *        Blocks whenever it is necessary.
 *        For performance reasons this will all be done in batches
 *        of 1000 Rows each.
 *
 * @tparam Executor A class that needs to have the following signature:
 *         Executor {
 *           using Fetcher = xxxFetcher;
 *           using Infos = xxxExecutorInfos;
 *           using Stats = xxxStats;
 *           static const struct {
 *             // Whether input rows stay in the same order.
 *             const bool preservesOrder;
 *
 *             // Whether input blocks can be reused as output blocks. This
 *             // can be true if:
 *             // - There will be exactly one output row per input row.
 *             // - produceRows() for row i will be called after fetchRow() of
 *             //   row i.
 *             // - The order of rows is preserved. i.e. preservesOrder
 *             // - The register planning must reserve the output register(s),
 *             //   if any, already in the input blocks.
 *             //   (This one is not really a property of the Executor, but it
 *             //   still must be true).
 *             // - It must use the SingleRowFetcher. This is not really a
 *             //   necessity, but it should always already be possible due to
 *             //   the properties above.
 *             const bool allowsBlockPassthrough;
 *
 *           } properties;
 *           Executor(Fetcher&, Infos&);
 *           std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);
 *         }
 *         The Executor is the implementation of one AQLNode.
 *         It may produce zero, one, or multiple outputRows at a time. The
 *         OutputAqlItemRow imposes a restriction (e.g. atMost) on how many.
 *         Currently, this is just the size of the block (because this is always
 *         restricted by atMost anyway). Later this may be replaced by a
 *         separate limit. It can fetch as many rows from above as it likes.
 *         It can only follow the xxxFetcher interface to get AqlItemRows from
 *         Upstream.
 */
template <class Executor>
class ExecutionBlockImpl final : public ExecutionBlock {
  using Fetcher = typename Executor::Fetcher;
  using ExecutorStats = typename Executor::Stats;
  using Infos = typename Executor::Infos;
  using DependencyProxy =
      typename aql::DependencyProxy<Executor::Properties::allowsBlockPassthrough>;

  static_assert(
      !Executor::Properties::allowsBlockPassthrough || Executor::Properties::preservesOrder,
      "allowsBlockPassthrough must imply preservesOrder, but does not!");

 public:
  /**
   * @brief Construct a new ExecutionBlock
   *        This API is subject to change, we want to make it as independent of
   *        AQL / Query interna as possible.
   *
   * @param engine The AqlExecutionEngine holding the query and everything
   *               required for the execution.
   * @param node The Node used to create this ExecutionBlock
   */
  ExecutionBlockImpl(ExecutionEngine* engine, ExecutionNode const* node,
                     typename Executor::Infos&&);

  ~ExecutionBlockImpl();

  /**
   * @brief Produce atMost many output rows, or less.
   *        May return waiting if I/O has to be performed
   *        so we can release this thread.
   *        Is required to return DONE if it is guaranteed
   *        that this block does not produce more rows,
   *        Returns HASMORE if the DONE guarantee cannot be given.
   *        HASMORE does not give strict guarantee, it maybe that
   *        HASMORE is returned but no more rows can be produced.
   *
   * @param atMost Upper bound of AqlItemRows to be returned.
   *               Target is to get as close to this upper bound
   *               as possible.
   *
   * @return A pair with the following properties:
   *         ExecutionState:
   *           WAITING => IO going on, immediately return to caller.
   *           DONE => No more to expect from Upstream, if you are done with
   *                   this row return DONE to caller.
   *           HASMORE => There is potentially more from above, call again if
   *                      you need more input.
   *         AqlItemBlock:
   *           A matrix of result rows.
   *           Guaranteed to be non nullptr in the HASMORE case, maybe a nullptr
   *           in DONE. Is a nullptr in WAITING.
   */
  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSome(size_t atMost) override;

  /**
   * @brief Like get some, but lines are skipped and not returned.
   *        This can use optimizations to not actually create the data.
   *
   * @param atMost Upper bound of AqlItemRows to be skipped.
   *               Target is to get as close to this upper bound
   *               as possible.
   *
   * @return A pair with the following properties:
   *         ExecutionState:
   *           WAITING => IO going on, immediatly return to caller.
   *           DONE => No more to expect from Upstream, if you are done with
   *                   this row return DONE to caller.
   *           HASMORE => There is potentially more from above, call again if
   *                   you need more input. size_t: Number of rows effectively
   *                   skipped. On WAITING this is always 0. On any other state
   *                   this is between 0 and atMost.
   */
  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override;

  std::pair<ExecutionState, Result> initializeCursor(InputAqlItemRow const& input) override;

  Infos const& infos() const { return _infos; }

  /// @brief shutdown, will be called exactly once for the whole query
  /// Special implementation for all Executors that need to implement Shutdown
  /// Most do not, we might be able to move their shutdown logic to a more
  /// central place.
  std::pair<ExecutionState, Result> shutdown(int) override;

 private:
  /**
   * @brief Inner getSome() part, without the tracing calls.
   */
  std::pair<ExecutionState, SharedAqlItemBlockPtr> getSomeWithoutTrace(size_t atMost);

  /**
   * @brief Allocates a new AqlItemBlock and returns it, with the specified
   *        number of rows (nrItems) and columns (nrRegs).
   *        In case the Executor supports pass-through of blocks (i.e. reuse the
   *        input blocks as output blocks), it returns such an input block. In
   *        this case, the number of columns must still match - this has to be
   *        guaranteed by register planning.
   *        The state will be HASMORE if and only if it returns an actual block,
   *        which it always will in the non-pass-through case (modulo
   *        exceptions). If it is WAITING or DONE, the returned block is always
   *        a nullptr. This happens only if upstream is WAITING, or
   *        respectively, if it is DONE and did not return a new block.
   */
  std::pair<ExecutionState, SharedAqlItemBlockPtr> requestWrappedBlock(size_t nrItems,
                                                                       RegisterId nrRegs);

  std::unique_ptr<OutputAqlItemRow> createOutputRow(SharedAqlItemBlockPtr& newBlock) const;

  Query const& getQuery() const { return _query; }

  Executor& executor() { return _executor; }

  bool const& isModificationNode() { return _isModificationNode; }

  /// @brief request an AqlItemBlock from the memory manager
  SharedAqlItemBlockPtr requestBlock(size_t nrItems, RegisterId nrRegs);

 private:
  /**
   * @brief Used to allow the row Fetcher to access selected methods of this
   *        ExecutionBlock object.
   */
  DependencyProxy _dependencyProxy;

  /**
   * @brief Fetcher used by the Executor. Calls this->fetchBlock() and handles
   *        memory management of AqlItemBlocks as needed by Executor.
   */
  Fetcher _rowFetcher;

  /**
   * @brief This is the working party of this implementation
   *        the template class needs to implement the logic
   *        to produce a single row from the upstream information.
   */
  Infos _infos;

  Executor _executor;

  std::unique_ptr<OutputAqlItemRow> _outputItemRow;

  Query const& _query;

  bool _isModificationNode;
};

}  // namespace aql
}  // namespace arangodb
#endif
