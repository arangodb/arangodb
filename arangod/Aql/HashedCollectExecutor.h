////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_HASHED_COLLECT_EXECUTOR_H
#define ARANGOD_AQL_HASHED_COLLECT_EXECUTOR_H

#include "Aql/Aggregator.h"
#include "Aql/AqlValueGroup.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"
#include "Aql/types.h"

#include <memory>
#include <unordered_map>

namespace arangodb {
struct ResourceMonitor;

namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
class OutputAqlItemRow;
class RegisterInfos;
template <BlockPassthrough>
class SingleRowFetcher;
struct Aggregator;

class HashedCollectExecutorInfos {
 public:
  /**
   * @brief Construct a new Hashed Collect Executor Infos object
   *
   * @param groupRegisters Registers the grouping is based on.
   *                       If values in the registers are identical,
   *                       the rows are considered as the same group.
   *                       Format: <outputRegister, inputRegister>
   * @param collectRegister Register to write the GroupingResult to
   *                        (COLLECT ... INTO collectRegister)
   * @param aggregateTypes Aggregation methods used
   * @param aggregateRegisters Input and output Register for Aggregation
   * @param trxPtr The AQL transaction, as it might be needed for aggregates
   */
  HashedCollectExecutorInfos(std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters,
                             RegisterId collectRegister, std::vector<std::string>&& aggregateTypes,
                             std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
                             velocypack::Options const* vpackOptions, 
                             arangodb::ResourceMonitor& resourceMonitor);

  HashedCollectExecutorInfos() = delete;
  HashedCollectExecutorInfos(HashedCollectExecutorInfos&&) = default;
  HashedCollectExecutorInfos(HashedCollectExecutorInfos const&) = delete;
  ~HashedCollectExecutorInfos() = default;

 public:
  std::vector<std::pair<RegisterId, RegisterId>> const& getGroupRegisters() const;
  std::vector<std::pair<RegisterId, RegisterId>> const& getAggregatedRegisters() const;
  std::vector<std::string> const& getAggregateTypes() const;
  velocypack::Options const* getVPackOptions() const;
  RegisterId getCollectRegister() const noexcept;
  arangodb::ResourceMonitor& getResourceMonitor() const;

 private:
  /// @brief aggregate types
  std::vector<std::string> _aggregateTypes;

  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _aggregateRegisters;

  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _groupRegisters;

  /// @brief the optional register that contains the values for each group
  /// if no values should be returned, then this has a value of MaxRegisterId
  /// this register is also used for counting in case WITH COUNT INTO var is
  /// used
  RegisterId _collectRegister;
  
  /// @brief the transaction for this query
  velocypack::Options const* _vpackOptions;

  /// @brief resource manager
  arangodb::ResourceMonitor& _resourceMonitor;
};

/**
 * @brief Implementation of Hashed Collect Executor
 */

class HashedCollectExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = HashedCollectExecutorInfos;
  using Stats = NoStats;

  HashedCollectExecutor() = delete;
  HashedCollectExecutor(HashedCollectExecutor&&) = default;
  HashedCollectExecutor(HashedCollectExecutor const&) = delete;
  HashedCollectExecutor(Fetcher& fetcher, Infos&);
  ~HashedCollectExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

  /**
   * @brief This Executor does not know how many distinct rows will be fetched
   * from upstream, it can only report how many it has found by itself, plus
   * it knows that it can only create as many new rows as pulled from upstream.
   * So it will overestimate.
   */
  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;

 private:
  struct ValueAggregators {
    ValueAggregators(std::vector<Aggregator::Factory const*> factories, velocypack::Options const* opts);
    ~ValueAggregators();
    std::size_t size() const;
    Aggregator& operator[](std::size_t index);
    static void operator delete(void* ptr);
   private:
    std::size_t _size;
  };
  using GroupKeyType = HashedAqlValueGroup;
  using GroupValueType = std::unique_ptr<ValueAggregators>;
  using GroupMapType =
      std::unordered_map<GroupKeyType, GroupValueType, AqlValueGroupHash, AqlValueGroupEqual>;

  Infos const& infos() const noexcept;

  /**
   * @brief Consumes all rows from upstream
   *        Every row is collected into one of the groups.
   *
   * @param inputRange Upstream range, will be fully consumed
   * @return true We have consumed everything, start output
   * @return false We do not have all input ask for more.
   */
  auto consumeInputRange(AqlItemBlockInputRange& inputRange) -> bool;

  /**
   * @brief State this Executor needs to report
   *
   * @return ExecutorState
   */
  auto returnState() const -> ExecutorState;

  void destroyAllGroupsAqlValues();

  static std::vector<Aggregator::Factory const*> createAggregatorFactories(HashedCollectExecutor::Infos const& infos);

  GroupMapType::iterator findOrEmplaceGroup(InputAqlItemRow& input);

  void consumeInputRow(InputAqlItemRow& input);

  void writeCurrentGroupToOutput(OutputAqlItemRow& output);

  std::unique_ptr<ValueAggregators> makeAggregateValues() const;

  size_t memoryUsageForGroup(GroupKeyType const& group, bool withBase) const;

 private:
  Infos const& _infos;

  /// @brief We need to save any input row (it really doesn't matter, except for
  /// when input blocks are freed - thus the last), so we can produce output
  /// rows later.
  InputAqlItemRow _lastInitializedInputRow;

  /// @brief hashmap of all encountered groups
  GroupMapType _allGroups;
  GroupMapType::const_iterator _currentGroup;

  bool _isInitialized;  // init() was called successfully (e.g. it returned DONE)

  std::vector<Aggregator::Factory const*> _aggregatorFactories;

  GroupKeyType _nextGroup;

  size_t _returnedGroups = 0;
};

}  // namespace aql
}  // namespace arangodb

#endif
