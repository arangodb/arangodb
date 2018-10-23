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

#ifndef ARANGOD_MMFILES_MMFILES_COLLECTION_KEYS_H
#define ARANGOD_MMFILES_MMFILES_COLLECTION_KEYS_H 1

#include "Basics/Common.h"
#include "Utils/CollectionKeys.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

struct TRI_vocbase_t;

namespace arangodb {

class CollectionGuard;
class MMFilesDocumentDitch;

typedef TRI_voc_tick_t CollectionKeysId;

class MMFilesCollectionKeys final : public CollectionKeys {
 public:
  MMFilesCollectionKeys(MMFilesCollectionKeys const&) = delete;
  MMFilesCollectionKeys& operator=(MMFilesCollectionKeys const&) = delete;

  MMFilesCollectionKeys(
    TRI_vocbase_t& vocbase,
    std::unique_ptr<CollectionGuard> guard,
    TRI_voc_tick_t blockerId,
    double ttl
  );

  ~MMFilesCollectionKeys();

  size_t count() const override {
    return _vpack.size();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initially creates the list of keys
  //////////////////////////////////////////////////////////////////////////////
  void create(TRI_voc_tick_t) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief hashes a chunk of keys
  //////////////////////////////////////////////////////////////////////////////
  std::tuple<std::string, std::string, uint64_t> hashChunk(size_t,
                                                           size_t) const override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief dumps keys into the result
  //////////////////////////////////////////////////////////////////////////////
  void dumpKeys(arangodb::velocypack::Builder&, size_t, size_t) const override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief dumps documents into the result
  //////////////////////////////////////////////////////////////////////////////
  void dumpDocs(arangodb::velocypack::Builder& result, size_t chunk, size_t chunkSize,
                size_t offsetInChunk, size_t maxChunkSize, arangodb::velocypack::Slice const& ids) const override;

 private:
  std::unique_ptr<arangodb::CollectionGuard> _guard;
  arangodb::MMFilesDocumentDitch* _ditch;
  arangodb::CollectionNameResolver _resolver;
  TRI_voc_tick_t _blockerId;
  std::vector<uint8_t const*> _vpack;
};

}

#endif