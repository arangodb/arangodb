////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_MATERIALIZE_EXECUTOR_H
#define ARANGOD_AQL_MATERIALIZE_EXECUTOR_H

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionState.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/types.h"
#include "Indexes/IndexIterator.h"
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
template <BlockPassthrough>
class SingleRowFetcher;
class NoStats;

template <typename T>
class MaterializerExecutorInfos {
 public:
  MaterializerExecutorInfos(T collectionSource, RegisterId inNmDocId,
                            RegisterId outDocRegId, aql::QueryContext& query);

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

  T collectionSource() const { return _collectionSource; }

 private:
  /// @brief register to store raw collection pointer or collection name
  T const _collectionSource;
  /// @brief register to store local document id
  RegisterId const _inNonMaterializedDocRegId;
  /// @brief register to store materialized document
  RegisterId const _outMaterializedDocumentRegId;

  aql::QueryContext& _query;
};

template <typename T>
class MaterializeExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = MaterializerExecutorInfos<T>;
  using Stats = NoStats;

  MaterializeExecutor(MaterializeExecutor&&) = default;
  MaterializeExecutor(MaterializeExecutor const&) = delete;
  MaterializeExecutor(Fetcher&, Infos& infos);

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

 protected:
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
    static arangodb::IndexIterator::DocumentCallback copyDocumentCallback(ReadContext& ctx);
  };
  
  transaction::Methods _trx;
  ReadContext _readDocumentContext;
  Infos const& _infos;

  // for single collection case
  LogicalCollection const* _collection = nullptr;
};

}  // namespace aql
}  // namespace arangodb

#endif
