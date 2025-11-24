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

/// RocksDBInvertedListsIterator
template<VectorIndexStoredValuesStrategy Strategy>
RocksDBInvertedListsIterator<Strategy>::RocksDBInvertedListsIterator(
    RocksDBVectorIndex* index, LogicalCollection* collection,
    transaction::Methods* trx, std::size_t listNumber, std::size_t codeSize)
    : RocksDBInvertedListsIteratorBase(index, collection, trx, listNumber,
                                       codeSize) {}

template<VectorIndexStoredValuesStrategy Strategy>
void RocksDBInvertedListsIterator<Strategy>::next() {
  _it->Next();
}

template<VectorIndexStoredValuesStrategy Strategy>
std::pair<faiss::idx_t, uint8_t const*>
RocksDBInvertedListsIterator<Strategy>::get_id_and_codes() {
  // Use strategy to extract entry from RocksDB
  auto [docId, entry] =
      Strategy::extractVectorIndexEntry(_it->key(), _it->value(), _codeSize);
  _currentEntry = std::move(entry);

  // Return pointer to encoded data (location differs based on strategy)
  if constexpr (Strategy::hasStoredValues) {
    TRI_ASSERT(_currentEntry.encodedValue.size() == _codeSize)
        << "The encoded size is: " << _currentEntry.encodedValue.size()
        << " should be: " << _codeSize;
    return {static_cast<faiss::idx_t>(docId.id()),
            _currentEntry.encodedValue.data()};
  } else {
    TRI_ASSERT(_currentEntry.size() == _codeSize)
        << "The encoded size is: " << _currentEntry.size()
        << " should be: " << _codeSize;
    return {static_cast<faiss::idx_t>(docId.id()), _currentEntry.data()};
  }
}

// Explicit instantiations
template struct RocksDBInvertedListsIterator<NoStoredValuesStrategy>;
template struct RocksDBInvertedListsIterator<WithStoredValuesStrategy>;

/// RocksDBInvertedListsFilteringIteratorBase
RocksDBInvertedListsFilteringIteratorBase::
    RocksDBInvertedListsFilteringIteratorBase(
        RocksDBVectorIndex* index, LogicalCollection* collection,
        SearchParametersContext& searchParametersContext,
        std::size_t listNumber, std::size_t codeSize)
    : RocksDBInvertedListsIteratorBase(
          index, collection, searchParametersContext.trx, listNumber, codeSize),
      _searchParametersContext(searchParametersContext) {
  TRI_ASSERT(searchParametersContext.filterExpression != nullptr);
}

[[nodiscard]] bool RocksDBInvertedListsFilteringIteratorBase::is_available()
    const {
  return _filteredIdsIt != _filteredIds.end() ||
         RocksDBInvertedListsIteratorBase::is_available();
}

