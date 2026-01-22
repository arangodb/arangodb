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
#include "RocksDBValue.h"
#include "RocksDBEngine/RocksDBIndex.h"

#include <faiss/IndexIVFFlat.h>
#include <faiss/MetricType.h>
#include <faiss/invlists/InvertedLists.h>
#include <velocypack/SharedSlice.h>
#include <velocypack/Slice.h>
#include <velocypack/SliceContainer.h>

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

////////////////////////////////////////////////////////////////////////////////
/// @brief Concept defining requirements for a vector index stored values
/// strategy
////////////////////////////////////////////////////////////////////////////////
// TODO(jbajic) could be used during insertion with a bit refactoring
template<typename T>
concept VectorIndexStoredValuesStrategy = requires {
  // Must have a compile-time constant indicating if stored values are
  // present
  { T::hasStoredValues } -> std::convertible_to<bool>;
}
&&requires(rocksdb::Slice const& key, rocksdb::Slice const& value,
           size_t codeSize, std::vector<uint8_t> const& encodedValue,
           velocypack::Slice storedValues) {
  // Extract encoded vector from raw bytes
  // Returns document ID and the encoded vector
  // Caller manages the vector lifetime and can extract pointer as needed
  {T::extractVectorIndexEntry(key, value, codeSize)};

  // Extract encoded vector from raw bytes
  // Returns the encoded vector
  // Caller manages the vector lifetime and can extract pointer as needed
  {T::extractVectorIndexValue(value, codeSize)};
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Strategy for vector indexes WITHOUT stored values
///
/// When stored values are not present, the RocksDB value contains only the
/// encoded vector data (raw bytes from FAISS).
////////////////////////////////////////////////////////////////////////////////
struct NoStoredValuesStrategy {
  static constexpr bool hasStoredValues = false;

  static std::pair<LocalDocumentId, std::vector<uint8_t>>
  extractVectorIndexEntry(rocksdb::Slice const& key,
                          rocksdb::Slice const& value, size_t codeSize) {
    auto const docId = RocksDBKey::indexDocumentId(key);
    std::vector<uint8_t> encodedValue(
        reinterpret_cast<uint8_t const*>(value.data()),
        reinterpret_cast<uint8_t const*>(value.data()) + codeSize);
    return {docId, std::move(encodedValue)};
  }

  static std::vector<uint8_t> extractVectorIndexValue(
      rocksdb::Slice const& value, size_t codeSize) {
    std::vector<uint8_t> encodedValue(
        reinterpret_cast<uint8_t const*>(value.data()),
        reinterpret_cast<uint8_t const*>(value.data()) + codeSize);
    return encodedValue;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Strategy for vector indexes WITH stored values
///
/// When stored values are present, the RocksDB value contains a serialized
/// RocksDBVectorIndexEntryValue with both encoded vector data and stored
/// values.
////////////////////////////////////////////////////////////////////////////////
struct WithStoredValuesStrategy {
  static constexpr bool hasStoredValues = true;

  static std::pair<LocalDocumentId, RocksDBVectorIndexEntryValue>
  extractVectorIndexEntry(rocksdb::Slice const& key,
                          rocksdb::Slice const& value, size_t /*codeSize*/) {
    auto const docId = RocksDBKey::indexDocumentId(key);
    auto entry = RocksDBValue::vectorIndexEntryValue(value);
    return {docId, std::move(entry)};
  }

  static RocksDBVectorIndexEntryValue extractVectorIndexValue(
      rocksdb::Slice const& value, size_t codeSize) {
    auto entry = RocksDBValue::vectorIndexEntryValue(value);
    return entry;
  }
};

// This Iterator is used by faiss library to iterate through RocksDB,
// we set the appropriate iterator in RocksDBInvertedLists which instantiates
// a new iterator for every nList that it needs to iterate through (nProbe)
// It contains the logic for how to read key value pairs that we wrote
struct RocksDBInvertedListsIteratorBase : faiss::InvertedListsIterator {
  RocksDBInvertedListsIteratorBase(RocksDBVectorIndex* index,
                                   LogicalCollection* collection,
                                   transaction::Methods* trx,
                                   std::size_t listNumber,
                                   std::size_t codeSize);

  virtual ~RocksDBInvertedListsIteratorBase() = default;

  [[nodiscard]] virtual bool is_available() const override;

 protected:
  RocksDBKey _rocksdbKey;
  arangodb::RocksDBVectorIndex* _index{nullptr};
  LogicalCollection* _collection{nullptr};
  std::size_t _listNumber;
  std::size_t _codeSize;

  std::unique_ptr<rocksdb::Iterator> _it;
};

// Simple iterator without filtering
// Template parameter allows compile-time selection of stored values strategy
template<VectorIndexStoredValuesStrategy Strategy>
struct RocksDBInvertedListsIterator final : RocksDBInvertedListsIteratorBase {
  RocksDBInvertedListsIterator(RocksDBVectorIndex* index,
                               LogicalCollection* collection,
                               transaction::Methods* trx,
                               std::size_t listNumber, std::size_t codeSize);

  void next() override;

  std::pair<faiss::idx_t, uint8_t const*> get_id_and_codes() override;

 private:
  // Storage varies based on strategy
  std::conditional_t<Strategy::hasStoredValues, RocksDBVectorIndexEntryValue,
                     std::vector<uint8_t>>
      _currentEntry;
};

struct SearchParametersContext {
  transaction::Methods* trx;
  aql::Expression* filterExpression;
  std::optional<aql::InputAqlItemRow> inputRow;
  aql::QueryContext* queryContext;
  std::vector<std::pair<aql::VariableId, aql::RegisterId>> const*
      filterVarsToRegs;
  bool isCoveredByStoredValues;
  aql::Variable const* documentVariable;
};

// This is used to pass a different search context via RocksDBInvertedList it
// the iterators
using RocksDBFaissSearchContext =
    std::variant<SearchParametersContext, transaction::Methods*>;

/// Base iterator for filtering iterators
struct RocksDBInvertedListsFilteringIteratorBase
    : public RocksDBInvertedListsIteratorBase {
  RocksDBInvertedListsFilteringIteratorBase(
      RocksDBVectorIndex* index, LogicalCollection* collection,
      SearchParametersContext& searchParametersContext, std::size_t listNumber,
      std::size_t codeSize);

  [[nodiscard]] bool is_available() const override;

  std::pair<faiss::idx_t, uint8_t const*> get_id_and_codes() override;

  [[nodiscard]] virtual bool searchFilteredIds() = 0;

  void next() override;

 protected:
  void skipOverFilteredDocuments();

  // batch size to reduce random RocksDB accesses. Chosen arbitrarily.
  constexpr static auto kBatchSize{1000};

  SearchParametersContext& _searchParametersContext;
  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;

  std::vector<std::pair<LocalDocumentId, std::vector<uint8_t>>> _filteredIds;

  // Current element from the _filteredIds, which is the current state of this
  // iterator
  std::vector<std::pair<LocalDocumentId, std::vector<uint8_t>>>::iterator
      _filteredIdsIt{_filteredIds.end()};
};

// Materializes document for every record
// Template parameter allows compile-time selection of stored values strategy
template<VectorIndexStoredValuesStrategy Strategy>
struct RocksDBInvertedListsFilteringIterator final
    : public RocksDBInvertedListsFilteringIteratorBase {
  RocksDBInvertedListsFilteringIterator(
      RocksDBVectorIndex* index, LogicalCollection* collection,
      SearchParametersContext& searchParametersContext, std::size_t listNumber,
      std::size_t codeSize);

  [[nodiscard]] bool searchFilteredIds() override;
};

// This iterator is similar as RocksDBInvertedListsFilteringIterator
// except it does not needs to materialize documents, since it contains
// values that will be used during expression evaluation.
// It can be used iff storedValues fully cover the filterExpression
struct RocksDBInvertedListsFilteringStoredValuesIterator final
    : public RocksDBInvertedListsFilteringIteratorBase {
  RocksDBInvertedListsFilteringStoredValuesIterator(
      RocksDBVectorIndex* index, LogicalCollection* collection,
      SearchParametersContext& searchParametersContext, std::size_t listNumber,
      std::size_t codeSize);

  [[nodiscard]] bool searchFilteredIds() override;
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
