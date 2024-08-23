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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionState.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Projections.h"
#include "Aql/RegisterInfos.h"
#include "Aql/types.h"
#include "Aql/MultiGet.h"
#include "Aql/SingleRowFetcher.h"
#include "IResearch/SearchDoc.h"
#include "Transaction/Methods.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"

#include <utils/empty.hpp>

#include <memory>

namespace arangodb::aql {

struct AqlCall;
class AqlItemBlockInputRange;
class InputAqlItemRow;
class RegisterInfos;
struct Collection;

class MaterializerExecutorInfos {
 public:
  MaterializerExecutorInfos(
      RegisterId inNmDocId, RegisterId outDocRegId, aql::QueryContext& query,
      Collection const* collection, Projections projections,
      containers::FlatHashMap<VariableId, RegisterId> variablesToRegisters)
      : _inNonMaterializedDocRegId(inNmDocId),
        _outMaterializedDocumentRegId(outDocRegId),
        _query(query),
        _collection(collection),
        _projections(std::move(projections)),
        _variablesToRegisters(std::move(variablesToRegisters)) {}

  MaterializerExecutorInfos() = delete;
  MaterializerExecutorInfos(MaterializerExecutorInfos&&) = default;
  MaterializerExecutorInfos(MaterializerExecutorInfos const&) = delete;
  ~MaterializerExecutorInfos() = default;

  RegisterId outputMaterializedDocumentRegId() const noexcept {
    return _outMaterializedDocumentRegId;
  }

  RegisterId inputNonMaterializedDocRegId() const noexcept {
    return _inNonMaterializedDocRegId;
  }

  aql::QueryContext& query() const noexcept { return _query; }

  Collection const* collection() const noexcept { return _collection; }

  Projections const& projections() const noexcept { return _projections; }

  RegisterId getRegisterForVariable(VariableId var) const noexcept;

 private:
  /// @brief register to store local document id
  RegisterId const _inNonMaterializedDocRegId;
  /// @brief register to store materialized document
  RegisterId const _outMaterializedDocumentRegId;
  aql::QueryContext& _query;
  Collection const* _collection;
  Projections const _projections;
  containers::FlatHashMap<VariableId, RegisterId> const _variablesToRegisters;
};

struct MaterializeExecutorBase {
  using Infos = MaterializerExecutorInfos;
  using Stats = MaterializeStats;

  explicit MaterializeExecutorBase(Infos& infos);

  void initializeCursor() {
    /* do nothing here, just prevent the executor from being recreated */
  }

 protected:
  transaction::Methods _trx;
  Infos& _infos;
};

class MaterializeRocksDBExecutor : public MaterializeExecutorBase {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Enable;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Stats = NoStats;

  MaterializeRocksDBExecutor(Fetcher&, Infos& infos);

  std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output);

 private:
  PhysicalCollection* _collection{};
  std::vector<LocalDocumentId> _docIds;
  std::vector<InputAqlItemRow> _inputRows;
  velocypack::Builder _projectionsBuilder;
};

class MaterializeSearchExecutor : public MaterializeExecutorBase {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    // FIXME(gnusi): enable?
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = MaterializerExecutorInfos;
  using Stats = MaterializeStats;

  MaterializeSearchExecutor(Fetcher&, Infos& infos);

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
  static constexpr size_t kInvalidRecord = std::numeric_limits<size_t>::max();

  struct Buffer {
    explicit Buffer(ResourceMonitor& monitor) noexcept : scope{monitor} {}

    void fill(AqlItemBlockInputRange& inputRange, RegisterId searchDocRegId);

    struct Record {
      explicit Record(iresearch::SearchDoc doc) noexcept
          : segment{doc.segment()}, search{doc.doc()} {}

      iresearch::ViewSegment const* segment;

      union {
        irs::doc_id_t search;
        LocalDocumentId storage;
        size_t executor;
      };
    };

    ResourceUsageScope scope;
    std::vector<Record> docs;
    std::vector<Record*> order;
  };

  Buffer _buffer;
  MultiGetContext _getCtx;
};

}  // namespace arangodb::aql