std::pair<faiss::idx_t, uint8_t const*>
RocksDBInvertedListsFilteringIteratorBase::get_id_and_codes() {
  return {static_cast<faiss::idx_t>(_filteredIdsIt->first.id()),
          _filteredIdsIt->second.data()};
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
    RocksDBInvertedListsFilteringIterator(
        RocksDBVectorIndex* index, LogicalCollection* collection,
        SearchParametersContext& searchParametersContext,
        std::size_t listNumber, std::size_t codeSize)
    : RocksDBInvertedListsFilteringIteratorBase(
          index, collection, searchParametersContext, listNumber, codeSize) {
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
      _searchParametersContext.trx, ids,
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
            *_searchParametersContext.trx,
            *_searchParametersContext.queryContext, _aqlFunctionsInternalCache,
            *_searchParametersContext.filterVarsToRegs,
            *_searchParametersContext.inputRow,
            _searchParametersContext.documentVariable);
        ctx.setCurrentDocument(doc);
        bool mustDestroy{false};  // will get filled by execution
        aql::AqlValue a = _searchParametersContext.filterExpression->execute(
            &ctx, mustDestroy);
        aql::AqlValueGuard guard(a, mustDestroy);
        auto const filterExpressionResult = a.toBoolean();
        if (filterExpressionResult) {
          // Use strategy to extract only the encoded part used by faiss
          auto entry = Strategy::extractVectorIndexValue(
              rocksdb::Slice(
                  reinterpret_cast<const char*>(idsToValue[id].data()),
                  idsToValue[id].size()),
              _codeSize);

          if constexpr (Strategy::hasStoredValues) {
            _filteredIds.emplace_back(id, std::move(entry.encodedValue));
          } else {
            _filteredIds.emplace_back(id, std::move(entry));
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
template struct RocksDBInvertedListsFilteringIterator<WithStoredValuesStrategy>;

/// RocksDBInvertedListsFilteringStoredValuesIterator
RocksDBInvertedListsFilteringStoredValuesIterator::
    RocksDBInvertedListsFilteringStoredValuesIterator(
        RocksDBVectorIndex* index, LogicalCollection* collection,
        SearchParametersContext& searchParametersContext,
        std::size_t listNumber, std::size_t codeSize)
    : RocksDBInvertedListsFilteringIteratorBase(
          index, collection, searchParametersContext, listNumber, codeSize) {
  TRI_ASSERT(index->hasStoredValues() &&
             searchParametersContext.isCoveredByStoredValues);
  skipOverFilteredDocuments();
}

bool RocksDBInvertedListsFilteringStoredValuesIterator::searchFilteredIds() {
  // Get documents ids from the vector index
  std::vector<std::pair<LocalDocumentId, RocksDBVectorIndexEntryValue>> items;
  items.reserve(kBatchSize);

  for (size_t i{0};
       i < kBatchSize && RocksDBInvertedListsIteratorBase::is_available();
       ++i, _it->Next()) {
    auto const id = LocalDocumentId(RocksDBKey::indexDocumentId(_it->key()));
    auto entryValue = RocksDBValue::vectorIndexEntryValue(_it->value());

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
        *_searchParametersContext.trx, *_searchParametersContext.queryContext,
        _aqlFunctionsInternalCache, *_searchParametersContext.filterVarsToRegs,
        *_searchParametersContext.inputRow,
        _searchParametersContext.documentVariable);
    ctx.setCurrentDocument(partialDocument.slice());

    bool mustDestroy{false};
    aql::AqlValue a =
        _searchParametersContext.filterExpression->execute(&ctx, mustDestroy);
    aql::AqlValueGuard guard(a, mustDestroy);
    auto const filterExpressionResult = a.toBoolean();
    if (filterExpressionResult) {
      // We do not keep whole value but only the encoded part used by faiss
      _filteredIds.emplace_back(id, std::move(value.encodedValue));
    }
  }
  _filteredIdsIt = _filteredIds.begin();

  return true;
}

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
  auto* iteratorContext = static_cast<RocksDBFaissSearchContext*>(context);
  TRI_ASSERT(iteratorContext != nullptr);

  return std::visit(
      overload{
          [&](SearchParametersContext& searchParametersContext)
              -> faiss::InvertedListsIterator* {
            // Use stored values filtering iterator if it can optimize the query
            if (searchParametersContext.isCoveredByStoredValues) {
              return new RocksDBInvertedListsFilteringStoredValuesIterator(
                  _index, _collection, searchParametersContext, listNumber,
                  this->code_size);
            }

            // Choose the filtering iterator based on whether index has stored
            // values
            if (_index->hasStoredValues()) {
              return new RocksDBInvertedListsFilteringIterator<
                  WithStoredValuesStrategy>(_index, _collection,
                                            searchParametersContext, listNumber,
                                            this->code_size);
            } else {
              return new RocksDBInvertedListsFilteringIterator<
                  NoStoredValuesStrategy>(_index, _collection,
                                          searchParametersContext, listNumber,
                                          this->code_size);
            }
          },
          [&](transaction::Methods* trx) -> faiss::InvertedListsIterator* {
            // Choose the simple iterator based on whether index has stored
            // values
            if (_index->hasStoredValues()) {
              return new RocksDBInvertedListsIterator<WithStoredValuesStrategy>(
                  _index, _collection, trx, listNumber, this->code_size);
            } else {
              return new RocksDBInvertedListsIterator<NoStoredValuesStrategy>(
                  _index, _collection, trx, listNumber, this->code_size);
            }
          },
      },
      *iteratorContext);
}
};  // namespace arangodb::vector
