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

#ifndef ARANGOD_VOC_BASE_MANAGED_DOCUMENT_RESULT_H
#define ARANGOD_VOC_BASE_MANAGED_DOCUMENT_RESULT_H 1

#include "Basics/Common.h"
#include "VocBase/RevisionCacheChunk.h"
#include "Logger/Logger.h"

namespace arangodb {
class Transaction;

struct ChunkCache {
  static constexpr size_t STATIC_ARRAY_SIZE = 4;
  size_t _chunksUsed;
  RevisionCacheChunk* _chunksArray[STATIC_ARRAY_SIZE];
  std::unordered_set<RevisionCacheChunk*> _chunksHash;

  ChunkCache() : _chunksUsed(0), _chunksArray() {}

  void add(RevisionCacheChunk* chunk) {
    if (_chunksUsed <= STATIC_ARRAY_SIZE) {
      if (_chunksUsed == STATIC_ARRAY_SIZE) {
        // transition static array to an unordered_map
        for (size_t i = 0; i < STATIC_ARRAY_SIZE; ++i) {
          _chunksHash.emplace(_chunksArray[i]);
        }
        _chunksHash.emplace(chunk);
      } else {
        if (_chunksUsed > 0) {
          TRI_ASSERT(_chunksUsed < STATIC_ARRAY_SIZE);
          // move elements in array
          for (size_t i = _chunksUsed; i > 0; --i) {
            _chunksArray[i] = _chunksArray[i - 1];
          }
        }

        // finally insert chunk at head of static array
        _chunksArray[0] = chunk;
      }
    } else {
      // insert chunk into map
      _chunksHash.emplace(chunk);
    }
    
    ++_chunksUsed;
  }

  bool contains(RevisionCacheChunk* chunk) const {
    if (_chunksUsed > STATIC_ARRAY_SIZE) {
      // lookup chunk in map
      return _chunksHash.find(chunk) != _chunksHash.end();
    }

    // look up chunk in static array
    for (size_t i = 0; i < _chunksUsed; ++i) {
      if (_chunksArray[i] == chunk) {
        return true;
      }
    }
    return false;
  }
};

class ManagedDocumentResult {
 public:
  ManagedDocumentResult() = delete;
  ManagedDocumentResult(ManagedDocumentResult const& other) = delete;
  ManagedDocumentResult(ManagedDocumentResult&& other) = delete;
  ManagedDocumentResult& operator=(ManagedDocumentResult const& other);
  ManagedDocumentResult& operator=(ManagedDocumentResult&& other) = delete;

  explicit ManagedDocumentResult(Transaction*);
  ~ManagedDocumentResult();

  inline uint8_t const* vpack() const { 
    TRI_ASSERT(_vpack != nullptr); 
    return _vpack; 
  }
  
  void add(ChunkProtector& protector, TRI_voc_rid_t revisionId);
  void addExisting(ChunkProtector& protector, TRI_voc_rid_t revisionId);

  bool hasSeenChunk(RevisionCacheChunk* chunk) const { return _chunkCache.contains(chunk); }
  inline TRI_voc_rid_t lastRevisionId() const { return _lastRevisionId; }

  inline void setCache(TRI_voc_rid_t revisionId, uint8_t const* vpack) {
    _lastRevisionId = revisionId;
    _vpack = vpack;
  }

  //void clear();

 private:
  Transaction* _trx;
  uint8_t const* _vpack;
  TRI_voc_rid_t _lastRevisionId;
  ChunkCache _chunkCache;
};

}

#endif
