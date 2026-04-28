////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/ExecutionNode/EnumerateNearVectorNode.h"
#include "Aql/Expression.h"
#include "Aql/Projections.h"
#include "Aql/QueryContext.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Containers/FlatHashMap.h"
#include "Containers/NodeHashMap.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <utility>
#include <vector>

#include <velocypack/Builder.h>

namespace arangodb::aql {

struct AqlCall;
class AqlItemBlockInputRange;
template<BlockPassthrough>
class SingleRowFetcher;

struct EnumerateNearVectorsExecutorInfos {
  // register layout
  RegisterId inputReg;
  RegisterId outDocumentIdReg;  // doc-id label, full doc, or unused (kCovered)
  RegisterId outDistancesReg;
  containers::FlatHashMap<VariableId, RegisterId> projectionVarsToRegs;

  // executor-bound state (held alive by the engine)
  transaction::Methods::IndexHandle index;
  QueryContext& queryContext;
  aql::Collection const* collection;

  // search configuration -- passed by reference straight to readBatch
  vector::SearchConfig searchConfig;

  // output strategy
  Projections const& projections;
  EnumerateNearVectorNode::Strategy strategy;
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

  // Extract per-projection slices from `docSlice` and write them into the
  // matching output registers. Shared by the kDocument and kCovered
  // strategies, which only differ in where `docSlice` came from.
  void writeProjections(velocypack::Slice docSlice, OutputAqlItemRow& output);

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
  // Per-doc VPack bytes the executor reads in fillOutput, keyed by the
  // surviving label. For kDocument the value is a full document; for
  // kCovered it is the partial doc the FAISS iterator built from
  // storedValues. Empty for kPassThroughId.
  containers::NodeHashMap<LocalDocumentId, velocypack::Buffer<uint8_t>>
      _documents;
  // Scratch builder used when producing per-projection registers from the
  // document slice.
  velocypack::Builder _projectionsBuilder;
  std::size_t _currentProcessedResultCount{0};
  // needed to enable fullCount to work
  std::size_t _processedInputs{0};
  bool _reportedCurrentRowForFullCount{false};
  std::size_t _collectionCount{
      _collection->count(&_trx, transaction::CountType::kNormal)};
};
}  // namespace arangodb::aql
