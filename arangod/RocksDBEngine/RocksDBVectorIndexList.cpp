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
#include "Inspection/VPack.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

#include <faiss/MetricType.h>
#include <faiss/invlists/InvertedLists.h>
#include <velocypack/Builder.h>

namespace arangodb {

namespace vector {

RocksDBInvertedListsIterator::RocksDBInvertedListsIterator(
    RocksDBVectorIndex* index, LogicalCollection* collection,
    transaction::Methods* trx, std::size_t listNumber, std::size_t codeSize)
    : InvertedListsIterator(),
      _index(index),
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

[[nodiscard]] bool RocksDBInvertedListsIterator::is_available() const {
  return _it->Valid() && _it->key().starts_with(_rocksdbKey.string());
}

void RocksDBInvertedListsIterator::next() { _it->Next(); }

std::pair<faiss::idx_t, uint8_t const*>
RocksDBInvertedListsIterator::get_id_and_codes() {
  auto const docId = RocksDBKey::indexDocumentId(_it->key());
  _currentValueEntry.clear();

  auto value =
      VPackSlice(reinterpret_cast<uint8_t const*>(_it->value().data()));
  // LOG_DEVEL << "READ STR: " << _it->value().ToStringView();
  // LOG_DEVEL << "READ SIZE: " << _it->value().size();
  auto status = velocypack::deserializeWithStatus(value, _currentValueEntry);
  // LOG_DEVEL << "READ encoded size: " <<
  // _currentValueEntry.encodedValue.size();
  TRI_ASSERT(_currentValueEntry.encodedValue.size() == _codeSize)
      << "The encoded size is: " << _currentValueEntry.encodedValue.size()
      << " shouldbe: " << _codeSize;

  return {static_cast<faiss::idx_t>(docId.id()),
          _currentValueEntry.encodedValue.data()};
}

RocksDBInvertedListsFilteringIterator::RocksDBInvertedListsFilteringIterator(
    RocksDBVectorIndex* index, LogicalCollection* collection,
    SearchParametersContext& searchParametersContext, std::size_t listNumber,
    std::size_t codeSize)
    : InvertedListsIterator(),
      _index(index),
      _collection(collection),
      _searchParametersContext(searchParametersContext),
      _listNumber(listNumber),
      _codeSize(codeSize) {
  TRI_ASSERT(searchParametersContext.filterExpression != nullptr);
  RocksDBTransactionMethods* mthds = RocksDBTransactionState::toMethods(
      searchParametersContext.trx, collection->id());

  _rocksdbKey.constructVectorIndexValue(_index->objectId(), _listNumber);
  _batchIt = mthds->NewIterator(index->columnFamily(), [&](auto& opts) {
    TRI_ASSERT(opts.prefix_same_as_start);
  });
  _batchIt->Seek(_rocksdbKey.string());
  setToValidIterator();
}

[[nodiscard]] bool RocksDBInvertedListsFilteringIterator::is_available() const {
  return _filteredIdsIt != _filteredIds.end() ||
         (_batchIt->Valid() &&
          _batchIt->key().starts_with(_rocksdbKey.string()));
}

bool RocksDBInvertedListsFilteringIterator::searchFilteredIds() {
  // Get documents ids from the vector index
  std::vector<LocalDocumentId> ids;
  std::unordered_map<LocalDocumentId, std::vector<uint8_t>> idsToValue;
  idsToValue.reserve(kBatchSize);
  ids.reserve(kBatchSize);

  for (size_t i{0}; i < kBatchSize && _batchIt->Valid() &&
                    _batchIt->key().starts_with(_rocksdbKey.string());
       ++i, _batchIt->Next()) {
    auto const id =
        LocalDocumentId(RocksDBKey::indexDocumentId(_batchIt->key()));
    ids.emplace_back(id);
    std::vector<uint8_t> value(
        _batchIt->value().data(),
        _batchIt->value().data() + _batchIt->value().size());
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
          // We do not keep whole value but only the encoded part used by faiss
          RocksDBVectorIndexEntryValue entry;
          auto slice = VPackSlice(
              reinterpret_cast<uint8_t const*>(idsToValue[id].data()));
          velocypack::deserialize(slice, entry);
          _filteredIds.emplace_back(id, std::move(entry.encodedValue));
        }

        return true;
      },
      {.countBytes = true});
  _filteredIdsIt = _filteredIds.begin();

  return true;
}

void RocksDBInvertedListsFilteringIterator::setToValidIterator() {
  while (_filteredIdsIt == _filteredIds.end()) {
    if (!searchFilteredIds()) {
      // If we enter here we could not produce any documents
      return;
    }
  }
}

void RocksDBInvertedListsFilteringIterator::next() {
  setToValidIterator();
  ++_filteredIdsIt;
  if (_filteredIdsIt == _filteredIds.end()) {
    setToValidIterator();
  }
}

std::pair<faiss::idx_t, uint8_t const*>
RocksDBInvertedListsFilteringIterator::get_id_and_codes() {
  return {static_cast<faiss::idx_t>(_filteredIdsIt->first.id()),
          _filteredIdsIt->second.data()};
}

