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
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

#include <faiss/MetricType.h>
#include <faiss/invlists/InvertedLists.h>

namespace arangodb::vector {

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
  RocksDBTransactionMethods* mthds = RocksDBTransactionState::toMethods(
      searchParametersContext.trx, collection->id());

  _it = mthds->NewIterator(index->columnFamily(), [&](auto& opts) {
    TRI_ASSERT(opts.prefix_same_as_start);
  });

  _rocksdbKey.constructVectorIndexValue(_index->objectId(), _listNumber);
  _it->Seek(_rocksdbKey.string());
  skipOverFilteredDocuments();
}

[[nodiscard]] bool RocksDBInvertedListsIterator::is_available() const {
  return _it->Valid() && _it->key().starts_with(_rocksdbKey.string());
}

// This should be only called when we have filterExpression
void RocksDBInvertedListsIterator::skipOverFilteredDocuments() {
  if (_searchParametersContext.filterExpression == nullptr) {
    return;
  }

  while (_it->Valid()) {
    auto const docId = RocksDBKey::indexDocumentId(_it->key());

    bool filterExpressionResult{false};
    _collection->getPhysical()->lookup(
        _searchParametersContext.trx, docId,
        [&](LocalDocumentId token, aql::DocumentData&& /*data */,
            VPackSlice doc) {
          TRI_ASSERT(_searchParametersContext.inputRow.has_value());
          aql::GenericDocumentExpressionContext ctx(
              *_searchParametersContext.trx,
              *_searchParametersContext.queryContext,
              _aqlFunctionsInternalCache,
              *_searchParametersContext.filterVarsToRegs,
              *_searchParametersContext.inputRow,
              _searchParametersContext.documentVariable);
          ctx.setCurrentDocument(doc);
          bool mustDestroy;  // will get filled by execution
          aql::AqlValue a = _searchParametersContext.filterExpression->execute(
              &ctx, mustDestroy);
          aql::AqlValueGuard guard(a, mustDestroy);
          filterExpressionResult = a.toBoolean();

          return true;
        },
        {.countBytes = true});

    if (filterExpressionResult) {
      break;
    }

    _it->Next();
  }
}

void RocksDBInvertedListsIterator::next() {
  _it->Next();
  skipOverFilteredDocuments();
}

std::pair<faiss::idx_t, uint8_t const*>
RocksDBInvertedListsIterator::get_id_and_codes() {
  auto const docId = RocksDBKey::indexDocumentId(_it->key());
  TRI_ASSERT(_codeSize == _it->value().size());
  auto const* value = reinterpret_cast<uint8_t const*>(_it->value().data());
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

};  // namespace arangodb::vector
