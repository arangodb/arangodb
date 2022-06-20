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
class TakeWhileStats;
template<BlockPassthrough>
class SingleRowFetcher;

class TakeWhileExecutorInfos {
 public:
  TakeWhileExecutorInfos(RegisterId inputRegister, bool emitFirstFalseLine);

  TakeWhileExecutorInfos() = delete;
  TakeWhileExecutorInfos(TakeWhileExecutorInfos&&) = default;
  TakeWhileExecutorInfos(TakeWhileExecutorInfos const&) = delete;
  ~TakeWhileExecutorInfos() = default;

  [[nodiscard]] auto getInputRegister() const noexcept -> RegisterId;

 private:
  // This is exactly the value in the parent member ExecutorInfo::_inRegs,
  // respectively getInputRegisters().
  RegisterId _inputRegister;
  bool _emitFirstFalseLine;
};

class TakeWhileExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = TakeWhileExecutorInfos;
  using Stats = TakeWhileStats;

  TakeWhileExecutor() = delete;
  TakeWhileExecutor(TakeWhileExecutor&&) = default;
  TakeWhileExecutor(TakeWhileExecutor const&) = delete;
  TakeWhileExecutor(Fetcher& fetcher, Infos&);
  ~TakeWhileExecutor() = default;

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

  // [[nodiscard]] auto expectedNumberOfRowsNew(
  //     AqlItemBlockInputRange const& input, AqlCall const& call) const
  //     noexcept
  //     -> size_t;

 private:
  Infos& _infos;
};

}  // namespace arangodb::aql