RocksDBInvertedListsFilteringStoredValuesIterator::
    RocksDBInvertedListsFilteringStoredValuesIterator(
        RocksDBVectorIndex* index, LogicalCollection* collection,
        SearchParametersContext& searchParametersContext,
        std::size_t listNumber, std::size_t codeSize)
    : InvertedListsIterator(),
      _index(index),
      _collection(collection),
      _searchParametersContext(searchParametersContext),
      _listNumber(listNumber),
      _codeSize(codeSize) {
  TRI_ASSERT(searchParametersContext.filterExpression != nullptr);
  TRI_ASSERT(_index->hasStoredValues());

  RocksDBTransactionMethods* mthds = RocksDBTransactionState::toMethods(
      searchParametersContext.trx, collection->id());

  _rocksdbKey.constructVectorIndexValue(_index->objectId(), _listNumber);
  _it = mthds->NewIterator(index->columnFamily(), [&](auto& opts) {
    TRI_ASSERT(opts.prefix_same_as_start);
  });
  _it->Seek(_rocksdbKey.string());
  setToValidIterator();
}

[[nodiscard]] bool
RocksDBInvertedListsFilteringStoredValuesIterator::is_available() const {
  return _filteredIdsIt != _filteredIds.end() ||
         (_it->Valid() && _it->key().starts_with(_rocksdbKey.string()));
}

bool RocksDBInvertedListsFilteringStoredValuesIterator::searchFilteredIds() {
  // Get documents ids from the vector index
  std::vector<std::pair<LocalDocumentId, RocksDBVectorIndexEntryValue>> items;
  items.reserve(kBatchSize);

  for (size_t i{0}; i < kBatchSize && _it->Valid() &&
                    _it->key().starts_with(_rocksdbKey.string());
       ++i, _it->Next()) {
    auto const id = LocalDocumentId(RocksDBKey::indexDocumentId(_it->key()));
    RocksDBVectorIndexEntryValue entryValue;
    auto slice =
        VPackSlice(reinterpret_cast<uint8_t const*>(_it->value().data()));
    velocypack::deserialize(slice, entryValue);
    LOG_DEVEL << "Inserting into items: " << slice.toJson();
    TRI_ASSERT(slice.isObject());

    items.emplace_back(id, std::move(entryValue));
  }
  if (items.empty()) {
    return false;
  }

  // Filter using stored values instead of fetching full documents
  _filteredIds.clear();
  for (auto const& [id, value] : items) {
    // value.storedValues is already a SharedSlice containing the array
    // (or None if index doesn't have stored values configured)
    auto storedValuesSlice = value.storedValues.slice();

    // If no stored values were stored during insertion, we cannot use them for
    // filtering. This might happen if:
    // 1) The index was created without storedValues and later modified
    // 2) Old documents inserted before storedValues was configured
    // In this case, skip this document as we cannot properly evaluate the
    // filter.
    if (storedValuesSlice.isNone()) {
      LOG_TOPIC("c42a1", WARN, Logger::ENGINES)
          << "Document " << id
          << " in vector index lacks stored values but filtering iterator "
          << "expects them. Skipping document.";
      continue;
    }

    LOG_DEVEL << ADB_HERE << " data: " << storedValuesSlice.toJson();
    LOG_DEVEL << ADB_HERE << " data: " << storedValuesSlice.toString();
    LOG_DEVEL << ADB_HERE
              << " type: " << static_cast<int>(storedValuesSlice.type())
              << " isArray: " << storedValuesSlice.isArray()
              << " isObject: " << storedValuesSlice.isObject()
              << " isNone: " << storedValuesSlice.isNone()
              << " isEmptyArray: " << storedValuesSlice.isEmptyArray()
              << " isEmptyObject: " << storedValuesSlice.isEmptyObject();
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

void RocksDBInvertedListsFilteringStoredValuesIterator::setToValidIterator() {
  while (_filteredIdsIt == _filteredIds.end()) {
    if (!searchFilteredIds()) {
      // If we enter here we could not produce any documents
      return;
    }
  }
}

void RocksDBInvertedListsFilteringStoredValuesIterator::next() {
  setToValidIterator();
  ++_filteredIdsIt;
  if (_filteredIdsIt == _filteredIds.end()) {
    setToValidIterator();
  }
}

std::pair<faiss::idx_t, uint8_t const*>
RocksDBInvertedListsFilteringStoredValuesIterator::get_id_and_codes() {
  return {static_cast<faiss::idx_t>(_filteredIdsIt->first.id()),
          _filteredIdsIt->second.data()};
}

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
            if (searchParametersContext.isCoveredByStoredValues) {
              LOG_DEVEL << ADB_HERE
                        << " adding "
                           "RocksDBInvertedListsFilteringStoredValuesIterator";
              return new RocksDBInvertedListsFilteringStoredValuesIterator(
                  _index, _collection, searchParametersContext, listNumber,
                  this->code_size);
            }

            LOG_DEVEL << ADB_HERE
                      << " adding RocksDBInvertedListsFilteringIterator";
            return new RocksDBInvertedListsFilteringIterator(
                _index, _collection, searchParametersContext, listNumber,
                this->code_size);
          },
          [&](transaction::Methods* trx) -> faiss::InvertedListsIterator* {
            LOG_DEVEL << ADB_HERE << " adding RocksDBInvertedListsIterator";
            return new RocksDBInvertedListsIterator(
                _index, _collection, trx, listNumber, this->code_size);
          },
      },
      *iteratorContext);
}
};  // namespace vector

};  // namespace arangodb
