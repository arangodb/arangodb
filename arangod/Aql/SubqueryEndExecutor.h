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
                           transaction::Methods* trxPtr, RegisterId inReg,
                           RegisterId outReg);

  SubqueryEndExecutorInfos() = delete;
  SubqueryEndExecutorInfos(SubqueryEndExecutorInfos&&);
  SubqueryEndExecutorInfos(SubqueryEndExecutorInfos const&) = delete;
  ~SubqueryEndExecutorInfos();

  transaction::Methods* getTrxPtr() const noexcept { return _trxPtr; }
  inline RegisterId getOutputRegister() const { return _outReg; }
  bool usesInputRegister() const;
  inline RegisterId getInputRegister() const { return _inReg; }

 private:
  transaction::Methods* _trxPtr;
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

  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);
  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const;

 private:
  enum State : int {
    ACCUMULATE,
    RELEVANT_SHADOW_ROW_PENDING,
    FORWARD_IRRELEVANT_SHADOW_ROWS
  };
  void resetAccumulator();

 private:
  Fetcher& _fetcher;
  SubqueryEndExecutorInfos& _infos;

  // Accumulator for rows between shadow rows.
  // We need to allocate the buffer ourselves because we need to keep
  // control of it to hand over to an AqlValue
  std::unique_ptr<arangodb::velocypack::Buffer<uint8_t>> _buffer;
  std::unique_ptr<VPackBuilder> _accumulator;

  State _state;
};
}  // namespace aql
}  // namespace arangodb

#endif
