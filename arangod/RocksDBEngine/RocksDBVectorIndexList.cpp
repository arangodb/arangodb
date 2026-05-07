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

#include "RocksDBVectorIndexList.h"

#include "Aql/DocumentExpressionContext.h"
#include "Aql/LateMaterializedExpressionContext.h"
#include "Indexes/IndexIterator.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

#include <faiss/MetricType.h>
#include <faiss/invlists/InvertedLists.h>
#include <rocksdb/slice.h>
#include <velocypack/Builder.h>

namespace arangodb::vector {

namespace {
// Copy a non-owning slice's bytes into a fresh owning SharedSlice.
velocypack::SharedSlice toOwnedSharedSlice(velocypack::Slice slice) {
  velocypack::Buffer<uint8_t> buf;
  buf.append(slice.start(), slice.byteSize());
  return velocypack::SharedSlice{std::move(buf)};
}
}  // namespace

/// RocksDBInvertedListsIteratorBase
RocksDBInvertedListsIteratorBase::RocksDBInvertedListsIteratorBase(
    RocksDBVectorIndex* index, LogicalCollection* collection,
    transaction::Methods* trx, std::size_t listNumber, std::size_t codeSize)
    : InvertedListsIterator(),
      _index(index),
      _collection(collection),
      _listNumber(listNumber),
      _codeSize(codeSize) {
  RocksDBTransactionMethods* mthds =
      RocksDBTransactionState::toMethods(trx, collection->id());

  _it = mthds->NewIterator(index->columnFamily(), [&](auto& opts) {
    TRI_ASSERT(opts.prefix_same_as_start);
  });

  _rocksdbKey.constructVectorIndexValue(_index->objectId(), _listNumber);
  _it->Seek(_rocksdbKey.string());
}

[[nodiscard]] bool RocksDBInvertedListsIteratorBase::is_available() const {
  return _it->Valid() && _it->key().starts_with(_rocksdbKey.string());
}

void RocksDBInvertedListsIteratorBase::on_heap_changed(faiss::idx_t newId,
                                                       faiss::idx_t evictedId) {
  TRI_ASSERT(_sink != nullptr);

  // FAISS pre-fills the heap with sentinel ids (-1); skip those evictions.
  if (evictedId >= 0) {
    _sink->erase(LocalDocumentId{static_cast<uint64_t>(evictedId)});
  }

  captureSurvivor(LocalDocumentId{static_cast<uint64_t>(newId)});
}

void RocksDBInvertedListsIteratorBase::captureSurvivor(LocalDocumentId id) {
  if (!_currentCaptureData.isNone()) {
    _sink->insert_or_assign(id, std::move(_currentCaptureData));
  }
}

/// RocksDBInvertedListsIterator
template<VectorIndexStoredValuesStrategy Strategy>
RocksDBInvertedListsIterator<Strategy>::RocksDBInvertedListsIterator(
    RocksDBVectorIndex* index, LogicalCollection* collection,
    IteratorContext const& ctx, std::size_t listNumber, std::size_t codeSize)
    : RocksDBInvertedListsIteratorBase(index, collection, ctx.trx, listNumber,
                                       codeSize) {
  if constexpr (Strategy::hasStoredValues) {
    _sink = ctx.capturedDocuments;
    has_search_callbacks_ = (_sink != nullptr);
  }
}

template<VectorIndexStoredValuesStrategy Strategy>
void RocksDBInvertedListsIterator<Strategy>::next() {
  _it->Next();
}

template<VectorIndexStoredValuesStrategy Strategy>
std::pair<faiss::idx_t, uint8_t const*>
RocksDBInvertedListsIterator<Strategy>::get_id_and_codes() {
  if constexpr (Strategy::kSupportsView) {
    // Zero-allocation: _currentEntry points into the iterator's value buffer.
    auto const docId = LocalDocumentId(RocksDBKey::indexDocumentId(_it->key()));
    _currentEntry = Strategy::extractView(_it->value(), _codeSize);
    return {static_cast<faiss::idx_t>(docId.id()), _currentEntry.encoded};
  } else {
    auto [docId, entry] =
        Strategy::extractVectorIndexEntry(_it->key(), _it->value(), _codeSize);
    _currentEntry = std::move(entry);

    if constexpr (Strategy::hasStoredValues) {
      TRI_ASSERT(_currentEntry.encodedValue.size() == _codeSize)
          << "The encoded size is: " << _currentEntry.encodedValue.size()
          << " should be: " << _codeSize;
      if (_sink != nullptr) {
        _currentCaptureData = _currentEntry.storedValues;
      }
      return {static_cast<faiss::idx_t>(docId.id()),
              _currentEntry.encodedValue.data()};
    } else {
      TRI_ASSERT(_currentEntry.size() == _codeSize)
          << "The encoded size is: " << _currentEntry.size()
          << " should be: " << _codeSize;
      return {static_cast<faiss::idx_t>(docId.id()), _currentEntry.data()};
    }
  }
}

template<VectorIndexStoredValuesStrategy Strategy>
void RocksDBInvertedListsIterator<Strategy>::captureSurvivor(
    LocalDocumentId id) {
  if constexpr (Strategy::kSupportsView) {
    // Promote the iterator-backed slice into an owned SharedSlice now that
    // we know this entry survived the top-K heap.
    auto const& view = _currentEntry.storedValues;
    if (!view.isNone()) {
      _sink->insert_or_assign(id, toOwnedSharedSlice(view));
    }
  } else {
    RocksDBInvertedListsIteratorBase::captureSurvivor(id);
  }
}

// Explicit instantiations
template struct RocksDBInvertedListsIterator<NoStoredValuesStrategy>;
template struct RocksDBInvertedListsIterator<WithStoredValuesV1Strategy>;
template struct RocksDBInvertedListsIterator<WithStoredValuesV2Strategy>;

/// RocksDBInvertedListsFilteringIteratorBase
RocksDBInvertedListsFilteringIteratorBase::
    RocksDBInvertedListsFilteringIteratorBase(
        RocksDBVectorIndex* index, LogicalCollection* collection,
        IteratorFilterContext& filterContext, std::size_t listNumber,
        std::size_t codeSize)
    : RocksDBInvertedListsIteratorBase(index, collection, filterContext.trx,
                                       listNumber, codeSize),
      _filterContext(filterContext) {
  TRI_ASSERT(filterContext.filterExpression != nullptr);
  _sink = _filterContext.capturedDocuments;
  has_search_callbacks_ = (_sink != nullptr);
}

[[nodiscard]] bool RocksDBInvertedListsFilteringIteratorBase::is_available()
    const {
  return _filteredIdsIt != _filteredIds.end() ||
         RocksDBInvertedListsIteratorBase::is_available();
}

std::pair<faiss::idx_t, uint8_t const*>
RocksDBInvertedListsFilteringIteratorBase::get_id_and_codes() {
  if (_sink != nullptr) {
    _currentCaptureData = std::move(_filteredIdsIt->captureData);
  }
  return {static_cast<faiss::idx_t>(_filteredIdsIt->id.id()),
          _filteredIdsIt->codes.data()};
}

void RocksDBInvertedListsFilteringIteratorBase::skipOverFilteredDocuments() {
  while (_filteredIdsIt == _filteredIds.end()) {
    if (!searchFilteredIds()) {
      // If we enter here we could not produce any documents
      return;
    }
  }
}

void RocksDBInvertedListsFilteringIteratorBase::next() {
  skipOverFilteredDocuments();
  ++_filteredIdsIt;
  if (_filteredIdsIt == _filteredIds.end()) {
    skipOverFilteredDocuments();
  }
}

/// RocksDBInvertedListsFilteringIterator
template<VectorIndexStoredValuesStrategy Strategy>
RocksDBInvertedListsFilteringIterator<Strategy>::
    RocksDBInvertedListsFilteringIterator(RocksDBVectorIndex* index,
                                          LogicalCollection* collection,
                                          IteratorFilterContext& filterContext,
                                          std::size_t listNumber,
                                          std::size_t codeSize)
    : RocksDBInvertedListsFilteringIteratorBase(
          index, collection, filterContext, listNumber, codeSize) {
  skipOverFilteredDocuments();
}

template<VectorIndexStoredValuesStrategy Strategy>
bool RocksDBInvertedListsFilteringIterator<Strategy>::searchFilteredIds() {
  // Get documents ids from the vector index
  std::vector<LocalDocumentId> ids;
  std::unordered_map<LocalDocumentId, std::vector<uint8_t>> idsToValue;
  idsToValue.reserve(kBatchSize);
  ids.reserve(kBatchSize);

  for (size_t i{0};
       i < kBatchSize && RocksDBInvertedListsIteratorBase::is_available();
       ++i, _it->Next()) {
    auto const id = LocalDocumentId(RocksDBKey::indexDocumentId(_it->key()));
    ids.emplace_back(id);
    std::vector<uint8_t> value(_it->value().data(),
                               _it->value().data() + _it->value().size());
    idsToValue.emplace(id, std::move(value));
  }
  if (ids.empty()) {
    return false;
  }

  // Multiget all those documents in a batch
  _filteredIds.clear();
  _collection->getPhysical()->lookup(
      _filterContext.trx, ids,
      [&](Result result, LocalDocumentId id, aql::DocumentData&& /*data */,
          VPackSlice doc) {
        if (result.fail()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              result.errorNumber(),
              basics::StringUtils::concatT("failed to materialize document ",
                                           RevisionId(id).toString(), " (",
                                           id.id(),
                                           ")"
                                           ": ",
                                           result.errorMessage()));
        }

        aql::GenericDocumentExpressionContext ctx(
            *_filterContext.trx, *_filterContext.queryContext,
            _aqlFunctionsInternalCache, *_filterContext.filterVarsToRegs,
            *_filterContext.inputRow, _filterContext.documentVariable);
        ctx.setCurrentDocument(doc);
        bool mustDestroy{false};  // will get filled by execution
        aql::AqlValue a =
            _filterContext.filterExpression->execute(&ctx, mustDestroy);
        aql::AqlValueGuard guard(a, mustDestroy);
        auto const filterExpressionResult = a.toBoolean();
        if (filterExpressionResult) {
          auto entry = Strategy::extractVectorIndexValue(
              rocksdb::Slice(
                  reinterpret_cast<const char*>(idsToValue[id].data()),
                  idsToValue[id].size()),
              _codeSize);

          velocypack::SharedSlice captureData;
          if (_filterContext.capturedDocuments != nullptr) {
            if (_filterContext.captureShape == CaptureShape::kFullDocument) {
              captureData = toOwnedSharedSlice(doc);
            } else if constexpr (Strategy::hasStoredValues) {
              captureData = entry.storedValues;
            }
          }

          if constexpr (Strategy::hasStoredValues) {
            _filteredIds.push_back({.id = id,
                                    .codes = std::move(entry.encodedValue),
                                    .captureData = std::move(captureData)});
          } else {
            _filteredIds.push_back({.id = id,
                                    .codes = std::move(entry),
                                    .captureData = std::move(captureData)});
          }
        }

        return true;
      },
      {.countBytes = true});
  _filteredIdsIt = _filteredIds.begin();

