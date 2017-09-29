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

#include "GlobalInitialSyncer.h"
#include "Basics/Result.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "RestServer/DatabaseFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"
#include "VocBase/Methods/Databases.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

GlobalInitialSyncer::GlobalInitialSyncer(
  ReplicationApplierConfiguration const* configuration,
  std::unordered_map<std::string, bool> const& restrictCollections,
  Syncer::RestrictType restrictType, bool verbose, bool skipCreateDrop)
: InitialSyncer(configuration, restrictCollections, restrictType, verbose, skipCreateDrop)  {

}

GlobalInitialSyncer::~GlobalInitialSyncer() {
  try {
    sendFinishBatch();
  } catch(...) {}
}

/// @brief run method, performs a full synchronization
Result GlobalInitialSyncer::run(bool incremental) {
  if (_client == nullptr || _connection == nullptr || _endpoint == nullptr) {
    return Result(TRI_ERROR_INTERNAL, "invalid endpoint");
  }
  
  setProgress("fetching master state");
  
  std::string errorMsg;
  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "client: getting master state";
  int res = getMasterState(errorMsg);
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION)
      << "client: got master state: " << res << " " << errorMsg;
    return res;
  }
  LOG_TOPIC(DEBUG, Logger::REPLICATION)
    << "client: got master state: " << res << " " << errorMsg;
  
  if (_masterInfo._majorVersion > 3 ||
      (_masterInfo._majorVersion == 3 && _masterInfo._minorVersion < 3)) {
    char const* msg = "global replication is not supported with a master <  ArangoDB 3.3";
    LOG_TOPIC(WARN, Logger::REPLICATION) << msg;
    return Result(TRI_ERROR_INTERNAL, msg);
  }
  
  // create a WAL logfile barrier that prevents WAL logfile collection
  res = sendCreateBarrier(errorMsg, _masterInfo._lastLogTick);
  if (res != TRI_ERROR_NO_ERROR) {
    return Result(res, errorMsg);
  }
  TRI_DEFER(sendRemoveBarrier());

  // start batch is required for the inventory request
  res = sendStartBatch(errorMsg);
  if (res != TRI_ERROR_NO_ERROR) {
    return Result(res, errorMsg);
  }
  TRI_DEFER(sendFinishBatch());
  
  VPackBuilder builder;
  res = fetchInventory(builder, errorMsg);
  if (res != TRI_ERROR_NO_ERROR) {
    return Result(res, errorMsg);
  }
  
  VPackSlice const databases = builder.slice().get("databases");
  VPackSlice const state = builder.slice().get("databases");
  if (!databases.isArray() || !state.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "database section or state is missing from response");
  }
  
  if (!_skipCreateDrop) {
    Result r = updateServerInventory(databases);
    if (r.fail()) {
      return r;
    }
  }
  
  try {
  
    // actually sync the database
    for (VPackSlice it : VPackArrayIterator(databases)) {
      if (!it.isObject()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      "database declaration is invalid in response");
      }
      
      VPackSlice const nameSlice = it.get("name");
      VPackSlice const idSlice = it.get("id");
      VPackSlice const collections = it.get("collections");
      if (!nameSlice.isString() ||
          !idSlice.isString() ||
          !collections.isArray()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      "database declaration is invalid in response");
      }
      std::string const dbName = nameSlice.copyString();
      TRI_voc_tick_t dbId = StringUtils::uint64(idSlice.copyString());
      if (dbId == 0) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      "database declaration contains invalid database Id");
      }
      
      TRI_vocbase_t* vocbase = loadVocbase(dbId);
      TRI_ASSERT(vocbase != nullptr);
      if (vocbase == nullptr) {
        return Result(TRI_ERROR_INTERNAL, "vocbase not found");
      }
      DatabaseInitialSyncer syncer(vocbase, &_configuration, _restrictCollections,
                                   _restrictType, _verbose, _skipCreateDrop);
      
      syncer.useAsChildSyncer(_barrierId, _barrierUpdateTime,
                              _batchId, _batchUpdateTime);
      
      
      res = syncer.run(errorMsg, false);
    
      // we need to pass on the update times to the next syncer
      _barrierUpdateTime = syncer.barrierUpdateTime();
      _batchUpdateTime = syncer.batchUpdateTime();
    
      sendExtendBatch();
      sendExtendBarrier();
      
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
  
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "caught an unexpected exception");
  }
  
  return TRI_ERROR_NO_ERROR;
}

