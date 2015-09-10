////////////////////////////////////////////////////////////////////////////////
/// @brief collection keys container
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Utils/CollectionKeys.h"
#include "Basics/hashes.h"
#include "Basics/JsonHelper.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionReadLocker.h"
#include "Utils/transactions.h"
#include "VocBase/compactor.h"
#include "VocBase/Ditch.h"
#include "VocBase/vocbase.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                              class CollectionKeys
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

CollectionKeys::CollectionKeys (TRI_vocbase_t* vocbase,
                                std::string const& name,
                                TRI_voc_tick_t blockerId,
                                double ttl)
  : _vocbase(vocbase),
    _guard(nullptr),
    _document(nullptr),
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
  _guard = new triagens::arango::CollectionGuard(vocbase, _name.c_str(), false);
  
  _document = _guard->collection()->_collection;
  TRI_ASSERT(_document != nullptr);
}

CollectionKeys::~CollectionKeys () {
  // remove compaction blocker
  TRI_RemoveBlockerCompactorVocBase(_vocbase, _blockerId);

  delete _markers;

  if (_ditch != nullptr) {
    _ditch->ditches()->freeDocumentDitch(_ditch, false);
  }

  delete _guard;
}
        
// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initially creates the list of keys
////////////////////////////////////////////////////////////////////////////////

void CollectionKeys::create (TRI_voc_tick_t maxTick) {
  // try to acquire the exclusive lock on the compaction
  while (! TRI_CheckAndLockCompactorVocBase(_document->_vocbase)) {
    // didn't get it. try again...
    usleep(5000);
  }
 
  // create a ditch under the compaction lock 
  _ditch = _document->ditches()->createDocumentDitch(false, __FILE__, __LINE__);
  
  // release the lock
  TRI_UnlockCompactorVocBase(_document->_vocbase);

  // now we either have a ditch or not
  if (_ditch == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }


  TRI_ASSERT(_markers == nullptr);
  _markers = new std::vector<TRI_df_marker_t const*>();

  // copy all datafile markers into the result under the read-lock 
  {
    SingleCollectionReadOnlyTransaction trx(new StandaloneTransactionContext(), _document->_vocbase, _name);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    auto idx = _document->primaryIndex();
    _markers->reserve(idx->size());

    triagens::basics::BucketPosition position;

    uint64_t total = 0;
    while (true) {
      auto ptr = idx->lookupSequential(position, total);

      if (ptr == nullptr) {
        // done
        break;
      }

      void const* marker = ptr->getDataPtr();

      if (TRI_IsWalDataMarkerDatafile(marker)) {
        continue;
      }
      
      auto df = static_cast<TRI_df_marker_t const*>(marker);

      if (df->_tick >= maxTick) {
        continue;
      }

      _markers->emplace_back(df);
    }

    trx.finish(res);
  }

  // now sort all markers without the read-lock
  std::sort(_markers->begin(), _markers->end(), [] (TRI_df_marker_t const* lhs, TRI_df_marker_t const* rhs) -> bool {
    int res = strcmp(TRI_EXTRACT_MARKER_KEY(lhs), TRI_EXTRACT_MARKER_KEY(rhs));

    return res < 0;
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes a chunk of keys
////////////////////////////////////////////////////////////////////////////////

std::tuple<std::string, std::string, uint64_t> CollectionKeys::hashChunk (size_t from, size_t to) const {
  if (from >= _markers->size() || to > _markers->size() || from >= to || to == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  std::string const first = TRI_EXTRACT_MARKER_KEY(_markers->at(from));
  std::string const last  = TRI_EXTRACT_MARKER_KEY(_markers->at(to - 1));

  uint64_t hash = 0x012345678;

  for (size_t i = from; i < to; ++i) {
    auto marker = _markers->at(i);
    char const* key = TRI_EXTRACT_MARKER_KEY(marker);

    hash ^= TRI_FnvHashString(key);
    hash ^= TRI_EXTRACT_MARKER_RID(marker);
  }
  
  return std::make_tuple(first, last, hash);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
