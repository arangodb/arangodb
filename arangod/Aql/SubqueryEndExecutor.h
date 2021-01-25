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
/// @author Michael Hackstein
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SUBQUERY_END_EXECUTOR_H
#define ARANGOD_AQL_SUBQUERY_END_EXECUTOR_H

#include "Aql/AqlCall.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

#include <velocypack/Builder.h>

namespace arangodb {
struct ResourceMonitor;

namespace aql {

class NoStats;
class OutputAqlItemRow;
template <BlockPassthrough>
class SingleRowFetcher;

class SubqueryEndExecutorInfos {
 public:
  SubqueryEndExecutorInfos(velocypack::Options const* options, 
                           arangodb::ResourceMonitor& resourceMonitor, 
                           RegisterId inReg,
                           RegisterId outReg);

  SubqueryEndExecutorInfos() = delete;
  SubqueryEndExecutorInfos(SubqueryEndExecutorInfos&&) = default;
  SubqueryEndExecutorInfos(SubqueryEndExecutorInfos const&) = delete;
  ~SubqueryEndExecutorInfos();

  [[nodiscard]] velocypack::Options const* vpackOptions() const noexcept;
  [[nodiscard]] RegisterId getOutputRegister() const noexcept;
  [[nodiscard]] bool usesInputRegister() const noexcept;
  [[nodiscard]] RegisterId getInputRegister() const noexcept;
  [[nodiscard]] arangodb::ResourceMonitor& getResourceMonitor() const noexcept;

 private:
  velocypack::Options const* _vpackOptions;
  arangodb::ResourceMonitor& _resourceMonitor;
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

  void initializeCursor();

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
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;

  /**
   * @brief Consume the given shadow row and write the aggregated value to it
   *
   * @param shadowRow The shadow row
   * @param output Output block
   */
  auto consumeShadowRow(ShadowAqlItemRow shadowRow, OutputAqlItemRow& output) -> void;

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
    explicit Accumulator(arangodb::ResourceMonitor& resourceMonitor, velocypack::Options const* options);
    ~Accumulator();
    
    void reset();

    void addValue(AqlValue const& value);

    AqlValueGuard stealValue(AqlValue& value);

    size_t numValues() const noexcept;

   private:
    arangodb::ResourceMonitor& _resourceMonitor;
    velocypack::Options const* _options;
    arangodb::velocypack::Buffer<uint8_t> _buffer;
    velocypack::Builder _builder;
    size_t _memoryUsage{0};
    size_t _numValues{0};
  };

 private:
  SubqueryEndExecutorInfos& _infos;

  Accumulator _accumulator;
};
}  // namespace aql
}  // namespace arangodb

#endif