  return true;
}

// Explicit instantiations
template struct RocksDBInvertedListsFilteringIterator<NoStoredValuesStrategy>;
template struct RocksDBInvertedListsFilteringIterator<
    WithStoredValuesV1Strategy>;
template struct RocksDBInvertedListsFilteringIterator<
    WithStoredValuesV2Strategy>;

/// RocksDBInvertedListsFilteringStoredValuesIterator
template<VectorIndexStoredValuesStrategy Strategy>
requires(Strategy::hasStoredValues)
    RocksDBInvertedListsFilteringStoredValuesIterator<Strategy>::
        RocksDBInvertedListsFilteringStoredValuesIterator(
            RocksDBVectorIndex* index, LogicalCollection* collection,
            IteratorFilterContext& filterContext, std::size_t listNumber,
            std::size_t codeSize)
    : RocksDBInvertedListsFilteringIteratorBase(
          index, collection, filterContext, listNumber, codeSize) {
  TRI_ASSERT(index->hasStoredValues() && filterContext.useStoredValuesIterator);
  skipOverFilteredDocuments();
}

template<VectorIndexStoredValuesStrategy Strategy>
requires(Strategy::hasStoredValues) bool RocksDBInvertedListsFilteringStoredValuesIterator<
    Strategy>::searchFilteredIds() {
  // Get documents ids from the vector index
  std::vector<std::pair<LocalDocumentId, RocksDBVectorIndexEntryValue>> items;
  items.reserve(kBatchSize);

  for (size_t i{0};
       i < kBatchSize && RocksDBInvertedListsIteratorBase::is_available();
       ++i, _it->Next()) {
    auto const id = LocalDocumentId(RocksDBKey::indexDocumentId(_it->key()));
    auto entryValue =
        Strategy::extractVectorIndexValue(_it->value(), _codeSize);

    items.emplace_back(id, std::move(entryValue));
  }
  if (items.empty()) {
    return false;
  }

  // Filter using stored values instead of fetching full documents
  _filteredIds.clear();
  for (auto const& [id, value] : items) {
    auto storedValuesSlice = value.storedValues.slice();

    // This should not happen...
    TRI_ASSERT(!storedValuesSlice.isNone());
    if (storedValuesSlice.isNone()) {
      LOG_TOPIC("c42a1", ERR, Logger::ENGINES)
          << "Document " << id
          << " in vector index lacks stored values but filtering iterator "
          << "expects them. Skipping document.";
      continue;
    }

    TRI_ASSERT(storedValuesSlice.isArray());
    TRI_ASSERT(storedValuesSlice.length() == _index->storedValues().size());

    // Construct partial document which contains only storedValues
    VPackBuilder partialDocument;
    {
      velocypack::ObjectBuilder guard(&partialDocument);

      size_t idx = 0;
      auto const& storedValues = _index->storedValues();
      for (size_t i{0}; i < storedValues.size(); ++i) {
        std::string fieldString;
        TRI_AttributeNamesToString(storedValues[i], fieldString);
        partialDocument.add(fieldString, storedValuesSlice.at(idx));
        ++idx;
      }
    }

    // Create expression context for filtering using stored values
    aql::GenericDocumentExpressionContext ctx(
        *_filterContext.trx, *_filterContext.queryContext,
        _aqlFunctionsInternalCache, *_filterContext.filterVarsToRegs,
        *_filterContext.inputRow, _filterContext.documentVariable);
    ctx.setCurrentDocument(partialDocument.slice());

    bool mustDestroy{false};
    aql::AqlValue a =
        _filterContext.filterExpression->execute(&ctx, mustDestroy);
    aql::AqlValueGuard guard(a, mustDestroy);
    auto const filterExpressionResult = a.toBoolean();
    if (filterExpressionResult) {
      velocypack::SharedSlice captureData;
      if (_filterContext.capturedDocuments != nullptr) {
        captureData = value.storedValues;
      }
      _filteredIds.push_back({.id = id,
                              .codes = std::move(value.encodedValue),
                              .captureData = std::move(captureData)});
    }
  }
  _filteredIdsIt = _filteredIds.begin();

  return true;
}

