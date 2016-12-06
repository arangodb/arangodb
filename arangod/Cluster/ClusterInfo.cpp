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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ClusterInfo.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/hashes.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Rest/HttpResponse.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/Events.h"
#include "VocBase/LogicalCollection.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/VocBase/SmartVertexCollection.h"
#include "Enterprise/VocBase/VirtualCollection.h"
#endif

#ifdef _WIN32
// turn off warnings about too long type name for debug symbols blabla in MSVC
// only...
#pragma warning(disable : 4503)
#endif

using namespace arangodb;

static std::unique_ptr<ClusterInfo> _instance;

////////////////////////////////////////////////////////////////////////////////
/// @brief a local helper to report errors and messages
////////////////////////////////////////////////////////////////////////////////

static inline int setErrormsg(int ourerrno, std::string& errorMsg) {
  errorMsg = TRI_errno_string(ourerrno);
  return ourerrno;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the JSON returns an error
////////////////////////////////////////////////////////////////////////////////

static inline bool hasError(VPackSlice const& slice) {
  return arangodb::basics::VelocyPackHelper::getBooleanValue(slice, "error",
                                                             false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the error message from a JSON
////////////////////////////////////////////////////////////////////////////////

static std::string extractErrorMessage(std::string const& shardId,
                                       VPackSlice const& slice) {
  std::string msg = " shardID:" + shardId + ": ";

  // add error message text
  msg += arangodb::basics::VelocyPackHelper::getStringValue(slice,
                                                            "errorMessage", "");

  // add error number
  if (slice.hasKey("errorNum")) {
    VPackSlice const errorNum = slice.get("errorNum");
    if (errorNum.isNumber()) {
      msg += " (errNum=" + arangodb::basics::StringUtils::itoa(
                               errorNum.getNumericValue<uint32_t>()) +
             ")";
    }
  }

  return msg;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an empty collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent(ShardID const& shardID,
                                             VPackSlice slice) {
  add(shardID, slice);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from another
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent(CollectionInfoCurrent const& other)
    : _vpacks(other._vpacks) {
  copyAllVPacks();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief moves a collection info current object from another
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent(CollectionInfoCurrent&& other) {
  _vpacks.swap(other._vpacks);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy assigns a collection info current object from another one
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent& CollectionInfoCurrent::operator=(
    CollectionInfoCurrent const& other) {
  if (this == &other) {
    return *this;
  }
  _vpacks = other._vpacks;
  copyAllVPacks();
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection info object from json
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent& CollectionInfoCurrent::operator=(
    CollectionInfoCurrent&& other) {
  if (this == &other) {
    return *this;
  }
  _vpacks.clear();
  _vpacks.swap(other._vpacks);
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::~CollectionInfoCurrent() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy slices behind the pointers in the map _vpacks
////////////////////////////////////////////////////////////////////////////////
void CollectionInfoCurrent::copyAllVPacks() {
  for (auto it : _vpacks) {
    auto builder = std::make_shared<VPackBuilder>();
    builder->add(it.second->slice());
    it.second = builder;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the clusterinfo instance
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::createInstance(
    AgencyCallbackRegistry* agencyCallbackRegistry) {
  _instance.reset(new ClusterInfo(agencyCallbackRegistry));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an instance of the cluster info class
////////////////////////////////////////////////////////////////////////////////

ClusterInfo* ClusterInfo::instance() { return _instance.get(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a cluster info object
////////////////////////////////////////////////////////////////////////////////

ClusterInfo::ClusterInfo(AgencyCallbackRegistry* agencyCallbackRegistry)
    : _agency(), _agencyCallbackRegistry(agencyCallbackRegistry), _uniqid() {
  _uniqid._currentValue = 1ULL;
  _uniqid._upperValue = 0ULL;

  // Actual loading into caches is postponed until necessary
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a cluster info object
////////////////////////////////////////////////////////////////////////////////

ClusterInfo::~ClusterInfo() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the uniqid value. if it exceeds the upper bound, fetch a
/// new upper bound value from the agency
////////////////////////////////////////////////////////////////////////////////

uint64_t ClusterInfo::uniqid(uint64_t count) {
  while (true) {
    uint64_t oldValue;
    {
      // The quick path, we have enough in our private reserve:
      MUTEX_LOCKER(mutexLocker, _idLock);

      if (_uniqid._currentValue + count - 1 <= _uniqid._upperValue) {
        uint64_t result = _uniqid._currentValue;
        _uniqid._currentValue += count;

        return result;
      }
      oldValue = _uniqid._currentValue;
    }

    // We need to fetch from the agency

    uint64_t fetch = count;

    if (fetch < MinIdsPerBatch) {
      fetch = MinIdsPerBatch;
    }

    uint64_t result = _agency.uniqid(fetch, 0.0);

    {
      MUTEX_LOCKER(mutexLocker, _idLock);

      if (oldValue == _uniqid._currentValue) {
        _uniqid._currentValue = result + count;
        _uniqid._upperValue = result + fetch - 1;

        return result;
      }
      // If we get here, somebody else tried succeeded in doing the same,
      // so we just try again.
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the caches (used for testing)
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::flush() {
  loadServers();
  loadCurrentDBServers();
  loadCurrentCoordinators();
  loadPlan();
  loadCurrent();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask whether a cluster database exists
////////////////////////////////////////////////////////////////////////////////

bool ClusterInfo::doesDatabaseExist(DatabaseID const& databaseID, bool reload) {
  int tries = 0;

  if (reload || !_planProt.isValid || !_currentProt.isValid ||
      !_DBServersProt.isValid) {
    loadPlan();
    loadCurrent();
    loadCurrentDBServers();
    ++tries;  // no need to reload if the database is not found
  }

  // From now on we know that all data has been valid once, so no need
  // to check the isValid flags again under the lock.

  while (true) {
    {
      size_t expectedSize;
      {
        READ_LOCKER(readLocker, _DBServersProt.lock);
        expectedSize = _DBServers.size();
      }

      // look up database by name:

      READ_LOCKER(readLocker, _planProt.lock);
      // _plannedDatabases is a map-type<DatabaseID, VPackSlice>
      auto it = _plannedDatabases.find(databaseID);

      if (it != _plannedDatabases.end()) {
        // found the database in Plan
        READ_LOCKER(readLocker, _currentProt.lock);
        // _currentDatabases is
        //     a map-type<DatabaseID, a map-type<ServerID, VPackSlice>>
        auto it2 = _currentDatabases.find(databaseID);

        if (it2 != _currentDatabases.end()) {
          // found the database in Current

          return ((*it2).second.size() >= expectedSize);
        }
      }
    }

    if (++tries >= 2) {
      break;
    }

    loadPlan();
    loadCurrent();
    loadCurrentDBServers();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of databases in the cluster
////////////////////////////////////////////////////////////////////////////////

std::vector<DatabaseID> ClusterInfo::databases(bool reload) {
  std::vector<DatabaseID> result;

  if (reload || !_planProt.isValid || !_currentProt.isValid ||
      !_DBServersProt.isValid) {
    loadPlan();
    loadCurrent();
    loadCurrentDBServers();
  }

  // From now on we know that all data has been valid once, so no need
  // to check the isValid flags again under the lock.

  size_t expectedSize;
  {
    READ_LOCKER(readLocker, _DBServersProt.lock);
    expectedSize = _DBServers.size();
  }

  {
    READ_LOCKER(readLockerPlanned, _planProt.lock);
    READ_LOCKER(readLockerCurrent, _currentProt.lock);
    // _plannedDatabases is a map-type<DatabaseID, VPackSlice>
    auto it = _plannedDatabases.begin();

    while (it != _plannedDatabases.end()) {
      // _currentDatabases is:
      //   a map-type<DatabaseID, a map-type<ServerID, VPackSlice>>
      auto it2 = _currentDatabases.find((*it).first);

      if (it2 != _currentDatabases.end()) {
        if ((*it2).second.size() >= expectedSize) {
          result.push_back((*it).first);
        }
      }

      ++it;
    }
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about our plan
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////
//
static std::string const prefixPlan = "Plan";

void ClusterInfo::loadPlan() {
  DatabaseFeature* databaseFeature =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database");
  ++_planProt.wantedVersion;  // Indicate that after *NOW* somebody has to
                              // reread from the agency!
  MUTEX_LOCKER(mutexLocker, _planProt.mutex);  // only one may work at a time
  uint64_t storedVersion = _planProt.wantedVersion;  // this is the version
                                                     // we will set in the end

  if (_planProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result = _agency.getValues(prefixPlan);

  if (result.successful()) {
    VPackSlice slice = result.slice()[0].get(
        std::vector<std::string>({AgencyCommManager::path(), "Plan"}));
    auto planBuilder = std::make_shared<VPackBuilder>();
    planBuilder->add(slice);

    VPackSlice planSlice = planBuilder->slice();

    if (planSlice.isObject()) {
      decltype(_plannedDatabases) newDatabases;
      decltype(_plannedCollections) newCollections;
      decltype(_shards) newShards;
      decltype(_shardKeys) newShardKeys;

      bool swapDatabases = false;
      bool swapCollections = false;

      VPackSlice databasesSlice;
      databasesSlice = planSlice.get("Databases");
      if (databasesSlice.isObject()) {
        for (auto const& database : VPackObjectIterator(databasesSlice)) {
          std::string const& name = database.key.copyString();

          newDatabases.insert(std::make_pair(name, database.value));
        }
        swapDatabases = true;
      }

      // mop: immediate children of collections are DATABASES, followed by their
      // collections
      databasesSlice = planSlice.get("Collections");
      if (databasesSlice.isObject()) {
        for (auto const& databasePairSlice :
             VPackObjectIterator(databasesSlice)) {
          VPackSlice const& collectionsSlice = databasePairSlice.value;
          if (!collectionsSlice.isObject()) {
            continue;
          }
          DatabaseCollections databaseCollections;
          std::string const databaseName = databasePairSlice.key.copyString();
          TRI_vocbase_t* vocbase = nullptr;
          if (ServerState::instance()->isCoordinator()) {
            vocbase = databaseFeature->lookupDatabaseCoordinator(databaseName);
          } else {
            vocbase = databaseFeature->lookupDatabase(databaseName);
          }
          TRI_ASSERT(vocbase != nullptr);
          if (vocbase == nullptr) {
            // No database with this name found.
            // We have an invalid state here.
            continue;
          }

          for (auto const& collectionPairSlice :
               VPackObjectIterator(collectionsSlice)) {
            VPackSlice const& collectionSlice = collectionPairSlice.value;
            if (!collectionSlice.isObject()) {
              continue;
            }

            std::string const collectionId =
                collectionPairSlice.key.copyString();
            try {
              std::shared_ptr<LogicalCollection> newCollection;
#ifndef USE_ENTERPRISE
              newCollection = std::make_shared<LogicalCollection>(
                  vocbase, collectionSlice, false);
#else
              VPackSlice isSmart = collectionSlice.get("isSmart");
              if (isSmart.isTrue()) {
                VPackSlice type = collectionSlice.get("type");
                if (type.isInteger() && type.getUInt() == TRI_COL_TYPE_EDGE) {
                  newCollection = std::make_shared<VirtualSmartEdgeCollection>(
                      vocbase, collectionSlice);
                } else {
                  newCollection = std::make_shared<SmartVertexCollection>(
                      vocbase, collectionSlice);
                }
              } else {
                newCollection = std::make_shared<LogicalCollection>(
                    vocbase, collectionSlice, false);
              }
#endif
              std::string const collectionName = newCollection->name();

              // mop: register with name as well as with id
              databaseCollections.emplace(
                  std::make_pair(collectionName, newCollection));
              databaseCollections.emplace(
                  std::make_pair(collectionId, newCollection));

              auto shardKeys = std::make_shared<std::vector<std::string>>(
                  newCollection->shardKeys());
              newShardKeys.insert(make_pair(collectionId, shardKeys));

              auto shardIDs = newCollection->shardIds();
              auto shards = std::make_shared<std::vector<std::string>>();
              for (auto const& p : *shardIDs) {
                shards->push_back(p.first);
              }
              // Sort by the number in the shard ID ("s0000001" for example):
              std::sort(shards->begin(), shards->end(),
                        [](std::string const& a, std::string const& b) -> bool {
                          return std::strtol(a.c_str() + 1, nullptr, 10) <
                                 std::strtol(b.c_str() + 1, nullptr, 10);
                        });
              newShards.emplace(std::make_pair(collectionId, shards));
            } catch (std::exception const& ex) {
              // The plan contains invalid collection information.
              // This should not happen in healthy situations.
              // If it happens in unhealthy situations the
              // cluster should not fail.
              LOG(ERR) << "Failed to load information for collection '"
                       << collectionId << "': " << ex.what()
                       << ". invalid information in plan. The collection will "
                          "be ignored for now and the invalid information will "
                          "be repaired. VelocyPack: "
                       << collectionSlice.toJson();

              TRI_ASSERT(false);
              continue;
            } catch (...) {
              // The plan contains invalid collection information.
              // This should not happen in healthy situations.
              // If it happens in unhealthy situations the
              // cluster should not fail.
              LOG(ERR) << "Failed to load information for collection '"
                       << collectionId
                       << ". invalid information in plan. The collection will "
                          "be ignored for now and the invalid information will "
                          "be repaired. VelocyPack: "
                       << collectionSlice.toJson();

              TRI_ASSERT(false);
              continue;
            }
          }

          newCollections.emplace(
              std::make_pair(databaseName, databaseCollections));
          swapCollections = true;
        }
      }

      WRITE_LOCKER(writeLocker, _planProt.lock);
      _plan = planBuilder;
      if (swapDatabases) {
        _plannedDatabases.swap(newDatabases);
      }
      if (swapCollections) {
        _plannedCollections.swap(newCollections);
        _shards.swap(newShards);
        _shardKeys.swap(newShardKeys);
      }
      _planProt.doneVersion = storedVersion;
      _planProt.isValid = true;  // will never be reset to false
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER) << "\"Plan\" is not an object in agency";
    }
    return;
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixPlan
      << " httpCode: " << result.httpCode()
      << " errorCode: " << result.errorCode()
      << " errorMessage: " << result.errorMessage()
      << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about current databases
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrent = "Current";

void ClusterInfo::loadCurrent() {
  ++_currentProt.wantedVersion;  // Indicate that after *NOW* somebody has to
                                 // reread from the agency!
  MUTEX_LOCKER(mutexLocker, _currentProt.mutex);  // only one may work at a time
  uint64_t storedVersion = _currentProt.wantedVersion;  // this is the version
  // we will set at the end
  if (_currentProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result = _agency.getValues(prefixCurrent);

  if (result.successful()) {
    velocypack::Slice slice = result.slice()[0].get(
        std::vector<std::string>({AgencyCommManager::path(), "Current"}));

    auto currentBuilder = std::make_shared<VPackBuilder>();
    currentBuilder->add(slice);

    VPackSlice currentSlice = currentBuilder->slice();

    if (currentSlice.isObject()) {
      decltype(_currentDatabases) newDatabases;
      decltype(_currentCollections) newCollections;
      decltype(_shardIds) newShardIds;

      bool swapDatabases = false;
      bool swapCollections = false;

      VPackSlice databasesSlice = currentSlice.get("Databases");
      if (databasesSlice.isObject()) {
        for (auto const& databaseSlicePair :
             VPackObjectIterator(databasesSlice)) {
          std::string const database = databaseSlicePair.key.copyString();

          if (!databaseSlicePair.value.isObject()) {
            continue;
          }

          std::unordered_map<ServerID, VPackSlice> serverList;
          for (auto const& serverSlicePair :
               VPackObjectIterator(databaseSlicePair.value)) {
            serverList.insert(std::make_pair(serverSlicePair.key.copyString(),
                                             serverSlicePair.value));
          }

          newDatabases.insert(std::make_pair(database, serverList));
        }
        swapDatabases = true;
      }

      databasesSlice = currentSlice.get("Collections");
      if (databasesSlice.isObject()) {
        for (auto const& databaseSlice : VPackObjectIterator(databasesSlice)) {
          std::string const databaseName = databaseSlice.key.copyString();

          DatabaseCollectionsCurrent databaseCollections;
          for (auto const& collectionSlice :
               VPackObjectIterator(databaseSlice.value)) {
            std::string const collectionName = collectionSlice.key.copyString();

            auto collectionDataCurrent =
                std::make_shared<CollectionInfoCurrent>();
            for (auto const& shardSlice :
                 VPackObjectIterator(collectionSlice.value)) {
              std::string const shardID = shardSlice.key.copyString();
              collectionDataCurrent->add(shardID, shardSlice.value);

              // Note that we have only inserted the CollectionInfoCurrent under
              // the collection ID and not under the name! It is not possible
              // to query the current collection info by name. This is because
              // the correct place to hold the current name is in the plan.
              // Thus: Look there and get the collection ID from there. Then
              // ask about the current collection info.

              // Now take note of this shard and its responsible server:
              auto servers = std::make_shared<std::vector<ServerID>>(
                  collectionDataCurrent->servers(shardID));
              newShardIds.insert(make_pair(shardID, servers));
            }
            databaseCollections.insert(
                std::make_pair(collectionName, collectionDataCurrent));
          }
          newCollections.emplace(
              std::make_pair(databaseName, databaseCollections));
        }
        swapCollections = true;
      }

      // Now set the new value:
      WRITE_LOCKER(writeLocker, _currentProt.lock);
      _current = currentBuilder;
      if (swapDatabases) {
        _currentDatabases.swap(newDatabases);
      }
      if (swapCollections) {
        LOG_TOPIC(TRACE, Logger::CLUSTER)
            << "Have loaded new collections current cache!";
        _currentCollections.swap(newCollections);
        _shardIds.swap(newShardIds);
      }
      _currentProt.doneVersion = storedVersion;
      _currentProt.isValid = true;  // will never be reset to false
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER) << "Current is not an object!";
    }

    return;
  }

  LOG_TOPIC(ERR, Logger::CLUSTER) << "Error while loading " << prefixCurrent
                                  << " httpCode: " << result.httpCode()
                                  << " errorCode: " << result.errorCode()
                                  << " errorMessage: " << result.errorMessage()
                                  << " body: " << result.body();
}

/// @brief ask about a collection
/// If it is not found in the cache, the cache is reloaded once

std::shared_ptr<LogicalCollection> ClusterInfo::getCollection(
    DatabaseID const& databaseID, CollectionID const& collectionID) {
  int tries = 0;

  if (!_planProt.isValid) {
    loadPlan();
    ++tries;
  }

  while (true) {  // left by break
    {
      READ_LOCKER(readLocker, _planProt.lock);
      // look up database by id
      AllCollections::const_iterator it = _plannedCollections.find(databaseID);

      if (it != _plannedCollections.end()) {
        // look up collection by id (or by name)
        DatabaseCollections::const_iterator it2 =
            (*it).second.find(collectionID);

        if (it2 != (*it).second.end()) {
          return (*it2).second;
        }
      }
    }
    if (++tries >= 2) {
      break;
    }

    // must load collections outside the lock
    loadPlan();
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
      "Collection not found: " + collectionID + " in database " + databaseID);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about all collections
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<LogicalCollection>> const
ClusterInfo::getCollections(DatabaseID const& databaseID) {
  std::vector<std::shared_ptr<LogicalCollection>> result;

  // always reload
  loadPlan();

  READ_LOCKER(readLocker, _planProt.lock);
  // look up database by id
  AllCollections::const_iterator it = _plannedCollections.find(databaseID);

  if (it == _plannedCollections.end()) {
    return result;
  }

  // iterate over all collections
  DatabaseCollections::const_iterator it2 = (*it).second.begin();
  while (it2 != (*it).second.end()) {
    char c = (*it2).first[0];

    if (c < '0' || c > '9') {
      // skip collections indexed by id
      result.push_back((*it2).second);
    }

    ++it2;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about a collection in current. This returns information about
/// all shards in the collection.
/// If it is not found in the cache, the cache is reloaded once.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<CollectionInfoCurrent> ClusterInfo::getCollectionCurrent(
    DatabaseID const& databaseID, CollectionID const& collectionID) {
  int tries = 0;

  if (!_currentProt.isValid) {
    loadCurrent();
    ++tries;
  }

  while (true) {
    {
      READ_LOCKER(readLocker, _currentProt.lock);
      // look up database by id
      AllCollectionsCurrent::const_iterator it =
          _currentCollections.find(databaseID);

      if (it != _currentCollections.end()) {
        // look up collection by id
        DatabaseCollectionsCurrent::const_iterator it2 =
            (*it).second.find(collectionID);

        if (it2 != (*it).second.end()) {
          return (*it2).second;
        }
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must load collections outside the lock
    loadCurrent();
  }

  return std::make_shared<CollectionInfoCurrent>();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create database in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::createDatabaseCoordinator(std::string const& name,
                                           VPackSlice const& slice,
                                           std::string& errorMsg,
                                           double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  auto DBServers =
      std::make_shared<std::vector<ServerID>>(getCurrentDBServers());
  std::shared_ptr<int> dbServerResult = std::make_shared<int>(-1);
  std::shared_ptr<std::string> errMsg = std::make_shared<std::string>();

  std::function<bool(VPackSlice const& result)> dbServerChanged =
      [=](VPackSlice const& result) {
        size_t numDbServers = DBServers->size();
        if (result.isObject() && result.length() >= numDbServers) {
          // We use >= here since the number of DBservers could have increased
          // during the creation of the database and we might not yet have
          // the latest list. Thus there could be more reports than we know
          // servers.
          VPackObjectIterator dbs(result);

          std::string tmpMsg = "";
          bool tmpHaveError = false;

          for (auto const& dbserver : dbs) {
            VPackSlice slice = dbserver.value;
            if (arangodb::basics::VelocyPackHelper::getBooleanValue(
                    slice, "error", false)) {
              tmpHaveError = true;
              tmpMsg += " DBServer:" + dbserver.key.copyString() + ":";
              tmpMsg += arangodb::basics::VelocyPackHelper::getStringValue(
                  slice, "errorMessage", "");
              if (slice.hasKey("errorNum")) {
                VPackSlice errorNum = slice.get("errorNum");
                if (errorNum.isNumber()) {
                  tmpMsg += " (errorNum=";
                  tmpMsg += basics::StringUtils::itoa(
                      errorNum.getNumericValue<uint32_t>());
                  tmpMsg += ")";
                }
              }
            }
          }
          if (tmpHaveError) {
            *errMsg = "Error in creation of database:" + tmpMsg;
            *dbServerResult = TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE;
            return true;
          }
          *dbServerResult = setErrormsg(TRI_ERROR_NO_ERROR, *errMsg);
        }
        return true;
      };

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback = std::make_shared<AgencyCallback>(
      ac, "Current/Databases/" + name, dbServerChanged, true, false);
  _agencyCallbackRegistry->registerCallback(agencyCallback);
  TRI_DEFER(_agencyCallbackRegistry->unregisterCallback(agencyCallback));

  AgencyOperation newVal("Plan/Databases/" + name,
                         AgencyValueOperationType::SET, slice);
  AgencyOperation incrementVersion("Plan/Version",
                                   AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition precondition("Plan/Databases/" + name,
                                  AgencyPrecondition::Type::EMPTY, true);
  AgencyWriteTransaction trx({newVal, incrementVersion}, precondition);

  res = ac.sendTransactionWithFailover(trx, realTimeout);

  if (!res.successful()) {
    if (res._statusCode ==
        (int)arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
      return setErrormsg(TRI_ERROR_ARANGO_DUPLICATE_NAME, errorMsg);
    }

    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN,
                       errorMsg);
  }

  // Now update our own cache of planned databases:
  loadPlan();

  {
    CONDITION_LOCKER(locker, agencyCallback->_cv);

    int count = 0;  // this counts, when we have to reload the DBServers
    while (true) {
      errorMsg = *errMsg;

      if (++count >=
          static_cast<int>(getReloadServerListTimeout() / interval)) {
        // We update the list of DBServers every minute in case one of them
        // was taken away since we last looked. This also helps (slightly)
        // if a new DBServer was added. However, in this case we report
        // success a bit too early, which is not too bad.
        loadCurrentDBServers();
        *DBServers = getCurrentDBServers();
        count = 0;
      }

      if (*dbServerResult >= 0) {
        loadCurrent();  // update our cache
        return *dbServerResult;
      }

      if (TRI_microtime() > endTime) {
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }

      agencyCallback->executeByCallbackOrTimeout(getReloadServerListTimeout() /
                                                 interval);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop database in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::dropDatabaseCoordinator(std::string const& name,
                                         std::string& errorMsg,
                                         double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  std::shared_ptr<int> dbServerResult = std::make_shared<int>(-1);
  std::function<bool(VPackSlice const& result)> dbServerChanged =
      [=](VPackSlice const& result) {
        if (result.isObject() && result.length() == 0) {
          *dbServerResult = 0;
        }
        return true;
      };

  std::string where("Current/Databases/" + name);

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback =
      std::make_shared<AgencyCallback>(ac, where, dbServerChanged, true, false);
  _agencyCallbackRegistry->registerCallback(agencyCallback);
  TRI_DEFER(_agencyCallbackRegistry->unregisterCallback(agencyCallback));

  // Transact to agency
  AgencyOperation delPlanDatabases("Plan/Databases/" + name,
                                   AgencySimpleOperationType::DELETE_OP);
  AgencyOperation delPlanCollections("Plan/Collections/" + name,
                                     AgencySimpleOperationType::DELETE_OP);
  AgencyOperation incrementVersion("Plan/Version",
                                   AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition databaseExists("Plan/Databases/" + name,
                                    AgencyPrecondition::Type::EMPTY, false);
  AgencyWriteTransaction trans(
      {delPlanDatabases, delPlanCollections, incrementVersion}, databaseExists);

  res = ac.sendTransactionWithFailover(trans);

  // Load our own caches:
  loadPlan();

  // Now wait stuff in Current to disappear and thus be complete:
  {
    CONDITION_LOCKER(locker, agencyCallback->_cv);
    while (true) {
      if (*dbServerResult >= 0) {
        res = ac.removeValues(where, true);
        if (res.successful()) {
          return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
        }
        return setErrormsg(
            TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT, errorMsg);
      }

      if (TRI_microtime() > endTime) {
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }

      agencyCallback->executeByCallbackOrTimeout(interval);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create collection in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::createCollectionCoordinator(std::string const& databaseName,
                                             std::string const& collectionID,
                                             uint64_t numberOfShards,
                                             uint64_t replicationFactor,
                                             VPackSlice const& json,
                                             std::string& errorMsg,
                                             double timeout) {
  using arangodb::velocypack::Slice;

  AgencyComm ac;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  std::string const name =
      arangodb::basics::VelocyPackHelper::getStringValue(json, "name", "");

  {
    // check if a collection with the same name is already planned
    loadPlan();

    READ_LOCKER(readLocker, _planProt.lock);
    AllCollections::const_iterator it = _plannedCollections.find(databaseName);
    if (it != _plannedCollections.end()) {
      DatabaseCollections::const_iterator it2 = (*it).second.find(name);

      if (it2 != (*it).second.end()) {
        // collection already exists!
        events::CreateCollection(name, TRI_ERROR_ARANGO_DUPLICATE_NAME);
        return TRI_ERROR_ARANGO_DUPLICATE_NAME;
      }
    }
  }

  // mop: why do these ask the agency instead of checking cluster info?
  if (!ac.exists("Plan/Databases/" + databaseName)) {
    events::CreateCollection(name, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return setErrormsg(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
  }

  if (ac.exists("Plan/Collections/" + databaseName + "/" + collectionID)) {
    events::CreateCollection(name, TRI_ERROR_CLUSTER_COLLECTION_ID_EXISTS);
    return setErrormsg(TRI_ERROR_CLUSTER_COLLECTION_ID_EXISTS, errorMsg);
  }

  std::shared_ptr<int> dbServerResult = std::make_shared<int>(-1);
  std::shared_ptr<std::string> errMsg = std::make_shared<std::string>();

  auto dbServers = getCurrentDBServers();
  std::function<bool(VPackSlice const& result)> dbServerChanged =
      [=](VPackSlice const& result) {
        if (result.isObject() && result.length() == (size_t)numberOfShards) {
          std::string tmpMsg = "";
          bool tmpHaveError = false;

          for (auto const& p : VPackObjectIterator(result)) {
            if (replicationFactor == 0) {
              VPackSlice servers = p.value.get("servers");
              if (!servers.isArray() || servers.length() < dbServers.size()) {
                return true;
              }
            }

            if (arangodb::basics::VelocyPackHelper::getBooleanValue(
                    p.value, "error", false)) {
              tmpHaveError = true;
              tmpMsg += " shardID:" + p.key.copyString() + ":";
              tmpMsg += arangodb::basics::VelocyPackHelper::getStringValue(
                  p.value, "errorMessage", "");
              if (p.value.hasKey("errorNum")) {
                VPackSlice const errorNum = p.value.get("errorNum");
                if (errorNum.isNumber()) {
                  tmpMsg += " (errNum=";
                  tmpMsg += basics::StringUtils::itoa(
                      errorNum.getNumericValue<uint32_t>());
                  tmpMsg += ")";
                }
              }
            }
          }
          if (tmpHaveError) {
            *errMsg = "Error in creation of collection:" + tmpMsg;
            *dbServerResult = TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION;
            return true;
          }
          *dbServerResult = setErrormsg(TRI_ERROR_NO_ERROR, *errMsg);
        }
        return true;
      };

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback = std::make_shared<AgencyCallback>(
      ac, "Current/Collections/" + databaseName + "/" + collectionID,
      dbServerChanged, true, false);
  _agencyCallbackRegistry->registerCallback(agencyCallback);
  TRI_DEFER(_agencyCallbackRegistry->unregisterCallback(agencyCallback));

  VPackBuilder builder;
  builder.add(json);

  AgencyOperation createCollection(
      "Plan/Collections/" + databaseName + "/" + collectionID,
      AgencyValueOperationType::SET, builder.slice());
  AgencyOperation increaseVersion("Plan/Version",
                                  AgencySimpleOperationType::INCREMENT_OP);

  AgencyPrecondition precondition = AgencyPrecondition(
      "Plan/Collections/" + databaseName + "/" + collectionID,
      AgencyPrecondition::Type::EMPTY, true);

  AgencyWriteTransaction transaction;

  transaction.operations.push_back(createCollection);
  transaction.operations.push_back(increaseVersion);
  transaction.preconditions.push_back(precondition);

  AgencyCommResult res = ac.sendTransactionWithFailover(transaction);

  if (!res.successful()) {
    events::CreateCollection(
        name, TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN);
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN,
                       errorMsg);
  }

  // Update our cache:
  loadPlan();
  if (numberOfShards == 0) {
    loadCurrent();
    events::CreateCollection(name, TRI_ERROR_NO_ERROR);
    return TRI_ERROR_NO_ERROR;
  }

  {
    CONDITION_LOCKER(locker, agencyCallback->_cv);

    while (true) {
      errorMsg = *errMsg;

      if (*dbServerResult >= 0) {
        loadCurrent();
        events::CreateCollection(name, *dbServerResult);
        return *dbServerResult;
      }

      if (TRI_microtime() > endTime) {
        LOG_TOPIC(ERR, Logger::CLUSTER)
            << "Timeout in _create collection"
            << ": database: " << databaseName << ", collId:" << collectionID
            << "\njson: " << json.toString()
            << "\ntransaction sent to agency: " << transaction.toJson();
        AgencyCommResult ag = ac.getValues("");
        if (ag.successful()) {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Agency dump:\n"
                                          << ag.slice().toJson();
        } else {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Could not get agency dump!";
        }
        events::CreateCollection(name, TRI_ERROR_CLUSTER_TIMEOUT);
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }

      agencyCallback->executeByCallbackOrTimeout(interval);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop collection in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::dropCollectionCoordinator(std::string const& databaseName,
                                           std::string const& collectionID,
                                           std::string& errorMsg,
                                           double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  auto dbServerResult = std::make_shared<int>(-1);
  auto errMsg = std::make_shared<std::string>();
  std::function<bool(VPackSlice const& result)> dbServerChanged =
      [=](VPackSlice const& result) {
        if (result.isObject() && result.length() == 0) {
          *dbServerResult = setErrormsg(TRI_ERROR_NO_ERROR, *errMsg);
        }
        return true;
      };

  // monitor the entry for the collection
  std::string const where =
      "Current/Collections/" + databaseName + "/" + collectionID;

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback =
      std::make_shared<AgencyCallback>(ac, where, dbServerChanged, true, false);
  _agencyCallbackRegistry->registerCallback(agencyCallback);
  TRI_DEFER(_agencyCallbackRegistry->unregisterCallback(agencyCallback));

  size_t numberOfShards = 0;
  res = ac.getValues("Plan/Collections/" + databaseName + "/" + collectionID +
                     "/shards");

  if (res.successful()) {
    velocypack::Slice shards = res.slice()[0].get(std::vector<std::string>(
        {AgencyCommManager::path(), "Plan", "Collections", databaseName,
         collectionID, "shards"}));
    if (shards.isObject()) {
      numberOfShards = shards.length();
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER)
          << "Missing shards information on dropping" << databaseName << "/"
          << collectionID;
    }
  }

  // Transact to agency
  AgencyOperation delPlanCollection(
      "Plan/Collections/" + databaseName + "/" + collectionID,
      AgencySimpleOperationType::DELETE_OP);
  AgencyOperation incrementVersion("Plan/Version",
                                   AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition precondition = AgencyPrecondition(
      "Plan/Databases/" + databaseName, AgencyPrecondition::Type::EMPTY, false);
  AgencyWriteTransaction trans({delPlanCollection, incrementVersion},
                               precondition);
  res = ac.sendTransactionWithFailover(trans);

  // Update our own cache:
  loadPlan();

  if (numberOfShards == 0) {
    loadCurrent();
    events::DropCollection(collectionID, TRI_ERROR_NO_ERROR);
    return TRI_ERROR_NO_ERROR;
  }

  {
    CONDITION_LOCKER(locker, agencyCallback->_cv);

    while (true) {
      errorMsg = *errMsg;

      if (*dbServerResult >= 0) {
        // ...remove the entire directory for the collection
        ac.removeValues(
            "Current/Collections/" + databaseName + "/" + collectionID, true);
        loadCurrent();
        events::DropCollection(collectionID, *dbServerResult);
        return *dbServerResult;
      }

      if (TRI_microtime() > endTime) {
        LOG_TOPIC(ERR, Logger::CLUSTER)
            << "Timeout in _drop collection"
            << ": database: " << databaseName << ", collId:" << collectionID
            << "\ntransaction sent to agency: " << trans.toJson();
        AgencyCommResult ag = ac.getValues("");
        if (ag.successful()) {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Agency dump:\n"
                                          << ag.slice().toJson();
        } else {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Could not get agency dump!";
        }
        events::DropCollection(collectionID, TRI_ERROR_CLUSTER_TIMEOUT);
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }

      agencyCallback->executeByCallbackOrTimeout(interval);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection properties in coordinator
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::setCollectionPropertiesCoordinator(
    std::string const& databaseName, std::string const& collectionID,
    LogicalCollection const* info) {
  AgencyComm ac;
  AgencyCommResult res;

  AgencyPrecondition databaseExists("Plan/Databases/" + databaseName,
                                    AgencyPrecondition::Type::EMPTY, false);
  AgencyOperation incrementVersion("Plan/Version",
                                   AgencySimpleOperationType::INCREMENT_OP);

  res = ac.getValues("Plan/Collections/" + databaseName + "/" + collectionID);

  if (!res.successful()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  velocypack::Slice collection = res.slice()[0].get(
      std::vector<std::string>({AgencyCommManager::path(), "Plan",
                                "Collections", databaseName, collectionID}));

  if (!collection.isObject()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  VPackBuilder copy;
  try {
    VPackObjectBuilder b(&copy);
    for (auto const& entry : VPackObjectIterator(collection)) {
      std::string key = entry.key.copyString();
      // Copy all values except the following
      // They are overwritten later
      if (key != "doCompact" && key != "journalSize" && key != "waitForSync" &&
          key != "indexBuckets") {
        copy.add(key, entry.value);
      }
    }
    copy.add("doCompact", VPackValue(info->doCompact()));
    copy.add("journalSize", VPackValue(info->journalSize()));
    copy.add("waitForSync", VPackValue(info->waitForSync()));
    copy.add("indexBuckets", VPackValue(info->indexBuckets()));
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  res.clear();

  AgencyOperation setColl(
      "Plan/Collections/" + databaseName + "/" + collectionID,
      AgencyValueOperationType::SET, copy.slice());

  AgencyWriteTransaction trans({setColl, incrementVersion}, databaseExists);

  res = ac.sendTransactionWithFailover(trans);

  if (res.successful()) {
    loadPlan();
    return TRI_ERROR_NO_ERROR;
  } else {
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection status in coordinator
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::setCollectionStatusCoordinator(
    std::string const& databaseName, std::string const& collectionID,
    TRI_vocbase_col_status_e status) {
  AgencyComm ac;
  AgencyCommResult res;

  AgencyPrecondition databaseExists("Plan/Databases/" + databaseName,
                                    AgencyPrecondition::Type::EMPTY, false);

  res = ac.getValues("Plan/Collections/" + databaseName + "/" + collectionID);

  if (!res.successful()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  VPackSlice col = res.slice()[0].get(
      std::vector<std::string>({AgencyCommManager::path(), "Plan",
                                "Collections", databaseName, collectionID}));

  if (!col.isObject()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_vocbase_col_status_e old = static_cast<TRI_vocbase_col_status_e>(
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          col, "status", static_cast<int>(TRI_VOC_COL_STATUS_CORRUPTED)));

  if (old == status) {
    // no status change
    return TRI_ERROR_NO_ERROR;
  }

  VPackBuilder builder;
  try {
    VPackObjectBuilder b(&builder);
    for (auto const& entry : VPackObjectIterator(col)) {
      std::string key = entry.key.copyString();
      if (key != "status") {
        builder.add(key, entry.value);
      }
    }
    builder.add("status", VPackValue(status));
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  res.clear();

  AgencyOperation setColl(
      "Plan/Collections/" + databaseName + "/" + collectionID,
      AgencyValueOperationType::SET, builder.slice());

  AgencyWriteTransaction trans(setColl, databaseExists);

  res = ac.sendTransactionWithFailover(trans);

  if (res.successful()) {
    loadPlan();
    return TRI_ERROR_NO_ERROR;
  } else {
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure an index in coordinator.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::ensureIndexCoordinator(
    std::string const& databaseName, std::string const& collectionID,
    VPackSlice const& slice, bool create,
    bool (*compare)(VPackSlice const&, VPackSlice const&),
    VPackBuilder& resultBuilder, std::string& errorMsg, double timeout) {
  AgencyComm ac;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  std::string where =
      "Current/Collections/" + databaseName + "/" + collectionID;

  // check index id
  uint64_t iid = 0;

  VPackSlice const idxSlice = slice.get("id");
  if (idxSlice.isString()) {
    // use predefined index id
    iid = arangodb::basics::StringUtils::uint64(idxSlice.copyString());
  }

  if (iid == 0) {
    // no id set, create a new one!
    iid = uniqid();
  }
  TRI_ASSERT(resultBuilder.isEmpty());

  std::string const key =
      "Plan/Collections/" + databaseName + "/" + collectionID;

  AgencyCommResult previous = ac.getValues(key);

  if (!previous.successful()) {
    return TRI_ERROR_CLUSTER_READING_PLAN_AGENCY;
  }

  velocypack::Slice collection = previous.slice()[0].get(
      std::vector<std::string>({AgencyCommManager::path(), "Plan",
                                "Collections", databaseName, collectionID}));

  if (!collection.isObject()) {
    return setErrormsg(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, errorMsg);
  }

  loadPlan();
  // It is possible that between the fetching of the planned collections
  // and the write lock we acquire below something has changed. Therefore
  // we first get the previous value and then do a compare and swap operation.

  auto numberOfShardsMutex = std::make_shared<Mutex>();
  auto numberOfShards = std::make_shared<int>(0);
  auto resBuilder = std::make_shared<VPackBuilder>();
  auto collectionBuilder = std::make_shared<VPackBuilder>();

  {
    std::shared_ptr<LogicalCollection> c =
        getCollection(databaseName, collectionID);

    // Note that nobody is removing this collection in the plan, since
    // we hold the write lock in the agency, therefore it does not matter
    // that getCollection fetches the read lock and releases it before
    // we get it again.
    //
    READ_LOCKER(readLocker, _planProt.lock);

    if (c == nullptr) {
      return setErrormsg(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, errorMsg);
    }

    std::shared_ptr<VPackBuilder> tmp = std::make_shared<VPackBuilder>();
    c->getIndexesVPack(*(tmp.get()), false);
    MUTEX_LOCKER(guard, *numberOfShardsMutex);
    { *numberOfShards = c->numberOfShards(); }
    VPackSlice const indexes = tmp->slice();

    if (indexes.isArray()) {
      VPackSlice const type = slice.get("type");

      if (!type.isString()) {
        return setErrormsg(TRI_ERROR_INTERNAL, errorMsg);
      }

      for (auto const& other : VPackArrayIterator(indexes)) {
        if (arangodb::basics::VelocyPackHelper::compare(type, other.get("type"),
                                                        false) != 0) {
          // compare index types first. they must match
          continue;
        }
        TRI_ASSERT(other.isObject());

        bool isSame = compare(slice, other);

        if (isSame) {
          // found an existing index...
          {
            // Copy over all elements in slice.
            VPackObjectBuilder b(resBuilder.get());
            for (auto const& entry : VPackObjectIterator(other)) {
              resBuilder->add(entry.key.copyString(), entry.value);
            }
            resBuilder->add("isNewlyCreated", VPackValue(false));
          }
          resultBuilder = *resBuilder;
          return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
        }
      }
    }

    // no existing index found.
    if (!create) {
      TRI_ASSERT(resultBuilder.isEmpty());
      return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
    }

    // now create a new index
    c->toVelocyPackForAgency(*collectionBuilder);
  }
  VPackSlice const collectionSlice = collectionBuilder->slice();

  auto newBuilder = std::make_shared<VPackBuilder>();
  if (!collectionSlice.isObject()) {
    return setErrormsg(TRI_ERROR_CLUSTER_AGENCY_STRUCTURE_INVALID, errorMsg);
  }

  std::string const idString = arangodb::basics::StringUtils::itoa(iid);

  try {
    VPackObjectBuilder b(newBuilder.get());
    // Create a new collection VPack with the new Index
    for (auto const& entry : VPackObjectIterator(collectionSlice)) {
      TRI_ASSERT(entry.key.isString());
      std::string key = entry.key.copyString();

      if (key == "indexes") {
        TRI_ASSERT(entry.value.isArray());
        newBuilder->add(key, VPackValue(VPackValueType::Array));
        // Copy over all indexes known so far
        for (auto const& idx : VPackArrayIterator(entry.value)) {
          newBuilder->add(idx);
        }
        {
          VPackObjectBuilder ob(newBuilder.get());
          // Add the new index ignoring "id"
          for (auto const& e : VPackObjectIterator(slice)) {
            TRI_ASSERT(e.key.isString());
            std::string tmpkey = e.key.copyString();
            if (tmpkey != "id") {
              newBuilder->add(tmpkey, e.value);
            }
          }
          newBuilder->add("id", VPackValue(idString));
        }
        newBuilder->close();  // the array
      } else {
        // Plain copy everything else
        newBuilder->add(key, entry.value);
      }
    }
  } catch (...) {
    return setErrormsg(TRI_ERROR_OUT_OF_MEMORY, errorMsg);
  }

  std::shared_ptr<int> dbServerResult = std::make_shared<int>(-1);
  std::shared_ptr<std::string> errMsg = std::make_shared<std::string>();

  std::function<bool(VPackSlice const& result)> dbServerChanged = [=](
      VPackSlice const& result) {
    int localNumberOfShards;
    {
      MUTEX_LOCKER(guard, *numberOfShardsMutex);
      localNumberOfShards = *numberOfShards;
    }

    // mop: uhhhh....we didn't even set the plan yet :O
    if (localNumberOfShards == 0) {
      return false;
    }

    if (!result.isObject()) {
      return true;
    }

    if (result.length() == (size_t)*numberOfShards) {
      size_t found = 0;
      for (auto const& shard : VPackObjectIterator(result)) {
        VPackSlice const slice = shard.value;
        if (slice.hasKey("indexes")) {
          VPackSlice const indexes = slice.get("indexes");
          if (!indexes.isArray()) {
            // no list, so our index is not present. we can abort searching
            break;
          }

          for (auto const& v : VPackArrayIterator(indexes)) {
            // check for errors
            if (hasError(v)) {
              std::string errorMsg =
                  extractErrorMessage(shard.key.copyString(), v);

              errorMsg = "Error during index creation: " + errorMsg;

              // Returns the specific error number if set, or the general
              // error
              // otherwise
              *dbServerResult =
                  arangodb::basics::VelocyPackHelper::getNumericValue<int>(
                      v, "errorNum", TRI_ERROR_ARANGO_INDEX_CREATION_FAILED);
              return true;
            }

            VPackSlice const k = v.get("id");

            if (!k.isString() || idString != k.copyString()) {
              // this is not our index
              continue;
            }

            // found our index
            found++;
            break;
          }
        }
      }

      if (found == (size_t)*numberOfShards) {
        VPackSlice indexFinder = newBuilder->slice();
        TRI_ASSERT(indexFinder.isObject());
        indexFinder = indexFinder.get("indexes");
        TRI_ASSERT(indexFinder.isArray());
        VPackValueLength l = indexFinder.length();
        indexFinder = indexFinder.at(l - 1);  // Get the last index
        TRI_ASSERT(indexFinder.isObject());
        {
          // Copy over all elements in slice.
          VPackObjectBuilder b(resBuilder.get());
          for (auto const& entry : VPackObjectIterator(indexFinder)) {
            resBuilder->add(entry.key.copyString(), entry.value);
          }
          resBuilder->add("isNewlyCreated", VPackValue(true));
        }
        *dbServerResult = setErrormsg(TRI_ERROR_NO_ERROR, *errMsg);
        return true;
      }
    }
    return true;
  };

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback =
      std::make_shared<AgencyCallback>(ac, where, dbServerChanged, true, false);
  _agencyCallbackRegistry->registerCallback(agencyCallback);
  TRI_DEFER(_agencyCallbackRegistry->unregisterCallback(agencyCallback));

  AgencyOperation newValue(key, AgencyValueOperationType::SET,
                           newBuilder->slice());
  AgencyOperation incrementVersion("Plan/Version",
                                   AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition oldValue(key, AgencyPrecondition::Type::VALUE, collection);
  AgencyWriteTransaction trx({newValue, incrementVersion}, oldValue);

  AgencyCommResult result = ac.sendTransactionWithFailover(trx, 0.0);

  if (!result.successful()) {
    resultBuilder = *resBuilder;
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN,
                       errorMsg);
  }

  loadPlan();

  if (*numberOfShards == 0) {
    errorMsg = *errMsg;
    resultBuilder = *resBuilder;
    loadCurrent();
    return TRI_ERROR_NO_ERROR;
  }

  {
    CONDITION_LOCKER(locker, agencyCallback->_cv);

    while (true) {
      errorMsg = *errMsg;
      resultBuilder = *resBuilder;

      if (*dbServerResult >= 0) {
        loadCurrent();
        return *dbServerResult;
      }

      if (TRI_microtime() > endTime) {
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }

      agencyCallback->executeByCallbackOrTimeout(interval);
    }
  }

  return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop an index in coordinator.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::dropIndexCoordinator(std::string const& databaseName,
                                      std::string const& collectionID,
                                      TRI_idx_iid_t iid, std::string& errorMsg,
                                      double timeout) {
  AgencyComm ac;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  auto numberOfShardsMutex = std::make_shared<Mutex>();
  auto numberOfShards = std::make_shared<int>(0);
  std::string const idString = arangodb::basics::StringUtils::itoa(iid);

  std::string const key =
      "Plan/Collections/" + databaseName + "/" + collectionID;

  AgencyCommResult res = ac.getValues(key);

  if (!res.successful()) {
    events::DropIndex(collectionID, idString,
                      TRI_ERROR_CLUSTER_READING_PLAN_AGENCY);
    return TRI_ERROR_CLUSTER_READING_PLAN_AGENCY;
  }

  velocypack::Slice previous = res.slice()[0].get(
      std::vector<std::string>({AgencyCommManager::path(), "Plan",
                                "Collections", databaseName, collectionID}));
  if (!previous.isObject()) {
    events::DropIndex(collectionID, idString,
                      TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_ASSERT(VPackObjectIterator(previous).size() > 0);

  std::string where =
      "Current/Collections/" + databaseName + "/" + collectionID;

  auto dbServerResult = std::make_shared<int>(-1);
  auto errMsg = std::make_shared<std::string>();
  std::function<bool(VPackSlice const& result)> dbServerChanged =
      [=](VPackSlice const& current) {
        int localNumberOfShards;

        {
          MUTEX_LOCKER(guard, *numberOfShardsMutex);
          localNumberOfShards = *numberOfShards;
        }

        if (localNumberOfShards == 0) {
          return false;
        }

        if (!current.isObject()) {
          return true;
        }

        VPackObjectIterator shards(current);

        if (shards.size() == (size_t)localNumberOfShards) {
          bool found = false;
          for (auto const& shard : shards) {
            VPackSlice const indexes = shard.value.get("indexes");

            if (indexes.isArray()) {
              for (auto const& v : VPackArrayIterator(indexes)) {
                if (v.isObject()) {
                  VPackSlice const k = v.get("id");
                  if (k.isString() && idString == k.copyString()) {
                    // still found the index in some shard
                    found = true;
                    break;
                  }
                }

                if (found) {
                  break;
                }
              }
            }
          }

          if (!found) {
            *dbServerResult = setErrormsg(TRI_ERROR_NO_ERROR, *errMsg);
          }
        }
        return true;
      };

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback =
      std::make_shared<AgencyCallback>(ac, where, dbServerChanged, true, false);
  _agencyCallbackRegistry->registerCallback(agencyCallback);
  TRI_DEFER(_agencyCallbackRegistry->unregisterCallback(agencyCallback));

  loadPlan();
  // It is possible that between the fetching of the planned collections
  // and the write lock we acquire below something has changed. Therefore
  // we first get the previous value and then do a compare and swap operation.

  VPackBuilder tmp;
  VPackSlice indexes;
  {
    std::shared_ptr<LogicalCollection> c =
        getCollection(databaseName, collectionID);

    READ_LOCKER(readLocker, _planProt.lock);

    if (c == nullptr) {
      return setErrormsg(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, errorMsg);
    }
    c->getIndexesVPack(tmp, false);
    indexes = tmp.slice();

    if (!indexes.isArray()) {
      // no indexes present, so we can't delete our index
      return setErrormsg(TRI_ERROR_ARANGO_INDEX_NOT_FOUND, errorMsg);
    }

    MUTEX_LOCKER(guard, *numberOfShardsMutex);
    *numberOfShards = c->numberOfShards();
  }

  bool found = false;
  VPackBuilder newIndexes;
  {
    VPackArrayBuilder newIndexesArrayBuilder(&newIndexes);
    // mop: eh....do we need a flag to mark it invalid until cache is renewed?
    // TRI_DeleteObjectJson(TRI_UNKNOWN_MEM_ZONE, collectionJson, "indexes");
    for (auto const& indexSlice : VPackArrayIterator(indexes)) {
      VPackSlice id = indexSlice.get("id");
      VPackSlice type = indexSlice.get("type");

      if (!id.isString() || !type.isString()) {
        continue;
      }
      if (idString == id.copyString()) {
        // found our index, ignore it when copying
        found = true;

        std::string const typeString = type.copyString();
        if (typeString == "primary" || typeString == "edge") {
          return setErrormsg(TRI_ERROR_FORBIDDEN, errorMsg);
        }
        continue;
      }
      newIndexes.add(indexSlice);
    }
  }
  if (!found) {
    return setErrormsg(TRI_ERROR_ARANGO_INDEX_NOT_FOUND, errorMsg);
  }

  VPackBuilder newCollectionBuilder;
  {
    VPackObjectBuilder newCollectionObjectBuilder(&newCollectionBuilder);
    for (auto const& property : VPackObjectIterator(previous)) {
      if (property.key.copyString() == "indexes") {
        newCollectionBuilder.add(property.key.copyString(), newIndexes.slice());
      } else {
        newCollectionBuilder.add(property.key.copyString(), property.value);
      }
    }
  }

  AgencyOperation newVal(key, AgencyValueOperationType::SET,
                         newCollectionBuilder.slice());
  AgencyOperation incrementVersion("Plan/Version",
                                   AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition prec(key, AgencyPrecondition::Type::VALUE, previous);
  AgencyWriteTransaction trx({newVal, incrementVersion}, prec);
  AgencyCommResult result = ac.sendTransactionWithFailover(trx, 0.0);

  if (!result.successful()) {
    events::DropIndex(collectionID, idString,
                      TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN);
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN,
                       errorMsg);
  }

  // load our own cache:
  loadPlan();
  if (*numberOfShards == 0) {
    loadCurrent();
    return TRI_ERROR_NO_ERROR;
  }

  {
    MUTEX_LOCKER(guard, *numberOfShardsMutex);
    TRI_ASSERT(*numberOfShards > 0);
  }

  {
    CONDITION_LOCKER(locker, agencyCallback->_cv);

    while (true) {
      errorMsg = *errMsg;

      if (*dbServerResult >= 0) {
        loadCurrent();
        events::DropIndex(collectionID, idString, *dbServerResult);
        return *dbServerResult;
      }

      if (TRI_microtime() > endTime) {
        events::DropIndex(collectionID, idString, TRI_ERROR_CLUSTER_TIMEOUT);
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }

      agencyCallback->executeByCallbackOrTimeout(interval);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about servers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixServers = "Current/ServersRegistered";

void ClusterInfo::loadServers() {
  ++_serversProt.wantedVersion;  // Indicate that after *NOW* somebody has to
                                 // reread from the agency!
  MUTEX_LOCKER(mutexLocker, _serversProt.mutex);
  uint64_t storedVersion = _serversProt.wantedVersion;  // this is the version
  // we will set in the end
  if (_serversProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result = _agency.getValues(prefixServers);

  if (result.successful()) {
    velocypack::Slice serversRegistered =
        result.slice()[0].get(std::vector<std::string>(
            {AgencyCommManager::path(), "Current", "ServersRegistered"}));

    if (serversRegistered.isObject()) {
      decltype(_servers) newServers;

      for (auto const& res : VPackObjectIterator(serversRegistered)) {
        velocypack::Slice slice = res.value;
        if (slice.isObject() && slice.hasKey("endpoint")) {
          std::string server =
              arangodb::basics::VelocyPackHelper::getStringValue(
                  slice, "endpoint", "");
          newServers.emplace(std::make_pair(res.key.copyString(), server));
        }
      }

      // Now set the new value:
      {
        WRITE_LOCKER(writeLocker, _serversProt.lock);
        _servers.swap(newServers);
        _serversProt.doneVersion = storedVersion;
        _serversProt.isValid = true;  // will never be reset to false
      }
      return;
    }
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixServers
      << " httpCode: " << result.httpCode()
      << " errorCode: " << result.errorCode()
      << " errorMessage: " << result.errorMessage()
      << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the endpoint of a server from its ID.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerEndpoint(ServerID const& serverID) {
  int tries = 0;

  if (!_serversProt.isValid) {
    loadServers();
    tries++;
  }

  while (true) {
    {
      READ_LOCKER(readLocker, _serversProt.lock);
      // _servers is a map-type <ServerId, std::string>
      auto it = _servers.find(serverID);

      if (it != _servers.end()) {
        return (*it).second;
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must call loadServers outside the lock
    loadServers();
  }

  return std::string("");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the ID of a server from its endpoint.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerName(std::string const& endpoint) {
  int tries = 0;

  if (!_serversProt.isValid) {
    loadServers();
    tries++;
  }

  while (true) {
    {
      READ_LOCKER(readLocker, _serversProt.lock);
      for (auto const& it : _servers) {
        if (it.second == endpoint) {
          return it.first;
        }
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must call loadServers outside the lock
    loadServers();
  }

  return std::string("");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about all coordinators from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrentCoordinators = "Current/Coordinators";

void ClusterInfo::loadCurrentCoordinators() {
  ++_coordinatorsProt.wantedVersion;  // Indicate that after *NOW* somebody
                                      // has to reread from the agency!
  MUTEX_LOCKER(mutexLocker, _coordinatorsProt.mutex);
  uint64_t storedVersion = _coordinatorsProt.wantedVersion;  // this is the
  // version we will set in the end
  if (_coordinatorsProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result = _agency.getValues(prefixCurrentCoordinators);

  if (result.successful()) {
    velocypack::Slice currentCoordinators =
        result.slice()[0].get(std::vector<std::string>(
            {AgencyCommManager::path(), "Current", "Coordinators"}));

    if (currentCoordinators.isObject()) {
      decltype(_coordinators) newCoordinators;

      for (auto const& coordinator : VPackObjectIterator(currentCoordinators)) {
        newCoordinators.emplace(std::make_pair(coordinator.key.copyString(),
                                               coordinator.value.copyString()));
      }

      // Now set the new value:
      {
        WRITE_LOCKER(writeLocker, _coordinatorsProt.lock);
        _coordinators.swap(newCoordinators);
        _coordinatorsProt.doneVersion = storedVersion;
        _coordinatorsProt.isValid = true;  // will never be reset to false
      }
      return;
    }
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixCurrentCoordinators
      << " httpCode: " << result.httpCode()
      << " errorCode: " << result.errorCode()
      << " errorMessage: " << result.errorMessage()
      << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about all DBservers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixCurrentDBServers = "Current/DBServers";
static std::string const prefixTarget = "Target";

void ClusterInfo::loadCurrentDBServers() {
  ++_DBServersProt.wantedVersion;  // Indicate that after *NOW* somebody has to
                                   // reread from the agency!
  MUTEX_LOCKER(mutexLocker, _DBServersProt.mutex);
  uint64_t storedVersion = _DBServersProt.wantedVersion;  // this is the version
  // we will set in the end
  if (_DBServersProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result = _agency.getValues(prefixCurrentDBServers);
  AgencyCommResult target = _agency.getValues(prefixTarget);

  if (result.successful() && target.successful()) {
    velocypack::Slice currentDBServers;
    velocypack::Slice failedDBServers;
    velocypack::Slice cleanedDBServers;

    if (result.slice().length() > 0) {
      currentDBServers = result.slice()[0].get(std::vector<std::string>(
          {AgencyCommManager::path(), "Current", "DBServers"}));
    }
    if (target.slice().length() > 0) {
      failedDBServers =
        target.slice()[0].get(std::vector<std::string>(
              {AgencyCommManager::path(), "Target", "FailedServers"}));
      cleanedDBServers =
        target.slice()[0].get(std::vector<std::string>(
              {AgencyCommManager::path(), "Target", "CleanedOutServers"}));
    }
    if (currentDBServers.isObject() && failedDBServers.isObject()) {
      decltype(_DBServers) newDBServers;

      for (auto const& dbserver : VPackObjectIterator(currentDBServers)) {
        bool found = false;
        if (failedDBServers.isObject()) {
          for (auto const& failedServer :
               VPackObjectIterator(failedDBServers)) {
            if (dbserver.key == failedServer.key) {
              found = true;
              break;
            }
          }
        }
        if (found) {
          continue;
        }

        if (cleanedDBServers.isArray()) {
          bool found = false;
          for (auto const& cleanedServer :
               VPackArrayIterator(cleanedDBServers)) {
            if (dbserver.key == cleanedServer) {
              found = true;
              continue;
            }
          }
          if (found) {
            continue;
          }
        }

        newDBServers.emplace(std::make_pair(dbserver.key.copyString(),
                                            dbserver.value.copyString()));
      }

      // Now set the new value:
      {
        WRITE_LOCKER(writeLocker, _DBServersProt.lock);
        _DBServers.swap(newDBServers);
        _DBServersProt.doneVersion = storedVersion;
        _DBServersProt.isValid = true;  // will never be reset to false
      }
      return;
    }
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixCurrentDBServers
      << " httpCode: " << result.httpCode()
      << " errorCode: " << result.errorCode()
      << " errorMessage: " << result.errorMessage()
      << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of all DBServers in the cluster that have
/// currently registered
////////////////////////////////////////////////////////////////////////////////

std::vector<ServerID> ClusterInfo::getCurrentDBServers() {
  std::vector<ServerID> result;
  int tries = 0;

  if (!_DBServersProt.isValid) {
    loadCurrentDBServers();
    tries++;
  }
  while (true) {
    {
      // return a consistent state of servers
      READ_LOCKER(readLocker, _DBServersProt.lock);

      result.reserve(_DBServers.size());

      for (auto& it : _DBServers) {
        result.emplace_back(it.first);
      }

      return result;
    }

    if (++tries >= 2) {
      break;
    }

    // loadCurrentDBServers needs the write lock
    loadCurrentDBServers();
  }

  // note that the result will be empty if we get here
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the servers who are responsible for a shard (one leader
/// and multiple followers)
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<std::vector<ServerID>> ClusterInfo::getResponsibleServer(
    ShardID const& shardID) {
  int tries = 0;

  if (!_currentProt.isValid) {
    loadCurrent();
    tries++;
  }

  while (true) {
    {
      READ_LOCKER(readLocker, _currentProt.lock);
      // _shardIds is a map-type <ShardId,
      // std::shared_ptr<std::vector<ServerId>>>
      auto it = _shardIds.find(shardID);

      if (it != _shardIds.end()) {
        auto serverList = (*it).second;
        if (serverList != nullptr && serverList->size() > 0 &&
            (*serverList)[0].size() > 0 && (*serverList)[0][0] == '_') {
          // This is a temporary situation in which the leader has already
          // resigned, let's wait half a second and try again.
          --tries;
          LOG_TOPIC(INFO, Logger::CLUSTER)
              << "getResponsibleServer: found resigned leader,"
              << "waiting for half a second...";
          usleep(500000);
        } else {
          return (*it).second;
        }
      }
    }

    if (++tries >= 2) {
      break;
    }

    // must load collections outside the lock
    loadCurrent();
  }

  return std::make_shared<std::vector<ServerID>>();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the shard list of a collection, sorted numerically
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<std::vector<ShardID>> ClusterInfo::getShardList(
    CollectionID const& collectionID) {
  if (!_planProt.isValid) {
    loadPlan();
  }

  int tries = 0;
  while (true) {
    {
      // Get the sharding keys and the number of shards:
      READ_LOCKER(readLocker, _planProt.lock);
      // _shards is a map-type <CollectionId, shared_ptr<vector<string>>>
      auto it = _shards.find(collectionID);

      if (it != _shards.end()) {
        return it->second;
      }
    }
    if (++tries >= 2) {
      return std::make_shared<std::vector<ShardID>>();
    }
    loadPlan();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the shard that is responsible for a document, which is given
/// as a VelocyPackSlice.
///
/// There are two modes, one assumes that the document is given as a
/// whole (`docComplete`==`true`), in this case, the non-existence of
/// values for some of the sharding attributes is silently ignored
/// and treated as if these values were `null`. In the second mode
/// (`docComplete`==false) leads to an error which is reported by
/// returning TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, which is the only
/// error code that can be returned.
///
/// In either case, if the collection is found, the variable
/// shardID is set to the ID of the responsible shard and the flag
/// `usesDefaultShardingAttributes` is used set to `true` if and only if
/// `_key` is the one and only sharding attribute.
////////////////////////////////////////////////////////////////////////////////

#ifndef USE_ENTERPRISE
int ClusterInfo::getResponsibleShard(LogicalCollection* collInfo,
                                     VPackSlice slice, bool docComplete,
                                     ShardID& shardID,
                                     bool& usesDefaultShardingAttributes,
                                     std::string const& key) {
  // Note that currently we take the number of shards and the shardKeys
  // from Plan, since they are immutable. Later we will have to switch
  // this to Current, when we allow to add and remove shards.
  if (!_planProt.isValid) {
    loadPlan();
  }

  int tries = 0;
  std::shared_ptr<std::vector<std::string>> shardKeysPtr;
  std::shared_ptr<std::vector<ShardID>> shards;
  bool found = false;
  CollectionID collectionId = std::to_string(collInfo->planId());

  while (true) {
    {
      // Get the sharding keys and the number of shards:
      READ_LOCKER(readLocker, _planProt.lock);
      // _shards is a map-type <CollectionId, shared_ptr<vector<string>>>
      auto it = _shards.find(collectionId);

      if (it != _shards.end()) {
        shards = it->second;
        // _shardKeys is a map-type <CollectionID, shared_ptr<vector<string>>>
        auto it2 = _shardKeys.find(collectionId);
        if (it2 != _shardKeys.end()) {
          shardKeysPtr = it2->second;
          usesDefaultShardingAttributes =
              shardKeysPtr->size() == 1 &&
              shardKeysPtr->at(0) == StaticStrings::KeyString;
          found = true;
          break;  // all OK
        }
      }
    }
    if (++tries >= 2) {
      break;
    }
    loadPlan();
  }

  if (!found) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  int error = TRI_ERROR_NO_ERROR;
  uint64_t hash = arangodb::basics::VelocyPackHelper::hashByAttributes(
      slice, *shardKeysPtr, docComplete, error, key);
  static char const* magicPhrase =
      "Foxx you have stolen the goose, give she back again!";
  static size_t const len = 52;
  // To improve our hash function:
  hash = TRI_FnvHashBlock(hash, magicPhrase, len);

  shardID = shards->at(hash % shards->size());
  return error;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of coordinator server names
////////////////////////////////////////////////////////////////////////////////

std::vector<ServerID> ClusterInfo::getCurrentCoordinators() {
  std::vector<ServerID> result;
  int tries = 0;

  if (!_coordinatorsProt.isValid) {
    loadCurrentCoordinators();
    tries++;
  }
  while (true) {
    {
      // return a consistent state of servers
      READ_LOCKER(readLocker, _coordinatorsProt.lock);

      result.reserve(_coordinators.size());

      for (auto& it : _coordinators) {
        result.emplace_back(it.first);
      }

      return result;
    }

    if (++tries >= 2) {
      break;
    }

    // loadCurrentCoordinators needs the write lock
    loadCurrentCoordinators();
  }

  // note that the result will be empty if we get here
  return result;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief invalidate plan
//////////////////////////////////////////////////////////////////////////////

void ClusterInfo::invalidatePlan() {
  {
    WRITE_LOCKER(writeLocker, _planProt.lock);
    _planProt.isValid = false;
  }
  {
    WRITE_LOCKER(writeLocker, _planProt.lock);
    _planProt.isValid = false;
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief invalidate current
//////////////////////////////////////////////////////////////////////////////

void ClusterInfo::invalidateCurrent() {
  {
    WRITE_LOCKER(writeLocker, _serversProt.lock);
    _serversProt.isValid = false;
  }
  {
    WRITE_LOCKER(writeLocker, _DBServersProt.lock);
    _DBServersProt.isValid = false;
  }
  {
    WRITE_LOCKER(writeLocker, _coordinatorsProt.lock);
    _coordinatorsProt.isValid = false;
  }
  {
    WRITE_LOCKER(writeLocker, _currentProt.lock);
    _currentProt.isValid = false;
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get current "Plan" structure
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> ClusterInfo::getPlan() {
  if (!_planProt.isValid) {
    loadPlan();
  }
  READ_LOCKER(readLocker, _planProt.lock);
  return _plan;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get current "Current" structure
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> ClusterInfo::getCurrent() {
  if (!_currentProt.isValid) {
    loadCurrent();
  }
  READ_LOCKER(readLocker, _currentProt.lock);
  return _current;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get information about current followers of a shard.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<std::vector<ServerID> const> FollowerInfo::get() {
  MUTEX_LOCKER(locker, _mutex);
  return _followers;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief change JSON under
/// Current/Collection/<DB-name>/<Collection-ID>/<shard-ID>
/// to add or remove a serverID, if add flag is true, the entry is added
/// (if it is not yet there), otherwise the entry is removed (if it was
/// there).
////////////////////////////////////////////////////////////////////////////////

static VPackBuilder newShardEntry(VPackSlice oldValue, ServerID const& sid,
                                  bool add) {
  VPackBuilder newValue;
  VPackSlice servers;
  {
    VPackObjectBuilder b(&newValue);
    // Now need to find the `servers` attribute, which is a list:
    for (auto const& it : VPackObjectIterator(oldValue)) {
      if (it.key.isEqualString("servers")) {
        servers = it.value;
      } else {
        newValue.add(it.key);
        newValue.add(it.value);
      }
    }
    newValue.add(VPackValue("servers"));
    if (servers.isArray() && servers.length() > 0) {
      VPackArrayBuilder bb(&newValue);
      newValue.add(servers[0]);
      VPackArrayIterator it(servers);
      bool done = false;
      for (++it; it.valid(); ++it) {
        if ((*it).isEqualString(sid)) {
          if (add) {
            newValue.add(*it);
            done = true;
          }
        } else {
          newValue.add(*it);
        }
      }
      if (add && !done) {
        newValue.add(VPackValue(sid));
      }
    } else {
      VPackArrayBuilder bb(&newValue);
      newValue.add(VPackValue(ServerState::instance()->getId()));
      if (add) {
        newValue.add(VPackValue(sid));
      }
    }
  }
  return newValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a follower to a shard, this is only done by the server side
/// of the "get-in-sync" capabilities. This reports to the agency under
/// `/Current` but in asynchronous "fire-and-forget" way.
////////////////////////////////////////////////////////////////////////////////

void FollowerInfo::add(ServerID const& sid) {
  MUTEX_LOCKER(locker, _mutex);

  // Fully copy the vector:
  auto v = std::make_shared<std::vector<ServerID>>(*_followers);
  v->push_back(sid);  // add a single entry
  _followers = v;     // will cast to std::vector<ServerID> const
  // Now tell the agency, path is
  //   Current/Collections/<dbName>/<collectionID>/<shardID>
  std::string path = "Current/Collections/";
  path += _docColl->vocbase()->name();
  path += "/";
  path += std::to_string(_docColl->planId());
  path += "/";
  path += _docColl->name();
  AgencyComm ac;
  double startTime = TRI_microtime();
  bool success = false;
  do {
    AgencyCommResult res = ac.getValues(path);

    if (res.successful()) {
      velocypack::Slice currentEntry =
          res.slice()[0].get(std::vector<std::string>(
              {AgencyCommManager::path(), "Current", "Collections",
               _docColl->vocbase()->name(), std::to_string(_docColl->planId()),
               _docColl->name()}));

      if (!currentEntry.isObject()) {
        LOG_TOPIC(ERR, Logger::CLUSTER)
            << "FollowerInfo::add, did not find object in " << path;
        if (!currentEntry.isNone()) {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Found: " << currentEntry.toJson();
        }
      } else {
        auto newValue = newShardEntry(currentEntry, sid, true);
        std::string key = "Current/Collections/" + _docColl->vocbase()->name() +
                          "/" + std::to_string(_docColl->planId()) + "/" +
                          _docColl->name();
        AgencyWriteTransaction trx;
        trx.preconditions.push_back(AgencyPrecondition(
            key, AgencyPrecondition::Type::VALUE, currentEntry));
        trx.operations.push_back(AgencyOperation(
            key, AgencyValueOperationType::SET, newValue.slice()));
        trx.operations.push_back(AgencyOperation(
            "Current/Version", AgencySimpleOperationType::INCREMENT_OP));
        AgencyCommResult res2 = ac.sendTransactionWithFailover(trx);
        if (res2.successful()) {
          success = true;
          break;  //
        } else {
          LOG_TOPIC(WARN, Logger::CLUSTER)
              << "FollowerInfo::add, could not cas key " << path;
        }
      }
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER) << "FollowerInfo::add, could not read "
                                      << path << " in agency.";
    }
    usleep(500000);
  } while (TRI_microtime() < startTime + 30);
  if (!success) {
    LOG_TOPIC(ERR, Logger::CLUSTER)
        << "FollowerInfo::add, timeout in agency operation for key " << path;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a follower from a shard, this is only done by the
/// server if a synchronous replication request fails. This reports to
/// the agency under `/Current` but in asynchronous "fire-and-forget"
/// way. The method fails silently, if the follower information has
/// since been dropped (see `dropFollowerInfo` below).
////////////////////////////////////////////////////////////////////////////////

void FollowerInfo::remove(ServerID const& sid) {
  MUTEX_LOCKER(locker, _mutex);

  auto v = std::make_shared<std::vector<ServerID>>();
  v->reserve(_followers->size() - 1);
  for (auto const& i : *_followers) {
    if (i != sid) {
      v->push_back(i);
    }
  }
  _followers = v;  // will cast to std::vector<ServerID> const
  // Now tell the agency, path is
  //   Current/Collections/<dbName>/<collectionID>/<shardID>
  std::string path = "Current/Collections/";
  path += _docColl->vocbase()->name();
  path += "/";
  path += std::to_string(_docColl->planId());
  path += "/";
  path += _docColl->name();
  AgencyComm ac;
  double startTime = TRI_microtime();
  bool success = false;
  do {
    AgencyCommResult res = ac.getValues(path);
    if (res.successful()) {
      velocypack::Slice currentEntry =
          res.slice()[0].get(std::vector<std::string>(
              {AgencyCommManager::path(), "Current", "Collections",
               _docColl->vocbase()->name(), std::to_string(_docColl->planId()),
               _docColl->name()}));

      if (!currentEntry.isObject()) {
        LOG_TOPIC(ERR, Logger::CLUSTER)
            << "FollowerInfo::remove, did not find object in " << path;
        if (!currentEntry.isNone()) {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Found: " << currentEntry.toJson();
        }
      } else {
        auto newValue = newShardEntry(currentEntry, sid, false);
        std::string key = "Current/Collections/" + _docColl->vocbase()->name() +
                          "/" + std::to_string(_docColl->planId()) + "/" +
                          _docColl->name();
        AgencyWriteTransaction trx;
        trx.preconditions.push_back(AgencyPrecondition(
            key, AgencyPrecondition::Type::VALUE, currentEntry));
        trx.operations.push_back(AgencyOperation(
            key, AgencyValueOperationType::SET, newValue.slice()));
        trx.operations.push_back(AgencyOperation(
            "Current/Version", AgencySimpleOperationType::INCREMENT_OP));
        AgencyCommResult res2 = ac.sendTransactionWithFailover(trx);
        if (res2.successful()) {
          success = true;
          break;  //
        } else {
          LOG_TOPIC(WARN, Logger::CLUSTER)
              << "FollowerInfo::remove, could not cas key " << path;
        }
      }
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER) << "FollowerInfo::remove, could not read "
                                      << path << " in agency.";
    }
    usleep(500000);
  } while (TRI_microtime() < startTime + 30);
  if (!success) {
    LOG_TOPIC(ERR, Logger::CLUSTER)
        << "FollowerInfo::remove, timeout in agency operation for key " << path;
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief clear follower list, no changes in agency necesary
//////////////////////////////////////////////////////////////////////////////

void FollowerInfo::clear() {
  MUTEX_LOCKER(locker, _mutex);
  auto v = std::make_shared<std::vector<ServerID>>();
  _followers = v;  // will cast to std::vector<ServerID> const
}
