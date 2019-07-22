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

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"
#include "Basics/system-functions.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace velocypack {
class Slice;
}

class LogicalCollection;

typedef TRI_voc_tick_t CollectionKeysId;

class CollectionKeys {
 public:
  CollectionKeys(CollectionKeys const&) = delete;
  CollectionKeys& operator=(CollectionKeys const&) = delete;

  CollectionKeys(TRI_vocbase_t*, double ttl);

  virtual ~CollectionKeys() = default;

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

  virtual size_t count() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initially creates the list of keys
  //////////////////////////////////////////////////////////////////////////////

  virtual void create(TRI_voc_tick_t) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief hashes a chunk of keys
  //////////////////////////////////////////////////////////////////////////////

  virtual std::tuple<std::string, std::string, uint64_t> hashChunk(size_t, size_t) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief dumps keys into the result
  //////////////////////////////////////////////////////////////////////////////

  virtual void dumpKeys(arangodb::velocypack::Builder&, size_t, size_t) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief dumps documents into the result
  //////////////////////////////////////////////////////////////////////////////

  virtual void dumpDocs(arangodb::velocypack::Builder& result, size_t chunk,
                        size_t chunkSize, size_t offsetInChunk, size_t maxChunkSize,
                        arangodb::velocypack::Slice const& ids) const = 0;

 protected:
  TRI_vocbase_t* _vocbase;
  arangodb::LogicalCollection* _collection;
  CollectionKeysId _id;
  double _ttl;
  double _expires;
  bool _isDeleted;
  bool _isUsed;
};
}  // namespace arangodb

#endif
