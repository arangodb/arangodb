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

#include "MMFilesCollectionKeys.h"
#include "Basics/StaticStrings.h"
#include "MMFiles/MMFilesCollection.h"
#include "MMFiles/MMFilesDitch.h"
#include "MMFiles/MMFilesEngine.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/ExecContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

MMFilesCollectionKeys::MMFilesCollectionKeys(TRI_vocbase_t& vocbase,
                                             std::unique_ptr<CollectionGuard> guard,
                                             TRI_voc_tick_t blockerId, double ttl)
    : CollectionKeys(&vocbase, ttl),
      _guard(std::move(guard)),
      _ditch(nullptr),
      _resolver(vocbase),
      _blockerId(blockerId) {
  TRI_ASSERT(_blockerId > 0);

  // prevent the collection from being unloaded while the export is ongoing
  // this may throw
  _collection = _guard->collection();
  TRI_ASSERT(_collection != nullptr);
}

MMFilesCollectionKeys::~MMFilesCollectionKeys() {
  // remove compaction blocker
  MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
  engine->removeCompactionBlocker(_vocbase, _blockerId);

  if (_ditch != nullptr) {
    _ditch->ditches()->freeMMFilesDocumentDitch(_ditch, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initially creates the list of keys
////////////////////////////////////////////////////////////////////////////////

void MMFilesCollectionKeys::create(TRI_voc_tick_t maxTick) {
  MMFilesLogfileManager::instance()->waitForCollectorQueue(_collection->id(), 30.0);
  MMFilesEngine* engine = static_cast<MMFilesEngine*>(EngineSelectorFeature::ENGINE);
  engine->preventCompaction(&(_collection->vocbase()), [this](TRI_vocbase_t* vocbase) {
    // create a ditch under the compaction lock
    _ditch = arangodb::MMFilesCollection::toMMFilesCollection(_collection)
                 ->ditches()
                 ->createMMFilesDocumentDitch(false, __FILE__, __LINE__);
  });

  // now we either have a ditch or not
  if (_ditch == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  _vpack.reserve(16384);

  // copy all document tokens into the result under the read-lock
  {
    auto ctx = transaction::StandaloneContext::Create(_collection->vocbase());
    SingleCollectionTransaction trx(ctx, *_collection, AccessMode::Type::READ);

    // already locked by _guard
    trx.addHint(transaction::Hints::Hint::NO_USAGE_LOCK);

    Result res = trx.begin();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    ManagedDocumentResult mdr;
    MMFilesCollection* mmColl = MMFilesCollection::toMMFilesCollection(_collection);

    trx.invokeOnAllElements(_collection->name(), [this, &trx, &maxTick, &mdr,
                                                  &mmColl](LocalDocumentId const& token) {
      if (mmColl->readDocumentConditional(&trx, token, maxTick, mdr)) {
        _vpack.emplace_back(mdr.vpack());
      }
      return true;
    });

    trx.finish(res);
  }

  // now sort all document tokens without the read-lock
  std::sort(_vpack.begin(), _vpack.end(), [](uint8_t const* lhs, uint8_t const* rhs) -> bool {
    return (arangodb::velocypack::StringRef(transaction::helpers::extractKeyFromDocument(VPackSlice(lhs))) <
            arangodb::velocypack::StringRef(transaction::helpers::extractKeyFromDocument(VPackSlice(rhs))));
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes a chunk of keys
////////////////////////////////////////////////////////////////////////////////

std::tuple<std::string, std::string, uint64_t> MMFilesCollectionKeys::hashChunk(size_t from,
                                                                                size_t to) const {
  if (from >= _vpack.size() || to > _vpack.size() || from >= to || to == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  VPackSlice first(_vpack.at(from));
  VPackSlice last(_vpack.at(to - 1));

  TRI_ASSERT(first.isObject());
  TRI_ASSERT(last.isObject());

  uint64_t hash = 0x012345678;

  for (size_t i = from; i < to; ++i) {
    VPackSlice current(_vpack.at(i));
    TRI_ASSERT(current.isObject());

    // we can get away with the fast hash function here, as key values are
    // restricted to strings
    hash ^= transaction::helpers::extractKeyFromDocument(current).hashString();
    hash ^= transaction::helpers::extractRevSliceFromDocument(current).hash();
  }

  return std::make_tuple(transaction::helpers::extractKeyFromDocument(first).copyString(),
                         transaction::helpers::extractKeyFromDocument(last).copyString(),
                         hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dumps keys into the result
////////////////////////////////////////////////////////////////////////////////

void MMFilesCollectionKeys::dumpKeys(VPackBuilder& result, size_t chunk, size_t chunkSize) const {
  size_t from = chunk * chunkSize;
  size_t to = (chunk + 1) * chunkSize;

  if (to > _vpack.size()) {
    to = _vpack.size();
  }

  if (from >= _vpack.size() || from >= to || to == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  for (size_t i = from; i < to; ++i) {
    VPackSlice current(_vpack.at(i));
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

void MMFilesCollectionKeys::dumpDocs(arangodb::velocypack::Builder& result,
                                     size_t chunk, size_t chunkSize, size_t offsetInChunk,
                                     size_t maxChunkSize, VPackSlice const& ids) const {
  if (!ids.isArray()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  auto buffer = result.buffer();
  size_t offset = 0;

  for (auto const& it : VPackArrayIterator(ids)) {
    if (!it.isNumber()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }

    size_t position = chunk * chunkSize + it.getNumber<size_t>();

    if (position >= _vpack.size()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }

    if (offset < offsetInChunk) {
      // skip over the initial few documents
      result.add(VPackValue(VPackValueType::Null));
    } else {
      VPackSlice current(_vpack.at(position));
      TRI_ASSERT(current.isObject());
      result.add(current);

      if (buffer->byteSize() > maxChunkSize) {
        // buffer is full
        break;
      }
    }
    ++offset;
  }
}
