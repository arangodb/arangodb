////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Containers/FlatHashMap.h"
#include "Indexes/IndexIterator.h"
#include "IResearch/SearchDoc.h"
#include "Transaction/Methods.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"

#include <iosfwd>
#include <memory>

namespace arangodb {
namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
class InputAqlItemRow;
class RegisterInfos;
template<BlockPassthrough>
class SingleRowFetcher;

// two storage varians as we need
// collection name to be stored only
// when needed.
class NoCollectionNameHolder {};
class StringCollectionNameHolder {
 public:
  StringCollectionNameHolder(std::string const& name)
      : _collectionSource(name) {}

  auto collectionSource() const { return _collectionSource; }

 protected:
  std::string const& _collectionSource;
};

template<typename T>
using MaterializerExecutorInfosBase =
    std::conditional_t<std::is_same_v<T, std::string const&>,
                       StringCollectionNameHolder, NoCollectionNameHolder>;

template<typename T>
class MaterializerExecutorInfos : public MaterializerExecutorInfosBase<T> {
 public:

  template<class... _Types>
  MaterializerExecutorInfos(RegisterId inNmDocId, RegisterId outDocRegId,
                            aql::QueryContext& query, _Types&&... Args)
      : MaterializerExecutorInfosBase<T>(Args...),
        _inNonMaterializedDocRegId(inNmDocId),
        _outMaterializedDocumentRegId(outDocRegId),
        _query(query) {}

  MaterializerExecutorInfos() = delete;
  MaterializerExecutorInfos(MaterializerExecutorInfos&&) = default;
  MaterializerExecutorInfos(MaterializerExecutorInfos const&) = delete;
  ~MaterializerExecutorInfos() = default;

  RegisterId outputMaterializedDocumentRegId() const {
    return _outMaterializedDocumentRegId;
  }

  RegisterId inputNonMaterializedDocRegId() const {
    return _inNonMaterializedDocRegId;
  }

  aql::QueryContext& query() const { return _query; }

private:
  /// @brief register to store local document id
  RegisterId const _inNonMaterializedDocRegId;
  /// @brief register to store materialized document
  RegisterId const _outMaterializedDocumentRegId;

  aql::QueryContext& _query;
};

template<typename T>
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
    if constexpr (isSingleCollection) {
      _collection = nullptr;
    }
  }

 protected:
  static constexpr bool isSingleCollection =
      std::is_same_v<T, std::string const&>;

  class ReadContext {
   public:
    explicit ReadContext(Infos& infos)
        : _infos(&infos),
          _inputRow(nullptr),
          _outputRow(nullptr),
          _callback(copyDocumentCallback(*this)) {}

    ReadContext(ReadContext&&) = default;

    const Infos* _infos;
    const arangodb::aql::InputAqlItemRow* _inputRow;
    arangodb::aql::OutputAqlItemRow* _outputRow;
    arangodb::IndexIterator::DocumentCallback const _callback;

   private:
    static arangodb::IndexIterator::DocumentCallback copyDocumentCallback(
        ReadContext& ctx);
  };

  void fillBuffer(AqlItemBlockInputRange& inputRange);
  using BufferRecord = std::tuple<iresearch::SearchDoc, LocalDocumentId,
                                  LogicalCollection const*>;
  using BufferedRecordsContainer = std::vector<BufferRecord>;
  BufferedRecordsContainer _bufferedDocs;

  transaction::Methods _trx;
  ReadContext _readDocumentContext;
  Infos const& _infos;

  ResourceUsageScope _memoryTracker;
  std::conditional_t<
      isSingleCollection, LogicalCollection const*,
      containers::FlatHashMap<DataSourceId, LogicalCollection const*>>
      _collection;
};

}  // namespace aql
}  // namespace arangodb
