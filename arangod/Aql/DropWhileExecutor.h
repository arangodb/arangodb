////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Aql/ExecutionState.h"
#include "Aql/RegisterInfos.h"
#include "Aql/types.h"

namespace arangodb::aql {

struct AqlCall;
class AqlItemBlockInputRange;
class InputAqlItemRow;
class OutputAqlItemRow;
class RegisterInfos;
class FilterStats;
template<BlockPassthrough>
class SingleRowFetcher;

class DropWhileExecutorInfos {
 public:
  explicit DropWhileExecutorInfos(RegisterId inputRegister);

  DropWhileExecutorInfos() = delete;
  DropWhileExecutorInfos(DropWhileExecutorInfos&&) = default;
  DropWhileExecutorInfos(DropWhileExecutorInfos const&) = delete;
  ~DropWhileExecutorInfos() = default;

  [[nodiscard]] auto getInputRegister() const noexcept -> RegisterId;

 private:
  RegisterId _inputRegister;
};

class DropWhileExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = DropWhileExecutorInfos;
  using Stats = FilterStats;

  DropWhileExecutor() = delete;
  DropWhileExecutor(DropWhileExecutor&&) = default;
  DropWhileExecutor(DropWhileExecutor const&) = delete;
  DropWhileExecutor(Fetcher&, Infos&);
  ~DropWhileExecutor() = default;

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input,
                                 OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& inputRange,
                                   AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

  [[nodiscard]] auto expectedNumberOfRowsNew(
      AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept
      -> size_t;

 private:
  Infos& _infos;
  bool _stopDropping = false;
};

}  // namespace arangodb::aql
