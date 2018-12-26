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
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class IndexIterator;
class SingleCollectionTransaction;

class RocksDBExportCursor final : public Cursor {
 public:
  RocksDBExportCursor(TRI_vocbase_t*, std::string const&,
                      CollectionExport::Restrictions const&, CursorId, size_t,
                      size_t, double, bool);

  ~RocksDBExportCursor();

 public:
  CursorType type() const override final { return CURSOR_EXPORT; }

  bool hasNext() override final;

  arangodb::velocypack::Slice next() override final;

  size_t count() const override final;

  void dump(velocypack::Builder&) override final;

 private:
  DatabaseGuard _guard;
  arangodb::CollectionNameResolver _resolver;
  CollectionExport::Restrictions _restrictions;
  std::string const _name;
  std::unique_ptr<SingleCollectionTransaction> _trx;
  ManagedDocumentResult _mdr;
  std::unique_ptr<IndexIterator> _iter;
  size_t _size;
};
}  // namespace arangodb

#endif
