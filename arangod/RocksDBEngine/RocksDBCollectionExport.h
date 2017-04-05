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

#ifndef ARANGOD_ROCKSDB_ROCKSDB_COLLECTION_EXPORT_H
#define ARANGOD_ROCKSDB_ROCKSDB_COLLECTION_EXPORT_H 1

#include "Basics/Common.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/voc-types.h"

#include <velocypack/SliceContainer.h>

struct TRI_vocbase_t;

namespace arangodb {

class CollectionGuard;
class RocksDBDocumentDitch;

class RocksDBCollectionExport {
  friend class RocksDBExportCursor;

 public:
  struct Restrictions {
    enum Type { RESTRICTION_NONE, RESTRICTION_INCLUDE, RESTRICTION_EXCLUDE };

    Restrictions() : fields(), type(RESTRICTION_NONE) {}

    std::unordered_set<std::string> fields;
    Type type;
  };

 public:
  RocksDBCollectionExport(RocksDBCollectionExport const&) = delete;
  RocksDBCollectionExport& operator=(RocksDBCollectionExport const&) = delete;

  RocksDBCollectionExport(TRI_vocbase_t*, std::string const&,
                          Restrictions const&);

  ~RocksDBCollectionExport();

 public:
  void run(uint64_t, size_t);

 private:
  std::unique_ptr<arangodb::CollectionGuard> _guard;
  LogicalCollection* _collection;
  std::string const _name;
  arangodb::CollectionNameResolver _resolver;
  Restrictions _restrictions;
  std::vector<velocypack::SliceContainer> _vpack;
};
}

#endif
