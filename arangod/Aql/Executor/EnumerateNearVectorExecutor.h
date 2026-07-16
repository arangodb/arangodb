////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/Expression.h"
#include "Aql/Projections.h"
#include "Aql/QueryContext.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Containers/FlatHashMap.h"
#include "Containers/NodeHashMap.h"
#include "VectorIndex/VectorReadBatch.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/SharedSlice.h>

namespace arangodb {
class RocksDBVectorIndex;
}

namespace arangodb::vector_graph {
class VectorGraphIndex;
}

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
  vector::VectorSearchConfig searchConfig;

  // output strategy
  Projections projections;
  vector::ProjectionMode projectionMode;
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

  // Reads the query point from the current input row into _inputRowConverted,
  // validating that it is an array of the index's dimension.
  void convertQueryVector(std::size_t dimension);

  void searchResults();

  // Search path for the graph-based vector index: runs GreedySearch over the
  // index segments and fills the label/distance/document buffers.
  // TODO(jbajic) just temp build new executor
  void searchGraphIndex();

  void fillOutput(OutputAqlItemRow& output);

  void writeProjectionsFromDocument(velocypack::Slice docSlice,
                                    OutputAqlItemRow& output);

  void writeProjectionsFromStoredValues(velocypack::Slice storedValuesSlice,
                                        OutputAqlItemRow& output);

  bool hasResults() const noexcept;

  uint64_t skipOutput(AqlCall::Limit toSkip) noexcept;

  // Resolve the concrete index from the index handle (unwrapping a
  // RocksDBBuilderIndex during construction). Exactly one of the two returns
  // non-nullptr, depending on the index type.
  static RocksDBVectorIndex const* resolveVectorIndex(Infos const& infos);
  static vector_graph::VectorGraphIndex const* resolveGraphIndex(
      Infos const& infos);

  Infos const& _infos;
  transaction::Methods _trx;
  aql::Collection const* _collection;
  RocksDBVectorIndex const* _vectorIndex;
  vector_graph::VectorGraphIndex const* _graphIndex;

  InputAqlItemRow _inputRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
  std::vector<float> _inputRowConverted;
  ExecutorState _state{ExecutorState::HASMORE};

  std::vector<float> _distances;
  std::vector<vector::VectorIndexLabelId> _labels;
  // VPack per surviving label: full doc Object for kDocument, storedValues
  // array for kCovered, empty for kPassThroughId.
  containers::NodeHashMap<LocalDocumentId, velocypack::SharedSlice> _documents;
  velocypack::Builder _projectionsBuilder;
  std::size_t _currentProcessedResultCount{0};
  // needed to enable fullCount to work
  std::size_t _processedInputs{0};
  bool _reportedCurrentRowForFullCount{false};
  std::size_t _collectionCount{
      _collection->count(&_trx, transaction::CountType::kNormal)};
};
}  // namespace arangodb::aql
