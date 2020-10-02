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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

GlobalInitialSyncer::GlobalInitialSyncer(ReplicationApplierConfiguration const& configuration)
    : InitialSyncer(configuration) {
  // has to be set here, otherwise broken
  _state.databaseName = StaticStrings::SystemDatabase;
}

GlobalInitialSyncer::~GlobalInitialSyncer() {
  try {
    if (!_state.isChildSyncer) {
      _batch.finish(_state.connection, _progress, _state.syncerId);
    }
  } catch (...) {
  }
}

/// @brief run method, performs a full synchronization
/// public method, catches exceptions
Result GlobalInitialSyncer::run(bool incremental, char const* context) {
  try {
    return runInternal(incremental, context);
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(),
                  std::string("initial synchronization for database '") +
                      _state.databaseName + "' failed with exception: " + ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL,
                  std::string("initial synchronization for database '") +
                      _state.databaseName + "' failed with exception: " + ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL,
                  std::string("initial synchronization for database '") +
                      _state.databaseName + "' failed with unknown exception");
  }
}

/// @brief run method, performs a full synchronization
/// internal method, may throw exceptions
Result GlobalInitialSyncer::runInternal(bool incremental, char const* context) {
  if (!_state.connection.valid()) {
    return Result(TRI_ERROR_INTERNAL, "invalid endpoint");
  } else if (_state.applier._server.isStopping()) {
    return Result(TRI_ERROR_SHUTTING_DOWN);
  }

  setAborted(false);

  LOG_TOPIC("23d92", DEBUG, Logger::REPLICATION) << "client: getting master state";
  Result r = _state.master.getState(_state.connection, _state.isChildSyncer, context);
  if (r.fail()) {
    return r;
  }

  if (_state.master.majorVersion < 3 ||
      (_state.master.majorVersion == 3 && _state.master.minorVersion < 3)) {
    char const* msg =
        "global replication is not supported with a master < ArangoDB 3.3";
    LOG_TOPIC("57394", WARN, Logger::REPLICATION) << msg;
    return Result(TRI_ERROR_INTERNAL, msg);
  }

  if (!_state.isChildSyncer) {
    // create a WAL logfile barrier that prevents WAL logfile collection
    r = _state.barrier.create(_state.connection, _state.master.lastLogTick);
    if (r.fail()) {
      return r;
    }
  }

  LOG_TOPIC("0bf0e", DEBUG, Logger::REPLICATION) << "created logfile barrier";
  TRI_DEFER(
      if (!_state.isChildSyncer) { _state.barrier.remove(_state.connection); });

  if (!_state.isChildSyncer) {
    // start batch is required for the inventory request
    LOG_TOPIC("0da14", DEBUG, Logger::REPLICATION) << "sending start batch";
    r = _batch.start(_state.connection, _progress, _state.master, _state.syncerId);
    if (r.fail()) {
      return r;
    }

    startRecurringBatchExtension();
  }
  TRI_DEFER(if (!_state.isChildSyncer) {
    _batchPingTimer.reset();
    _batch.finish(_state.connection, _progress, _state.syncerId);
  });
  LOG_TOPIC("62fb5", DEBUG, Logger::REPLICATION) << "sending start batch done";

  VPackBuilder builder;
  LOG_TOPIC("c7021", DEBUG, Logger::REPLICATION) << "fetching inventory";
  r = fetchInventory(builder);
  LOG_TOPIC("1fe0b", DEBUG, Logger::REPLICATION) << "inventory done: " << r.errorNumber();
  if (r.fail()) {
    return r;
  }

  LOG_TOPIC("1bd5b", DEBUG, Logger::REPLICATION) << "inventory: " << builder.slice().toJson();
  VPackSlice const databases = builder.slice().get("databases");
  VPackSlice const state = builder.slice().get("state");
  if (!databases.isObject() || !state.isObject()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  "database section or state section is missing from response "
                  "or is invalid");
  }

  if (!_state.applier._skipCreateDrop) {
    LOG_TOPIC("af241", DEBUG, Logger::REPLICATION) << "updating server inventory";
    r = updateServerInventory(databases);
    if (r.fail()) {
      LOG_TOPIC("5fc1c", DEBUG, Logger::REPLICATION)
          << "updating server inventory failed";
      return r;
    }
  }

  LOG_TOPIC("d7e85", DEBUG, Logger::REPLICATION) << "databases: " << databases.toJson();

  try {
    // actually sync the database
    for (auto const& dbEntry : VPackObjectIterator(databases)) {
      if (_state.applier._server.isStopping()) {
        return Result(TRI_ERROR_SHUTTING_DOWN);
      } else if (isAborted()) {
        return Result(TRI_ERROR_REPLICATION_APPLIER_STOPPED);
      }

      VPackSlice dbInventory = dbEntry.value;
      if (!dbInventory.isObject()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      "database declaration is invalid in response");
      }

      VPackSlice const nameSlice = dbInventory.get("name");
      VPackSlice const idSlice = dbInventory.get("id");
      VPackSlice const collections = dbInventory.get("collections");
      if (!nameSlice.isString() || !idSlice.isString() || !collections.isArray()) {
        return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                      "database declaration is invalid in response");
      }

      TRI_vocbase_t* vocbase = resolveVocbase(nameSlice);

      if (vocbase == nullptr) {
        return Result(TRI_ERROR_INTERNAL, "vocbase not found");
      }

      DatabaseGuard guard(nameSlice.copyString());

      // change database name in place
      ReplicationApplierConfiguration configurationCopy = _state.applier;
      configurationCopy._database = nameSlice.copyString();

      auto syncer = std::make_shared<DatabaseInitialSyncer>(*vocbase, configurationCopy);
      syncer->useAsChildSyncer(_state.master, _state.syncerId, _state.barrier.id,
                               _state.barrier.updateTime, _batch.id, _batch.updateTime);

      // run the syncer with the supplied inventory collections
      r = syncer->runWithInventory(incremental, dbInventory);
      if (r.fail()) {
        return r;
      }

      // we need to pass on the update times to the next syncer
      _state.barrier.updateTime = syncer->barrierUpdateTime();
      _batch.updateTime = syncer->batchUpdateTime();

      if (!_state.isChildSyncer) {
        _batch.extend(_state.connection, _progress, _state.syncerId);
        _state.barrier.extend(_state.connection);
      }
    }
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), std::string("syncer caught an unexpected exception: ") + ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, std::string("syncer caught an unexpected exception: ") + ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "syncer caught an unexpected exception");
  }

  return Result();
}

