////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_LIMIT_EXECUTOR_H
#define ARANGOD_AQL_LIMIT_EXECUTOR_H

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionState.h"
#include "Aql/LimitStats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/types.h"

#include <iosfwd>
#include <memory>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class RegisterInfos;
template <BlockPassthrough>
class SingleRowFetcher;

class LimitExecutorInfos {
 public:
  LimitExecutorInfos(size_t offset, size_t limit, bool fullCount);

  LimitExecutorInfos() = delete;
  LimitExecutorInfos(LimitExecutorInfos&&) = default;
  LimitExecutorInfos(LimitExecutorInfos const&) = delete;
  ~LimitExecutorInfos() = default;

  size_t getOffset() const noexcept { return _offset; };
  size_t getLimit() const noexcept { return _limit; };
  size_t getLimitPlusOffset() const noexcept { return _offset + _limit; };
  bool isFullCountEnabled() const noexcept { return _fullCount; };

 private:
  /// @brief the remaining offset
  size_t const _offset;

  /// @brief the limit
  size_t const _limit;

  /// @brief whether or not the node should fully count what it limits
  bool const _fullCount;
};

/**
 * @brief Implementation of Limit Node
 */

class LimitExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Enable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = LimitExecutorInfos;
  using Stats = LimitStats;

  LimitExecutor() = delete;
  LimitExecutor(LimitExecutor&&) = default;
  LimitExecutor(LimitExecutor const&) = delete;
  LimitExecutor(Fetcher& fetcher, Infos&);
  ~LimitExecutor();

  /**
   * @brief produce the next Rows of Aql Values.
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

 private:
  Infos const& infos() const noexcept { return _infos; };

  auto remainingOffset() const noexcept -> size_t {
    auto const offset = infos().getOffset();

    // Restricted value of _counter in [0, offset]
    auto const boundedCounter = std::min(offset, _counter);
    TRI_ASSERT(boundedCounter <= offset);

    return offset - boundedCounter;
  }

  auto remainingLimit() const noexcept -> size_t {
    auto const offset = infos().getOffset();
    auto const limitPlusOffset = infos().getLimitPlusOffset();

    // Restricted value of _counter in [offset, limitPlusOffset]
    auto const boundedCounter = std::min(limitPlusOffset, std::max(offset, _counter));
    TRI_ASSERT(offset <= boundedCounter);
    TRI_ASSERT(boundedCounter <= limitPlusOffset);
    return limitPlusOffset - boundedCounter;
  }

  [[nodiscard]] auto limitFulfilled() const noexcept -> bool;

  auto calculateUpstreamCall(const AqlCall& clientCall) const -> AqlCall;

 private:
  Infos const& _infos;
  InputAqlItemRow _lastRowToOutput;
  // Number of input lines seen
  size_t _counter = 0;
  bool _didProduceRows = false;
};

}  // namespace aql
}  // namespace arangodb

#endif
