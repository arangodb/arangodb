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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionState.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/types.h"
#include "Aql/MultiGet.h"
#include "Indexes/IndexIterator.h"
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
template<BlockPassthrough>
class SingleRowFetcher;

template<typename T>
class CollectionNameHolder {
 public:
  explicit CollectionNameHolder(std::string_view name) noexcept
      : _collectionSource{name} {}

  auto collectionSource() const { return _collectionSource; }

 protected:
  std::string_view _collectionSource;
};

template<>
class CollectionNameHolder<void> {};

template<typename T>
class MaterializerExecutorInfos : public CollectionNameHolder<T> {
 public:
  template<class... _Types>
  MaterializerExecutorInfos(RegisterId inNmDocId, RegisterId outDocRegId,
                            aql::QueryContext& query, _Types&&... Args)
      : CollectionNameHolder<T>(Args...),
        _inNonMaterializedDocRegId(inNmDocId),
        _outMaterializedDocumentRegId(outDocRegId),
        _query(query) {}

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

 private:
  /// @brief register to store local document id
  RegisterId const _inNonMaterializedDocRegId;
  /// @brief register to store materialized document
  RegisterId const _outMaterializedDocumentRegId;
  aql::QueryContext& _query;
};

template<typename T, bool localDocumentId>
class MaterializeExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    // FIXME(gnusi): enable?
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
    // TODO this could be set to true!
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = MaterializerExecutorInfos<T>;
  using Stats = MaterializeStats;

  MaterializeExecutor(MaterializeExecutor&&) = default;
  MaterializeExecutor(MaterializeExecutor const&) = delete;
  MaterializeExecutor(Fetcher&, Infos& infos);

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

  void initializeCursor() noexcept {
    // Does nothing but prevents the whole executor to be re-created.
    _collection = {};
  }

 private:
  static constexpr bool isSingleCollection = !std::is_void_v<T>;

  class ReadContext {
   public:
    explicit ReadContext(Infos& infos);

    ReadContext(ReadContext&& other) = default;

    void moveInto(std::unique_ptr<uint8_t[]> data);

    Infos const* infos;
    InputAqlItemRow const* inputRow{};
    OutputAqlItemRow* outputRow{};
    IndexIterator::DocumentCallback callback;
  };

  static constexpr size_t kInvalidRecord = std::numeric_limits<size_t>::max();

  struct Buffer {
    explicit Buffer(ResourceMonitor& monitor) noexcept : scope{monitor} {}

    void fill(AqlItemBlockInputRange& inputRange, ReadContext& ctx);

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

  IRS_NO_UNIQUE_ADDRESS
  irs::utils::Need<!localDocumentId, Buffer> _buffer;
  IRS_NO_UNIQUE_ADDRESS
  irs::utils::Need<!localDocumentId, MultiGetContext> _getCtx;

  transaction::Methods _trx;
  ReadContext _readCtx;
  IRS_NO_UNIQUE_ADDRESS
  irs::utils::Need<localDocumentId, PhysicalCollection*> _collection{};
};

}  // namespace arangodb::aql
