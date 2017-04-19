////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_REPLICATION_CONTEXT_H
#define ARANGO_ROCKSDB_ROCKSDB_REPLICATION_CONTEXT_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "Transaction/Methods.h"
#include "VocBase/vocbase.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

class RocksDBReplicationResult : public Result {
 public:
  RocksDBReplicationResult(int, uint64_t);
  uint64_t maxTick() const;

 private:
  uint64_t _maxTick;
};

class RocksDBReplicationContext {
 private:
  typedef std::function<void(DocumentIdentifierToken const& token)>
      TokenCallback;

 public:
  RocksDBReplicationContext();

  TRI_voc_tick_t id() const;

  // creates new transaction/snapshot, returns inventory
  std::pair<RocksDBReplicationResult, std::shared_ptr<VPackBuilder>>
  getInventory(TRI_vocbase_t* vocbase, bool includeSystem);

  // iterates over at most 'limit' documents in the collection specified,
  // creating a new iterator if one does not exist for this collection
  std::pair<RocksDBReplicationResult, bool> dump(
      TRI_vocbase_t* vocbase, std::string const& collectionName,
      TokenCallback cb, size_t limit);

  // iterates over WAL starting at 'from' and returns up to 'limit' documents
  // from the corresponding database
  RocksDBReplicationResult tail(TRI_vocbase_t* vocbase, uint64_t from,
                                size_t limit, VPackBuilder& builder);

 private:
  std::unique_ptr<transaction::Methods> createTransaction(
      TRI_vocbase_t* vocbase);

  static bool filterCollection(arangodb::LogicalCollection* collection,
                               void* data);

  static bool sortCollections(arangodb::LogicalCollection const* l,
                              arangodb::LogicalCollection const* r);

 private:
  TRI_voc_tick_t _id;
  uint64_t _lastTick;
  std::unique_ptr<transaction::Methods> _trx;
  LogicalCollection* _collection;
  std::unique_ptr<IndexIterator> _iter;
  ManagedDocumentResult _mdr;
};

}  // namespace arangodb

#endif
