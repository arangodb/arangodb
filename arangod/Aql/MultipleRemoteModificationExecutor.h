////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/InputAqlItemRow.h"
#include "Aql/ModificationExecutorHelpers.h"
#include "Aql/ModificationExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Transaction/Methods.h"

namespace arangodb {
namespace aql {
class ExecutionEngine;

struct MultipleRemoteModificationInfos : ModificationExecutorInfos {
  MultipleRemoteModificationInfos(
      ExecutionEngine* engine, RegisterId inputRegister,
      RegisterId outputNewRegisterId, RegisterId outputOldRegisterId,
      RegisterId outputRegisterId, arangodb::aql::QueryContext& query,
      OperationOptions options, aql::Collection const* aqlCollection,
      ConsultAqlWriteFilter consultAqlWriteFilter, IgnoreErrors ignoreErrors,
      IgnoreDocumentNotFound ignoreDocumentNotFound, bool hasParent,
      bool isExclusive)
      : ModificationExecutorInfos(
            engine, inputRegister, RegisterPlan::MaxRegisterId,
            RegisterPlan::MaxRegisterId, outputNewRegisterId,
            outputOldRegisterId, outputRegisterId, query, std::move(options),
            aqlCollection, ExecutionBlock::DefaultBatchSize,
            ProducesResults(false), consultAqlWriteFilter, ignoreErrors,
            DoCount(true), IsReplace(false), ignoreDocumentNotFound),
        _hasParent(hasParent),
        _isExclusive(isExclusive) {}

  bool _hasParent;  // node->hasParent();
  bool _isExclusive;
};

struct MultipleRemoteModificationExecutor {
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };
  using Infos = MultipleRemoteModificationInfos;
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Stats = CoordinatorModificationStats;

  MultipleRemoteModificationExecutor(Fetcher&, Infos&);
  ~MultipleRemoteModificationExecutor() = default;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input,
                                 OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

 protected:
  auto doMultipleRemoteOperations(InputAqlItemRow&, Stats&) -> OperationResult;
  auto doMultipleRemoteModificationOutput(InputAqlItemRow&, OutputAqlItemRow&,
                                          OperationResult&) -> void;

  static transaction::Methods createTransaction(
      std::shared_ptr<transaction::Context> ctx, Infos& info);

  std::shared_ptr<transaction::Context> _ctx;
  transaction::Methods _trx;
  Infos& _info;
  ExecutionState _upstreamState;
  bool _hasFetchedDataRow{false};
};

}  // namespace aql
}  // namespace arangodb
