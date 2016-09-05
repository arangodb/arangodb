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

#include "CollectionKeys.h"
#include "Basics/StaticStrings.h"
#include "Basics/WriteLocker.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/CollectionGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/Ditch.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/MasterPointer.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "Wal/LogfileManager.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

CollectionKeys::CollectionKeys(TRI_vocbase_t* vocbase, std::string const& name,
                               TRI_voc_tick_t blockerId, double ttl)
    : _vocbase(vocbase),
      _guard(nullptr),
      _collection(nullptr),
      _ditch(nullptr),
      _name(name),
      _resolver(vocbase),
      _blockerId(blockerId),
      _markers(nullptr),
      _id(0),
      _ttl(ttl),
      _expires(0.0),
      _isDeleted(false),
      _isUsed(false) {
  _id = TRI_NewTickServer();
  _expires = TRI_microtime() + _ttl;
  TRI_ASSERT(_blockerId > 0);

  // prevent the collection from being unloaded while the export is ongoing
  // this may throw
  _guard = new arangodb::CollectionGuard(vocbase, _name.c_str(), false);

  _collection = _guard->collection();
  TRI_ASSERT(_collection != nullptr);
}

CollectionKeys::~CollectionKeys() {
  // remove compaction blocker
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->removeCompactionBlocker(_vocbase, _blockerId);

  delete _markers;

  if (_ditch != nullptr) {
    _ditch->ditches()->freeDocumentDitch(_ditch, false);
  }

  delete _guard;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initially creates the list of keys
////////////////////////////////////////////////////////////////////////////////

void CollectionKeys::create(TRI_voc_tick_t maxTick) {
  arangodb::wal::LogfileManager::instance()->waitForCollectorQueue(
      _collection->cid(), 30.0);
  
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->preventCompaction(_collection->vocbase(), [this](TRI_vocbase_t* vocbase) {
    // create a ditch under the compaction lock
    _ditch = _collection->ditches()->createDocumentDitch(false, __FILE__, __LINE__);
  });

  // now we either have a ditch or not
  if (_ditch == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_ASSERT(_markers == nullptr);
  _markers = new std::vector<void const*>();
  _markers->reserve(16384);

  // copy all datafile markers into the result under the read-lock
  {
    SingleCollectionTransaction trx(
        StandaloneTransactionContext::Create(_collection->vocbase()), _name,
        TRI_TRANSACTION_READ);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    trx.invokeOnAllElements(
        _collection->name(), [this, &maxTick](TRI_doc_mptr_t const* mptr) {
          // only use those markers that point into datafiles
          if (!mptr->pointsToWal()) {
            TRI_df_marker_t const* marker = mptr->getMarkerPtr();

            if (marker->getTick() <= maxTick) {
              _markers->emplace_back(mptr->vpack());
            }
          }

          return true;
        });

    trx.finish(res);
  }

  // now sort all markers without the read-lock
  std::sort(_markers->begin(), _markers->end(),
            [](void const* lhs, void const* rhs) -> bool {
    VPackSlice l(reinterpret_cast<char const*>(lhs));
    VPackSlice r(reinterpret_cast<char const*>(rhs));

    return (l.get(StaticStrings::KeyString).copyString() <
            r.get(StaticStrings::KeyString).copyString());
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes a chunk of keys
////////////////////////////////////////////////////////////////////////////////

std::tuple<std::string, std::string, uint64_t> CollectionKeys::hashChunk(
    size_t from, size_t to) const {
  if (from >= _markers->size() || to > _markers->size() || from >= to ||
      to == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  VPackSlice first(reinterpret_cast<char const*>(_markers->at(from)));
  VPackSlice last(reinterpret_cast<char const*>(_markers->at(to - 1)));

  TRI_ASSERT(first.isObject());
  TRI_ASSERT(last.isObject());

  uint64_t hash = 0x012345678;

  for (size_t i = from; i < to; ++i) {
    VPackSlice current(reinterpret_cast<char const*>(_markers->at(i)));
    TRI_ASSERT(current.isObject());

    // we can get away with the fast hash function here, as key values are 
    // restricted to strings
    hash ^= current.get(StaticStrings::KeyString).hash();
    hash ^= current.get(StaticStrings::RevString).hash();
  }

  return std::make_tuple(
    first.get(StaticStrings::KeyString).copyString(), 
    last.get(StaticStrings::KeyString).copyString(), 
    hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dumps keys into the result
////////////////////////////////////////////////////////////////////////////////

void CollectionKeys::dumpKeys(VPackBuilder& result, size_t chunk,
                              size_t chunkSize) const {
  size_t from = chunk * chunkSize;
  size_t to = (chunk + 1) * chunkSize;

  if (to > _markers->size()) {
    to = _markers->size();
  }

  if (from >= _markers->size() || from >= to || to == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  
  for (size_t i = from; i < to; ++i) {
    VPackSlice current(reinterpret_cast<char const*>(_markers->at(i)));
    TRI_ASSERT(current.isObject());

    result.openArray();
    result.add(current.get(StaticStrings::KeyString));
    result.add(current.get(StaticStrings::RevString));
    result.close();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dumps documents into the result
////////////////////////////////////////////////////////////////////////////////

void CollectionKeys::dumpDocs(arangodb::velocypack::Builder& result, size_t chunk,
                              size_t chunkSize, VPackSlice const& ids) const {
  if (!ids.isArray()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  for (auto const& it : VPackArrayIterator(ids)) {
    if (!it.isNumber()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }

    size_t position = chunk * chunkSize + it.getNumber<size_t>();

    if (position >= _markers->size()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }
    
    VPackSlice current(reinterpret_cast<char const*>(_markers->at(position)));
    TRI_ASSERT(current.isObject());
  
    result.add(current);
  }
}

