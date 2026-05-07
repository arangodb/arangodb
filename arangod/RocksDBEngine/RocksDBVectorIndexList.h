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
#include "Containers/NodeHashMap.h"
#include "VectorIndex/VectorIndexDefinition.h"
#include "RocksDBIndex.h"
#include "RocksDBValue.h"
#include "RocksDBEngine/RocksDBIndex.h"

#include <faiss/IndexIVFFlat.h>
#include <faiss/MetricType.h>
#include <faiss/invlists/InvertedLists.h>
#include <velocypack/Buffer.h>
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

// =====================================================================
// Iterator selection table
// =====================================================================
//
// The vector index search path picks one of three iterators based
// on (a) whether a filter is pushed down, (b) whether the index storedValues
// cover the filter expression (scF), and (c) whether they cover the
// projection list (scP). Iterators:
//
//   IVT   = RocksDBInvertedListsIterator                (no filter)
//   IVFT  = RocksDBInvertedListsFilteringIterator       (filter; loads doc)
//   IVFST = RocksDBInvertedListsFilteringStoredValuesIterator
//                                                       (filter; uses
//                                                        storedValues only)
//
//   "+ on_heap" = capture per-survivor data via on_heap_changed so the
//                 executor can serve projections without re-reading RocksDB.
//                 The captured shape depends on the row.
//
// Proj | Filter | scF | scP | Iterator         | Capture
// ---- | ------ | --- | --- | ---------------- | --------------------
//   F  |   F    |  -  |  -  | IVT              | none
//   T  |   F    |  -  |  F  | IVT              | none -- batch-fetch
//                                                docs post-search
//   T  |   F    |  -  |  T  | IVT + on_heap    | storedValues array
//   F  |   T    |  F  |  -  | IVFT             | none
//   F  |   T    |  T  |  -  | IVFST            | none
//   T  |   T    |  F  |  F  | IVFT + on_heap   | full document
//   T  |   T    |  T  |  F  | IVFST            | none -- downstream
//                                                MaterializeNode handles
//                                                projections (scP=F means
//                                                we don't have the data
//                                                here anyway)
//   T  |   T    |  F  |  T  | IVFT + on_heap   | storedValues array
//                                                (filter loaded the doc but
//                                                projections cover, so we
//                                                only stash the array)
//   T  |   T    |  T  |  T  | IVFST + on_heap  | storedValues array
//
// Threading note: this code assumes single-threaded execution within one
// search call. FAISS's default parallel_mode (0) parallelises over query
// vectors, and we always pass n=1, so all iterators for one search run on
// the same thread. capturedDocuments is therefore unsynchronised. If
// parallel_mode is ever changed to 1 or 2, or n>1 with a shared sink, this
// needs revisiting.
// =====================================================================

// Common per-search context shared by every iterator path.
//
// `capturedDocuments` is an optional output sink: when non-null, the iterator
// stashes a SharedSlice per surviving entry so the executor can serve
// projections without re-reading RocksDB. Shape depends on which iterator
// runs -- see the selection table above.
struct IteratorContext {
  transaction::Methods* trx;
  containers::NodeHashMap<LocalDocumentId, velocypack::SharedSlice>*
      capturedDocuments{nullptr};
};

// What the filtering iterator should put into capturedDocuments. Only
// meaningful when capturedDocuments is non-null. IVFT supports both
// shapes; IVFST only supports kStoredValues.
enum class CaptureShape : std::uint8_t {
  kStoredValues,  // raw storedValues array from the index entry
  kFullDocument,  // full document Object loaded for filter eval (IVFT only)
};

