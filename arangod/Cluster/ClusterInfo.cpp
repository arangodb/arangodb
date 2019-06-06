////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/hashes.h"
#include "Cluster/ClusterHelpers.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/HttpResponse.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Utils/Events.h"
#include "VocBase/LogicalCollection.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/VocBase/SmartVertexCollection.h"
#include "Enterprise/VocBase/VirtualCollection.h"
#endif

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

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
  return arangodb::basics::VelocyPackHelper::getBooleanValue(slice, "error", false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the error message from a JSON
////////////////////////////////////////////////////////////////////////////////

static std::string extractErrorMessage(std::string const& shardId, VPackSlice const& slice) {
  std::string msg = " shardID:" + shardId + ": ";

  // add error message text
  msg += arangodb::basics::VelocyPackHelper::getStringValue(slice,
                                                            "errorMessage", "");

  // add error number
  if (slice.hasKey(StaticStrings::ErrorNum)) {
    VPackSlice const errorNum = slice.get(StaticStrings::ErrorNum);
    if (errorNum.isNumber()) {
      msg += " (errNum=" +
             arangodb::basics::StringUtils::itoa(errorNum.getNumericValue<uint32_t>()) +
             ")";
    }
  }

  return msg;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an empty collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::CollectionInfoCurrent(uint64_t currentVersion)
    : _currentVersion(currentVersion) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a collection info object
////////////////////////////////////////////////////////////////////////////////

CollectionInfoCurrent::~CollectionInfoCurrent() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the clusterinfo instance
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::createInstance(AgencyCallbackRegistry* agencyCallbackRegistry) {
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
    : _agency(),
      _agencyCallbackRegistry(agencyCallbackRegistry),
      _planVersion(0),
      _currentVersion(0),
      _uniqid() {
  _uniqid._currentValue = 1ULL;
  _uniqid._upperValue = 0ULL;

  // Actual loading into caches is postponed until necessary
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a cluster info object
////////////////////////////////////////////////////////////////////////////////

ClusterInfo::~ClusterInfo() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup method which frees cluster-internal shared ptrs on shutdown
////////////////////////////////////////////////////////////////////////////////

void ClusterInfo::cleanup() {
  ClusterInfo* theInstance = instance();
  if (theInstance == nullptr) {
    return;
  }

  theInstance->_plannedCollections.clear();
  theInstance->_shards.clear();
  theInstance->_shardKeys.clear();
  theInstance->_shardIds.clear();
  theInstance->_currentCollections.clear();
}

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
  loadCurrentMappings();
  loadPlan();
  loadCurrent();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask whether a cluster database exists
////////////////////////////////////////////////////////////////////////////////

bool ClusterInfo::doesDatabaseExist(DatabaseID const& databaseID, bool reload) {
  int tries = 0;

  if (reload || !_planProt.isValid || !_currentProt.isValid || !_DBServersProt.isValid) {
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

  if (_clusterId.empty()) {
    loadClusterId();
  }

  if (reload || !_planProt.isValid || !_currentProt.isValid || !_DBServersProt.isValid) {
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

/// @brief Load cluster ID
void ClusterInfo::loadClusterId() {
  // Contact agency for /<prefix>/Cluster
  AgencyCommResult result = _agency.getValues("Cluster");

  // Parse
  if (result.successful()) {
    VPackSlice slice = result.slice()[0].get(
        std::vector<std::string>({AgencyCommManager::path(), "Cluster"}));
    if (slice.isString()) {
      _clusterId = slice.copyString();
    }
  }
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

  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "loadPlan: wantedVersion=" << storedVersion
                                    << ", doneVersion=" << _planProt.doneVersion;
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
      uint64_t newPlanVersion = 0;
      VPackSlice planVersionSlice = planSlice.get("Version");
      if (planVersionSlice.isNumber()) {
        try {
          newPlanVersion = planVersionSlice.getNumber<uint64_t>();
        } catch (...) {
        }
      }
      LOG_TOPIC(TRACE, Logger::CLUSTER) << "loadPlan: newPlanVersion=" << newPlanVersion;
      if (newPlanVersion == 0) {
        LOG_TOPIC(WARN, Logger::CLUSTER) << "Attention: /arango/Plan/Version "
                                            "in the agency is not set or not "
                                            "a positive number.";
      }
      decltype(_plannedDatabases) newDatabases;
      decltype(_plannedCollections) newCollections;  // map<string /*database id*/
                                                     //    ,map<string /*collection id*/
                                                     //        ,shared_ptr<LogicalCollection>
                                                     //        >
                                                     //    >
      decltype(_shards) newShards;
      decltype(_shardServers) newShardServers;
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

      //{
      //  "_system": {
      //    "3010001": {
      //      "deleted": false,
      //      "doCompact": true,
      //      "id": "3010001",
      //      "indexBuckets": 8,
      //      "indexes": [
      //        {
      //          "fields": [
      //            "_key"
      //          ],
      //          "id": "0",
      //          "sparse": false,
      //          "type": "primary",
      //          "unique": true
      //        }
      //      ],
      //      "isSmart": false,
      //      "isSystem": true,
      //      "isVolatile": false,
      //      "journalSize": 1048576,
      //      "keyOptions": {
      //        "allowUserKeys": true,
      //        "lastValue": 0,
      //        "type": "traditional"
      //      },
      //      "name": "_graphs",
      //      "numberOfSh ards": 1,
      //      "path": "",
      //      "replicationFactor": 2,
      //      "shardKeys": [
      //        "_key"
      //      ],
      //      "shards": {
      //        "s3010002": [
      //          "PRMR-bf44d6fe-e31c-4b09-a9bf-e2df6c627999",
      //          "PRMR-11a29830-5aca-454b-a2c3-dac3a08baca1"
      //        ]
      //      },
      //      "status": 3,
      //      "statusString": "loaded",
      //      "type": 2,
      //      "waitForSync": false
      //    },...
      //  },...
      //}

      databasesSlice = planSlice.get("Collections");  // format above
      if (databasesSlice.isObject()) {
        bool isCoordinator = ServerState::instance()->isCoordinator();
        for (auto const& databasePairSlice : VPackObjectIterator(databasesSlice)) {
          VPackSlice const& collectionsSlice = databasePairSlice.value;
          if (!collectionsSlice.isObject()) {
            continue;
          }
          DatabaseCollections databaseCollections;
          std::string const databaseName = databasePairSlice.key.copyString();
          TRI_vocbase_t* vocbase = nullptr;
          if (isCoordinator) {
            vocbase = databaseFeature->lookupDatabaseCoordinator(databaseName);
          } else {
            vocbase = databaseFeature->lookupDatabase(databaseName);
          }

          if (vocbase == nullptr) {
            // No database with this name found.
            // We have an invalid state here.
            continue;
          }

          for (auto const& collectionPairSlice : VPackObjectIterator(collectionsSlice)) {
            VPackSlice const& collectionSlice = collectionPairSlice.value;
            if (!collectionSlice.isObject()) {
              continue;
            }

            std::string const collectionId = collectionPairSlice.key.copyString();

            decltype(vocbase->lookupCollection(collectionId)->clusterIndexEstimates()) selectivityEstimates;
            double selectivityTTL = 0;
            if (isCoordinator) {
              // accessing _plannedCollections in read-only mode is safe here,
              // as there is only one place (which is this mutex-protected
              // function) that modifies _plannedCollections
              auto it = _plannedCollections.find(databaseName);
              if (it != _plannedCollections.end()) {
                auto it2 = (*it).second.find(collectionId);
                if (it2 != (*it).second.end()) {
                  auto collection = (*it2).second;
                  if (collection) {
                    selectivityEstimates =
                        collection->clusterIndexEstimates(/*do not update*/ true);
                    selectivityTTL = collection->clusterIndexEstimatesTTL();
                  }
                }
              }
            }

            try {
              std::shared_ptr<LogicalCollection> newCollection;
#ifndef USE_ENTERPRISE
              newCollection =
                  std::make_shared<LogicalCollection>(vocbase, collectionSlice, true);
#else
              VPackSlice isSmart = collectionSlice.get("isSmart");
              if (isSmart.isTrue()) {
                VPackSlice type = collectionSlice.get("type");
                if (type.isInteger() && type.getUInt() == TRI_COL_TYPE_EDGE) {
                  newCollection =
                      std::make_shared<VirtualSmartEdgeCollection>(vocbase, collectionSlice);
                } else {
                  newCollection =
                      std::make_shared<SmartVertexCollection>(vocbase, collectionSlice);
                }
              } else {
                newCollection =
                    std::make_shared<LogicalCollection>(vocbase, collectionSlice, true);
              }
#endif
              newCollection->setPlanVersion(newPlanVersion);
              std::string const collectionName = newCollection->name();
              if (isCoordinator && !selectivityEstimates.empty()) {
                LOG_TOPIC(TRACE, Logger::CLUSTER) << "copy index estimates";
                newCollection->clusterIndexEstimates(std::move(selectivityEstimates));
                newCollection->clusterIndexEstimatesTTL(selectivityTTL);
                for (auto i : newCollection->getIndexes()) {
                  i->updateClusterEstimate();
                }
              }
              // mop: register with name as well as with id
              databaseCollections.emplace(std::make_pair(collectionName, newCollection));
              databaseCollections.emplace(std::make_pair(collectionId, newCollection));

              auto shardKeys =
                  std::make_shared<std::vector<std::string>>(newCollection->shardKeys());
              newShardKeys.insert(make_pair(collectionId, shardKeys));

              auto shardIDs = newCollection->shardIds();
              auto shards = std::make_shared<std::vector<std::string>>();
              for (auto const& p : *shardIDs) {
                shards->push_back(p.first);
                newShardServers.emplace(p.first, p.second);
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
              LOG_TOPIC(ERR, Logger::AGENCY)
                  << "Failed to load information for collection '"
                  << collectionId << "': " << ex.what()
                  << ". invalid information in plan. The"
                     "collection will be ignored for now and the invalid "
                     "information"
                     "will be repaired. VelocyPack: "
                  << collectionSlice.toJson();

              TRI_ASSERT(false);
              continue;
            } catch (...) {
              // The plan contains invalid collection information.
              // This should not happen in healthy situations.
              // If it happens in unhealthy situations the
              // cluster should not fail.
              LOG_TOPIC(ERR, Logger::AGENCY)
                  << "Failed to load information for collection '" << collectionId
                  << ". invalid information in plan. The collection will "
                     "be ignored for now and the invalid information will "
                     "be repaired. VelocyPack: "
                  << collectionSlice.toJson();

              TRI_ASSERT(false);
              continue;
            }
          }

          newCollections.emplace(std::make_pair(databaseName, databaseCollections));
          swapCollections = true;
        }
      }

      WRITE_LOCKER(writeLocker, _planProt.lock);
      _plan = planBuilder;
      _planVersion = newPlanVersion;
      if (swapDatabases) {
        _plannedDatabases.swap(newDatabases);
      }
      if (swapCollections) {
        _plannedCollections.swap(newCollections);
        _shards.swap(newShards);
        _shardKeys.swap(newShardKeys);
        _shardServers.swap(newShardServers);
      }
      _planProt.doneVersion = storedVersion;
      _planProt.isValid = true;
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER) << "\"Plan\" is not an object in agency";
    }
    LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "loadPlan done: wantedVersion=" << storedVersion
      << ", doneVersion=" << _planProt.doneVersion;
    return;
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixPlan
      << " httpCode: " << result.httpCode() << " errorCode: " << result.errorCode()
      << " errorMessage: " << result.errorMessage() << " body: " << result.body();
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

  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "loadCurrent: wantedVersion: "
    << _currentProt.wantedVersion;

  // Now contact the agency:
  AgencyCommResult result = _agency.getValues(prefixCurrent);

  if (result.successful()) {
    velocypack::Slice slice = result.slice()[0].get(
        std::vector<std::string>({AgencyCommManager::path(), "Current"}));

    auto currentBuilder = std::make_shared<VPackBuilder>();
    currentBuilder->add(slice);

    VPackSlice currentSlice = currentBuilder->slice();

    if (currentSlice.isObject()) {
      uint64_t newCurrentVersion = 0;
      VPackSlice currentVersionSlice = currentSlice.get("Version");
      if (currentVersionSlice.isNumber()) {
        try {
          newCurrentVersion = currentVersionSlice.getNumber<uint64_t>();
        } catch (...) {
        }
      }
      if (newCurrentVersion == 0) {
        LOG_TOPIC(WARN, Logger::CLUSTER)
            << "Attention: /arango/Current/Version in the agency is not set or "
               "not a positive number.";
      }

      decltype(_currentDatabases) newDatabases;
      decltype(_currentCollections) newCollections;
      decltype(_shardIds) newShardIds;

      bool swapDatabases = false;
      bool swapCollections = false;

      VPackSlice databasesSlice = currentSlice.get("Databases");
      if (databasesSlice.isObject()) {
        for (auto const& databaseSlicePair : VPackObjectIterator(databasesSlice)) {
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
          for (auto const& collectionSlice : VPackObjectIterator(databaseSlice.value)) {
            std::string const collectionName = collectionSlice.key.copyString();

            auto collectionDataCurrent =
                std::make_shared<CollectionInfoCurrent>(newCurrentVersion);
            for (auto const& shardSlice : VPackObjectIterator(collectionSlice.value)) {
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
            databaseCollections.insert(std::make_pair(collectionName, collectionDataCurrent));
          }
          newCollections.emplace(std::make_pair(databaseName, databaseCollections));
        }
        swapCollections = true;
      }

      // Now set the new value:
      WRITE_LOCKER(writeLocker, _currentProt.lock);
      _current = currentBuilder;
      _currentVersion = newCurrentVersion;
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
      _currentProt.isValid = true;
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER) << "Current is not an object!";
    }

    LOG_TOPIC(DEBUG, Logger::CLUSTER) << "loadCurrent done.";

    return;
  }

  LOG_TOPIC(ERR, Logger::CLUSTER)
      << "Error while loading " << prefixCurrent
      << " httpCode: " << result.httpCode() << " errorCode: " << result.errorCode()
      << " errorMessage: " << result.errorMessage() << " body: " << result.body();
}

/// @brief ask about a collection
/// If it is not found in the cache, the cache is reloaded once
/// if the collection is not found afterwards, this method will throw an exception

std::shared_ptr<LogicalCollection> ClusterInfo::getCollection(DatabaseID const& databaseID,
                                                              CollectionID const& collectionID) {
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
        DatabaseCollections::const_iterator it2 = (*it).second.find(collectionID);

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
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                 "Collection not found: " + collectionID +
                                     " in database " + databaseID);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ask about all collections
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<LogicalCollection>> const ClusterInfo::getCollections(DatabaseID const& databaseID) {
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
      AllCollectionsCurrent::const_iterator it = _currentCollections.find(databaseID);

      if (it != _currentCollections.end()) {
        // look up collection by id
        DatabaseCollectionsCurrent::const_iterator it2 = (*it).second.find(collectionID);

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

  return std::make_shared<CollectionInfoCurrent>(0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create database in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::createDatabaseCoordinator(std::string const& name, VPackSlice const& slice,
                                           std::string& errorMsg, double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  auto DBServers = std::make_shared<std::vector<ServerID>>(getCurrentDBServers());
  auto dbServerResult = std::make_shared<std::atomic<int>>(-1);
  std::shared_ptr<std::string> errMsg = std::make_shared<std::string>();

  std::function<bool(VPackSlice const& result)> dbServerChanged = [=](VPackSlice const& result) {
    size_t numDbServers = DBServers->size();
    if (result.isObject() && result.length() >= numDbServers) {
      // We use >= here since the number of DBservers could have increased
      // during the creation of the database and we might not yet have
      // the latest list. Thus there could be more reports than we know
      // servers.
      VPackObjectIterator dbs(result);

      std::string tmpMsg = "";
      bool tmpHaveError = false;

      for (VPackObjectIterator::ObjectPair dbserver : dbs) {
        VPackSlice slice = dbserver.value;
        if (arangodb::basics::VelocyPackHelper::getBooleanValue(slice, "error", false)) {
          tmpHaveError = true;
          tmpMsg += " DBServer:" + dbserver.key.copyString() + ":";
          tmpMsg += arangodb::basics::VelocyPackHelper::getStringValue(slice, StaticStrings::ErrorMessage,
                                                                       "");
          if (slice.hasKey(StaticStrings::ErrorNum)) {
            VPackSlice errorNum = slice.get(StaticStrings::ErrorNum);
            if (errorNum.isNumber()) {
              tmpMsg += " (errorNum=";
              tmpMsg += basics::StringUtils::itoa(errorNum.getNumericValue<uint32_t>());
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
  auto agencyCallback =
      std::make_shared<AgencyCallback>(ac, "Current/Databases/" + name,
                                       dbServerChanged, true, false);
  _agencyCallbackRegistry->registerCallback(agencyCallback);
  TRI_DEFER(_agencyCallbackRegistry->unregisterCallback(agencyCallback));

  AgencyOperation newVal("Plan/Databases/" + name, AgencyValueOperationType::SET, slice);
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition precondition("Plan/Databases/" + name,
                                  AgencyPrecondition::Type::EMPTY, true);
  AgencyWriteTransaction trx({newVal, incrementVersion}, precondition);

  res = ac.sendTransactionWithFailover(trx, realTimeout);

  if (!res.successful()) {
    if (res._statusCode == (int)arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
      return setErrormsg(TRI_ERROR_ARANGO_DUPLICATE_NAME, errorMsg);
    }
    errorMsg = std::string("Failed to create database at ") + __FILE__ + ":" +
               std::to_string(__LINE__);
    return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN, errorMsg);
  }

  // Now update our own cache of planned databases:
  loadPlan();

  {
    CONDITION_LOCKER(locker, agencyCallback->_cv);

    int count = 0;  // this counts, when we have to reload the DBServers
    while (true) {
      errorMsg = *errMsg;

      if (++count >= static_cast<int>(getReloadServerListTimeout() / interval)) {
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

      agencyCallback->executeByCallbackOrTimeout(getReloadServerListTimeout() / interval);
      if (!application_features::ApplicationServer::isRetryOK()) {
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop database in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::dropDatabaseCoordinator(std::string const& name,
                                         std::string& errorMsg, double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  auto dbServerResult = std::make_shared<std::atomic<int>>(-1);
  std::function<bool(VPackSlice const& result)> dbServerChanged = [=](VPackSlice const& result) {
    if (result.isNone() || (result.isObject() && result.length() == 0)) {
      *dbServerResult = TRI_ERROR_NO_ERROR;
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
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition databaseExists("Plan/Databases/" + name,
                                    AgencyPrecondition::Type::EMPTY, false);
  AgencyWriteTransaction trans({delPlanDatabases, delPlanCollections, incrementVersion},
                               databaseExists);

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
        return setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT, errorMsg);
      }

      if (TRI_microtime() > endTime) {
        AgencyCommResult ag = ac.getValues("/");
        if (ag.successful()) {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Agency dump:\n"
                                          << ag.slice().toJson();
        } else {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Could not get agency dump!";
        }

        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }

      agencyCallback->executeByCallbackOrTimeout(interval);
      if (!application_features::ApplicationServer::isRetryOK()) {
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create collection in coordinator, the return value is an ArangoDB
/// error code and the errorMsg is set accordingly. One possible error
/// is a timeout, a timeout of 0.0 means no timeout.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::createCollectionCoordinator(
    std::string const& databaseName, std::string const& collectionID,
    uint64_t numberOfShards, uint64_t replicationFactor, bool waitForReplication,
    VPackSlice const& json, std::string& errorMsg, double timeout) {
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

  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "createCollectionCoordinator, checking things...";

  // mop: why do these ask the agency instead of checking cluster info?
  if (!ac.exists("Plan/Databases/" + databaseName)) {
    events::CreateCollection(name, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return setErrormsg(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
  }

  if (ac.exists("Plan/Collections/" + databaseName + "/" + collectionID)) {
    events::CreateCollection(name, TRI_ERROR_CLUSTER_COLLECTION_ID_EXISTS);
    return setErrormsg(TRI_ERROR_CLUSTER_COLLECTION_ID_EXISTS, errorMsg);
  }

  // The following three are used for synchronization between the callback
  // closure and the main thread executing this function. Note that it can
  // happen that the callback is called only after we return from this
  // function!
  auto dbServerResult = std::make_shared<std::atomic<int>>(-1);
  auto errMsg = std::make_shared<std::string>();
  auto cacheMutex = std::make_shared<Mutex>();

  auto dbServers = getCurrentDBServers();
  std::function<bool(VPackSlice const& result)> dbServerChanged = [=](VPackSlice const& result) {
    MUTEX_LOCKER(locker, *cacheMutex);
    if (result.isObject() && result.length() == (size_t)numberOfShards) {
      std::string tmpError = "";
      for (auto const& p : VPackObjectIterator(result)) {
        if (arangodb::basics::VelocyPackHelper::getBooleanValue(p.value, "error", false)) {
          tmpError += " shardID:" + p.key.copyString() + ":";
          tmpError +=
              arangodb::basics::VelocyPackHelper::getStringValue(p.value,
                                                                 "errorMessage",
                                                                 "");
          if (p.value.hasKey(StaticStrings::ErrorNum)) {
            VPackSlice const errorNum = p.value.get(StaticStrings::ErrorNum);
            if (errorNum.isNumber()) {
              tmpError += " (errNum=";
              tmpError +=
                  basics::StringUtils::itoa(errorNum.getNumericValue<uint32_t>());
              tmpError += ")";
            }
          }
        }

        // wait that all followers have created our new collection
        if (tmpError.empty() && waitForReplication) {
          std::vector<ServerID> plannedServers;
          {
            READ_LOCKER(readLocker, _planProt.lock);
            auto it = _shardServers.find(p.key.copyString());
            if (it != _shardServers.end()) {
              plannedServers = (*it).second;
            } else {
              LOG_TOPIC(DEBUG, Logger::CLUSTER)
                  << "Did not find shard in _shardServers: " << p.key.copyString()
                  << ". Maybe the collection is already dropped.";
              *errMsg = "Error in creation of collection: " + p.key.copyString() +
                        ". Collection already dropped. " + __FILE__ +
                        std::to_string(__LINE__);
              *dbServerResult = TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION;
              return true;
            }
          }
          if (plannedServers.empty()) {
            LOG_TOPIC(DEBUG, Logger::CLUSTER)
                << "This should never have happened, Plan empty. Dumping "
                   "_shards in Plan:";
            for (auto const& p : _shards) {
              LOG_TOPIC(DEBUG, Logger::CLUSTER) << "Shard: " << p.first;
              for (auto const& q : *(p.second)) {
                LOG_TOPIC(DEBUG, Logger::CLUSTER) << "  Server: " << q;
              }
            }
            TRI_ASSERT(false);
          }
          std::vector<ServerID> currentServers;
          VPackSlice servers = p.value.get("servers");
          if (!servers.isArray()) {
            return true;
          }
          for (auto const& server : VPackArrayIterator(servers)) {
            if (!server.isString()) {
              return true;
            }
            currentServers.push_back(server.copyString());
          }
          if (!ClusterHelpers::compareServerLists(plannedServers, currentServers)) {
            LOG_TOPIC(DEBUG, Logger::CLUSTER)
                << "Still waiting for all servers to ACK creation of " << name
                << ". Planned: " << plannedServers << ", Current: " << currentServers;
            return true;
          }
        }
      }
      if (!tmpError.empty()) {
        *errMsg = "Error in creation of collection:" + tmpError + " " +
                  __FILE__ + std::to_string(__LINE__);
        *dbServerResult = TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION;
      } else {
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
      std::make_shared<AgencyCallback>(ac,
                                       "Current/Collections/" + databaseName +
                                           "/" + collectionID,
                                       dbServerChanged, true, false);
  _agencyCallbackRegistry->registerCallback(agencyCallback);
  TRI_DEFER(_agencyCallbackRegistry->unregisterCallback(agencyCallback));

  // We run a loop here to send the agency transaction, since there might
  // be a precondition failed, in which case we want to retry for some time:
  while (true) {
    if (TRI_microtime() > endTime) {
      LOG_TOPIC(ERR, Logger::CLUSTER)
          << "Timeout in _create collection"
          << ": database: " << databaseName << ", collId:" << collectionID
          << "\njson: " << json.toString() << "\ncould not send transaction to agency.";
      return TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN;
    }
    std::vector<AgencyOperation> opers(
        {AgencyOperation("Plan/Collections/" + databaseName + "/" + collectionID,
                         AgencyValueOperationType::SET, json),
         AgencyOperation("Plan/Version", AgencySimpleOperationType::INCREMENT_OP)});

    std::vector<AgencyPrecondition> precs;

    std::shared_ptr<ShardMap> otherCidShardMap = nullptr;
    if (json.hasKey("distributeShardsLike")) {
      auto const otherCidString = json.get("distributeShardsLike").copyString();
      if (!otherCidString.empty()) {
        otherCidShardMap = getCollection(databaseName, otherCidString)->shardIds();
        // Any of the shards locked?
        for (auto const& shard : *otherCidShardMap) {
          precs.emplace_back(AgencyPrecondition("Supervision/Shards/" + shard.first,
                                                AgencyPrecondition::Type::EMPTY, true));
        }
      }
    }

    AgencyWriteTransaction transaction(opers, precs);

    {  // we hold this mutex from now on until we have updated our cache
      // using loadPlan, this is necessary for the callback closure to
      // see the new planned state for this collection. Otherwise it cannot
      // recognize completion of the create collection operation properly:
      MUTEX_LOCKER(locker, *cacheMutex);

      auto res = ac.sendTransactionWithFailover(transaction);

      // Only if not precondition failed
      if (!res.successful()) {
        if (res.httpCode() == (int)arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
          auto result = res.slice();
          AgencyCommResult ag = ac.getValues("/");

          if (result.isArray() && result.length() > 0) {
            if (result[0].isObject()) {
              auto tres = result[0];
              if (tres.hasKey(std::vector<std::string>(
                      {AgencyCommManager::path(), "Supervision"}))) {
                for (const auto& s : VPackObjectIterator(tres.get(
                         std::vector<std::string>({AgencyCommManager::path(),
                                                   "Supervision", "Shards"})))) {
                  errorMsg += std::string("Shard ") + s.key.copyString();
                  errorMsg +=
                      " of prototype collection is blocked by supervision job ";
                  errorMsg += s.value.copyString();
                }
              }
              return TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN;
            }
          }

          if (ag.successful()) {
            LOG_TOPIC(ERR, Logger::CLUSTER) << "Agency dump:\n"
                                            << ag.slice().toJson();
          } else {
            LOG_TOPIC(ERR, Logger::CLUSTER) << "Could not get agency dump!";
          }
          // Agency is currently unhappy, try again in a few seconds:
          std::this_thread::sleep_for(std::chrono::seconds(5));
          continue;
        } else {
          errorMsg += std::string("\n") + __FILE__ + std::to_string(__LINE__);
          errorMsg += std::string("\n") + res.errorMessage();
          errorMsg += std::string("\n") + res.errorDetails();
          errorMsg += std::string("\n") + res.body();
          events::CreateCollection(name, TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN);
          return TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN;
        }
      }

      // Update our cache:
      loadPlan();
    }
    break;  // Leave loop, since we are done
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "createCollectionCoordinator, Plan changed, waiting for success...";

  bool isSmart = false;
  VPackSlice smartSlice = json.get("isSmart");
  if (smartSlice.isBool() && smartSlice.getBool()) {
    isSmart = true;
  }

  if (numberOfShards == 0 || isSmart) {
    loadCurrent();
    events::CreateCollection(name, TRI_ERROR_NO_ERROR);
    return TRI_ERROR_NO_ERROR;
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "createCollectionCoordinator, loading Plan from agency...";

  {
    while (true) {
      int tmpRes = dbServerResult->load(std::memory_order_acquire);
      if (tmpRes >= 0) {
        CONDITION_LOCKER(locker, agencyCallback->_cv);
        errorMsg = *errMsg;
        loadCurrent();
        events::CreateCollection(name, *dbServerResult);
        return tmpRes;
      }

      if (TRI_microtime() > endTime) {
        LOG_TOPIC(ERR, Logger::CLUSTER)
            << "Timeout in _create collection"
            << ": database: " << databaseName << ", collId:" << collectionID
            << "\njson: " << json.toString();
        AgencyCommResult ag = ac.getValues("");
        if (ag.successful()) {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Agency dump:\n"
                                          << ag.slice().toJson();
        } else {
          LOG_TOPIC(ERR, Logger::CLUSTER) << "Could not get agency dump!";
        }

        // Now we ought to remove the collection again in the Plan:
        AgencyOperation removeCollection("Plan/Collections/" + databaseName +
                                             "/" + collectionID,
                                         AgencySimpleOperationType::DELETE_OP);
        AgencyOperation increaseVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
        AgencyWriteTransaction transaction;

        transaction.operations.push_back(removeCollection);
        transaction.operations.push_back(increaseVersion);

        // This is a best effort, in the worst case the collection stays:
        ac.sendTransactionWithFailover(transaction);

        events::CreateCollection(name, TRI_ERROR_CLUSTER_TIMEOUT);
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }

      if (application_features::ApplicationServer::isStopping()) {
        events::CreateCollection(name, TRI_ERROR_SHUTTING_DOWN);
        return setErrormsg(TRI_ERROR_SHUTTING_DOWN, errorMsg);
      }

      {
        CONDITION_LOCKER(locker, agencyCallback->_cv);
        agencyCallback->executeByCallbackOrTimeout(interval);
      }
      if (!application_features::ApplicationServer::isRetryOK()) {
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }
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
                                           std::string& errorMsg, double timeout) {
  AgencyComm ac;
  AgencyCommResult res;

  // First check that no other collection has a distributeShardsLike
  // entry pointing to us:
  auto coll = getCollection(databaseName, collectionID);
  auto colls = getCollections(databaseName);
  std::vector<std::string> clones;
  for (std::shared_ptr<LogicalCollection> const& p : colls) {
    if (p->distributeShardsLike() == coll->name() || p->distributeShardsLike() == collectionID) {
      clones.push_back(p->name());
    }
  }

  if (!clones.empty()) {
    errorMsg +=
        "Collection must not be dropped while it is a sharding "
        "prototype for collection(s)";
    for (auto const& i : clones) {
      errorMsg += std::string(" ") + i;
    }
    errorMsg += ".";
    return TRI_ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE;
  }

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  auto dbServerResult = std::make_shared<std::atomic<int>>(-1);
  auto errMsg = std::make_shared<std::string>();

  std::function<bool(VPackSlice const& result)> dbServerChanged = [=](VPackSlice const& result) {
    if (result.isNone() || (result.isObject() && result.length() == 0)) {
      *dbServerResult = setErrormsg(TRI_ERROR_NO_ERROR, *errMsg);
    }
    return true;
  };

  // monitor the entry for the collection
  std::string const where = "Current/Collections/" + databaseName + "/" + collectionID;

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
          << "Missing shards information on dropping " << databaseName << "/"
          << collectionID;
      return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
    }
  }

  // Transact to agency
  AgencyOperation delPlanCollection("Plan/Collections/" + databaseName + "/" + collectionID,
                                    AgencySimpleOperationType::DELETE_OP);
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition precondition =
      AgencyPrecondition("Plan/Databases/" + databaseName,
                         AgencyPrecondition::Type::EMPTY, false);
  AgencyWriteTransaction trans({delPlanCollection, incrementVersion}, precondition);
  res = ac.sendTransactionWithFailover(trans);

  if (!res.successful()) {
    if (res.httpCode() == (int)arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
      LOG_TOPIC(ERR, Logger::CLUSTER)
          << "Precondition failed for this agency transaction: " << trans.toJson()
          << ", return code: " << res.httpCode();
    }
    AgencyCommResult ag = ac.getValues("");
    if (ag.successful()) {
      LOG_TOPIC(ERR, Logger::CLUSTER) << "Agency dump:\n"
                                      << ag.slice().toJson();
    } else {
      LOG_TOPIC(ERR, Logger::CLUSTER) << "Could not get agency dump!";
    }
    return TRI_ERROR_CLUSTER_COULD_NOT_DROP_COLLECTION;
  }

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
        ac.removeValues("Current/Collections/" + databaseName + "/" + collectionID, true);
        loadCurrent();
        events::DropCollection(collectionID, *dbServerResult);
        return *dbServerResult;
      }

      if (TRI_microtime() > endTime) {
        LOG_TOPIC(ERR, Logger::CLUSTER)
            << "Timeout in _drop collection (" << realTimeout << ")"
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
      if (!application_features::ApplicationServer::isRetryOK()) {
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection properties in coordinator
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::setCollectionPropertiesCoordinator(std::string const& databaseName,
                                                    std::string const& collectionID,
                                                    LogicalCollection const* info) {
  AgencyComm ac;
  AgencyCommResult res;

  AgencyPrecondition databaseExists("Plan/Databases/" + databaseName,
                                    AgencyPrecondition::Type::EMPTY, false);
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);

  res = ac.getValues("Plan/Collections/" + databaseName + "/" + collectionID);

  if (!res.successful()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  velocypack::Slice collection = res.slice()[0].get(std::vector<std::string>(
      {AgencyCommManager::path(), "Plan", "Collections", databaseName, collectionID}));

  if (!collection.isObject()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  VPackBuilder temp;
  temp.openObject();
  temp.add("waitForSync", VPackValue(info->waitForSync()));
  temp.add("replicationFactor", VPackValue(info->replicationFactor()));
  info->getPhysical()->getPropertiesVPackCoordinator(temp);
  temp.close();

  VPackBuilder builder = VPackCollection::merge(collection, temp.slice(), true);

  res.clear();

  AgencyOperation setColl("Plan/Collections/" + databaseName + "/" + collectionID,
                          AgencyValueOperationType::SET, builder.slice());

  AgencyWriteTransaction trans({setColl, incrementVersion}, databaseExists);

  res = ac.sendTransactionWithFailover(trans);

  if (res.successful()) {
    loadPlan();
    return TRI_ERROR_NO_ERROR;
  } else {
    return TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED;
  }

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set collection status in coordinator
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::setCollectionStatusCoordinator(std::string const& databaseName,
                                                std::string const& collectionID,
                                                TRI_vocbase_col_status_e status) {
  AgencyComm ac;
  AgencyCommResult res;

  AgencyPrecondition databaseExists("Plan/Databases/" + databaseName,
                                    AgencyPrecondition::Type::EMPTY, false);

  res = ac.getValues("Plan/Collections/" + databaseName + "/" + collectionID);

  if (!res.successful()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  VPackSlice col = res.slice()[0].get(std::vector<std::string>(
      {AgencyCommManager::path(), "Plan", "Collections", databaseName, collectionID}));

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

  AgencyOperation setColl("Plan/Collections/" + databaseName + "/" + collectionID,
                          AgencyValueOperationType::SET, builder.slice());
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);

  AgencyWriteTransaction trans({setColl, incrementVersion}, databaseExists);

  res = ac.sendTransactionWithFailover(trans);

  if (res.successful()) {
    loadPlan();
    return TRI_ERROR_NO_ERROR;
  } else {
    return TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED;
  }

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure an index in coordinator.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::ensureIndexCoordinator(std::string const& databaseName,
                                        std::string const& collectionID,
                                        VPackSlice const& slice, bool create,
                                        VPackBuilder& resultBuilder,
                                        std::string& errorMsg, double timeout) {
  // check index id
  uint64_t iid = 0;

  VPackSlice const idSlice = slice.get("id");
  if (idSlice.isString()) {
    // use predefined index id
    iid = arangodb::basics::StringUtils::uint64(idSlice.copyString());
  }
  if (iid == 0) {
    // no id set, create a new one!
    iid = uniqid();
  }
  std::string const idString = arangodb::basics::StringUtils::itoa(iid);

  int errorCode;
  try {
    auto start = std::chrono::steady_clock::now();
    // Keep trying for 2 minutes, if it's preconditions, which are stopping us
    do {
      resultBuilder.clear();
      errorCode =
          ensureIndexCoordinatorInner(databaseName, collectionID, idString, slice,
                                      create, resultBuilder, errorMsg, timeout);
      // Note that this function sets the errorMsg unless it is precondition
      // failed, in which case we retry, if this times out, we need to set
      // it ourselves, otherwise all is done!

      if (errorCode == TRI_ERROR_HTTP_PRECONDITION_FAILED) {
        auto diff = std::chrono::steady_clock::now() - start;
        if (diff < std::chrono::seconds(120)) {
          uint32_t wt = RandomGenerator::interval(static_cast<uint32_t>(1000));
          std::this_thread::sleep_for(std::chrono::steady_clock::duration(wt));
          continue;
        }
        errorCode = setErrormsg(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN, errorMsg);
      }
      break;
    } while (true);
  } catch (basics::Exception const& ex) {
    errorCode = ex.code();
    setErrormsg(errorCode, errorMsg);
    errorMsg += std::string(", exception: ") + ex.what();
  } catch (...) {
    errorCode = TRI_ERROR_INTERNAL;
    setErrormsg(errorCode, errorMsg);
  }

  // We get here in any case eventually, regardless of whether we have
  //   - succeeded with lookup or index creation
  //   - failed because of a timeout and rollback
  //   - some other error
  // There is nothing more to do here.

  if (!application_features::ApplicationServer::isStopping()) {
    loadPlan();
  }
  return errorCode;
}

// The following function does the actual work of index creation: Create
// in Plan, watch Current until all dbservers for all shards have done their
// bit. If this goes wrong with a timeout, the creation operation is rolled
// back. If the `create` flag is false, this is actually a lookup operation.
// In any case, no rollback has to happen in the calling function
// ClusterInfo::ensureIndexCoordinator. Note that this method here
// sets the `isBuilding` attribute to `true`, which leads to the fact
// that the index is not yet used by queries. There is code in the
// Agency Supervision which deletes this flag once everything has been
// built successfully. This is a more robust and self-repairing solution
// than if we would take out the `isBuilding` here, since it survives a
// coordinator crash and failover operations.
// Finally note that the retry loop for the case of a failed precondition
// is outside this function here in `ensureIndexCoordinator`.

int ClusterInfo::ensureIndexCoordinatorInner(std::string const& databaseName,
                                             std::string const& collectionID,
                                             std::string const& idString,
                                             VPackSlice const& slice, bool create,
                                             VPackBuilder& resultBuilder,
                                             std::string& errorMsg, double timeout) {
  AgencyComm ac;

  using namespace std::chrono;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  TRI_ASSERT(resultBuilder.isEmpty());

  std::string const planCollKey = "Plan/Collections/" + databaseName + "/" + collectionID;
  std::string const planIndexesKey = planCollKey + "/indexes";

  AgencyCommResult previous = ac.getValues(planCollKey);
  if (!previous.successful()) {
    return setErrormsg(TRI_ERROR_CLUSTER_READING_PLAN_AGENCY, errorMsg);
  }

  velocypack::Slice collection = previous.slice()[0].get(std::vector<std::string>(
      {AgencyCommManager::path(), "Plan", "Collections", databaseName, collectionID}));

  if (!collection.isObject()) {
    return setErrormsg(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, errorMsg);
  }

  const size_t numberOfShards =
      basics::VelocyPackHelper::readNumericValue<size_t>(collection,
                                                         "numberOfShards", 1);
  VPackSlice indexes = collection.get("indexes");
  if (indexes.isArray()) {
    auto type = slice.get("type");
    if (!type.isString()) {
      return setErrormsg(TRI_ERROR_INTERNAL, errorMsg);
    }

    for (auto const& other : VPackArrayIterator(indexes)) {
      TRI_ASSERT(other.isObject());
      if (true == arangodb::Index::Compare(slice, other)) {
        {  // found an existing index...
          // Copy over all elements in slice.
          VPackObjectBuilder b(&resultBuilder);
          resultBuilder.add(VPackObjectIterator(other));
          resultBuilder.add("isNewlyCreated", VPackValue(false));
        }
        return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
      }
    }
  }

  // no existing index found.
  if (!create) {
    TRI_ASSERT(resultBuilder.isEmpty());
    return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
  }

  // will contain the error number and message
  std::atomic<int> dbServerResult(-1);
  std::shared_ptr<std::string> errMsg = std::make_shared<std::string>();

  std::function<bool(VPackSlice const& result)> dbServerChanged = [=, &dbServerResult](
                                                                      VPackSlice const& result) {
    if (!result.isObject() || result.length() != numberOfShards) {
      return true;
    }

    size_t found = 0;
    for (auto const& shard : VPackObjectIterator(result)) {
      VPackSlice const slice = shard.value;
      if (slice.hasKey("indexes")) {
        VPackSlice const indexes = slice.get("indexes");
        if (!indexes.isArray()) {
          break;  // no list, so our index is not present. we can abort searching
        }

        for (auto const& v : VPackArrayIterator(indexes)) {
          VPackSlice const k = v.get("id");
          if (!k.isString() || idString != k.copyString()) {
            continue;  // this is not our index
          }

          // check for errors
          if (hasError(v)) {
            // Note that this closure runs with the mutex in the condition
            // variable of the agency callback, which protects writing
            // to *errMsg:
            *errMsg = extractErrorMessage(shard.key.copyString(), v);
            *errMsg = "Error during index creation: " + *errMsg;
            // Returns the specific error number if set, or the general
            // error otherwise
            int errNum = arangodb::basics::VelocyPackHelper::readNumericValue<int>(
                v, StaticStrings::ErrorNum, TRI_ERROR_ARANGO_INDEX_CREATION_FAILED);
            dbServerResult.store(errNum, std::memory_order_release);
            return true;
          }

          found++;  // found our index
          break;
        }
      }
    }

    if (found == (size_t)numberOfShards) {
      dbServerResult.store(setErrormsg(TRI_ERROR_NO_ERROR, *errMsg), std::memory_order_release);
    }

    return true;
  };

  VPackBuilder newIndexBuilder;
  {
    VPackObjectBuilder ob(&newIndexBuilder);
    // Add the new index ignoring "id"
    for (auto const& e : VPackObjectIterator(slice)) {
      TRI_ASSERT(e.key.isString());
      std::string const& key = e.key.copyString();
      if (key != "id" && key != "isBuilding") {
        ob->add(e.key);
        ob->add(e.value);
      }
    }
    if (numberOfShards > 0 &&
        !slice.get("type").isEqualString("arangosearch")) {
      ob->add("isBuilding", VPackValue(true));
    }
    ob->add("id", VPackValue(idString));
  }

  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  std::string where = "Current/Collections/" + databaseName + "/" + collectionID;
  auto agencyCallback =
      std::make_shared<AgencyCallback>(ac, where, dbServerChanged, true, false);
  _agencyCallbackRegistry->registerCallback(agencyCallback);
  TRI_DEFER(_agencyCallbackRegistry->unregisterCallback(agencyCallback));

  AgencyOperation newValue(planIndexesKey, AgencyValueOperationType::PUSH,
                           newIndexBuilder.slice());
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition oldValue(planCollKey, AgencyPrecondition::Type::VALUE, collection);
  AgencyWriteTransaction trx({newValue, incrementVersion}, oldValue);

  AgencyCommResult result = ac.sendTransactionWithFailover(trx, 0.0);

  if (!result.successful()) {
    if (result.httpCode() == (int)arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
      // Retry loop is outside!
      return TRI_ERROR_HTTP_PRECONDITION_FAILED;
    } else {
      errorMsg += " Failed to execute ";
      errorMsg += trx.toJson();
      errorMsg += " ResultCode: " + std::to_string(result.errorCode()) + " ";
      errorMsg += " HttpCode: " + std::to_string(result.httpCode()) + " ";
      errorMsg += std::string(__FILE__) + ":" + std::to_string(__LINE__);
    }
    return TRI_ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN;
  }

  // From here on we want to roll back the index creation if we run into
  // the timeout. If this coordinator crashes, the worst that can happen
  // is that the index stays in some state. In most cases, it will converge
  // against the planned state.
  loadPlan();

  if (numberOfShards == 0) {  // smart "dummy" collection has no shards
    TRI_ASSERT(collection.get("isSmart").getBool());
    {
      // Copy over all elements in slice.
      VPackObjectBuilder b(&resultBuilder);
      resultBuilder.add("isSmart", VPackValue(true));
    }
    loadCurrent();
    return setErrormsg(TRI_ERROR_NO_ERROR, errorMsg);
  }

  {
    while (!application_features::ApplicationServer::isStopping()) {
      int tmpRes = dbServerResult.load(std::memory_order_acquire);

      if (tmpRes < 0) {
        // index has not shown up in Current yet,  follow up check to
        // ensure it is still in plan (not dropped between iterations)

        AgencyCommResult result = _agency.sendTransactionWithFailover(
            AgencyReadTransaction(AgencyCommManager::path(planIndexesKey)));

        if (result.successful()) {
          auto indexes = result.slice()[0].get(
              std::vector<std::string>{AgencyCommManager::path(), "Plan",
                                       "Collections", databaseName,
                                       collectionID, "indexes"});

          bool found = false;
          if (indexes.isArray()) {
            for (size_t i = 0; i < indexes.length(); i++) {
              VPackSlice v = indexes.at(i);
              VPackSlice const k = v.get(StaticStrings::IndexId);
              if (k.isString() && idString == k.copyString()) {
                // index is still here
                found = true;
                break;
              }
            }
          }

          if (!found) {
            errorMsg = "index was dropped during creation";
            return TRI_ERROR_ARANGO_INDEX_CREATION_FAILED;
          }
        }
      }

      if (tmpRes == 0) {
        // Finally, in case all is good, remove the `isBuilding` flag
        // check that the index has appeared. Note that we have to have
        // a precondition since the collection could have been deleted
        // in the meantime:
        VPackBuilder finishedPlanIndex;
        {
          VPackObjectBuilder o(&finishedPlanIndex);
          for (auto const& entry : VPackObjectIterator(newIndexBuilder.slice())) {
            auto const key = entry.key.copyString();
            if (key != "isBuilding" && key != "isNewlyCreated") {
              finishedPlanIndex.add(entry.key.copyString(), entry.value);
            }
          }
        }
        AgencyWriteTransaction trx(
            {AgencyOperation(planIndexesKey, AgencyValueOperationType::REPLACE,
                             finishedPlanIndex.slice(), newIndexBuilder.slice()),
             AgencyOperation("Plan/Version", AgencySimpleOperationType::INCREMENT_OP)},
            AgencyPrecondition(planIndexesKey, AgencyPrecondition::Type::EMPTY, false));
        TRI_idx_iid_t indexId = arangodb::basics::StringUtils::uint64(
            newIndexBuilder.slice().get("id").copyString());
        if (!_agency.sendTransactionWithFailover(trx, 0.0).successful()) {
          // We just log the problem and move on, the Supervision will repair
          // things in due course:
          LOG_TOPIC(INFO, Logger::CLUSTER)
              << "Could not remove isBuilding flag in new index " << indexId
              << ", this will be repaired automatically.";
        }
        loadPlan();
        // Finally check if it has appeared, if not, we take another turn,
        // which does not do any harm:
        auto coll = getCollection(databaseName, collectionID);
        auto indexes = coll->getIndexes();
        if (std::any_of(indexes.begin(), indexes.end(),
                        [indexId](std::shared_ptr<arangodb::Index>& index) -> bool {
                          return indexId == index->id() && !index->isBuilding();
                        })) {
          // unregister cb before accessing errMsg
          _agencyCallbackRegistry->unregisterCallback(agencyCallback);
          loadCurrent();
          {
            // Copy over all elements in slice.
            VPackObjectBuilder b(&resultBuilder);
            resultBuilder.add(VPackObjectIterator(finishedPlanIndex.slice()));
            resultBuilder.add("isNewlyCreated", VPackValue(true));
          }
          // The mutex in the condition variable protects the access to
          // *errMsg:
          CONDITION_LOCKER(locker, agencyCallback->_cv);
          errorMsg = *errMsg;
          return tmpRes;
        }
        LOG_TOPIC(DEBUG, Logger::CLUSTER)
            << "Index "
            << indexId << " is complete, waiting for Supervision to remove isBuilding flag.";
      }

      if (tmpRes > 0 || TRI_microtime() > endTime) {
        // At this time the index creation has failed and we want to
        // roll back the plan entry, provided the collection still exists:
        AgencyWriteTransaction trx(
            std::vector<AgencyOperation>(
                {AgencyOperation(planIndexesKey, AgencyValueOperationType::ERASE,
                                 newIndexBuilder.slice()),
                 AgencyOperation("Plan/Version", AgencySimpleOperationType::INCREMENT_OP)}),
            AgencyPrecondition(planCollKey, AgencyPrecondition::Type::EMPTY, false));

        int sleepFor = 50;
        auto rollbackEndTime = steady_clock::now() + std::chrono::seconds(10);
        while (true) {
          AgencyCommResult update = _agency.sendTransactionWithFailover(trx, 0.0);
          if (update.successful()) {
            loadPlan();
            if (tmpRes < 0) {  // timeout
              errorMsg =
                  "Index could not be created within timeout, giving up and "
                  "rolling back index creation.";
              return TRI_ERROR_CLUSTER_TIMEOUT;
            }
            return tmpRes;
          }
          if (update._statusCode == TRI_ERROR_HTTP_PRECONDITION_FAILED) {
            // Collection was removed, let's break here and report outside
            break;
          }
          if (steady_clock::now() > rollbackEndTime) {
            LOG_TOPIC(ERR, Logger::CLUSTER)
                << "Couldn't roll back index creation of " << idString
                << ". Database: " << databaseName << ", Collection " << collectionID;
            if (tmpRes < 0) {  // timeout
              errorMsg =
                  "Timed out while trying to roll back index creation failure";
              return TRI_ERROR_CLUSTER_TIMEOUT;
            }
            return tmpRes;
          }
          if (sleepFor <= 2500) {
            sleepFor *= 2;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(sleepFor));
        }
        // We only get here if the collection was dropped just in the moment
        // when we wanted to roll back the index creation.
      }

      auto c = getCollection(databaseName, collectionID);
      if (!c) {
        errorMsg = "collection was dropped during ensureIndex";
        return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
      }

      {
        CONDITION_LOCKER(locker, agencyCallback->_cv);
        agencyCallback->executeByCallbackOrTimeout(interval);
      }
    }
  }

  return setErrormsg(TRI_ERROR_SHUTTING_DOWN, errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop an index in coordinator.
////////////////////////////////////////////////////////////////////////////////

int ClusterInfo::dropIndexCoordinator(std::string const& databaseName,
                                      std::string const& collectionID, TRI_idx_iid_t iid,
                                      std::string& errorMsg, double timeout) {
  AgencyComm ac;

  double const realTimeout = getTimeout(timeout);
  double const endTime = TRI_microtime() + realTimeout;
  double const interval = getPollInterval();

  auto numberOfShardsMutex = std::make_shared<Mutex>();
  auto numberOfShards = std::make_shared<std::atomic<int>>(0);
  std::string const idString = arangodb::basics::StringUtils::itoa(iid);

  std::string const key = "Plan/Collections/" + databaseName + "/" + collectionID;

  AgencyCommResult res = ac.getValues(key);

  if (!res.successful()) {
    events::DropIndex(collectionID, idString, TRI_ERROR_CLUSTER_READING_PLAN_AGENCY);
    return TRI_ERROR_CLUSTER_READING_PLAN_AGENCY;
  }

  velocypack::Slice previous = res.slice()[0].get(std::vector<std::string>(
      {AgencyCommManager::path(), "Plan", "Collections", databaseName, collectionID}));
  if (!previous.isObject()) {
    events::DropIndex(collectionID, idString, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  TRI_ASSERT(VPackObjectIterator(previous).size() > 0);

  std::string where = "Current/Collections/" + databaseName + "/" + collectionID;

  auto dbServerResult = std::make_shared<int>(-1);
  auto errMsg = std::make_shared<std::string>();
  std::function<bool(VPackSlice const& result)> dbServerChanged = [=](VPackSlice const& current) {
    MUTEX_LOCKER(guard, *numberOfShardsMutex);

    if (*numberOfShards == 0) {
      return false;
    }

    if (!current.isObject()) {
      return true;
    }

    VPackObjectIterator shards(current);

    if (shards.size() == (size_t)(*numberOfShards)) {
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
    std::shared_ptr<LogicalCollection> c = getCollection(databaseName, collectionID);

    READ_LOCKER(readLocker, _planProt.lock);

    c->getIndexesVPack(tmp, false, false);
    indexes = tmp.slice();

    if (!indexes.isArray()) {
      try {
        LOG_TOPIC(DEBUG, Logger::CLUSTER)
            << "Failed to find index " << databaseName << "/" << collectionID
            << "/" << iid << " - " << indexes.toJson();
      } catch (std::exception const& e) {
        LOG_TOPIC(DEBUG, Logger::CLUSTER)
            << "Failed to find index " << databaseName << "/" << collectionID
            << "/" << iid << " - " << e.what();
      }
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
    try {
      // it is not necessarily an error that the index is not found,
      // for example if one tries to drop a non-existing index. so we
      // do not log any warnings but just in debug mode
      LOG_TOPIC(DEBUG, Logger::CLUSTER)
          << "Failed to find index " << databaseName << "/" << collectionID
          << "/" << iid << " - " << indexes.toJson();
    } catch (std::exception const& e) {
      LOG_TOPIC(DEBUG, Logger::CLUSTER)
          << "Failed to find index " << databaseName << "/" << collectionID
          << "/" << iid << " - " << e.what();
    }
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
  AgencyOperation incrementVersion("Plan/Version", AgencySimpleOperationType::INCREMENT_OP);
  AgencyPrecondition prec(key, AgencyPrecondition::Type::VALUE, previous);
  AgencyWriteTransaction trx({newVal, incrementVersion}, prec);
  AgencyCommResult result = ac.sendTransactionWithFailover(trx, 0.0);

  if (!result.successful()) {
    errorMsg += " Failed to execute ";
    errorMsg += trx.toJson();
    errorMsg += " ResultCode: " + std::to_string(result.errorCode()) + " ";

    events::DropIndex(collectionID, idString, TRI_ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN);
    return TRI_ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN;
  }

  // load our own cache:
  loadPlan();
  {
    MUTEX_LOCKER(guard, *numberOfShardsMutex);
    if (*numberOfShards == 0) {
      loadCurrent();
      return TRI_ERROR_NO_ERROR;
    }
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
      if (!application_features::ApplicationServer::isRetryOK()) {
        return setErrormsg(TRI_ERROR_CLUSTER_TIMEOUT, errorMsg);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief (re-)load the information about servers from the agency
/// Usually one does not have to call this directly.
////////////////////////////////////////////////////////////////////////////////

static std::string const prefixServers = "Current/ServersRegistered";
static std::string const mapUniqueToShortId = "Target/MapUniqueToShortID";

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

  AgencyCommResult result = _agency.sendTransactionWithFailover(AgencyReadTransaction(
      std::vector<std::string>({AgencyCommManager::path(prefixServers),
                                AgencyCommManager::path(mapUniqueToShortId)})));

  if (result.successful()) {
    velocypack::Slice serversRegistered = result.slice()[0].get(std::vector<std::string>(
        {AgencyCommManager::path(), "Current", "ServersRegistered"}));

    velocypack::Slice serversAliases = result.slice()[0].get(std::vector<std::string>(
        {AgencyCommManager::path(), "Target", "MapUniqueToShortID"}));

    if (serversRegistered.isObject()) {
      decltype(_servers) newServers;
      decltype(_serverAliases) newAliases;

      for (auto const& res : VPackObjectIterator(serversRegistered)) {
        velocypack::Slice slice = res.value;

        if (slice.isObject() && slice.hasKey("endpoint")) {
          std::string server =
              arangodb::basics::VelocyPackHelper::getStringValue(slice,
                                                                 "endpoint", "");

          std::string serverId = res.key.copyString();
          try {
            velocypack::Slice serverSlice;
            serverSlice = serversAliases.get(serverId);
            if (serverSlice.isObject()) {
              std::string alias = arangodb::basics::VelocyPackHelper::getStringValue(
                  serverSlice, "ShortName", "");
              newAliases.emplace(std::make_pair(alias, serverId));
            }
          } catch (...) {
          }
          newServers.emplace(std::make_pair(serverId, server));
        }
      }

      // Now set the new value:
      {
        WRITE_LOCKER(writeLocker, _serversProt.lock);
        _servers.swap(newServers);
        _serverAliases.swap(newAliases);
        _serversProt.doneVersion = storedVersion;
        _serversProt.isValid = true;
      }
      return;
    }
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixServers
      << " httpCode: " << result.httpCode() << " errorCode: " << result.errorCode()
      << " errorMessage: " << result.errorMessage() << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the endpoint of a server from its ID.
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterInfo::getServerEndpoint(ServerID const& serverID) {
#ifdef DEBUG_SYNC_REPLICATION
  if (serverID == "debug-follower") {
    return "tcp://127.0.0.1:3000";
  }
#endif
  int tries = 0;

  if (!_serversProt.isValid) {
    loadServers();
    tries++;
  }

  std::string serverID_ = serverID;

  while (true) {
    {
      READ_LOCKER(readLocker, _serversProt.lock);

      // _serversAliases is a map-type <Alias, ServerID>
      auto ita = _serverAliases.find(serverID_);

      if (ita != _serverAliases.end()) {
        serverID_ = (*ita).second;
      }

      // _servers is a map-type <ServerId, std::string>
      auto it = _servers.find(serverID_);

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

  return std::string();
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

  return std::string();
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
    velocypack::Slice currentCoordinators = result.slice()[0].get(std::vector<std::string>(
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
        _coordinatorsProt.isValid = true;
      }
      return;
    }
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixCurrentCoordinators
      << " httpCode: " << result.httpCode() << " errorCode: " << result.errorCode()
      << " errorMessage: " << result.errorMessage() << " body: " << result.body();
}

static std::string const prefixMappings = "Target/MapUniqueToShortID";

void ClusterInfo::loadCurrentMappings() {
  ++_mappingsProt.wantedVersion;  // Indicate that after *NOW* somebody
                                  // has to reread from the agency!
  MUTEX_LOCKER(mutexLocker, _mappingsProt.mutex);
  uint64_t storedVersion = _mappingsProt.wantedVersion;  // this is the
                                                         // version we will
                                                         // set in the end
  if (_mappingsProt.doneVersion == storedVersion) {
    // Somebody else did, what we intended to do, so just return
    return;
  }

  // Now contact the agency:
  AgencyCommResult result = _agency.getValues(prefixMappings);

  if (result.successful()) {
    velocypack::Slice mappings = result.slice()[0].get(std::vector<std::string>(
        {AgencyCommManager::path(), "Target", "MapUniqueToShortID"}));

    if (mappings.isObject()) {
      decltype(_coordinatorIdMap) newCoordinatorIdMap;

      for (auto const& mapping : VPackObjectIterator(mappings)) {
        ServerID fullId = mapping.key.copyString();
        auto mapObject = mapping.value;
        if (mapObject.isObject()) {
          ServerShortName shortName = mapObject.get("ShortName").copyString();

          ServerShortID shortId =
              mapObject.get("TransactionID").getNumericValue<ServerShortID>();
          static std::string const expectedPrefix{"Coordinator"};
          if (shortName.size() > expectedPrefix.size() &&
              shortName.substr(0, expectedPrefix.size()) == expectedPrefix) {
            newCoordinatorIdMap.emplace(shortId, fullId);
          }
        }
      }

      // Now set the new value:
      {
        WRITE_LOCKER(writeLocker, _mappingsProt.lock);
        _coordinatorIdMap.swap(newCoordinatorIdMap);
        _mappingsProt.doneVersion = storedVersion;
        _mappingsProt.isValid = true;
      }
      return;
    }
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixMappings
      << " httpCode: " << result.httpCode() << " errorCode: " << result.errorCode()
      << " errorMessage: " << result.errorMessage() << " body: " << result.body();
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
      failedDBServers = target.slice()[0].get(std::vector<std::string>(
          {AgencyCommManager::path(), "Target", "FailedServers"}));
      cleanedDBServers = target.slice()[0].get(std::vector<std::string>(
          {AgencyCommManager::path(), "Target", "CleanedOutServers"}));
    }
    if (currentDBServers.isObject() && failedDBServers.isObject()) {
      decltype(_DBServers) newDBServers;

      for (auto const& dbserver : VPackObjectIterator(currentDBServers)) {
        bool found = false;
        if (failedDBServers.isObject()) {
          for (auto const& failedServer : VPackObjectIterator(failedDBServers)) {
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
          for (auto const& cleanedServer : VPackArrayIterator(cleanedDBServers)) {
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
        _DBServersProt.isValid = true;
      }
      return;
    }
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER)
      << "Error while loading " << prefixCurrentDBServers
      << " httpCode: " << result.httpCode() << " errorCode: " << result.errorCode()
      << " errorMessage: " << result.errorMessage() << " body: " << result.body();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of all DBServers in the cluster that have
/// currently registered
////////////////////////////////////////////////////////////////////////////////

std::vector<ServerID> ClusterInfo::getCurrentDBServers() {
  std::vector<ServerID> result;

  if (!_DBServersProt.isValid) {
    loadCurrentDBServers();
  }
  // return a consistent state of servers
  READ_LOCKER(readLocker, _DBServersProt.lock);

  result.reserve(_DBServers.size());

  for (auto& it : _DBServers) {
    result.emplace_back(it.first);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find the servers who are responsible for a shard (one leader
/// and multiple followers)
/// If it is not found in the cache, the cache is reloaded once, if
/// it is still not there an empty string is returned as an error.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<std::vector<ServerID>> ClusterInfo::getResponsibleServer(ShardID const& shardID) {
  int tries = 0;

  if (!_currentProt.isValid) {
    loadCurrent();
    tries++;
  }

  while (true) {
    {
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
          } else {
            return (*it).second;
          }
        }
      }
      usleep(500000);
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

std::shared_ptr<std::vector<ShardID>> ClusterInfo::getShardList(CollectionID const& collectionID) {
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
int ClusterInfo::getResponsibleShard(LogicalCollection* collInfo, VPackSlice slice,
                                     bool docComplete, ShardID& shardID,
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
              shardKeysPtr->size() == 1 && shardKeysPtr->at(0) == StaticStrings::KeyString;
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
  uint64_t hash =
      arangodb::basics::VelocyPackHelper::hashByAttributes(slice, *shardKeysPtr,
                                                           docComplete, error, key);
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

  if (!_coordinatorsProt.isValid) {
    loadCurrentCoordinators();
  }

  // return a consistent state of servers
  READ_LOCKER(readLocker, _coordinatorsProt.lock);

  result.reserve(_coordinators.size());

  for (auto& it : _coordinators) {
    result.emplace_back(it.first);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup full coordinator ID from short ID
////////////////////////////////////////////////////////////////////////////////

ServerID ClusterInfo::getCoordinatorByShortID(ServerShortID shortId) {
  ServerID result;

  if (!_mappingsProt.isValid) {
    loadCurrentMappings();
  }

  // return a consistent state of servers
  READ_LOCKER(readLocker, _mappingsProt.lock);

  auto it = _coordinatorIdMap.find(shortId);
  if (it != _coordinatorIdMap.end()) {
    result = it->second;
  }

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
/// @brief invalidate current coordinators
//////////////////////////////////////////////////////////////////////////////

void ClusterInfo::invalidateCurrentCoordinators() {
  {
    WRITE_LOCKER(writeLocker, _coordinatorsProt.lock);
    _coordinatorsProt.isValid = false;
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief invalidate current mappings
//////////////////////////////////////////////////////////////////////////////

void ClusterInfo::invalidateCurrentMappings() {
  {
    WRITE_LOCKER(writeLocker, _mappingsProt.lock);
    _mappingsProt.isValid = false;
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
    WRITE_LOCKER(writeLocker, _currentProt.lock);
    _currentProt.isValid = false;
  }
  invalidateCurrentCoordinators();
  invalidateCurrentMappings();
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

std::unordered_map<ServerID, std::string> ClusterInfo::getServers() {
  if (!_serversProt.isValid) {
    loadServers();
  }
  READ_LOCKER(readLocker, _serversProt.lock);
  std::unordered_map<ServerID, std::string> serv = _servers;
  return serv;
}

std::unordered_map<ServerID, std::string> ClusterInfo::getServerAliases() {
  READ_LOCKER(readLocker, _serversProt.lock);
  std::unordered_map<std::string, std::string> ret;
  for (const auto& i : _serverAliases) {
    ret.emplace(i.second, i.first);
  }
  return ret;
}

arangodb::Result ClusterInfo::agencyDump(std::shared_ptr<VPackBuilder> body) {

  AgencyCommResult dump = _agency.dump();
  body->add(dump.slice());
  return Result();

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
