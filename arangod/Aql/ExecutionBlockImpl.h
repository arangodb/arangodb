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

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/BlockFetcherInterfaces.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;
class AqlItemRow;
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
 *           Executor(xxxFetcher&)
 *           std::pair<ExecutionState, std::unique_ptr<AqlItemRow>> produceRow(); 
 *         }
 *         The Executor is the implementation of one AQLNode.
 *         It is only allowed to produce one outputRow at a time, but can fetch
 *         as many rows from above as it likes. It can only follow the
 *         xxxFetcher interface to get AqlItemRows from Upstream.
 */
template<class Executor>
class ExecutionBlockImpl : public ExecutionBlock, public SingleRowFetcher {
 public:

   /**
    * @brief Construct a new ExecutionBlock
    *        This API is subject to change, we want to make it as independent of
    *        AQL / Query interna as possible.
    *
    * @param engine The AqlExecutionEngine holding the query and everything required
    *               for the execution.
    * @param node The Node used to create this ExecutionBlock
    */
  ExecutionBlockImpl(ExecutionEngine* engine, ExecutionNode const* node);
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
   *           WAITING => IO going on, immediatly return to caller.
   *           DONE => No more to expect from Upstream, if you are done with this row return DONE to caller.
   *           HASMORE => There is potentially more from above, call again if you need more input.
   *         AqlItemBlock:
   *           A matrix of result rows.
   *           Guaranteed to be non nullptr in HASMORE cas, maybe a nullptr in DONE.
   *           Is a nullptr in WAITING
   */
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> getSome(
      size_t atMost) override;

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
   *           DONE => No more to expect from Upstream, if you are done with this row return DONE to caller.
   *           HASMORE => There is potentially more from above, call again if you need more input.
   *         size_t:
   *           Number of rows effectively skipped.
   *           On WAITING this is always 0.
   *           On any other state this is between 0 and atMost.
   */
  std::pair<ExecutionState, size_t> skipSome(size_t atMost) override;

  /**
   * @brief Fetch one new AqlItemRow from upstream.
   *        **Guarantee**: the pointer returned is valid only
   *        until the next call to fetchRow.
   *
   * @return A pair with the following properties:
   *         ExecutionState:
   *           WAITING => IO going on, immediatly return to caller.
   *           DONE => No more to expect from Upstream, if you are done with this row return DONE to caller.
   *           HASMORE => There is potentially more from above, call again if you need more input.
   *         AqlItemRow:
   *           If WAITING => Do not use this Row, it is a nullptr.
   *           If HASMORE => The Row is guaranteed to not be a nullptr.
   *           If DONE => Row can be a nullptr (nothing received) or valid.
   */
  std::pair<ExecutionState, AqlItemRow const*> fetchRow() override;

 private:

  /**
   * @brief Internal helper function that fetches the next block
   *        of AqlItemRows from upstream.
   *        Will be called by the public fetchXXX() functions.
   *        **Guarantee:** Fetches a batch of 1000 rows from
   *        upstream, if successful this batch (AqlItemBlock)
   *        is retained by this class only until the next call
   *        to fetchBlock.
   *
   * @return The upstream state, can be DONE, WAITING or HASMORE.
   */
  ExecutionState fetchBlock();

 private:

  /**
   * @brief This is the working party of this implementation
   *        the template class needs to implement the logic
   *        to produce a single row from the upstream information.
   */
  Executor& _executor;
};


} // aql
} // arangodb
#endif
