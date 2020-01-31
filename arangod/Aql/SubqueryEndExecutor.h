////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SUBQUERY_END_EXECUTOR_H
#define ARANGOD_AQL_SUBQUERY_END_EXECUTOR_H

#include "Aql/AqlCall.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {

class NoStats;
class OutputAqlItemRow;
template <BlockPassthrough>
class SingleRowFetcher;

class SubqueryEndExecutorInfos : public ExecutorInfos {
 public:
  SubqueryEndExecutorInfos(std::shared_ptr<std::unordered_set<RegisterId>> readableInputRegisters,
                           std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
                           RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                           std::unordered_set<RegisterId> const& registersToClear,
                           std::unordered_set<RegisterId> registersToKeep,
                           velocypack::Options const* options, RegisterId inReg,
                           RegisterId outReg);

  SubqueryEndExecutorInfos() = delete;
  SubqueryEndExecutorInfos(SubqueryEndExecutorInfos&&) noexcept = default;
  SubqueryEndExecutorInfos(SubqueryEndExecutorInfos const&) = delete;
  ~SubqueryEndExecutorInfos();

  [[nodiscard]] velocypack::Options const* vpackOptions() const noexcept;
  [[nodiscard]] RegisterId getOutputRegister() const noexcept;
  [[nodiscard]] bool usesInputRegister() const noexcept;
  [[nodiscard]] RegisterId getInputRegister() const noexcept;

 private:
  velocypack::Options const* _vpackOptions;
  RegisterId const _outReg;
  RegisterId const _inReg;
};

class SubqueryEndExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };

  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = SubqueryEndExecutorInfos;
  using Stats = NoStats;

  SubqueryEndExecutor(Fetcher& fetcher, SubqueryEndExecutorInfos& infos);
  ~SubqueryEndExecutor();

  [[deprecated]] std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);
  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const;

  // produceRows accumulates all input rows it can get into _accumulator, which
  // will then be read out by ExecutionBlockImpl
  // TODO: can the production of output be moved to produceRows again?
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;
  // skipRowsRange consumes all data rows available on the input and just
  // ignores it. real skips of a subqueries will not execute the whole subquery
  // so this might not be necessary at all (except for modifying subqueries,
  // where we have to execute the subquery)
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
      -> std::tuple<ExecutorState, size_t, AqlCall>;
  [[nodiscard]] auto stealValue(AqlValue& result) -> AqlValueGuard;

 private:
  enum class State {
    ACCUMULATE_DATA_ROWS,
    PROCESS_SHADOW_ROWS,
  };

  // Accumulator for rows between shadow rows.
  // We need to allocate the buffer ourselves because we need to keep
  // control of it to hand over to an AqlValue
  class Accumulator {
   public:
    explicit Accumulator(VPackOptions const* options);
    void reset();

    void addValue(AqlValue const& value);

    AqlValueGuard stealValue(AqlValue& value);

    size_t numValues() const noexcept;

   private:
    VPackOptions const* const _options;
    std::unique_ptr<arangodb::velocypack::Buffer<uint8_t>> _buffer{nullptr};
    std::unique_ptr<VPackBuilder> _builder{nullptr};
    size_t _numValues{0};
  };

 private:
  Fetcher& _fetcher;
  SubqueryEndExecutorInfos& _infos;

  Accumulator _accumulator;

  State _state{State::ACCUMULATE_DATA_ROWS};
};
}  // namespace aql
}  // namespace arangodb

#endif
