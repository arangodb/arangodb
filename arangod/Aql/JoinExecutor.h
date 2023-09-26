////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/Ast.h"
#include "Aql/DocumentProducingHelper.h"
#include "Aql/ExecutionState.h"
#include "Aql/IndexMerger.h"
#include "Aql/JoinNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/NonConstExpressionContainer.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"
#include "Transaction/Methods.h"

#include <memory>

namespace arangodb {
class IndexIterator;
struct ResourceMonitor;

namespace aql {

class ExecutionEngine;
class ExecutorExpressionContext;
class RegisterInfos;
class Expression;
class InputAqlItemRow;
class Projections;
class QueryContext;

template<BlockPassthrough>
class SingleRowFetcher;

struct AstNode;
struct Collection;
struct NonConstExpression;

struct JoinExecutorInfos {
  struct IndexInfo {
    // Register to load the document into
    RegisterId documentOutputRegister;
    // Sorted lists of registers to load the projections into, pointing into
    // `registers`.
    std::span<RegisterId> projectionOutputRegisters;

    // Associated document collection for this index
    Collection const* collection;
    // Index handle
    transaction::Methods::IndexHandle index;
  };

  std::vector<IndexInfo> indexes;
  std::vector<RegisterId> registers;
  QueryContext* query;
};

/**
 * @brief Implementation of Join Node
 */
class JoinExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };

  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = JoinExecutorInfos;
  using Stats = IndexStats;

  JoinExecutor() = delete;
  JoinExecutor(JoinExecutor&&) = delete;
  JoinExecutor(JoinExecutor const&) = delete;
  JoinExecutor(Fetcher& fetcher, Infos&);
  ~JoinExecutor();

  auto produceRows(AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  auto skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& clientCall)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

 private:
  void constructMerger();

  Fetcher& _fetcher;
  Infos& _infos;
  std::unique_ptr<AqlIndexMerger> _merger;

  transaction::Methods _trx;

  InputAqlItemRow _currentRow{CreateInvalidInputRowHint()};
  ExecutorState _currentRowState;
};

}  // namespace aql
}  // namespace arangodb
