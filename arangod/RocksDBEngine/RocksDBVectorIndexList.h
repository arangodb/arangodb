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

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/Expression.h"
#include "Aql/InputAqlItemRow.h"
#include "Indexes/VectorIndexDefinition.h"
#include "RocksDBIndex.h"
#include "RocksDBEngine/RocksDBIndex.h"

#include <faiss/IndexIVFFlat.h>
#include <faiss/MetricType.h>
#include <faiss/invlists/InvertedLists.h>

namespace arangodb {

class RocksDBVectorIndex;
class LogicalCollection;

namespace vector {

inline faiss::MetricType metricToFaissMetric(
    SimilarityMetric const metric) noexcept {
  switch (metric) {
    case SimilarityMetric::kL2:
      return faiss::MetricType::METRIC_L2;
    case SimilarityMetric::kCosine:
      return faiss::MetricType::METRIC_INNER_PRODUCT;
    case SimilarityMetric::kInnerProduct:
      return faiss::METRIC_INNER_PRODUCT;
  }
}

struct SearchParametersContext {
  transaction::Methods* trx;
  aql::Expression* filterExpression;
  std::optional<aql::InputAqlItemRow> inputRow;
  aql::QueryContext* queryContext;
  std::vector<std::pair<aql::VariableId, aql::RegisterId>> const*
      filterVarsToRegs;
  aql::Variable const* documentVariable;
};

// This Iterator is used by faiss library to iterate through RocksDB,
// we set the appropriate iterator in RocksDBInvertedLists which instantiates
// a new iterator for every nList that it needs to iterate through.
// It contains the logic for how to read key value pairs that we wrote
// It can also filter out certain pairs if the filterExpression has been
// set
struct RocksDBInvertedListsIterator : faiss::InvertedListsIterator {
  RocksDBInvertedListsIterator(RocksDBVectorIndex* index,
                               LogicalCollection* collection,
                               SearchParametersContext& searchParametersContext,
                               std::size_t listNumber, std::size_t codeSize);
  [[nodiscard]] bool is_available() const override;

  // This should be only called when we have filterExpression
  void skipOverFilteredDocuments();

  void next() override;

  std::pair<faiss::idx_t, uint8_t const*> get_id_and_codes() override;

 private:
  RocksDBKey _rocksdbKey;
  arangodb::RocksDBVectorIndex* _index{nullptr};
  LogicalCollection* _collection{nullptr};
  SearchParametersContext& _searchParametersContext;
  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;

  std::unique_ptr<rocksdb::Iterator> _it;
  std::size_t _listNumber;
  std::size_t _codeSize;
};

struct RocksDBInvertedLists : faiss::InvertedLists {
  RocksDBInvertedLists(RocksDBVectorIndex* index, LogicalCollection* collection,
                       std::size_t nlist, size_t codeSize);

  std::size_t list_size(std::size_t /*listNumber*/) const override {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "faiss list_size not supported");
  }

  std::uint8_t const* get_codes(std::size_t /*listNumber*/) const override {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "faiss get_codes not supported");
  }

  faiss::idx_t const* get_ids(std::size_t /*listNumber*/) const override {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                   "faiss get_ids not supported");
  }

  size_t add_entries(std::size_t listNumber, std::size_t nEntry,
                     faiss::idx_t const* ids,
                     std::uint8_t const* code) override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  void update_entries(std::size_t /*listNumber*/, std::size_t /*offset*/,
                      std::size_t /*n_entry*/, const faiss::idx_t* /*ids*/,
                      const std::uint8_t* /*code*/) override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  void resize(std::size_t /*listNumber*/, std::size_t /*new_size*/) override {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  void remove_id(size_t list_no, faiss::idx_t id) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  faiss::InvertedListsIterator* get_iterator(std::size_t listNumber,
                                             void* context) const override;

 private:
  RocksDBVectorIndex* _index;
  LogicalCollection* _collection;
};
};  // namespace vector
};  // namespace arangodb