/// @brief add or remove databases such that the local inventory
/// mirrors the masters
Result GlobalInitialSyncer::updateServerInventory(VPackSlice const& masterDatabases) {
  std::set<std::string> existingDBs;
  DatabaseFeature::DATABASE->enumerateDatabases(
      [&](TRI_vocbase_t& vocbase) -> void { existingDBs.insert(vocbase.name()); });

  for (auto const& database : VPackObjectIterator(masterDatabases)) {
    VPackSlice it = database.value;

    if (!it.isObject()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "database declaration is invalid in response");
    }

    VPackSlice const nameSlice = it.get("name");
    VPackSlice const idSlice = it.get("id");
    VPackSlice const collections = it.get("collections");
    if (!nameSlice.isString() || !idSlice.isString() || !collections.isArray()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                    "database declaration is invalid in response");
    }

    std::string const dbName = nameSlice.copyString();
    TRI_vocbase_t* vocbase = resolveVocbase(nameSlice);

    if (vocbase == nullptr) {
      // database is missing. we need to create it now
      Result r = methods::Databases::create(_state.applier._server, dbName,
                                            VPackSlice::emptyArraySlice(),
                                            VPackSlice::emptyObjectSlice());
      if (r.fail()) {
        LOG_TOPIC("cf124", WARN, Logger::REPLICATION)
            << "Creating the db failed on replicant";
        return r;
      }
      vocbase = resolveVocbase(nameSlice);
      TRI_ASSERT(vocbase != nullptr);  // must be loaded now
      if (vocbase == nullptr) {
        char const* msg = "DB was created with wrong id on replicant";
        LOG_TOPIC("a3b6f", WARN, Logger::REPLICATION) << msg;
        return Result(TRI_ERROR_INTERNAL, msg);
      }
    } else {
      // database already exists. now check which collections should survive
      std::unordered_set<std::string> survivingCollections;

      for (auto const& coll : VPackArrayIterator(collections)) {
        if (!coll.isObject() || !coll.hasKey("parameters")) {
          continue;  // somehow invalid
        }

        VPackSlice const params = coll.get("parameters");
        auto existingCollection = resolveCollection(*vocbase, params);

        if (existingCollection != nullptr) {
          survivingCollections.emplace(existingCollection->guid());
        }
      }

      std::vector<arangodb::LogicalCollection*> toDrop;

      // drop all collections that do not exist (anymore) on the master
      vocbase->processCollections(
          [&survivingCollections, &toDrop](arangodb::LogicalCollection* collection) {
            if (survivingCollections.find(collection->guid()) !=
                survivingCollections.end()) {
              // collection should surive
              return;
            }
            if (!collection->system()) {  // we will not drop system collections here
              toDrop.emplace_back(collection);
            }
          },
          false);

      for (auto const& collection : toDrop) {
        try {
          auto res = vocbase->dropCollection(collection->id(), false, -1.0).errorNumber();

          if (res != TRI_ERROR_NO_ERROR) {
            LOG_TOPIC("f04bb", ERR, Logger::REPLICATION)
                << "unable to drop collection " << collection->name() << ": "
                << TRI_errno_string(res);
          }
        } catch (...) {
          LOG_TOPIC("69fc4", ERR, Logger::REPLICATION)
              << "unable to drop collection " << collection->name();
        }
      }
    }

    existingDBs.erase(dbName);  // remove dbs that exists on the master

    if (!_state.isChildSyncer) {
      _batch.extend(_state.connection, _progress, _state.syncerId);
      _state.barrier.extend(_state.connection);
    }
  }

  // all dbs left in this list no longer exist on the master
  for (std::string const& dbname : existingDBs) {
    _state.vocbases.erase(dbname);  // make sure to release the db first

    auto r = _state.applier._server.hasFeature<arangodb::SystemDatabaseFeature>()
                 ? methods::Databases::drop(_state.applier._server
                                                .getFeature<arangodb::SystemDatabaseFeature>()
                                                .use()
                                                .get(),
                                            dbname)
                 : arangodb::Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);

    if (r.fail()) {
      LOG_TOPIC("0a282", WARN, Logger::REPLICATION) << "Dropping db failed on replicant";
      return r;
    }

    if (!_state.isChildSyncer) {
      _batch.extend(_state.connection, _progress, _state.syncerId);
      _state.barrier.extend(_state.connection);
    }
  }

  return Result();
}

