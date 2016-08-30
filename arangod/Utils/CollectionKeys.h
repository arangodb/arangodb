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

#ifndef ARANGOD_UTILS_COLLECTION_KEYS_H
#define ARANGOD_UTILS_COLLECTION_KEYS_H 1

#include "Basics/Common.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

struct TRI_df_marker_t;
struct TRI_vocbase_t;

namespace arangodb {
namespace velocypack {
class Slice;
}

class CollectionGuard;
class DocumentDitch;

typedef TRI_voc_tick_t CollectionKeysId;

class CollectionKeys {
 public:
  CollectionKeys(CollectionKeys const&) = delete;
  CollectionKeys& operator=(CollectionKeys const&) = delete;

  CollectionKeys(TRI_vocbase_t*, std::string const&, TRI_voc_tick_t,
                 double ttl);

  ~CollectionKeys();

 public:
  CollectionKeysId id() const { return _id; }

  double ttl() const { return _ttl; }

  double expires() const { return _expires; }

  bool isUsed() const { return _isUsed; }

  bool isDeleted() const { return _isDeleted; }

  void deleted() { _isDeleted = true; }

  void use() {
    TRI_ASSERT(!_isDeleted);
    TRI_ASSERT(!_isUsed);

    _isUsed = true;
    _expires = TRI_microtime() + _ttl;
  }

  void release() {
    TRI_ASSERT(_isUsed);
    _isUsed = false;
  }

  size_t count() const {
    TRI_ASSERT(_markers != nullptr);
    return _markers->size();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initially creates the list of keys
  //////////////////////////////////////////////////////////////////////////////

  void create(TRI_voc_tick_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief hashes a chunk of keys
  //////////////////////////////////////////////////////////////////////////////

  std::tuple<std::string, std::string, uint64_t> hashChunk(size_t,
                                                           size_t) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief dumps keys into the result
  //////////////////////////////////////////////////////////////////////////////

  void dumpKeys(arangodb::velocypack::Builder&, size_t, size_t) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief dumps documents into the result
  //////////////////////////////////////////////////////////////////////////////

  void dumpDocs(arangodb::velocypack::Builder&, size_t, size_t,
                arangodb::velocypack::Slice const&) const;

 private:
  struct TRI_vocbase_t* _vocbase;
  arangodb::CollectionGuard* _guard;
  arangodb::LogicalCollection* _collection;
  arangodb::DocumentDitch* _ditch;
  std::string const _name;
  arangodb::CollectionNameResolver _resolver;
  TRI_voc_tick_t _blockerId;
  std::vector<void const*>* _markers;

  CollectionKeysId _id;
  double _ttl;
  double _expires;
  bool _isDeleted;
  bool _isUsed;
};
}

#endif
