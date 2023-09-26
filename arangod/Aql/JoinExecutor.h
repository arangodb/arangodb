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
#include "Aql/IndexNode.h"
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

class JoinExecutorInfos {};

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
  Fetcher& _fetcher;
  Infos& _infos;
};

}  // namespace aql
}  // namespace arangodb
