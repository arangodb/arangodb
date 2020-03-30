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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_MULTI_DEPENDENCY_SINGLE_ROW_FETCHER_H
#define ARANGOD_AQL_MULTI_DEPENDENCY_SINGLE_ROW_FETCHER_H

#include "Aql/AqlCallSet.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/MultiAqlItemBlockInputRange.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include <memory>

namespace arangodb::aql {

class AqlItemBlock;
template <BlockPassthrough>
class DependencyProxy;
class ShadowAqlItemRow;
class SkipResult;

/**
 * @brief Interface for all AqlExecutors that do need one
 *        row at a time from every dependency in order to make progress.
 *        The guarantee is the following:
 *        If fetchRowForDependency returns a row of the given dependency,
 *        the pointer to
 *        this row stays valid until the next call
 *        of fetchRowForDependency.
 *        So we can have one Row per dependency in flight.
 */
class MultiDependencySingleRowFetcher {
 private:
  /**
   * @brief helper struct to contain all information about dependency-states
   */
  struct DependencyInfo {
   public:
    DependencyInfo();
    ~DependencyInfo() = default;
    /**
     * @brief Holds state returned by the last fetchBlock() call.
     *        This is similar to ExecutionBlock::_upstreamState, but can also be
     *        WAITING.
     *        Part of the Fetcher, and may be moved if the Fetcher
     * implementations are moved into separate classes.
     */
    ExecutionState _upstreamState;

    /**
     * @brief Input block currently in use. Used for memory management by the
     *        SingleRowFetcher. May be moved if the Fetcher implementations
     *        are moved into separate classes.
     */
    SharedAqlItemBlockPtr _currentBlock;

    /**
     * @brief Index of the row to be returned next by fetchRow(). This is valid
     *        iff _currentBlock != nullptr and it's smaller or equal than
     *        _currentBlock->size(). May be moved if the Fetcher implementations
     *        are moved into separate classes.
     */
    size_t _rowIndex;
  };

 public:
  using DataRange = MultiAqlItemBlockInputRange;
  explicit MultiDependencySingleRowFetcher(DependencyProxy<BlockPassthrough::Disable>& executionBlock);
  TEST_VIRTUAL ~MultiDependencySingleRowFetcher() = default;

  void init();

 protected:
  // only for testing! Does not initialize _dependencyProxy!
  MultiDependencySingleRowFetcher();

 public:
  std::pair<ExecutionState, size_t> preFetchNumberOfRows(size_t atMost);
  std::pair<ExecutionState, size_t> preFetchNumberOfRowsForDependency(size_t dependency,
                                                                      size_t atMost);

  // May only be called once, after the dependencies are injected.
  void initDependencies();

  size_t numberDependencies();

  /**
   * @brief Fetch one new AqlItemRow from the specified upstream dependency.
   *
   * @param atMost may be passed if a block knows the maximum it might want to
   *        fetch from upstream. Will not fetch more than the default batch
   *        size, so passing something greater than it will not have any effect.
   *
   * @return A pair with the following properties:
   *         ExecutionState:
   *           WAITING => IO going on, immediately return to caller.
   *           DONE => No more to expect from Upstream, if you are done with
   *                   this row return DONE to caller.
   *           HASMORE => There is potentially more from above, call again if
   *                      you need more input.
   *         AqlItemRow:
   *           If WAITING => Do not use this Row, it is a nullptr.
   *           If HASMORE => The Row is guaranteed to not be a nullptr.
   *           If DONE => Row can be a nullptr (nothing received) or valid.
   */
  // This is only TEST_VIRTUAL, so we ignore this lint warning:
  // NOLINTNEXTLINE google-default-arguments
  TEST_VIRTUAL std::pair<ExecutionState, InputAqlItemRow> fetchRowForDependency(
      size_t dependency, size_t atMost = ExecutionBlock::DefaultBatchSize);

  std::pair<ExecutionState, ShadowAqlItemRow> fetchShadowRow(size_t atMost = ExecutionBlock::DefaultBatchSize);

  //@deprecated
  auto useStack(AqlCallStack const& stack) -> void;

  [[nodiscard]] auto execute(AqlCallStack const&, AqlCallSet const&)
      -> std::tuple<ExecutionState, SkipResult, std::vector<std::pair<size_t, AqlItemBlockInputRange>>>;

  [[nodiscard]] auto upstreamState() const -> ExecutionState;

 private:
  DependencyProxy<BlockPassthrough::Disable>* _dependencyProxy;

  /**
   * @brief Holds the information for all dependencies
   */
  std::vector<DependencyInfo> _dependencyInfos;
  std::vector<ExecutionState> _dependencyStates;

  /// @brief Only needed for parallel executors; could be omitted otherwise
  ///        It's size is >0 after init() is called, and this is currently used
  ///        in initOnce() to make sure that init() is called exactly once.
  std::vector<std::optional<AqlCallStack>> _callsInFlight;

  bool _didReturnSubquerySkips{false};

 private:
  [[nodiscard]] auto executeForDependency(size_t dependency, AqlCallStack& stack)
      -> std::tuple<ExecutionState, SkipResult, AqlItemBlockInputRange>;

  /**
   * @brief Delegates to ExecutionBlock::fetchBlock()
   */
  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlockForDependency(size_t dependency,
                                                                           size_t atMost);

  std::pair<ExecutionState, size_t> skipSomeForDependency(size_t dependency, size_t atMost);

  /**
   * @brief Delegates to ExecutionBlock::getNrInputRegisters()
   */
  RegisterId getNrInputRegisters() const;

  bool indexIsValid(DependencyInfo const& info) const;

  bool isDone(DependencyInfo const& info) const;

  bool isLastRowInBlock(DependencyInfo const& info) const;

  /**
   * @brief If it returns true, there are no more data row in the current
   * subquery level. If it returns false, there may or may not be more.
   */
  bool noMoreDataRows(DependencyInfo const& info) const;

  bool isAtShadowRow(DependencyInfo const& info) const;

  bool fetchBlockIfNecessary(const size_t dependency, const size_t atMost);
};

}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_SINGLE_ROW_FETCHER_H