// Adds the filter-evaluation state used by the filtering iterator paths.
struct IteratorFilterContext : IteratorContext {
  aql::Expression* filterExpression;
  std::optional<aql::InputAqlItemRow> inputRow;
  aql::QueryContext* queryContext;
  std::vector<std::pair<aql::VariableId, aql::RegisterId>> const*
      filterVarsToRegs;
  // True iff the storedValues-only iterator should be used: the filter is
  // expressible against storedValues AND projections are too, so the FAISS
  // layer can skip loading documents entirely.
  bool useStoredValuesIterator;
  aql::Variable const* documentVariable;
  CaptureShape captureShape{CaptureShape::kStoredValues};
};

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
  // No stored values to view; out of scope for the zero-copy path.
  static constexpr bool kSupportsView = false;

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
/// @brief Strategy for vector indexes WITH stored values, v1 layout
///
/// V1 stores the entry as a VPack-serialized RocksDBVectorIndexEntryValue
/// (self-describing). codeSize is unused because the format carries its own
/// boundaries.
////////////////////////////////////////////////////////////////////////////////
struct WithStoredValuesV1Strategy {
  static constexpr bool hasStoredValues = true;
  // V1 must allocate to deserialize VPack; no zero-copy view path.
  static constexpr bool kSupportsView = false;

  static std::pair<LocalDocumentId, RocksDBVectorIndexEntryValue>
  extractVectorIndexEntry(rocksdb::Slice const& key,
                          rocksdb::Slice const& value, size_t /*codeSize*/) {
    auto const docId = RocksDBKey::indexDocumentId(key);
    return {docId, RocksDBValue::vectorIndexEntryValueV1(value)};
  }

  static RocksDBVectorIndexEntryValue extractVectorIndexValue(
      rocksdb::Slice const& value, size_t /*codeSize*/) {
    return RocksDBValue::vectorIndexEntryValueV1(value);
  }
};

/// @brief Non-owning view into a v2 vector-index entry. Backed directly by
/// the rocksdb iterator's value() bytes; valid only until the iterator
/// advances. Promoted to an owned SharedSlice at capture time
/// (on_heap_changed) for top-K survivors only. The encoded bytes are
/// always exactly the index's codeSize, so size isn't carried here.
struct RocksDBVectorIndexEntryViewV2 {
  uint8_t const* encoded{nullptr};
  velocypack::Slice storedValues;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Strategy for vector indexes WITH stored values, v2 layout
///
/// V2 stores the entry as raw concat: [encodedValue codeSize bytes]
/// [storedValues VPack slice]. codeSize is required to locate the boundary.
////////////////////////////////////////////////////////////////////////////////
struct WithStoredValuesV2Strategy {
  static constexpr bool hasStoredValues = true;
  // V2's raw layout lets the search-path iterator parse without allocating.
  static constexpr bool kSupportsView = true;

  static std::pair<LocalDocumentId, RocksDBVectorIndexEntryValue>
  extractVectorIndexEntry(rocksdb::Slice const& key,
                          rocksdb::Slice const& value, size_t codeSize) {
    auto const docId = RocksDBKey::indexDocumentId(key);
    return {docId, RocksDBValue::vectorIndexEntryValueV2(value, codeSize)};
  }

  static RocksDBVectorIndexEntryValue extractVectorIndexValue(
      rocksdb::Slice const& value, size_t codeSize) {
    return RocksDBValue::vectorIndexEntryValueV2(value, codeSize);
  }

