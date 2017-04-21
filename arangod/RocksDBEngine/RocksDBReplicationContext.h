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
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBReplicationCommon.h"
#include "RocksDBEngine/RocksDBToken.h"
#include "Transaction/Methods.h"
#include "VocBase/vocbase.h"

#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

class RocksDBReplicationContext {
 public:
  /// default time-to-live for contexts
  static double const DefaultTTL;

 private:
  typedef std::function<void(DocumentIdentifierToken const& token)>
      TokenCallback;

 public:
  RocksDBReplicationContext();
  ~RocksDBReplicationContext();

  TRI_voc_tick_t id() const;
  uint64_t lastTick() const;

  // creates new transaction/snapshot
  void bind(TRI_vocbase_t*);

  // returns inventory
  std::pair<RocksDBReplicationResult, std::shared_ptr<VPackBuilder>>
  getInventory(TRI_vocbase_t* vocbase, bool includeSystem);

  // iterates over at most 'limit' documents in the collection specified,
  // creating a new iterator if one does not exist for this collection
  RocksDBReplicationResult dump(TRI_vocbase_t* vocbase,
                                std::string const& collectionName,
                                basics::StringBuffer&, uint64_t chunkSize);

  double expires() const;
  bool isDeleted() const;
  void deleted();
  bool isUsed() const;
  void use(double ttl);
  bool more() const;
  /// remove use flag
  void release();

 private:
  void releaseDumpingResources();

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
  std::shared_ptr<arangodb::velocypack::CustomTypeHandler> _customTypeHandler;
  arangodb::velocypack::Options _vpackOptions;

  double _expires;
  bool _isDeleted;
  bool _isUsed;
  bool _hasMore;
};

}  // namespace arangodb

#endif
