////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "WalAccess.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

bool WalAccessContext::shouldHandleDB(TRI_voc_tick_t dbid) const {
  return _filter.vocbase == 0 || _filter.vocbase == dbid;
}

/// @brief Check if collection is in filter
bool WalAccessContext::shouldHandleCollection(TRI_voc_tick_t dbid,
                                              TRI_voc_cid_t cid) const {
  return _filter.vocbase == 0 || (_filter.vocbase == dbid &&
         (_filter.collection == 0 || _filter.collection == cid));
}

/// @brief try to get collection, may return null
TRI_vocbase_t* WalAccessContext::loadVocbase(TRI_voc_tick_t dbid) {
  TRI_ASSERT(dbid != 0);
  auto const& it = _vocbases.find(dbid);
  if (it == _vocbases.end()) {
    TRI_vocbase_t* vocbase = DatabaseFeature::DATABASE->useDatabase(dbid);
    if (vocbase != nullptr) {
      TRI_DEFER(vocbase->release());
      _vocbases.emplace(dbid, DatabaseGuard(vocbase));
    }
    return vocbase;
  } else {
    return it->second.database();
  }
}

LogicalCollection* WalAccessContext::loadCollection(TRI_voc_tick_t dbid,
                                                    TRI_voc_cid_t cid) {
  TRI_ASSERT(dbid != 0);
  TRI_ASSERT(cid != 0);
  TRI_vocbase_t* vocbase = loadVocbase(dbid);
  if (vocbase != nullptr) {
    auto const& it = _collectionCache.find(cid);
    if (it != _collectionCache.end()) {
      return it->second.collection();
    }
    LogicalCollection* collection = vocbase->lookupCollection(cid);
    if (collection != nullptr) {
      _collectionCache.emplace(cid, CollectionGuard(vocbase, collection));
      return collection;
    }
  }
  return nullptr;
}
