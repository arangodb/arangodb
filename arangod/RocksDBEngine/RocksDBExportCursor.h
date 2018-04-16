////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_EXPORT_CURSOR_H
#define ARANGOD_ROCKSDB_ROCKSDB_EXPORT_CURSOR_H 1

#include "Basics/Common.h"
#include "Utils/CollectionExport.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Cursor.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class IndexIterator;
class SingleCollectionTransaction;

class RocksDBExportCursor final : public Cursor {
 public:
  RocksDBExportCursor(
    TRI_vocbase_t& vocbase,
    std::string const& name,
    CollectionExport::Restrictions const& restrictions,
    CursorId id,
    size_t limit,
    size_t batchSize,
    double ttl,
    bool hasCount
  );

  ~RocksDBExportCursor();

  CursorType type() const override final { return CURSOR_EXPORT; }

  bool hasNext();

  arangodb::velocypack::Slice next();

  size_t count() const override final;

  Result dump(velocypack::Builder&) override final;

  std::shared_ptr<transaction::Context> context() const override final;

 private:
  DatabaseGuard _guard;
  arangodb::CollectionNameResolver _resolver;
  CollectionExport::Restrictions _restrictions;
  std::string const _name;
  std::unique_ptr<SingleCollectionTransaction> _trx;
  std::unique_ptr<IndexIterator> _iter;
  size_t _position;
  size_t _size;
};

}

#endif