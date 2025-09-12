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
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

#include <faiss/MetricType.h>
#include <faiss/invlists/InvertedLists.h>


namespace arangodb {

namespace vector {

#define LOG_ADDITIONAL_ENABLED false
#define LOG_ADDITIONAL_IF(cond) LOG_DEVEL_IF((LOG_ADDITIONAL_ENABLED) && (cond))
#define LOG_ADDITIONAL LOG_ADDITIONAL_IF(true)

RocksDBInvertedListsIterator::RocksDBInvertedListsIterator(
    RocksDBVectorIndex* index, LogicalCollection* collection,
    SearchParametersContext& searchParametersContext, std::size_t listNumber,
    std::size_t codeSize)
    : InvertedListsIterator(),
      _index(index),
      _collection(collection),
      _searchParametersContext(searchParametersContext),
      _listNumber(listNumber),
      _codeSize(codeSize) {
  LOG_ADDITIONAL << ADB_HERE << " CONSTRUCTOR";
  RocksDBTransactionMethods* mthds = RocksDBTransactionState::toMethods(
      searchParametersContext.trx, collection->id());

  _it = mthds->NewIterator(index->columnFamily(), [&](auto& opts) {
    TRI_ASSERT(opts.prefix_same_as_start);
  });
  _rocksdbKey.constructVectorIndexValue(_index->objectId(), _listNumber);
  _it->Seek(_rocksdbKey.string());

  if (_searchParametersContext.filterExpression) {
    _batchIt = mthds->NewIterator(index->columnFamily(), [&](auto& opts) {
      TRI_ASSERT(opts.prefix_same_as_start);
    });
    _batchIt->Seek(_rocksdbKey.string());
    setToValidIterator();
  }
}

[[nodiscard]] bool RocksDBInvertedListsIterator::is_available() const {
  if (_searchParametersContext.filterExpression) {
    LOG_ADDITIONAL << ADB_HERE << " Is iterator ended: "
              << (_filteredIdsIt == _filteredIds.end())
              << " is batchIt valid: " << (_batchIt->Valid());
    LOG_ADDITIONAL << ADB_HERE << ": "
              << (_filteredIdsIt != _filteredIds.end() || _batchIt->Valid());
    return _filteredIdsIt != _filteredIds.end() ||
           (_batchIt->Valid() &&
            _batchIt->key().starts_with(_rocksdbKey.string()));
  }
  return _it->Valid() && _it->key().starts_with(_rocksdbKey.string());
}

bool RocksDBInvertedListsIterator::searchFilteredIds() {
  LOG_ADDITIONAL << ADB_HERE;
  //  Get documents ids from the vector index
  std::vector<LocalDocumentId> ids;
  std::unordered_map<LocalDocumentId, std::vector<uint8_t>> idsToValue;
  constexpr auto batchSize = 1000;
  idsToValue.reserve(batchSize);
  ids.reserve(batchSize);

  for (size_t i{0}; i < batchSize && _batchIt->Valid() &&
                    _batchIt->key().starts_with(_rocksdbKey.string());
       ++i, _batchIt->Next()) {
    auto const id =
        LocalDocumentId(RocksDBKey::indexDocumentId(_batchIt->key()));
    ids.emplace_back(id);
    std::vector<uint8_t> value(
        _batchIt->value().data(),
        _batchIt->value().data() + _batchIt->value().size());
    LOG_ADDITIONAL << ADB_HERE << " id: " << id << " value size: " << value.size();
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
        bool mustDestroy;  // will get filled by execution
        aql::AqlValue a = _searchParametersContext.filterExpression->execute(
            &ctx, mustDestroy);
        aql::AqlValueGuard guard(a, mustDestroy);
        auto const filterExpressionResult = a.toBoolean();
        if (filterExpressionResult) {
          LOG_ADDITIONAL << "Adding " << id << " doc: " << doc.toJson();
          _filteredIds.emplace_back(id, std::move(idsToValue[id]));
        }

        return true;
      },
      {.countBytes = true});
  LOG_ADDITIONAL << "FILTEREDIDS: " << _filteredIds << " Restarting iterator";
  _filteredIdsIt = _filteredIds.begin();

  return true;
}

// This should be only called when we have filterExpression
void RocksDBInvertedListsIterator::setToValidIterator() {
  LOG_ADDITIONAL << ADB_HERE;
  TRI_ASSERT(_searchParametersContext.filterExpression != nullptr);

  while (_filteredIdsIt == _filteredIds.end()) {
    if (!searchFilteredIds()) {
      // If we enter here we could not produce any documents ids so we just
      // invalidate the current _it
      LOG_ADDITIONAL << ADB_HERE << " _it: "
                << LocalDocumentId(RocksDBKey::indexDocumentId(_it->key()))
                << " and _batchId: "
                << LocalDocumentId(
                       RocksDBKey::indexDocumentId(_batchIt->key()));
      return;
    }
  }

  // TRI_ASSERT(_filteredIdsIt != _filteredIds.end());
  //++_filteredIdsIt;
}

void RocksDBInvertedListsIterator::next() {
  if (_searchParametersContext.filterExpression) {
    if (_filteredIdsIt != _filteredIds.end()) {
      LOG_ADDITIONAL << ADB_HERE << " Current iterator is " << *_filteredIdsIt;
    } else {
      LOG_ADDITIONAL << ADB_HERE << " Current iterator has ended";
    }
    setToValidIterator();
    ++_filteredIdsIt;
    if (_filteredIdsIt != _filteredIds.end()) {
      LOG_ADDITIONAL << ADB_HERE << " Next iterator is " << *_filteredIdsIt;
    } else {
      LOG_ADDITIONAL << ADB_HERE << " Next iterator has ended";
    }
  } else {
    LOG_ADDITIONAL << ADB_HERE << " Cannot enter here";
    _it->Next();
  }
}

std::pair<faiss::idx_t, uint8_t const*>
RocksDBInvertedListsIterator::get_id_and_codes() {
  LOG_ADDITIONAL << ADB_HERE << " _filteredIds: " << _filteredIds;
  if (_searchParametersContext.filterExpression != nullptr) {
    LOG_ADDITIONAL << ADB_HERE << " id: " << _filteredIdsIt->first.id()
              << " value size: " << _filteredIdsIt->second.size();
    return {static_cast<faiss::idx_t>(_filteredIdsIt->first.id()),
            _filteredIdsIt->second.data()};
  }
  auto const docId = RocksDBKey::indexDocumentId(_it->key());
  TRI_ASSERT(_codeSize == _it->value().size());
  auto const* value = reinterpret_cast<uint8_t const*>(_it->value().data());
  LOG_ADDITIONAL << ADB_HERE << " id: " << docId.id();
  return {static_cast<faiss::idx_t>(docId.id()), value};
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
  auto* searchParametersContext =
      static_cast<SearchParametersContext*>(context);
  return new RocksDBInvertedListsIterator(_index, _collection,
                                          *searchParametersContext, listNumber,
                                          this->code_size);
}
};  // namespace vector

};  // namespace arangodb
