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
/// @author Jure Bajic
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/QueryContext.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <utility>
#include <utils/empty.hpp>

namespace arangodb::aql {

struct AqlCall;
class AqlItemBlockInputRange;
template<BlockPassthrough>
class SingleRowFetcher;

struct EnumerateNearVectorsExecutorInfos {
  EnumerateNearVectorsExecutorInfos(
      RegisterId inNmDocId, RegisterId outDocRegId, RegisterId outDistanceRegId,
      transaction::Methods::IndexHandle index, QueryContext& queryContext,
      aql::Collection const* collection, std::size_t topK, std::size_t offset,
      SearchParameters searchParameters)
      : inputReg(inNmDocId),
        outDocumentIdReg(outDocRegId),
        outDistancesReg(outDistanceRegId),
        index(std::move(index)),
        queryContext(queryContext),
        collection(collection),
        topK(topK),
        offset(offset),
        searchParameters(searchParameters) {}

  EnumerateNearVectorsExecutorInfos() = delete;
  EnumerateNearVectorsExecutorInfos(EnumerateNearVectorsExecutorInfos&&) =
      default;
  EnumerateNearVectorsExecutorInfos(EnumerateNearVectorsExecutorInfos const&) =
      delete;
  ~EnumerateNearVectorsExecutorInfos() = default;

  // total number of result per one query point
  std::size_t getNumberOfResults() const noexcept { return topK + offset; }

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
  std::size_t offset;
  SearchParameters searchParameters;
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
  // TODO(jbajic) add stats
  using Stats = NoStats;

  EnumerateNearVectorsExecutor(Fetcher&, Infos&);
  EnumerateNearVectorsExecutor(EnumerateNearVectorsExecutor const&) = delete;
  EnumerateNearVectorsExecutor(EnumerateNearVectorsExecutor&&) = default;
  ~EnumerateNearVectorsExecutor() = default;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output);

  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

 private:
  void fillInput(AqlItemBlockInputRange& inputRange);

  void searchResults();

  void fillOutput(OutputAqlItemRow& output);

  bool hasResults() const noexcept;

  uint64_t skipOutput(AqlCall::Limit toSkip) noexcept;

  Infos const& _infos;
  transaction::Methods _trx;
  aql::Collection const* _collection;

  InputAqlItemRow _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  std::vector<float> _inputRowConverted;
  ExecutorState _state{ExecutorState::HASMORE};

  std::vector<float> _distances;
  std::vector<VectorIndexLabelId> _labels;
  std::size_t _currentProcessedResultCount{0};
  // needed to enable fullCount to work
  std::size_t _processedInputs{0};
  std::size_t _collectionCount{
      _collection->count(&_trx, transaction::CountType::kNormal)};
};
}  // namespace arangodb::aql
