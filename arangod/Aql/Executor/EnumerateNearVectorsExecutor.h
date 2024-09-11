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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/QueryContext.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <utils/empty.hpp>

namespace arangodb {
namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
template<BlockPassthrough>
class SingleRowFetcher;

struct EnumerateNearVectorsExecutorInfos {
  EnumerateNearVectorsExecutorInfos(
      RegisterId inNmDocId, RegisterId outDocRegId, RegisterId outDistanceRegId,
      transaction::Methods::IndexHandle index, QueryContext& queryContext,
      aql::Collection const* collection, std::size_t topK)
      : inputReg(inNmDocId),
        outDocumentIdReg(outDocRegId),
        outDistancesReg(outDistanceRegId),
        index(index),
        queryContext(queryContext),
        collection(collection),
        topK(topK) {}

  EnumerateNearVectorsExecutorInfos() = delete;
  EnumerateNearVectorsExecutorInfos(EnumerateNearVectorsExecutorInfos&&) =
      default;
  EnumerateNearVectorsExecutorInfos(EnumerateNearVectorsExecutorInfos const&) =
      delete;
  ~EnumerateNearVectorsExecutorInfos() = default;

  /// @brief register to store local document id
  RegisterId const inputReg;
  /// @brief register to store document ids
  RegisterId const outDocumentIdReg;
  /// @brief register to store distance
  RegisterId const outDistancesReg;

  transaction::Methods::IndexHandle index;
  QueryContext& queryContext;
  aql::Collection const* collection;
  std::size_t topK;
};

class EnumerateNearVectorsExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };

  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = EnumerateNearVectorsExecutorInfos;
  using Stats = NoStats;

  EnumerateNearVectorsExecutor(Fetcher&, Infos&);
  EnumerateNearVectorsExecutor(EnumerateNearVectorsExecutor const&) = delete;
  EnumerateNearVectorsExecutor(EnumerateNearVectorsExecutor&&) = default;
  ~EnumerateNearVectorsExecutor() = default;

  void initializeCursor() {
    /* do nothing here, just prevent the executor from being recreated */
  }

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& inputRange,
                                 OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& inputRange,
                                   AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

 private:
  void fillInput(AqlItemBlockInputRange& inputRange,
                 std::vector<float>& inputRowsJoined);

  void searchResults(std::vector<float>& inputRowsJoined);

  void fillOutput(OutputAqlItemRow& output);

  std::vector<InputAqlItemRow> _inputRows;

  std::vector<float> _distances;
  std::vector<LocalDocumentId::BaseType> _labels;
  bool _initialized{false};
  std::size_t _currentProcessedResultCount{0};
  Infos const& _infos;
  transaction::Methods _trx;
  aql::Collection const* _collection;
};
}  // namespace aql
}  // namespace arangodb
