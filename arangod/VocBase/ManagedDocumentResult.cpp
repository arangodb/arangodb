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

#include "ManagedDocumentResult.h"
#include "Logger/Logger.h"
#include "Utils/Transaction.h"

using namespace arangodb;

ManagedDocumentResult::ManagedDocumentResult(Transaction* trx) 
        : _trx(trx), _vpack(nullptr), _lastRevisionId(0) {}

ManagedDocumentResult::~ManagedDocumentResult() {
//  clear();
}
  
//static std::atomic<uint64_t> ADDCALLS(0);
void ManagedDocumentResult::add(ChunkProtector& protector, TRI_voc_rid_t revisionId) {
//  if (++ADDCALLS % 10000 == 0) {
//    LOG(ERR) << ADDCALLS.load() << " CALLS TO ADD";
//  }
  /*
  if (protector.chunk() != _chunk) {
    clear();
  }
  
  _vpack = protector.vpack();
  _chunk = protector.chunk();
  */
  if (_trx != nullptr) {
    _trx->addChunk(protector.chunk());
    protector.steal();
  }
  _vpack = protector.vpack();
  _lastRevisionId = revisionId;
  _chunkCache.add(protector.chunk());
}

//static std::atomic<uint64_t> ADDEXISTINGCALLS(0);
void ManagedDocumentResult::addExisting(ChunkProtector& protector, TRI_voc_rid_t revisionId) {
//  if (++ADDEXISTINGCALLS % 10000 == 0) {
//    LOG(ERR) << ADDEXISTINGCALLS.load() << " CALLS TO ADDEXISTING";
//  }
  /*
  if (protector.chunk() != _chunk) {
    clear();
  }
  
  _vpack = protector.vpack();
  _chunk = protector.chunk();
  */
  _vpack = protector.vpack();
  _lastRevisionId = revisionId;
}

void ManagedDocumentResult::clear(size_t threshold) {
  if (_trx != nullptr) {
    _trx->clearChunks(threshold);
  }
  _chunkCache.clear();
  setCache(0, nullptr);
}
  
ManagedDocumentResult& ManagedDocumentResult::operator=(ManagedDocumentResult const& other) {
  if (this != &other) {
  //  clear();
    _trx = other._trx;
    _vpack = other._vpack;
    _chunkCache = other._chunkCache;
    _lastRevisionId = 0; // clear cache
  //  _chunk->use();
  }
  return *this;
}

