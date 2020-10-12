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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_ID_EXECUTOR_H
#define ARANGOD_AQL_ID_EXECUTOR_H

#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionState.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"

#include <tuple>
#include <utility>

// There are currently three variants of IdExecutor in use:
//
// - IdExecutor<ConstFetcher>
//     This is the SingletonBlock.
// - IdExecutor<SingleRowFetcher<BlockPassthrough::Enable>>
//     This is the DistributeConsumerBlock. It holds a distributeId and honors
//     isResponsibleForInitializeCursor.
//
// The last variant using the SingleRowFetcher could be replaced by the (faster)
// void variant. It only has to learn distributeId and
// isResponsibleForInitializeCursor for that.

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
class ExecutionEngine;
class ExecutionNode;
class RegisterInfos;
class CountStats;
class OutputAqlItemRow;

class IdExecutorInfos {
 public:
  explicit IdExecutorInfos(bool doCount, RegisterId outputRegister = 0,
                           std::string distributeId = {""},
                           bool isResponsibleForInitializeCursor = true);

  IdExecutorInfos() = delete;
  IdExecutorInfos(IdExecutorInfos&&) = default;
  IdExecutorInfos(IdExecutorInfos const&) = delete;
  ~IdExecutorInfos() = default;

  [[nodiscard]] auto doCount() const noexcept -> bool;

  [[nodiscard]] auto getOutputRegister() const noexcept -> RegisterId;

  [[nodiscard]] std::string const& distributeId() const noexcept;

  [[nodiscard]] bool isResponsibleForInitializeCursor() const noexcept;

 private:
  bool _doCount;
  
  bool const _isResponsibleForInitializeCursor;

  RegisterId _outputRegister;

  std::string const _distributeId;
};

template <class UsedFetcher>
// cppcheck-suppress noConstructor
class IdExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Enable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  // Only Supports SingleRowFetcher and ConstFetcher
  using Fetcher = UsedFetcher;
  using Infos = IdExecutorInfos;
  using Stats = CountStats;

  IdExecutor(Fetcher&, IdExecutorInfos& infos);
  ~IdExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  auto skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
      -> std::tuple<ExecutorState, CountStats, size_t, AqlCall>;

 private:
  Fetcher& _fetcher;
  Infos& _infos;
};
}  // namespace aql
}  // namespace arangodb

#endif
