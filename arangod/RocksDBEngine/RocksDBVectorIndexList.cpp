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
#include "Basics/overload.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

#include <faiss/MetricType.h>
#include <faiss/invlists/InvertedLists.h>

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
  TRI_ASSERT(_codeSize == _it->value().size());
  auto const docId = RocksDBKey::indexDocumentId(_it->key());
  auto const* value = reinterpret_cast<uint8_t const*>(_it->value().data());
  return {static_cast<faiss::idx_t>(docId.id()), value};
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
  skipOverFilteredDocuments();
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
          _filteredIds.emplace_back(id, std::move(idsToValue[id]));
        }

        return true;
      },
      {.countBytes = true});
  _filteredIdsIt = _filteredIds.begin();

  return true;
}

void RocksDBInvertedListsFilteringIterator::skipOverFilteredDocuments() {
  while (_filteredIdsIt == _filteredIds.end()) {
    if (!searchFilteredIds()) {
      // If we enter here we could not produce any documents
      return;
    }
  }
}

void RocksDBInvertedListsFilteringIterator::next() {
  skipOverFilteredDocuments();
  ++_filteredIdsIt;
  if (_filteredIdsIt == _filteredIds.end()) {
    skipOverFilteredDocuments();
  }
}

std::pair<faiss::idx_t, uint8_t const*>
RocksDBInvertedListsFilteringIterator::get_id_and_codes() {
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
            return new RocksDBInvertedListsFilteringIterator(
                _index, _collection, searchParametersContext, listNumber,
                this->code_size);
          },
          [&](transaction::Methods* trx) -> faiss::InvertedListsIterator* {
            return new RocksDBInvertedListsIterator(
                _index, _collection, trx, listNumber, this->code_size);
          },
      },
      *iteratorContext);
}
};  // namespace vector

};  // namespace arangodb