  // Returns a non-owning view into the rocksdb iterator's value buffer.
  // Lifetime is bounded by the rocksdb iterator's current position.
  static RocksDBVectorIndexEntryViewV2 extractView(rocksdb::Slice const& value,
                                                   size_t codeSize) {
    auto const* data = reinterpret_cast<uint8_t const*>(value.data());
    return {.encoded = data,
            .storedValues = velocypack::Slice(data + codeSize)};
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

  void on_heap_changed(faiss::idx_t new_id, faiss::idx_t evicted_id) final;

 protected:
  // Insert hook for the topK heap; default takes ownership of
  // _currentCaptureData. V2 overrides to promote a non-owning view.
  virtual void captureSurvivor(LocalDocumentId id);

  RocksDBKey _rocksdbKey;
  arangodb::RocksDBVectorIndex* _index{nullptr};
  LogicalCollection* _collection{nullptr};
  std::size_t _listNumber;
  std::size_t _codeSize;

  std::unique_ptr<rocksdb::Iterator> _it;

  // When capture is wanted, derived classes wire _sink in their ctor and
  // refresh _currentCaptureData from get_id_and_codes; on_heap_changed
  // pushes it into the sink when the entry survives the topK heap.
  containers::NodeHashMap<LocalDocumentId, velocypack::SharedSlice>* _sink{
      nullptr};
  velocypack::SharedSlice _currentCaptureData;
};

// Simple iterator without filtering
// Template parameter allows compile-time selection of stored values strategy
template<VectorIndexStoredValuesStrategy Strategy>
struct RocksDBInvertedListsIterator final : RocksDBInvertedListsIteratorBase {
  RocksDBInvertedListsIterator(RocksDBVectorIndex* index,
                               LogicalCollection* collection,
                               IteratorContext const& ctx,
                               std::size_t listNumber, std::size_t codeSize);

  void next() override;

  std::pair<faiss::idx_t, uint8_t const*> get_id_and_codes() override;

  void captureSurvivor(LocalDocumentId id) override;

 private:
  std::conditional_t<
      Strategy::kSupportsView, RocksDBVectorIndexEntryViewV2,
      std::conditional_t<Strategy::hasStoredValues,
                         RocksDBVectorIndexEntryValue, std::vector<uint8_t>>>
      _currentEntry;
};

using RocksDBFaissIteratorContext =
    std::variant<IteratorFilterContext, IteratorContext>;

/// Base iterator for filtering iterators
struct RocksDBInvertedListsFilteringIteratorBase
    : public RocksDBInvertedListsIteratorBase {
  RocksDBInvertedListsFilteringIteratorBase(
      RocksDBVectorIndex* index, LogicalCollection* collection,
      IteratorFilterContext& filterContext, std::size_t listNumber,
      std::size_t codeSize);

  [[nodiscard]] bool is_available() const override;

  std::pair<faiss::idx_t, uint8_t const*> get_id_and_codes() override;

  [[nodiscard]] virtual bool searchFilteredIds() = 0;

  void next() override;

 protected:
  void skipOverFilteredDocuments();

  // Batch size to reduce random RocksDB accesses.
  constexpr static auto kBatchSize{1000};

  // captureData is empty when no sink is wired; otherwise it is moved into
  // the sink by on_heap_changed if the entry survives the topK heap.
  struct FilteredEntry {
    LocalDocumentId id;
    std::vector<uint8_t> codes;
    velocypack::SharedSlice captureData;
  };

  IteratorFilterContext& _filterContext;
  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;

  std::vector<FilteredEntry> _filteredIds;
  std::vector<FilteredEntry>::iterator _filteredIdsIt{_filteredIds.end()};
};

// Materializes document for every record
// Template parameter allows compile-time selection of stored values strategy
template<VectorIndexStoredValuesStrategy Strategy>
struct RocksDBInvertedListsFilteringIterator final
    : public RocksDBInvertedListsFilteringIteratorBase {
  RocksDBInvertedListsFilteringIterator(RocksDBVectorIndex* index,
                                        LogicalCollection* collection,
                                        IteratorFilterContext& filterContext,
                                        std::size_t listNumber,
                                        std::size_t codeSize);

  [[nodiscard]] bool searchFilteredIds() override;
};

// This iterator is similar as RocksDBInvertedListsFilteringIterator
// except it does not needs to materialize documents, since it contains
// values that will be used during expression evaluation.
// It can be used iff storedValues fully cover the filterExpression
template<VectorIndexStoredValuesStrategy Strategy>
requires(Strategy::hasStoredValues) struct
    RocksDBInvertedListsFilteringStoredValuesIterator final
    : public RocksDBInvertedListsFilteringIteratorBase {
  RocksDBInvertedListsFilteringStoredValuesIterator(
      RocksDBVectorIndex* index, LogicalCollection* collection,
      IteratorFilterContext& filterContext, std::size_t listNumber,
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