/// @brief add or remove databases such that the local inventory mirrors the masters
Result GlobalInitialSyncer::updateServerInventory(VPackSlice const& masterDatabases) {
  
  std::set<TRI_voc_tick_t> existingDBs;
  DatabaseFeature::DATABASE->enumerateDatabases([&](TRI_vocbase_t* vocbase) {
    existingDBs.insert(vocbase->id());
  });
  
  for (VPackSlice it : VPackArrayIterator(masterDatabases)) {
    if (!it.isObject()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "database declaration is invalid in response");
    }
    
    VPackSlice const nameSlice = it.get("name");
    VPackSlice const idSlice = it.get("id");
    VPackSlice const collections = it.get("collections");
    if (!nameSlice.isString() ||
        !idSlice.isString() ||
        !collections.isArray()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "database declaration is invalid in response");
    }
    std::string const dbName = nameSlice.copyString();
    TRI_voc_tick_t dbId = StringUtils::uint64(idSlice.copyString());
    if (dbId == 0) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "database declaration contains invalid database Id");
    }
    
    TRI_vocbase_t* vocbase = loadVocbase(dbId);
    if (vocbase == nullptr) {
      // database is missing we need to create it now
      
      VPackBuilder optionsBuilder;
      optionsBuilder.openObject();
      optionsBuilder.add("id", VPackValue((uint64_t)dbId));
      optionsBuilder.close();
      Result r = methods::Databases::create(dbName, VPackSlice::emptyArraySlice(),
                                            optionsBuilder.slice());
      if (r.fail()) {
        LOG_TOPIC(WARN, Logger::REPLICATION) << "Creating the db failed on replicant";
        return r;
      }
      vocbase = loadVocbase(dbId);
      TRI_ASSERT(vocbase != nullptr); // must be loaded now
      if (vocbase == nullptr) {
        char const* msg = "DB was created with wrong id on replicant";
        LOG_TOPIC(WARN, Logger::REPLICATION) << msg;
        return Result(TRI_ERROR_INTERNAL, msg);
      }
    }
    existingDBs.erase(dbId); // remove dbs that exists on the master
    
    sendExtendBatch();
    sendExtendBarrier();
  }
  
  // all dbs left in this list no longer exist on the master
  for (TRI_voc_tick_t dbId : existingDBs) {
    _vocbaseCache.erase(dbId); // make sure to release the db first
    
    Result r = methods::Databases::dropLocal(dbId);
    if (r.fail()) {
      LOG_TOPIC(WARN, Logger::REPLICATION) << "Dropping db failed on replicant";
      return r;
    }
    
    sendExtendBatch();
    sendExtendBarrier();
  }
  
  return TRI_ERROR_NO_ERROR;
}

/*
void GlobalInitialSyncer::setProgress(std::string const& msg) {
  _progress = msg;
  
  if (_verbose) {
    LOG_TOPIC(INFO, Logger::REPLICATION) << msg;
  } else {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << msg;
  }
  
  TRI_replication_applier_t* applier = vocbase()->replicationApplier();
  if (applier != nullptr) {
    applier->setProgress(msg.c_str(), true);
  }
}*/

int GlobalInitialSyncer::fetchInventory(VPackBuilder& builder,
                                        std::string& errorMsg) {
  
  std::string url = BaseUrl + "/inventory?serverId=" + _localServerIdString +
  "&batchId=" + std::to_string(_batchId) + "&global=true";
  if (_includeSystem) {
    url += "&includeSystem=true";
  }
  
  // send request
  std::string const progress = "fetching master inventory from " + url;
  setProgress(progress);
  
  std::unique_ptr<SimpleHttpResult> response(
                                             _client->retryRequest(rest::RequestType::GET, url, nullptr, 0));
  
  if (response == nullptr || !response->isComplete()) {
    errorMsg = "could not connect to master at " + _masterInfo._endpoint +
    ": " + _client->getErrorMessage();
    
    sendFinishBatch();
    
    return TRI_ERROR_REPLICATION_NO_RESPONSE;
  }
  
  TRI_ASSERT(response != nullptr);
  
  if (response->wasHttpError()) {
    errorMsg = "got invalid response from master at " +
    _masterInfo._endpoint + ": HTTP " +
    StringUtils::itoa(response->getHttpReturnCode()) + ": " +
    response->getHttpReturnMessage();
    return TRI_ERROR_REPLICATION_MASTER_ERROR;
  }
  int res = parseResponse(builder, response.get());
  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg = "got invalid response from master at " +
    std::string(_masterInfo._endpoint) +
    ": invalid response type for initial data. expecting array";
    
    return res;
  }
  
  VPackSlice const slice = builder.slice();
  if (!slice.isObject()) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "client: InitialSyncer::run - "
    "inventoryResponse is not an "
    "object";
    
    errorMsg = "got invalid response from master at " +
    _masterInfo._endpoint + ": invalid JSON";
    return TRI_ERROR_REPLICATION_INVALID_RESPONSE;
  }
  return res;
}

