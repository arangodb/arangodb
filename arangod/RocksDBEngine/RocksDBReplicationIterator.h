////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_REPLICATION_ITERATOR_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_REPLICATION_ITERATOR_H 1

#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/snapshot.h>

#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "StorageEngine/ReplicationIterator.h"

namespace arangodb {
class LogicalCollection;

class RocksDBRevisionReplicationIterator : public RevisionReplicationIterator {
 public:
  RocksDBRevisionReplicationIterator(LogicalCollection&, rocksdb::Snapshot const*);
  RocksDBRevisionReplicationIterator(LogicalCollection&, transaction::Methods&);

  virtual bool hasMore() const override;
  virtual void reset() override;

  virtual RevisionId revision() const override;
  virtual VPackSlice document() const override;

  virtual void next() override;
  virtual void seek(RevisionId) override;

 private:
  std::unique_ptr<rocksdb::Iterator> _iter;
  rocksdb::ReadOptions _readOptions;
  RocksDBKeyBounds _bounds;
  rocksdb::Comparator const* _cmp;
};

}  // namespace arangodb
#endif