/// @brief fetch the server's inventory, public method for TailingSyncer
Result GlobalInitialSyncer::getInventory(VPackBuilder& builder) {
  if (!_state.connection.valid()) {
    return Result(TRI_ERROR_INTERNAL, "invalid endpoint");
  } else if (_state.applier._server.isStopping()) {
    return Result(TRI_ERROR_SHUTTING_DOWN);
  }

  auto r = _batch.start(_state.connection, _progress, _state.master, _state.syncerId);
  if (r.fail()) {
    return r;
  }

  TRI_DEFER(_batch.finish(_state.connection, _progress, _state.syncerId));

  // caller did not supply an inventory, we need to fetch it
  return fetchInventory(builder);
}

Result GlobalInitialSyncer::fetchInventory(VPackBuilder& builder) {
  std::string url = replutils::ReplicationUrl +
                    "/inventory?serverId=" + _state.localServerIdString +
                    "&batchId=" + std::to_string(_batch.id) + "&global=true";
  if (_state.applier._includeSystem) {
    url += "&includeSystem=true";
  }
  if (_state.applier._includeFoxxQueues) {
    url += "&includeFoxxQueues=true";
  }

  // send request
  std::unique_ptr<httpclient::SimpleHttpResult> response;
  _state.connection.lease([&](httpclient::SimpleHttpClient* client) {
    response.reset(client->retryRequest(rest::RequestType::GET, url, nullptr, 0));
  });

  if (replutils::hasFailed(response.get())) {
    if (!_state.isChildSyncer) {
      _batch.finish(_state.connection, _progress, _state.syncerId);
    }
    return replutils::buildHttpError(response.get(), url, _state.connection);
  }

  Result r = replutils::parseResponse(builder, response.get());

  if (r.fail()) {
    return Result(
        r.errorNumber(),
        std::string("got invalid response from master at ") + _state.master.endpoint +
            ": invalid response type for initial data. expecting array");
  }

  VPackSlice const slice = builder.slice();

  if (!slice.isObject()) {
    LOG_TOPIC("1db22", DEBUG, Logger::REPLICATION)
        << "client: InitialSyncer::run - inventoryResponse is not an object";
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE,
                  std::string("got invalid response from master at ") +
                      _state.master.endpoint + ": invalid JSON");
  }

  return Result();
}
