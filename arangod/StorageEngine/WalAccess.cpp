////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ScopeGuard.h"
#include "Replication/common-defines.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

/// @brief check if db should be handled, might already be deleted
bool WalAccessContext::shouldHandleDB(TRI_voc_tick_t dbid) const {
  return _filter.vocbase == 0 || _filter.vocbase == dbid;
}

/// @brief check if view should be handled, might already be deleted
bool WalAccessContext::shouldHandleView(TRI_voc_tick_t dbid,
                                        DataSourceId vid) const {
  if (dbid == 0 || vid.empty() || !shouldHandleDB(dbid)) {
    return false;
  }

  if (_filter.vocbase == 0 ||
      (_filter.vocbase == dbid &&
       (_filter.collection.empty() || _filter.collection == vid))) {
    return true;
  }
  return false;
}

/// @brief Check if collection is in filter, will load collection
bool WalAccessContext::shouldHandleCollection(TRI_voc_tick_t dbid,
                                              DataSourceId cid) {
  if (dbid == 0 || cid.empty() || !shouldHandleDB(dbid)) {
    return false;
  }
  if (_filter.vocbase == 0 ||
      (_filter.vocbase == dbid &&
       (_filter.collection.empty() || _filter.collection == cid))) {
    LogicalCollection* collection = loadCollection(dbid, cid);
    if (collection == nullptr) {
      return false;
    }
    return !TRI_ExcludeCollectionReplication(
        collection->name(), _filter.includeSystem, _filter.includeFoxxQueues);
  }
  return false;
}

/// @brief try to get collection, may return null
TRI_vocbase_t* WalAccessContext::loadVocbase(TRI_voc_tick_t dbid) {
  TRI_ASSERT(dbid != 0);

  if (auto const it = _vocbases.find(dbid); it == _vocbases.end()) {
    if (auto vocbase = _server.getFeature<DatabaseFeature>().useDatabase(dbid);
        vocbase != nullptr) {
      auto& [_, db] = *_vocbases.try_emplace(dbid, std::move(vocbase)).first;
      return db.get();
    }
    return nullptr;
  } else {
    return it->second.get();
  }
}

LogicalCollection* WalAccessContext::loadCollection(TRI_voc_tick_t dbid,
                                                    DataSourceId cid) {
  TRI_ASSERT(dbid != 0);
  TRI_ASSERT(cid.isSet());
  TRI_vocbase_t* vocbase = loadVocbase(dbid);
  if (vocbase != nullptr) {
    try {
      auto it = _collectionCache.try_emplace(cid, CollectionGuard(vocbase, cid))
                    .first;
      return it->second.collection();
    } catch (...) {
      // weglaecheln
    }
  }
  return nullptr;
}
