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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_ALL_ROWS_FETCHER_H
#define ARANGOD_AQL_ALL_ROWS_FETCHER_H

#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionState.h"

#include <memory>

namespace arangodb {
namespace aql {

class AqlItemBlock;
class ExecutionBlock;

/**
 * @brief Interface for all AqlExecutors that do need all
 *        rows at a time in order to make progress.
 *        The guarantee is the following:
 *        If fetchAllRows returns a matrix the pointer to
 *        this matrix stays valid until the next call
 *        of fetchAllRows.
 */
class AllRowsFetcher {
 public:
  explicit AllRowsFetcher(ExecutionBlock& executionBlock);

  TEST_VIRTUAL ~AllRowsFetcher() = default;

 protected:
  // only for testing! Does not initialize _executionBlock!
  AllRowsFetcher() = default;

 public:
  /**
   * @brief Fetch one new AqlItemRow from upstream.
   *        **Guarantee**: the pointer returned is valid only
   *        until the next call to fetchRow.
   *
   * @return A pair with the following properties:
   *         ExecutionState:
   *           WAITING => IO going on, immediatly return to caller.
   *           DONE => No more to expect from Upstream, if you are done with
   *                   this row return DONE to caller.
   *           HASMORE => There is potentially more from above, call again if
   *                      you need more input.
   *         AqlItemRow:
   *           If WAITING => Do not use this Row, it is a nullptr.
   *           If HASMORE => The Row is guaranteed to not be a nullptr.
   *           If DONE => Row can be a nullptr (nothing received) or valid.
   */
  TEST_VIRTUAL std::pair<ExecutionState, AqlItemMatrix const*> fetchAllRows();

 private:
  ExecutionBlock* _executionBlock;

  /**
   * @brief Holds state returned by the last fetchBlock() call.
   *        This is similar to ExecutionBlock::_upstreamState, but can also be
   *        WAITING.
   *        Part of the Fetcher, and may be moved if the Fetcher implementations
   *        are moved into separate classes.
   */
  ExecutionState _upstreamState;

 private:
  /**
   * @brief Delegates to ExecutionBlock::fetchBlock()
   */
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> fetchBlock();

  /**
   * @brief Delegates to ExecutionBlock::getNrInputRegisters()
   */
  RegisterId getNrInputRegisters() const;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_ALL_ROWS_FETCHER_H