// Explicit instantiations
template struct RocksDBInvertedListsFilteringStoredValuesIterator<
    WithStoredValuesV1Strategy>;
template struct RocksDBInvertedListsFilteringStoredValuesIterator<
    WithStoredValuesV2Strategy>;

/// RocksDBInvertedLists
RocksDBInvertedLists::RocksDBInvertedLists(RocksDBVectorIndex* index,
                                           LogicalCollection* collection,
                                           std::size_t nlist, size_t codeSize)
    : InvertedLists(nlist, codeSize), _index(index), _collection(collection) {
  use_iterator = true;
  assert(status.ok());
}

faiss::InvertedListsIterator* RocksDBInvertedLists::get_iterator(
    std::size_t listNumber, void* context) const {
  auto* iteratorContext = static_cast<RocksDBFaissIteratorContext*>(context);
  TRI_ASSERT(iteratorContext != nullptr);

  bool const isV2 = _index->formatVersion() == VectorIndexFormatVersion::kV2;

  return std::visit(
      overload{
          [&](IteratorFilterContext& filterContext)
              -> faiss::InvertedListsIterator* {
            // Use stored values filtering iterator when both the filter and
            // the projections are coverable -- in that case the FAISS layer
            // never needs to touch the underlying documents.
            if (filterContext.useStoredValuesIterator) {
              if (isV2) {
                return new RocksDBInvertedListsFilteringStoredValuesIterator<
                    WithStoredValuesV2Strategy>(_index, _collection,
                                                filterContext, listNumber,
                                                this->code_size);
              }
              return new RocksDBInvertedListsFilteringStoredValuesIterator<
                  WithStoredValuesV1Strategy>(_index, _collection,
                                              filterContext, listNumber,
                                              this->code_size);
            }

            // Choose the filtering iterator based on whether the index has
            // stored values, and on the on-disk format version when it does.
            if (_index->hasStoredValues()) {
              if (isV2) {
                return new RocksDBInvertedListsFilteringIterator<
                    WithStoredValuesV2Strategy>(_index, _collection,
                                                filterContext, listNumber,
                                                this->code_size);
              }
              return new RocksDBInvertedListsFilteringIterator<
                  WithStoredValuesV1Strategy>(_index, _collection,
                                              filterContext, listNumber,
                                              this->code_size);
            }
            return new RocksDBInvertedListsFilteringIterator<
                NoStoredValuesStrategy>(_index, _collection, filterContext,
                                        listNumber, this->code_size);
          },
          [&](IteratorContext const& simpleCtx)
              -> faiss::InvertedListsIterator* {
            if (_index->hasStoredValues()) {
              if (isV2) {
                return new RocksDBInvertedListsIterator<
                    WithStoredValuesV2Strategy>(_index, _collection, simpleCtx,
                                                listNumber, this->code_size);
              }
              return new RocksDBInvertedListsIterator<
                  WithStoredValuesV1Strategy>(_index, _collection, simpleCtx,
                                              listNumber, this->code_size);
            }
            return new RocksDBInvertedListsIterator<NoStoredValuesStrategy>(
                _index, _collection, simpleCtx, listNumber, this->code_size);
          },
      },
      *iteratorContext);
}
};  // namespace arangodb::vector
