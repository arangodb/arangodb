////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "RestReplicationHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Basics/ConditionLocker.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/NumberUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/hashes.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterHelpers.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/RebootTracker.h"
#include "Cluster/ResignShardLeadership.h"
#include "Containers/MerkleTree.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Indexes/Index.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/GlobalInitialSyncer.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationClients.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "Sharding/ShardingInfo.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/Methods/Collections.h"

#include <Containers/HashSet.h>
#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::cluster;

namespace {
std::string const dataString("data");
std::string const keyString("key");
std::string const typeString("type");
}  // namespace

uint64_t const RestReplicationHandler::_defaultChunkSize = 128 * 1024;
uint64_t const RestReplicationHandler::_maxChunkSize = 128 * 1024 * 1024;
std::chrono::hours const RestReplicationHandler::_tombstoneTimeout =
    std::chrono::hours(24);

basics::ReadWriteLock RestReplicationHandler::_tombLock;
std::unordered_map<std::string, std::chrono::time_point<std::chrono::steady_clock>>
    RestReplicationHandler::_tombstones = {};

static TransactionId ExtractReadlockId(VPackSlice slice) {
  TRI_ASSERT(slice.isString());
  return TransactionId{StringUtils::uint64(slice.copyString())};
}

static bool ignoreHiddenEnterpriseCollection(std::string const& name, bool force) {
#ifdef USE_ENTERPRISE
  if (!force && name[0] == '_') {
    if (strncmp(name.c_str(), "_local_", 7) == 0 ||
        strncmp(name.c_str(), "_from_", 6) == 0 || strncmp(name.c_str(), "_to_", 4) == 0) {
      LOG_TOPIC("944c4", WARN, arangodb::Logger::REPLICATION)
          << "Restore ignoring collection " << name
          << ". Will be created via SmartGraphs of a full dump. If you want to "
          << "restore ONLY this collection use 'arangorestore --force'. "
          << "However this is not recommended and you should instead restore "
          << "the EdgeCollection of the SmartGraph instead.";
      return true;
    }
  }
#endif
  return false;
}

static Result checkPlanLeaderDirect(std::shared_ptr<LogicalCollection> const& col,
                                    std::string const& claimLeaderId) {
  std::vector<std::string> agencyPath = {"Plan",
                                         "Collections",
                                         col->vocbase().name(),
                                         std::to_string(col->planId().id()),
                                         "shards",
                                         col->name()};

  std::string shardAgencyPathString = StringUtils::join(agencyPath, '/');

  AgencyComm ac(col->vocbase().server());
  AgencyCommResult res = ac.getValues(shardAgencyPathString);

  if (res.successful()) {
    // This is bullshit. Why does the *fancy* AgencyComm Manager
    // prepend the agency url with `arango` but in the end returns an object
    // that is prepended by `arango`! WTF!?
    VPackSlice plan = res.slice().at(0).get(AgencyCommHelper::path()).get(agencyPath);
    TRI_ASSERT(plan.isArray() && plan.length() > 0);

    VPackSlice leaderSlice = plan.at(0);
    TRI_ASSERT(leaderSlice.isString());
    if (leaderSlice.isEqualString(claimLeaderId)) {
      return Result{};
    }
  }

  return Result{TRI_ERROR_FORBIDDEN};
}

static Result restoreDataParser(char const* ptr, char const* pos,
                                std::string const& collectionName, int line,
                                VPackBuilder& builder,
                                VPackSlice& doc, TRI_replication_operation_e& type) {
  builder.clear();

  try {
    VPackParser parser(builder, builder.options);
    parser.parse(ptr, static_cast<size_t>(pos - ptr));
  } catch (std::exception const& ex) {
    // Could not even build the string
    return Result{TRI_ERROR_HTTP_CORRUPTED_JSON,
                  "received invalid JSON data for collection '" + collectionName +
                      "' on line " + std::to_string(line) + ": " + ex.what()};
  } catch (...) {
    return Result{TRI_ERROR_INTERNAL};
  }

  VPackSlice slice = builder.slice();

  if (!slice.isObject()) {
    return Result{TRI_ERROR_HTTP_CORRUPTED_JSON,
                  "received invalid JSON data for collection '" +
                      collectionName + "' on line " + std::to_string(line) +
                      ": data is no object"};
  }

  type = REPLICATION_INVALID;

  VPackSlice value = slice.get(StaticStrings::KeyString);
  if (value.isString()) {
    // compact format
    type = REPLICATION_MARKER_DOCUMENT;
    // for a valid document marker without envelope, doc points to the actual docuemnt
    doc = slice;
  } else {
    // enveloped (old) format. each document is wrapped into a
    // {"type":2300,"data":{...}} envelope
    value = slice.get(::typeString);
    if (value.isNumber()) {
      int v = value.getNumericValue<int>();
      if (v == 2301) {
        // pre-3.0 type for edges
        type = REPLICATION_MARKER_DOCUMENT;
      } else {
        type = static_cast<TRI_replication_operation_e>(v);
      }
    
      if (type == REPLICATION_MARKER_DOCUMENT) {
        // for dumps taken with RocksDB and/or MMFiles
        value = slice.get(::dataString);
        if (!value.isObject()) {
          type = REPLICATION_INVALID;
        } else {
          // for a valid document marker, doc points to the actual docuemnt
          doc = value;
        }
      } else if (type == REPLICATION_MARKER_REMOVE) {
        // edge case: only needed for old dumps taken with MMFiles
        value = slice.get(::dataString);
        if (value.isObject()) {
          value = value.get(StaticStrings::KeyString);
        } else {
          value = slice.get("key");
        }
        if (!value.isString()) {
          type = REPLICATION_INVALID;
        } else {
          // for a valid remove marker, doc points to the key string
          doc = value;
        }
      }
    }
  }

  TRI_ASSERT(type != REPLICATION_MARKER_DOCUMENT || doc.isObject());

  if (type != REPLICATION_MARKER_DOCUMENT && type != REPLICATION_MARKER_REMOVE) {
    // we can only handle REPLICATION_MARKER_DOCUMENT and REPLICATION_MARKER_REMOVE here
    return Result{TRI_ERROR_HTTP_CORRUPTED_JSON,
                  "received invalid JSON data for collection '" +
                      collectionName + "' on line " + std::to_string(line) +
                      ": got an invalid input document"};
  }

  return {};
}

RestReplicationHandler::RestReplicationHandler(application_features::ApplicationServer& server,
                                               GeneralRequest* request,
                                               GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestReplicationHandler::~RestReplicationHandler() = default;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error if called on a coordinator server
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::isCoordinatorError() {
  if (_vocbase.type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "replication API is not supported on a coordinator");

    return true;
  }

  return false;
}

std::string const RestReplicationHandler::LoggerState = "logger-state";
std::string const RestReplicationHandler::LoggerTickRanges =
    "logger-tick-ranges";
std::string const RestReplicationHandler::LoggerFirstTick = "logger-first-tick";
std::string const RestReplicationHandler::LoggerFollow = "logger-follow";
std::string const RestReplicationHandler::OpenTransactions =
    "determine-open-transactions";
std::string const RestReplicationHandler::Batch = "batch";
std::string const RestReplicationHandler::Barrier = "barrier";
std::string const RestReplicationHandler::Inventory = "inventory";
std::string const RestReplicationHandler::Keys = "keys";
std::string const RestReplicationHandler::Revisions = "revisions";
std::string const RestReplicationHandler::Tree = "tree";
std::string const RestReplicationHandler::Ranges = "ranges";
std::string const RestReplicationHandler::Documents = "documents";
std::string const RestReplicationHandler::Dump = "dump";
std::string const RestReplicationHandler::RestoreCollection =
    "restore-collection";
std::string const RestReplicationHandler::RestoreIndexes = "restore-indexes";
std::string const RestReplicationHandler::RestoreData = "restore-data";
std::string const RestReplicationHandler::RestoreView = "restore-view";
std::string const RestReplicationHandler::Sync = "sync";
std::string const RestReplicationHandler::MakeFollower = "make-follower";
std::string const RestReplicationHandler::ServerId = "server-id";
std::string const RestReplicationHandler::ApplierConfig = "applier-config";
std::string const RestReplicationHandler::ApplierStart = "applier-start";
std::string const RestReplicationHandler::ApplierStop = "applier-stop";
std::string const RestReplicationHandler::ApplierState = "applier-state";
std::string const RestReplicationHandler::ApplierStateAll = "applier-state-all";
std::string const RestReplicationHandler::ClusterInventory = "clusterInventory";
std::string const RestReplicationHandler::AddFollower = "addFollower";
std::string const RestReplicationHandler::RemoveFollower = "removeFollower";
std::string const RestReplicationHandler::SetTheLeader = "set-the-leader";
std::string const RestReplicationHandler::HoldReadLockCollection =
    "holdReadLockCollection";

// main function that dispatches the different routes and commands
RestStatus RestReplicationHandler::execute() {
  auto res = testPermissions();
  if (!res.ok()) {
    generateError(res);
    return RestStatus::DONE;
  }
  // extract the request type
  auto const type = _request->requestType();
  auto const& suffixes = _request->suffixes();

  size_t const len = suffixes.size();

  if (len >= 1) {
    std::string const& command = suffixes[0];

    if (command == LoggerState) {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandLoggerState();
    } else if (command == LoggerTickRanges) {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }
      handleCommandLoggerTickRanges();
    } else if (command == LoggerFirstTick) {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }
      handleCommandLoggerFirstTick();
    } else if (command == LoggerFollow) {
      if (type != rest::RequestType::GET && type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }
      // track the number of parallel invocations of the tailing API
      auto& rf = _vocbase.server().getFeature<ReplicationFeature>();
      // this may throw when too many threads are going into tailing
      rf.trackTailingStart();

      auto guard = scopeGuard([&rf]() { rf.trackTailingEnd(); });

      handleCommandLoggerFollow();
    } else if (command == OpenTransactions) {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandDetermineOpenTransactions();
    } else if (command == Batch) {
      // access batch context in context manager
      // example call: curl -XPOST --dump - --data '{}'
      // http://localhost:5555/_api/replication/batch
      // the object may contain a "ttl" for the context

      // POST - create batch id / handle
      // PUT  - extend batch id / handle
      // DEL  - delete batchid

      if (ServerState::instance()->isCoordinator()) {
        handleUnforwardedTrampolineCoordinator();
      } else {
        handleCommandBatch();
      }
    } else if (command == Barrier) {
      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }
      handleCommandBarrier();
    } else if (command == Inventory) {
      // get overview of collections and indexes followed by some extra data
      // example call: curl --dump -
      // http://localhost:5555/_api/replication/inventory?batchId=75

      // {
      //    collections : [ ... ],
      //    "state" : {
      //      "running" : true,
      //      "lastLogTick" : "10528",
      //      "lastUncommittedLogTick" : "10531",
      //      "totalEvents" : 3782,
      //      "time" : "2017-07-19T21:50:59Z"
      //    },
      //   "tick" : "10531"
      // }

      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (ServerState::instance()->isCoordinator()) {
        handleUnforwardedTrampolineCoordinator();
      } else {
        handleCommandInventory();
      }
    } else if (command == Keys) {
      // preconditions for calling this route are unclear and undocumented --
      // FIXME
      if (type != rest::RequestType::GET && type != rest::RequestType::POST &&
          type != rest::RequestType::PUT && type != rest::RequestType::DELETE_REQ) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      if (type == rest::RequestType::POST) {
        // has to be called first will bind the iterator to a collection

        // example: curl -XPOST --dump -
        // 'http://localhost:5555/_db/_system/_api/replication/keys/?collection=_users&batchId=169'
        // ; echo
        // returns
        // { "id": <context id - int>,
        //   "count": <number of documents in collection - int>
        // }
        handleCommandCreateKeys();
      } else if (type == rest::RequestType::GET) {
        // curl --dump -
        // 'http://localhost:5555/_db/_system/_api/replication/keys/123?collection=_users'
        // ; echo # id is batchid
        handleCommandGetKeys();
      } else if (type == rest::RequestType::PUT) {
        handleCommandFetchKeys();
      } else if (type == rest::RequestType::DELETE_REQ) {
        handleCommandRemoveKeys();
      }
    } else if (command == Revisions) {
      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      if (len > 1) {
        std::string const& subCommand = suffixes[1];
        if (type == rest::RequestType::GET && subCommand == Tree) {
          handleCommandRevisionTree();
        } else if (type == rest::RequestType::POST && subCommand == Tree) {
          handleCommandRebuildRevisionTree();
        } else if (type == rest::RequestType::PUT && subCommand == Ranges) {
          handleCommandRevisionRanges();
        } else if (type == rest::RequestType::PUT && subCommand == Documents) {
          handleCommandRevisionDocuments();
        }
      } else {
        goto BAD_CALL;
      }
    } else if (command == Dump) {
      // works on collections
      // example: curl --dump -
      // 'http://localhost:5555/_db/_system/_api/replication/dump?collection=test&batchId=115'
      // requires batch-id
      // does internally an
      //   - get inventory
      //   - purge local
      //   - dump remote to local

      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }

      if (ServerState::instance()->isCoordinator()) {
        handleUnforwardedTrampolineCoordinator();
      } else {
        handleCommandDump();
      }
    } else if (command == RestoreCollection) {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreCollection();
    } else if (command == RestoreIndexes) {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreIndexes();
    } else if (command == RestoreData) {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      handleCommandRestoreData();
    } else if (command == RestoreView) {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreView();
    } else if (command == Sync) {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandSync();
    } else if (command == MakeFollower ||
               command == "make-slave" /*deprecated*/) {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandMakeFollower();
    } else if (command == ServerId) {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandServerId();
    } else if (command == ApplierConfig) {
      if (type == rest::RequestType::GET) {
        handleCommandApplierGetConfig();
      } else {
        if (type != rest::RequestType::PUT) {
          goto BAD_CALL;
        }
        handleCommandApplierSetConfig();
      }
    } else if (command == ApplierStart) {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandApplierStart();
    } else if (command == ApplierStop) {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandApplierStop();
    } else if (command == ApplierState) {
      if (type == rest::RequestType::DELETE_REQ) {
        handleCommandApplierDeleteState();
      } else {
        if (type != rest::RequestType::GET) {
          goto BAD_CALL;
        }
        handleCommandApplierGetState();
      }
    } else if (command == ApplierStateAll) {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandApplierGetStateAll();
    } else if (command == ClusterInventory) {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isCoordinator()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
      } else {
        handleCommandClusterInventory();
      }
    } else if (command == AddFollower) {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        handleCommandAddFollower();
      }
    } else if (command == RemoveFollower) {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        handleCommandRemoveFollower();
      }
    } else if (command == SetTheLeader) {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        handleCommandSetTheLeader();
      }
    } else if (command == HoldReadLockCollection) {
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        if (type == rest::RequestType::POST) {
          handleCommandHoldReadLockCollection();
        } else if (type == rest::RequestType::PUT) {
          handleCommandCheckHoldReadLockCollection();
        } else if (type == rest::RequestType::DELETE_REQ) {
          handleCommandCancelHoldReadLockCollection();
        } else if (type == rest::RequestType::GET) {
          handleCommandGetIdForReadLockCollection();
        } else {
          goto BAD_CALL;
        }
      }
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    std::string("invalid command '") + command + "'");
    }

    return RestStatus::DONE;
  }

BAD_CALL:
  if (len != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "expecting URL /_api/replication/<command>");
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }

  return RestStatus::DONE;
}

Result RestReplicationHandler::testPermissions() {
  if (!_request->authenticated()) {
    return TRI_ERROR_NO_ERROR;
  }

  std::string user = _request->user();
  auto const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();
  if (len >= 1) {
    auto const type = _request->requestType();
    std::string const& command = suffixes[0];
    if ((command == Batch) || (command == Inventory && type == rest::RequestType::GET) ||
        (command == Dump && type == rest::RequestType::GET) ||
        (command == RestoreCollection && type == rest::RequestType::PUT)) {
      if (command == Dump) {
        // check dump collection permissions (at least ro needed)
        std::string collectionName = _request->value("collection");

        if (ServerState::instance()->isCoordinator()) {
          // We have a shard id, need to translate
          ClusterInfo& ci = server().getFeature<ClusterFeature>().clusterInfo();
          collectionName = ci.getCollectionNameForShard(collectionName);
        }

        if (!collectionName.empty()) {
          auto& exec = ExecContext::current();
          ExecContextSuperuserScope escope(exec.isAdminUser());
          if (!exec.isAdminUser() &&
              !exec.canUseCollection(collectionName, auth::Level::RO)) {
            // not enough rights
            return Result(TRI_ERROR_FORBIDDEN);
          }
        } else {
          // not found, return 404
          return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
        }
      } else if (command == RestoreCollection) {
        VPackSlice const slice = _request->payload();
        VPackSlice const parameters = slice.get("parameters");
        if (parameters.isObject()) {
          if (parameters.get("name").isString()) {
            std::string collectionName = parameters.get("name").copyString();
            if (!collectionName.empty()) {
              std::string dbName = _request->databaseName();
              DatabaseFeature& databaseFeature =
                  _vocbase.server().getFeature<DatabaseFeature>();
              TRI_vocbase_t* vocbase = databaseFeature.lookupDatabase(dbName);
              if (vocbase == nullptr) {
                return Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
              }

              std::string const& overwriteCollection =
                  _request->value("overwrite");

              auto& exec = ExecContext::current();
              ExecContextSuperuserScope escope(exec.isAdminUser());

              if (overwriteCollection == "true" ||
                  vocbase->lookupCollection(collectionName) == nullptr) {
                // 1.) re-create collection, means: overwrite=true (rw database)
                // OR 2.) not existing, new collection (rw database)
                if (!exec.isAdminUser() && !exec.canUseDatabase(dbName, auth::Level::RW)) {
                  return Result(TRI_ERROR_FORBIDDEN);
                }
              } else {
                // 3.) Existing collection (ro database, rw collection)
                // no overwrite. restoring into an existing collection
                if (!exec.isAdminUser() &&
                    !exec.canUseCollection(collectionName, auth::Level::RW)) {
                  return Result(TRI_ERROR_FORBIDDEN);
                }
              }
            } else {
              return Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                            "empty collection name");
            }
          } else {
            return Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                          "invalid collection name type");
          }
        } else {
          return Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                        "invalid collection parameter type");
        }
      }
    }
  }
  return Result(TRI_ERROR_NO_ERROR);
}

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>> RestReplicationHandler::forwardingTarget() {
  auto base = RestVocbaseBaseHandler::forwardingTarget();
  if (base.ok() && !std::get<0>(base.get()).empty()) {
    return base;
  }

  auto res = testPermissions();
  if (!res.ok()) {
    return res;
  }

  ServerID const& DBserver = _request->value("DBserver");
  if (!DBserver.empty()) {
    // if DBserver property present, remove auth header
    return std::make_pair(DBserver, true);
  }

  return {std::make_pair(StaticStrings::Empty, false)};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_makeFollower
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandMakeFollower() {
  bool isGlobal = false;
  ReplicationApplier* applier = getApplier(isGlobal);
  if (applier == nullptr) {
    return;
  }

  bool success = false;
  VPackSlice body = this->parseVPackBody(success);
  if (!success) {
    // error already created
    return;
  }

  std::string databaseName;

  if (!isGlobal) {
    databaseName = _vocbase.name();
  }

  ReplicationApplierConfiguration configuration =
      ReplicationApplierConfiguration::fromVelocyPack(_vocbase.server(), body, databaseName);
  configuration._skipCreateDrop = false;

  // will throw if invalid
  configuration.validate();

  // allow access to _users if appropriate
  ExecContextSuperuserScope escope(ExecContext::current().isAdminUser());

  // forget about any existing replication applier configuration
  applier->forget();
  applier->reconfigure(configuration);
  applier->startReplication();

  while (applier->isInitializing()) {  // wait for initial sync
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (_vocbase.server().isStopping()) {
      generateError(Result(TRI_ERROR_SHUTTING_DOWN));
      return;
    }
  }
  // applier->startTailing(lastLogTick, true, barrierId);

  VPackBuilder result;
  result.openObject();
  applier->toVelocyPack(result);
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle an unforwarded command in the coordinator case
/// If the request is well-formed and has the DBserver set, then the request
/// should already be forwarded by other means. We should only get here if
/// the request is null or the DBserver parameter is missing. This method
/// now just does a bit of error handling.
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleUnforwardedTrampolineCoordinator() {
  if (_request == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request");
  }

  // First check the DBserver component of the body json:
  ServerID DBserver = _request->value("DBserver");

  if (DBserver.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "need \"DBserver\" parameter");
    return;
  }

  TRI_ASSERT(false);  // should only get here if request is not well-formed
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_cluster_inventory
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandClusterInventory() {
  auto& replicationFeature = _vocbase.server().getFeature<ReplicationFeature>();
  replicationFeature.trackInventoryRequest();

  std::string const& dbName = _request->databaseName();
  bool includeSystem = _request->parsedValue("includeSystem", true);

  ClusterInfo& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::vector<std::shared_ptr<LogicalCollection>> cols = ci.getCollections(dbName);
  VPackBuilder resultBuilder;
  resultBuilder.openObject();

  DatabaseFeature& databaseFeature = _vocbase.server().getFeature<DatabaseFeature>();
  TRI_vocbase_t* vocbase = databaseFeature.lookupDatabase(dbName);
  if (vocbase) {
    resultBuilder.add(VPackValue(StaticStrings::Properties));
    vocbase->toVelocyPack(resultBuilder);
  }

  auto& exec = ExecContext::current();
  ExecContextSuperuserScope escope(exec.isAdminUser());

  resultBuilder.add("collections", VPackValue(VPackValueType::Array));
  for (std::shared_ptr<LogicalCollection> const& c : cols) {
    if (!exec.isAdminUser() &&
        !exec.canUseCollection(vocbase->name(), c->name(), auth::Level::RO)) {
      continue;
    }

    // We want to check if the collection is usable and all followers
    // are in sync:
    std::shared_ptr<ShardMap> shardMap = c->shardIds();
    // shardMap is an unordered_map from ShardId (string) to a vector of
    // servers (strings), wrapped in a shared_ptr
    auto cic = ci.getCollectionCurrent(dbName, basics::StringUtils::itoa(c->id().id()));
    // Check all shards:
    bool isReady = true;
    bool allInSync = true;
    for (auto const& p : *shardMap) {
      auto currentServerList = cic->servers(p.first /* shardId */);
      if (currentServerList.size() == 0 || p.second.size() == 0 ||
          currentServerList[0] != p.second[0] ||
          (!p.second[0].empty() && p.second[0][0] == '_')) {
        isReady = false;
      }
      if (!ClusterHelpers::compareServerLists(p.second, currentServerList)) {
        allInSync = false;
      }
    }
    c->toVelocyPackForClusterInventory(resultBuilder, includeSystem, isReady, allInSync);
  }
  resultBuilder.close();  // collections
  resultBuilder.add("views", VPackValue(VPackValueType::Array));
  LogicalView::enumerate(_vocbase, [&resultBuilder](LogicalView::ptr const& view) -> bool {
    if (view) {
      resultBuilder.openObject();
      view->properties(resultBuilder, LogicalDataSource::Serialization::Inventory);
      // details, !forPersistence because on restore any datasource ids will
      // differ, so need an end-user representation
      resultBuilder.close();
    }

    return true;
  });
  resultBuilder.close();  // views

  TRI_voc_tick_t tick = TRI_CurrentTickServer();
  resultBuilder.add("tick", VPackValue(std::to_string(tick)));
  resultBuilder.add("state", VPackValue("unused"));
  resultBuilder.close();  // base
  generateResult(rest::ResponseCode::OK, resultBuilder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreCollection() {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return;
  }
  auto pair = rocksutils::stripObjectIds(body);
  VPackSlice const slice = pair.first;

  bool overwrite = _request->parsedValue<bool>("overwrite", false);
  bool force = _request->parsedValue<bool>("force", false);
  bool ignoreDistributeShardsLikeErrors =
      _request->parsedValue<bool>("ignoreDistributeShardsLikeErrors", false);

  Result res = processRestoreCollection(slice, overwrite, force, ignoreDistributeShardsLikeErrors);

  if (res.is(TRI_ERROR_ARANGO_DUPLICATE_NAME)) {
    VPackSlice name = slice.get("parameters");
    if (name.isObject()) {
      name = name.get("name");
      if (name.isString() && !name.copyString().empty() && name.copyString()[0] == '_') {
        // system collection, may have been created in the meantime by a
        // background action collection should be there now
        res.reset();
      }
    }
  }

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("result", VPackValue(true));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreIndexes() {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return;
  }

  bool force = _request->parsedValue("force", false);

  Result res;
  if (ServerState::instance()->isCoordinator()) {
    res = processRestoreIndexesCoordinator(body, force);
  } else {
    res = processRestoreIndexes(body, force);
  }

  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  VPackBuilder result;
  result.openObject();
  result.add("result", VPackValue(true));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreData() {
  std::string const& colName = _request->value("collection");

  if (colName.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter, not given");
    return;
  }

  Result res = processRestoreData(colName);

  if (res.fail()) {
    if (res.errorMessage().empty()) {
      generateError(GeneralResponse::responseCode(res.errorNumber()), res.errorNumber());
    } else {
      generateError(res);
    }
  } else {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("result", VPackValue(true));
    result.close();
    generateResult(rest::ResponseCode::OK, result.slice());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreCollection(VPackSlice const& collection,
                                                        bool dropExisting, bool force,
                                                        bool ignoreDistributeShardsLikeErrors) {
  if (!collection.isObject()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                  "collection declaration is invalid");
  }

  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                  "collection parameters declaration is invalid");
  }

  // only local
  VPackSlice const indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                  "collection indexes declaration is invalid");
  }
  // end only local

  std::string const name =
      arangodb::basics::VelocyPackHelper::getStringValue(parameters, "name", "");

  if (name.empty()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "collection name is missing");
  }

  if (ignoreHiddenEnterpriseCollection(name, force)) {
    return {TRI_ERROR_NO_ERROR};
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted", false)) {
    // we don't care about deleted collections
    return Result();
  }

  if (ServerState::instance()->isCoordinator()) {
    Result res =
        ShardingInfo::validateShardsAndReplicationFactor(parameters, server(), true);
    if (res.fail()) {
      return res;
    }
  }

  // only local
  ExecContextSuperuserScope escope(
      ExecContext::current().isSuperuser() ||
      (ExecContext::current().isAdminUser() && !ServerState::readOnly()));
  // end only local

  std::shared_ptr<LogicalCollection> col;
  auto lookupResult = methods::Collections::lookup(_vocbase, name, col);
  if (lookupResult.ok()) {
    TRI_ASSERT(col);
    if (dropExisting) {
      try {
        if (ServerState::instance()->isCoordinator() && name == StaticStrings::AnalyzersCollection &&
            server().hasFeature<iresearch::IResearchAnalyzerFeature>()) {
          // We have ArangoSearch here. So process analyzers accordingly.
          // We can`t just recreate/truncate collection. Agency should be
          // properly notified analyzers are gone.
          // The single server and DBServer case is handled after restore of data.
          return server().getFeature<iresearch::IResearchAnalyzerFeature>().removeAllAnalyzers(_vocbase);
        }

        auto dropResult = methods::Collections::drop(*col, true, 300.0, true);
        if (dropResult.fail()) {
          if (dropResult.is(TRI_ERROR_FORBIDDEN) || dropResult.is(TRI_ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE)) {
            // If we are not allowed to drop the collection.
            // Try to truncate.
            auto ctx = transaction::StandaloneContext::Create(_vocbase);
            SingleCollectionTransaction trx(ctx, *col, AccessMode::Type::EXCLUSIVE);

            trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
            trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
            auto trxRes = trx.begin();

            if (!trxRes.ok()) {
              return trxRes;
            }

            OperationOptions options;
            OperationResult opRes = trx.truncate(name, options);

            return trx.finish(opRes.result);
          }

          return Result(dropResult.errorNumber(),
                        arangodb::basics::StringUtils::concatT(
                            "unable to drop collection '", name,
                            "': ", dropResult.errorMessage()));
        }
      } catch (basics::Exception const& ex) {
        LOG_TOPIC("41579", DEBUG, Logger::REPLICATION)
            << "processRestoreCollection "
            << "could not drop collection: " << ex.what();
      } catch (...) {
      }
    }
  } else if (lookupResult.isNot(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    return lookupResult;
  }

  // Now we have done our best to remove the Collection.
  // It shall not be there anymore. Try to recreate it.

  // TODO the following shall be unified as well.
  if (ServerState::instance()->isCoordinator()) {
    // Build up new information that we need to merge with the given one
    VPackBuilder toMerge;
    toMerge.openObject();

    ClusterInfo& ci = server().getFeature<ClusterFeature>().clusterInfo();
    // We always need a new id
    TRI_voc_tick_t newIdTick = ci.uniqid(1);
    std::string newId = StringUtils::itoa(newIdTick);
    toMerge.add("id", VPackValue(newId));

    if (_vocbase.server().getFeature<ClusterFeature>().forceOneShard() ||
        _vocbase.isOneShard()) {
      auto const isSatellite =
          VelocyPackHelper::getStringRef(parameters, StaticStrings::ReplicationFactor,
                                         velocypack::StringRef{""}) == StaticStrings::Satellite;

      // force one shard, and force distributeShardsLike to be "_graphs"
      toMerge.add(StaticStrings::NumberOfShards, VPackValue(1));
      if (!_vocbase.IsSystemName(name) && !isSatellite) {
        // system-collections will be sharded normally. only user collections
        // will get the forced sharding. SatelliteCollections must not be
        // sharded like a non-satellite collection.
        toMerge.add(StaticStrings::DistributeShardsLike,
                    VPackValue(_vocbase.shardingPrototypeName()));
      }
    } else {
      // Number of shards. Will be overwritten if not existent
      VPackSlice const numberOfShardsSlice = parameters.get(StaticStrings::NumberOfShards);
      if (!numberOfShardsSlice.isInteger()) {
        // The information does not contain numberOfShards. Overwrite it.
        size_t numberOfShards = 1;
        VPackSlice const shards = parameters.get("shards");
        if (shards.isObject()) {
          numberOfShards = static_cast<uint64_t>(shards.length());
        }
        TRI_ASSERT(numberOfShards > 0);
        toMerge.add(StaticStrings::NumberOfShards, VPackValue(numberOfShards));
      }
    }

    if (parameters.get(StaticStrings::DataSourceGuid).isString()) {
      std::string const uuid = parameters.get(StaticStrings::DataSourceGuid).copyString();
      bool valid = false;
      NumberUtils::atoi_positive<uint64_t>(uuid.data(), uuid.data() + uuid.size(), valid);
      if (valid) {
        // globallyUniqueId is only numeric. This causes ambiguities later
        // and can only happen for collections created with v3.3.0 (the GUID
        // generation process was changed in v3.3.1 already to fix this issue).
        // remove the globallyUniqueId so a new one will be generated server.side
        toMerge.add(StaticStrings::DataSourceGuid, VPackSlice::nullSlice());
      }
    }

    // Replication Factor. Will be overwritten if not existent
    VPackSlice const replicationFactorSlice =
        parameters.get(StaticStrings::ReplicationFactor);
    // not an error: for historical reasons the write concern is read from the
    // variable "minReplicationFactor"
    VPackSlice writeConcernSlice = parameters.get(StaticStrings::WriteConcern);
    if (writeConcernSlice.isNone()) {  // minReplicationFactor is deprecated in 3.6
      writeConcernSlice = parameters.get(StaticStrings::MinReplicationFactor);
    }

    bool isValidReplicationFactorSlice =
        replicationFactorSlice.isInteger() ||
        (replicationFactorSlice.isString() &&
         replicationFactorSlice.isEqualString(StaticStrings::Satellite));

    bool isValidWriteConcernSlice =
        replicationFactorSlice.isInteger() && writeConcernSlice.isInteger() &&
        (writeConcernSlice.getInt() <= replicationFactorSlice.getInt()) &&
        writeConcernSlice.getInt() > 0;

    if (!isValidReplicationFactorSlice) {
      size_t replicationFactor =
          _vocbase.server().getFeature<ClusterFeature>().defaultReplicationFactor();
      if (replicationFactor == 0) {
        replicationFactor = 1;
      }
      TRI_ASSERT(replicationFactor > 0);
      toMerge.add(StaticStrings::ReplicationFactor, VPackValue(replicationFactor));
    }

    if (!isValidWriteConcernSlice) {
      size_t writeConcern = 1;
      if (replicationFactorSlice.isString() &&
          replicationFactorSlice.isEqualString(StaticStrings::Satellite)) {
        writeConcern = 0;
      }
      // not an error: for historical reasons the write concern is stored in the
      // variable "minReplicationFactor"
      toMerge.add(StaticStrings::MinReplicationFactor, VPackValue(writeConcern));
      toMerge.add(StaticStrings::WriteConcern, VPackValue(writeConcern));
    }

    // always use current version number when restoring a collection,
    // because the collection is effectively NEW
    toMerge.add(StaticStrings::Version, VPackSlice::nullSlice());
    if (!name.empty() && name[0] == '_' && !parameters.hasKey(StaticStrings::DataSourceSystem)) {
      // system collection?
      toMerge.add(StaticStrings::DataSourceSystem, VPackValue(true));
    }

#ifndef USE_ENTERPRISE
    std::vector<std::string> changes;

    // when in the Community Edition, we need to turn off specific attributes
    // because they are only supported in Enterprise Edition

    // watch out for "isSmart" -> we need to set this to false in the Community Edition
    VPackSlice s = parameters.get(StaticStrings::GraphIsSmart);
    if (s.isBoolean() && s.getBoolean()) {
      // isSmart needs to be turned off in the Community Edition
      toMerge.add(StaticStrings::GraphIsSmart, VPackValue(false));
      changes.push_back("changed 'isSmart' attribute value to false");
    }

    // "smartGraphAttribute" needs to be set to be removed too
    s = parameters.get(StaticStrings::GraphSmartGraphAttribute);
    if (s.isString() && !s.copyString().empty()) {
      // smartGraphAttribute needs to be removed
      toMerge.add(StaticStrings::GraphSmartGraphAttribute, VPackSlice::nullSlice());
      changes.push_back("removed 'smartGraphAttribute' attribute value");
    }

    // same for "smartJoinAttribute"
    s = parameters.get(StaticStrings::SmartJoinAttribute);
    if (s.isString() && !s.copyString().empty()) {
      // smartJoinAttribute needs to be removed
      toMerge.add(StaticStrings::SmartJoinAttribute, VPackSlice::nullSlice());
      changes.push_back("removed 'smartJoinAttribute' attribute value");
    }

    // finally rewrite all Enterprise Edition sharding strategies to a simple hash-based strategy
    s = parameters.get("shardingStrategy");
    if (s.isString() && s.copyString().find("enterprise") != std::string::npos) {
      // downgrade sharding strategy to just hash
      toMerge.add("shardingStrategy", VPackValue("hash"));
      changes.push_back("changed 'shardingStrategy' attribute value to 'hash'");
    }

    s = parameters.get(StaticStrings::ReplicationFactor);
    if (s.isString() && s.copyString() == StaticStrings::Satellite) {
      // set "satellite" replicationFactor to the default replication factor
      ClusterFeature& cl = _vocbase.server().getFeature<ClusterFeature>();

      uint32_t replicationFactor = cl.systemReplicationFactor();
      toMerge.add(StaticStrings::ReplicationFactor, VPackValue(replicationFactor));
      changes.push_back(
          std::string("changed 'replicationFactor' attribute value to ") +
          std::to_string(replicationFactor));
    }

    if (!changes.empty()) {
      LOG_TOPIC("fc359", INFO, Logger::CLUSTER)
          << "rewrote info for collection '"
          << name << "' on restore for usage with the Community Edition. the following changes were applied: "
          << basics::StringUtils::join(changes, ". ");
    }
#endif

    // Always ignore `shadowCollections` they were accidentially dumped in
    // arangodb versions earlier than 3.3.6
    toMerge.add("shadowCollections", arangodb::velocypack::Slice::nullSlice());
    toMerge.close();  // TopLevel

    VPackSlice const type = parameters.get("type");
    if (!type.isNumber()) {
      return Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                    "collection type not given or wrong");
    }

    VPackSlice const sliceToMerge = toMerge.slice();
    VPackBuilder mergedBuilder =
        VPackCollection::merge(parameters, sliceToMerge, false, /*nullMeansRemove*/ true);
    VPackBuilder arrayWrapper;
    arrayWrapper.openArray();
    arrayWrapper.add(mergedBuilder.slice());
    arrayWrapper.close();
    VPackSlice const merged = arrayWrapper.slice();

    try {
      bool createWaitsForSyncReplication =
          _vocbase.server().getFeature<ClusterFeature>().createWaitsForSyncReplication();
      // in the replication case enforcing the replication factor is absolutely
      // not desired, so it is hardcoded to false
      auto cols =
          ClusterMethods::createCollectionOnCoordinator(_vocbase, merged, ignoreDistributeShardsLikeErrors,
                                                        createWaitsForSyncReplication,
                                                        false, false, nullptr);
      ExecContext const& exec = ExecContext::current();
      TRI_ASSERT(cols.size() == 1);
      if (name[0] != '_' && !exec.isSuperuser()) {
        auth::UserManager* um = AuthenticationFeature::instance()->userManager();
        TRI_ASSERT(um != nullptr);  // should not get here
        if (um != nullptr) {
          um->updateUser(exec.user(), [&](auth::User& entry) {
            for (auto const& col : cols) {
              TRI_ASSERT(col != nullptr);
              entry.grantCollection(_vocbase.name(), col->name(), auth::Level::RW);
            }
            return TRI_ERROR_NO_ERROR;
          });
        }
      }
    } catch (basics::Exception const& ex) {
      // Error, report it.
      return Result(ex.code(), ex.what());
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL, ex.what());
    }
    // All other errors are thrown to the outside.
    return Result();
  } else {
    // We do never take any responsibility of the
    // value this pointer will point to.
    LogicalCollection* colPtr = nullptr;
    auto res = createCollection(parameters, &colPtr);

    if (res != TRI_ERROR_NO_ERROR) {
      return Result(res, StringUtils::concatT("unable to create collection: ",
                                              TRI_errno_string(res)));
    }
    // If we get here, we must have a collection ptr.
    TRI_ASSERT(colPtr != nullptr);

    // might be also called on dbservers
    if (name[0] != '_' && !ExecContext::current().isSuperuser() &&
        ServerState::instance()->isSingleServer()) {
      auth::UserManager* um = AuthenticationFeature::instance()->userManager();
      TRI_ASSERT(um != nullptr);  // should not get here
      if (um != nullptr) {
        um->updateUser(ExecContext::current().user(), [&](auth::User& entry) {
          entry.grantCollection(_vocbase.name(), col->name(), auth::Level::RW);
          return TRI_ERROR_NO_ERROR;
        });
      }
    }

    return Result();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreData(std::string const& colName) {
#ifdef USE_ENTERPRISE
  bool force = _request->parsedValue("force", false);
  if (ignoreHiddenEnterpriseCollection(colName, force)) {
    return {TRI_ERROR_NO_ERROR};
  }
#endif

  bool const generateNewRevisionIds =
      !_request->parsedValue(StaticStrings::PreserveRevisionIds, false);

  ExecContextSuperuserScope escope(
      ExecContext::current().isSuperuser() ||
      (ExecContext::current().isAdminUser() && !ServerState::readOnly()));

  if (colName == StaticStrings::UsersCollection) {
    // We need to handle the _users in a special way
    return processRestoreUsersBatch(generateNewRevisionIds);
  } 

  if (colName == StaticStrings::AnalyzersCollection &&
      ServerState::instance()->isCoordinator() &&
      _vocbase.server().hasFeature<iresearch::IResearchAnalyzerFeature>()){
    // _analyzers should be inserted via analyzers API
    return processRestoreCoordinatorAnalyzersBatch(generateNewRevisionIds);
  }

  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  SingleCollectionTransaction trx(ctx, colName, AccessMode::Type::WRITE);

  Result res = trx.begin();

  if (!res.ok()) {
    res.reset(res.errorNumber(), arangodb::basics::StringUtils::concatT(
                                     "unable to start transaction: ", res.errorMessage()));
    return res;
  }

  res = processRestoreDataBatch(trx, colName, generateNewRevisionIds);
  res = trx.finish(res);

  // for single-server we just trigger analyzers cache reload
  // once replication updated _analyzers collection. This
  // will make all newly imported analyzers available
  if (res.ok() && colName == StaticStrings::AnalyzersCollection &&
      ServerState::instance()->isSingleServer() &&
      _vocbase.server().hasFeature<iresearch::IResearchAnalyzerFeature>()) {
    _vocbase.server().getFeature<iresearch::IResearchAnalyzerFeature>().invalidate(_vocbase);
  }
  return res;
}

Result RestReplicationHandler::parseBatch(transaction::Methods& trx, 
                                          std::string const& collectionName,
                                          VPackBuilder& documentsToInsert,
                                          std::unordered_set<std::string>& documentsToRemove,
                                          bool generateNewRevisionIds) {
  // simon: originally VST was not allowed here, but in 3.7 the content-type
  // is properly set, so we can use it
  if (_request->transportType() != Endpoint::TransportType::HTTP &&
      _request->contentType() != ContentType::DUMP) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request type");
  }
 
  LogicalCollection* collection = trx.documentCollection(collectionName);
  if (!collection) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }
  PhysicalCollection* physical = collection->getPhysical();
  if (!physical) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  bool const isUsersCollection = collectionName == StaticStrings::UsersCollection;

  VPackStringRef bodyStr = _request->rawPayload();
  char const* ptr = bodyStr.data();
  char const* end = ptr + bodyStr.size();
  
  VPackBuilder builder(&basics::VelocyPackHelper::strictRequestValidationOptions);

  // First parse and collect all markers, we assemble everything in one
  // large builder holding an array
  documentsToRemove.clear();
  documentsToInsert.clear();
  documentsToInsert.openArray();

  int line = 0;
  while (ptr < end) {
    char const* pos = strchr(ptr, '\n');

    if (pos == nullptr) {
      pos = end;
    } else {
      *(const_cast<char*>(pos)) = '\0';
      ++line;
    }

    if (pos - ptr > 1) {
      // found something
      VPackSlice doc;
      TRI_replication_operation_e type = REPLICATION_INVALID;

      Result res = restoreDataParser(ptr, pos, collectionName, line, builder, doc, type);
      if (res.fail()) {
        if (res.is(TRI_ERROR_HTTP_CORRUPTED_JSON)) {
          using namespace std::literals::string_literals;
          auto data = std::string(ptr, pos);
          res.withError([&](result::Error& err) {
            err.appendErrorMessage(" in message '"s + data + "'");
          });
        }
        return res;
      }

      TRI_ASSERT(type != REPLICATION_INVALID);

      // Put into array of all parsed markers:
      if (type == REPLICATION_MARKER_DOCUMENT) {
        documentsToInsert.openObject();

        TRI_ASSERT(doc.isObject());
        bool checkKey = true;
        for (auto it : VPackObjectIterator(doc, true)) {
          // only check for "_key" attribute here if we still have to.
          // once we have seen it, it will not show up again in the same document
          bool const isKey = checkKey && (arangodb::velocypack::StringRef(it.key) == StaticStrings::KeyString);
  
          if (isKey) {
            // prevent checking for _key twice in the same document
            checkKey = false;

            if (isUsersCollection) {
              // ignore _key for _users
              continue;
            }
            if (!documentsToRemove.empty()) {
              // for any document key that we have tracked in documentsToRemove,
              // we now got a new version to insert, so we need to remove the key
              // from documentsToRemove.
              // this is expensive, but we only have to pay for it if there are
              // REPLICATION_MARKER_REMOVE markers present, which can only happen
              // with MMFiles dumps from <= 3.6
              documentsToRemove.erase(it.value.copyString());
            }
          }

          documentsToInsert.add(it.key);

          if (generateNewRevisionIds && 
              !isKey && 
              arangodb::velocypack::StringRef(it.key) == StaticStrings::RevString) {
            char ridBuffer[arangodb::basics::maxUInt64StringSize];
            RevisionId newRid = physical->newRevisionId();
            documentsToInsert.add(newRid.toValuePair(ridBuffer));
          } else {
            documentsToInsert.add(it.value);
          }
        }

        documentsToInsert.close();
      } else if (type == REPLICATION_MARKER_REMOVE) {
        // keep track of which documents to remove.
        // in case we add a document to remove here that is already in documentsToInsert,
        // this case will be detected later.
        TRI_ASSERT(doc.isString());
        documentsToRemove.emplace(doc.copyString());
      }
    }

    ptr = pos + 1;
  }

  // close array
  documentsToInsert.close();

  return {};
}

Result RestReplicationHandler::parseBatchForSystemCollection(std::string const& collectionName,
                                                             VPackBuilder& documentsToInsert,
                                                             std::unordered_set<std::string>& documentsToRemove,
                                                             bool generateNewRevisionIds) {
  TRI_ASSERT(documentsToInsert.isEmpty());

  // this "fake" transaction here is only needed to get access to the underlying
  // system collection. we will not write anything into the collection here
  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  SingleCollectionTransaction trx(ctx, collectionName, AccessMode::Type::READ);

  Result res = trx.begin();
  if (res.ok()) {
    res = parseBatch(trx, collectionName, documentsToInsert, documentsToRemove, generateNewRevisionIds);
  }
  // transaction will end here, without anything written
  return res;
}

Result RestReplicationHandler::processRestoreCoordinatorAnalyzersBatch(bool generateNewRevisionIds) {
  VPackBuilder documentsToInsert;
  std::unordered_set<std::string> documentsToRemove;
  Result res = parseBatchForSystemCollection(StaticStrings::AnalyzersCollection, documentsToInsert, documentsToRemove, generateNewRevisionIds);

  if (res.ok() && !documentsToInsert.slice().isEmptyArray()) {
    auto& analyzersFeature = _vocbase.server().getFeature<iresearch::IResearchAnalyzerFeature>();
    return analyzersFeature.bulkEmplace(_vocbase, documentsToInsert.slice());
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of _users collection
/// Special case, we shall allow to import in this collection,
/// however it is not sharded by key and we need to replace by name instead of
/// by key
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreUsersBatch(bool generateNewRevisionIds) {
  VPackBuilder documentsToInsert;
  std::unordered_set<std::string> documentsToRemove;
  Result res = parseBatchForSystemCollection(StaticStrings::UsersCollection, documentsToInsert, documentsToRemove, generateNewRevisionIds);
  if (res.fail()) {
    return res;
  }

  std::string aql(
      "FOR u IN @restored UPSERT {user: u.user} INSERT u REPLACE u "
      "INTO @@collection OPTIONS {ignoreErrors: true, silent: true, "
      "waitForSync: false, isRestore: true}");

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(StaticStrings::UsersCollection));
  bindVars->add(VPackValue("restored"));
  bindVars->add(documentsToInsert.slice());
  bindVars->close();  // bindVars

  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  arangodb::aql::Query query(ctx, arangodb::aql::QueryString(aql), bindVars);
  aql::QueryResult queryResult = query.executeSync();

  // neither agency nor dbserver should get here
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af->userManager() != nullptr);
  if (af->userManager() != nullptr) {
    af->userManager()->triggerCacheRevalidation();
  }

  return queryResult.result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreDataBatch(transaction::Methods& trx,
                                                       std::string const& collectionName,
                                                       bool generateNewRevisionIds) {
  // we'll build all documents to insert in this builder
  VPackBuilder documentsToInsert;
  std::unordered_set<std::string> documentsToRemove;
  Result res = parseBatch(trx, collectionName, documentsToInsert, documentsToRemove, generateNewRevisionIds);
  if (res.fail()) {
    return res;
  }

  OperationOptions options(_context);
  options.silent = false;
  options.ignoreRevs = true;
  options.isRestore = true;
  options.waitForSync = false;
  options.overwriteMode = OperationOptions::OverwriteMode::Replace;
  OperationResult opRes(Result(), options);

  // only go into the remove documents case when we really have to.
  if (!documentsToRemove.empty()) {
    VPackBuilder toRemove;
    toRemove.openArray();
    for (auto const& it : documentsToRemove) {
      toRemove.openObject();
      toRemove.add(StaticStrings::KeyString, VPackValue(it));
      toRemove.close();
    }
    toRemove.close();
    // we let failing removes succeed here. in case a document is not found upon remove,
    // the end result will be the same (document does not exist). in addition, the removal
    // case  is an edge case for old dumps from MMFiles from <= 3.6
    trx.remove(collectionName, toRemove.slice(), options);

    // if we have pending removes we need to rewrite the entire documentsToInsert array.
    // this is expensive, but an absolute edge case as mentioned before
    documentsToInsert = VPackCollection::filter(documentsToInsert.slice(), [&documentsToRemove](VPackSlice const& current, VPackValueLength) {
      TRI_ASSERT(current.isObject());
      VPackSlice key = current.get(StaticStrings::KeyString);
      return key.isString() && documentsToRemove.find(key.copyString()) == documentsToRemove.end();
    });
  }

  // Now try to insert all keys for which the last marker was a document marker
  VPackSlice requestSlice = documentsToInsert.slice();

  try {
    opRes = trx.insert(collectionName, requestSlice, options);
    if (opRes.fail()) {
      LOG_TOPIC("e6280", WARN, Logger::CLUSTER)
          << "could not insert " << requestSlice.length()
          << " documents for restore: " << opRes.result.errorMessage();
      return opRes.result;
    }
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC("8e8e1", WARN, Logger::CLUSTER)
        << "could not insert documents for restore exception: " << ex.what();
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    LOG_TOPIC("f1e7f", WARN, Logger::CLUSTER)
        << "could not insert documents for restore exception: " << ex.what();
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    LOG_TOPIC("368bb", WARN, Logger::CLUSTER)
        << "could not insert documents for restore exception.";
    return Result(TRI_ERROR_INTERNAL);
  }

  // Now go through the individual results and check each errors
  VPackArrayIterator itRequest(requestSlice);
  VPackArrayIterator itResult(opRes.slice());

  while (itRequest.valid()) {
    VPackSlice result = *itResult;
    VPackSlice error = result.get(StaticStrings::Error);
    if (error.isTrue()) {
      error = result.get(StaticStrings::ErrorNum);
      if (error.isNumber()) {
        auto code = ErrorCode{error.getNumericValue<int>()};
        error = result.get(StaticStrings::ErrorMessage);
        if (error.isString()) {
          return { code, error.copyString() };
        }
        return { code };
      }
    }
    itRequest.next();
    itResult.next();
  }

  return Result();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreIndexes(VPackSlice const& collection, bool force) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  if (!collection.isObject()) {
    std::string errorMsg = "collection declaration is invalid";
    return {TRI_ERROR_HTTP_BAD_PARAMETER, errorMsg};
  }

  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    std::string errorMsg = "collection parameters declaration is invalid";
    return {TRI_ERROR_HTTP_BAD_PARAMETER, errorMsg};
  }

  VPackSlice const indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    std::string errorMsg = "collection indexes declaration is invalid";
    return {TRI_ERROR_HTTP_BAD_PARAMETER, errorMsg};
  }

  VPackValueLength const n = indexes.length();

  if (n == 0) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  std::string const name =
      arangodb::basics::VelocyPackHelper::getStringValue(parameters, "name", "");

  if (name.empty()) {
    std::string errorMsg = "collection name is missing";
    return {TRI_ERROR_HTTP_BAD_PARAMETER, errorMsg};
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted", false)) {
    // we don't care about deleted collections
    return {};
  }

  std::shared_ptr<LogicalCollection> coll = _vocbase.lookupCollection(name);
  if (!coll) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  // may be used for rebuilding index infos
  VPackBuilder rebuilder;

  Result fres;

  ExecContextSuperuserScope escope(
      ExecContext::current().isSuperuser() ||
      (ExecContext::current().isAdminUser() && !ServerState::readOnly()));
  
  READ_LOCKER(readLocker, _vocbase._inventoryLock);

  // look up the collection
  try {
    auto physical = coll->getPhysical();
    TRI_ASSERT(physical != nullptr);

    for (VPackSlice idxDef : VPackArrayIterator(indexes)) {
      // {"id":"229907440927234","type":"hash","unique":false,"fields":["x","Y"]}
      VPackSlice type = idxDef.get(StaticStrings::IndexType);

      if (!type.isString()) {
        continue;
      }

      if (type.isEqualString(StaticStrings::IndexNamePrimary) || type.isEqualString(StaticStrings::IndexNameEdge)) {
        continue;
      }
    
      if (type.isEqualString("geo1") || type.isEqualString("geo2")) {
        // transform type "geo1" or "geo2" into "geo".
        rebuilder.clear();
        rebuilder.openObject();
        rebuilder.add(StaticStrings::IndexType, VPackValue("geo"));
        for (auto const& it : VPackObjectIterator(idxDef)) {
          if (!it.key.isEqualString(StaticStrings::IndexType)) {
            rebuilder.add(it.key);
            rebuilder.add(it.value);
          }
        }
        rebuilder.close();
        idxDef = rebuilder.slice();
      }

      std::shared_ptr<arangodb::Index> idx;
      try {
        bool created = false;
        idx = physical->createIndex(idxDef, /*restore*/ true, created);
      } catch (basics::Exception const& e) {
        if (e.code() == TRI_ERROR_NOT_IMPLEMENTED) {
          continue;
        }

        std::string errorMsg = "could not create index: " + e.message();
        fres.reset(e.code(), errorMsg);
        break;
      }
      TRI_ASSERT(idx != nullptr);
    }
  } catch (arangodb::basics::Exception const& ex) {
    // fix error handling
    std::string errorMsg =
        "could not create index: " + std::string(TRI_errno_string(ex.code()));
    fres.reset(ex.code(), errorMsg);
  } catch (...) {
    std::string errorMsg = "could not create index: unknown error";
    fres.reset(TRI_ERROR_INTERNAL, errorMsg);
  }

  return fres;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreIndexesCoordinator(VPackSlice const& collection,
                                                                bool force) {
  if (!collection.isObject()) {
    std::string errorMsg = "collection declaration is invalid";
    return {TRI_ERROR_HTTP_BAD_PARAMETER, errorMsg};
  }
  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    std::string errorMsg = "collection parameters declaration is invalid";
    return {TRI_ERROR_HTTP_BAD_PARAMETER, errorMsg};
  }

  VPackSlice indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    std::string errorMsg = "collection indexes declaration is invalid";
    return {TRI_ERROR_HTTP_BAD_PARAMETER, errorMsg};
  }

  size_t const n = static_cast<size_t>(indexes.length());

  if (n == 0) {
    // nothing to do
    return {};
  }

  std::string name =
      arangodb::basics::VelocyPackHelper::getStringValue(parameters, "name", "");

  if (name.empty()) {
    std::string errorMsg = "collection indexes declaration is invalid";
    return {TRI_ERROR_HTTP_BAD_PARAMETER, errorMsg};
  }

  if (ignoreHiddenEnterpriseCollection(name, force)) {
    return {};
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted", false)) {
    // we don't care about deleted collections
    return {};
  }

  auto& dbName = _vocbase.name();

  // in a cluster, we only look up by name:
  ClusterInfo& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::shared_ptr<LogicalCollection> col = ci.getCollectionNT(dbName, name);
  if (col == nullptr) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            ClusterInfo::getCollectionNotFoundMsg(dbName, name)};
  }

  TRI_ASSERT(col != nullptr);

  auto& cluster = _vocbase.server().getFeature<ClusterFeature>();

  // may be used for rebuilding index infos
  VPackBuilder rebuilder;

  Result res;

  for (VPackSlice idxDef : VPackArrayIterator(indexes)) {
    VPackSlice type = idxDef.get(StaticStrings::IndexType);

    if (!type.isString()) {
      // invalid type, cannot handle it
      continue;
    }

    if (type.isEqualString(StaticStrings::IndexNamePrimary) || type.isEqualString(StaticStrings::IndexNameEdge)) {
      // must ignore these types of indexes during restore
      continue;
    }

    if (type.isEqualString("geo1") || type.isEqualString("geo2")) {
      // transform type "geo1" or "geo2" into "geo".
      rebuilder.clear();
      rebuilder.openObject();
      rebuilder.add(StaticStrings::IndexType, VPackValue("geo"));
      for (auto const& it : VPackObjectIterator(idxDef)) {
        if (!it.key.isEqualString(StaticStrings::IndexType)) {
          rebuilder.add(it.key);
          rebuilder.add(it.value);
        }
      }
      rebuilder.close();
      idxDef = rebuilder.slice();
    }

    if (type.isEqualString("fulltext")) {
      VPackSlice minLength = idxDef.get("minLength");
      if (minLength.isNumber()) {
        int length = minLength.getNumericValue<int>();
        if (length <= 0) {
          rebuilder.clear();
          rebuilder.openObject();
          rebuilder.add("minLength", VPackValue(1));
          for (auto const& it : VPackObjectIterator(idxDef)) {
            if (!it.key.isEqualString("minLength")) {
              rebuilder.add(it.key);
              rebuilder.add(it.value);
            }
          }
          rebuilder.close();
          idxDef = rebuilder.slice();
        }
      }
    }

    VPackBuilder tmp;

    res = ci.ensureIndexCoordinator(*col, idxDef, true, tmp, cluster.indexCreationTimeout());

    if (res.fail()) {
      return res.reset(res.errorNumber(),
                       StringUtils::concatT("could not create index: ", res.errorMessage()));
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the views
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreView() {
  bool parseSuccess = false;
  VPackSlice slice = this->parseVPackBody(parseSuccess);

  if (!parseSuccess) {
    return;  // error message generated in parseVPackBody
  }

  if (!slice.isObject()) {
    generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);

    return;
  }

  bool overwrite = _request->parsedValue<bool>("overwrite", false);
  auto nameSlice = slice.get(StaticStrings::DataSourceName);
  auto typeSlice = slice.get(StaticStrings::DataSourceType);

  if (!nameSlice.isString() || !typeSlice.isString()) {
    generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    return;
  }

  LOG_TOPIC("f874e", TRACE, Logger::REPLICATION)
      << "restoring view: " << nameSlice.copyString();

  try {
    CollectionNameResolver resolver(_vocbase);
    auto view = resolver.getView(nameSlice.copyString());

    if (view) {
      if (!overwrite) {
        generateError(GeneralResponse::responseCode(TRI_ERROR_ARANGO_DUPLICATE_NAME),
                      TRI_ERROR_ARANGO_DUPLICATE_NAME,
                      StringUtils::concatT("unable to restore view '",
                                           nameSlice.copyString(), ": ",
                                           TRI_errno_string(TRI_ERROR_ARANGO_DUPLICATE_NAME)));
        return;
      }

      auto res = view->drop();

      if (!res.ok()) {
        generateError(res);
        return;
      }
    }

    auto res = LogicalView::create(view, _vocbase, slice);  // must create() since view was drop()ed

    if (!res.ok()) {
      generateError(res);

      return;
    }

    if (view == nullptr) {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                    "problem creating view");

      return;
    }
  } catch (basics::Exception const& ex) {
    generateError(GeneralResponse::responseCode(ex.code()), ex.code(), ex.message());

    return;
  } catch (...) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "problem creating view");

    return;
  }

  VPackBuilder result;

  result.openObject();
  result.add("result", VPackValue(true));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_serverID
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandServerId() {
  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  std::string const serverId = StringUtils::itoa(ServerIdFeature::getId().id());
  result.add("serverId", VPackValue(serverId));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_synchronize
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandSync() {
  bool isGlobal;
  ReplicationApplier* applier = getApplier(isGlobal);
  if (applier == nullptr) {
    return;
  }

  bool success = false;
  VPackSlice const body = this->parseVPackBody(success);
  if (!success) {
    // error already created
    return;
  }

  std::string const endpoint =
      VelocyPackHelper::getStringValue(body, "endpoint", "");
  if (endpoint.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "<endpoint> must be a valid endpoint");
    return;
  }

  std::string dbname = isGlobal ? "" : _vocbase.name();
  auto config =
      ReplicationApplierConfiguration::fromVelocyPack(_vocbase.server(), body, dbname);

  // will throw if invalid
  config.validate();

  std::shared_ptr<InitialSyncer> syncer;

  if (isGlobal) {
    syncer = GlobalInitialSyncer::create(config);
  } else {
    syncer = DatabaseInitialSyncer::create(_vocbase, config);
  }

  Result r = syncer->run(config._incremental);

  if (r.fail()) {
    LOG_TOPIC("c4818", ERR, Logger::REPLICATION)
        << "failed to sync: " << r.errorMessage();
    generateError(r);
    return;
  }

  // FIXME: change response for databases
  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("collections", VPackValue(VPackValueType::Array));
  for (auto const& it : syncer->getProcessedCollections()) {
    std::string const cidString = StringUtils::itoa(it.first.id());
    // Insert a collection
    result.add(VPackValue(VPackValueType::Object));
    result.add("id", VPackValue(cidString));
    result.add("name", VPackValue(it.second));
    result.close();  // one collection
  }
  result.close();  // collections

  auto tickString = std::to_string(syncer->getLastLogTick());
  result.add("lastLogTick", VPackValue(tickString));

  result.close();  // base
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierGetConfig() {
  bool isGlobal;
  ReplicationApplier* applier = getApplier(isGlobal);
  if (applier == nullptr) {
    return;
  }

  auto configuration = applier->configuration();
  VPackBuilder builder;
  builder.openObject();
  configuration.toVelocyPack(builder, false, false);
  builder.close();

  generateResult(rest::ResponseCode::OK, builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_adjust
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierSetConfig() {
  bool isGlobal;
  ReplicationApplier* applier = getApplier(isGlobal);
  if (applier == nullptr) {
    return;
  }

  bool success = false;
  VPackSlice const body = this->parseVPackBody(success);
  if (!success) {
    // error already created
    return;
  }

  std::string databaseName;

  if (!isGlobal) {
    databaseName = _vocbase.name();
  }

  auto config =
      ReplicationApplierConfiguration::fromVelocyPack(applier->configuration(),
                                                      body, databaseName);
  // will throw if invalid
  config.validate();

  applier->reconfigure(config);
  handleCommandApplierGetConfig();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_start
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierStart() {
  bool isGlobal;
  ReplicationApplier* applier = getApplier(isGlobal);
  if (applier == nullptr) {
    return;
  }

  bool found;
  std::string const& value1 = _request->value("from", found);

  TRI_voc_tick_t initialTick = 0;
  bool useTick = false;

  if (found) {
    // query parameter "from" specified
    initialTick = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value1));
    useTick = true;
  }

  applier->startTailing(initialTick, useTick);
  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_stop
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierStop() {
  bool isGlobal;
  ReplicationApplier* applier = getApplier(isGlobal);
  if (applier == nullptr) {
    return;
  }

  applier->stopAndJoin();
  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_applier_state
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierGetState() {
  bool isGlobal;
  ReplicationApplier* applier = getApplier(isGlobal);
  if (applier == nullptr) {
    return;
  }

  VPackBuilder builder;
  builder.openObject();
  applier->toVelocyPack(builder);
  builder.close();
  generateResult(rest::ResponseCode::OK, builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_applier_state_all
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierGetStateAll() {
  if (_request->databaseName() != StaticStrings::SystemDatabase) {
    generateError(
        rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
        "global inventory can only be fetched from within _system database");
    return;
  }
  DatabaseFeature& databaseFeature = _vocbase.server().getFeature<DatabaseFeature>();

  VPackBuilder builder;
  builder.openObject();
  for (auto& name : databaseFeature.getDatabaseNames()) {
    TRI_vocbase_t* vocbase = databaseFeature.lookupDatabase(name);

    if (vocbase == nullptr) {
      continue;
    }

    ReplicationApplier* applier = vocbase->replicationApplier();

    if (applier == nullptr) {
      continue;
    }

    builder.add(name, VPackValue(VPackValueType::Object));
    applier->toVelocyPack(builder);
    builder.close();
  }
  builder.close();

  generateResult(rest::ResponseCode::OK, builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete the state of the replication applier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierDeleteState() {
  bool isGlobal;
  ReplicationApplier* applier = getApplier(isGlobal);
  if (applier == nullptr) {
    return;
  }

  applier->forget();

  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a follower of a shard to the list of followers
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandAddFollower() {
  TRI_ASSERT(ServerState::instance()->isDBServer());

  bool success = false;
  VPackSlice const body = this->parseVPackBody(success);
  if (!success) {
    // error already created
    return;
  }
  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an object with attributes 'followerId' "
                  "and 'shard'");
    return;
  }
  VPackSlice const followerIdSlice = body.get("followerId");
  VPackSlice const readLockIdSlice = body.get("readLockId");
  VPackSlice const shardSlice = body.get("shard");
  VPackSlice const checksumSlice = body.get("checksum");
  if (!followerIdSlice.isString() || !shardSlice.isString() || !checksumSlice.isString()) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "'followerId', 'shard' and 'checksum' attributes must be strings");
    return;
  }

  auto col = _vocbase.lookupCollection(shardSlice.copyString());
  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "did not find collection");
    return;
  }

  std::string const followerId = followerIdSlice.copyString();
  LOG_TOPIC("312cc", DEBUG, Logger::REPLICATION)
      << "Attempt to Add Follower: " << followerId << " to shard "
      << col->name() << " in database: " << _vocbase.name();
  // Short cut for the case that the collection is empty
  if (readLockIdSlice.isNone()) {
    LOG_TOPIC("aaff2", DEBUG, Logger::REPLICATION)
        << "Try add follower fast-path (no documents)";
    auto ctx = transaction::StandaloneContext::Create(_vocbase);
    SingleCollectionTransaction trx(ctx, *col, AccessMode::Type::EXCLUSIVE);
    auto res = trx.begin();

    if (res.ok()) {
      OperationOptions options(_context);
      auto countRes = trx.count(col->name(), transaction::CountType::Normal, options);

      if (countRes.ok()) {
        VPackSlice nrSlice = countRes.slice();
        uint64_t nr = nrSlice.getNumber<uint64_t>();
        LOG_TOPIC("533c3", DEBUG, Logger::REPLICATION)
            << "Compare with shortcut Leader: " << nr
            << " == Follower: " << checksumSlice.copyString();
        if (nr == 0 && checksumSlice.isEqualString("0")) {
          res = col->followers()->add(followerId);

          if (res.fail()) {
            // this will create an error response with the appropriate message
            THROW_ARANGO_EXCEPTION(res);
          }

          VPackBuilder b;
          {
            VPackObjectBuilder bb(&b);
            b.add(StaticStrings::Error, VPackValue(false));
          }

          generateResult(rest::ResponseCode::OK, b.slice());
          LOG_TOPIC("c316e", DEBUG, Logger::REPLICATION)
              << followerId << " is now following on shard " << _vocbase.name()
              << "/" << col->name();
          return;
        }
      }
    }
    // If we get here, we have to report an error:
    generateError(rest::ResponseCode::FORBIDDEN,
                  TRI_ERROR_REPLICATION_SHARD_NONEMPTY, "shard not empty");
    LOG_TOPIC("8bf6e", DEBUG, Logger::REPLICATION)
        << followerId << " is not yet in sync with " << _vocbase.name() << "/"
        << col->name();
    return;
  }

  if (!readLockIdSlice.isString() || readLockIdSlice.getStringLength() == 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'readLockId' is not a string or empty");
    return;
  }
  LOG_TOPIC("23b9f", DEBUG, Logger::REPLICATION)
      << "Try add follower with documents";
  // previous versions < 3.3x might not send the checksum, if mixed clusters
  // get into trouble here we may need to be more lenient
  TRI_ASSERT(checksumSlice.isString() && readLockIdSlice.isString());

  TransactionId readLockId = ExtractReadlockId(readLockIdSlice);

  // referenceChecksum is the stringified number of documents in the collection
  ResultT<std::string> referenceChecksum =
      computeCollectionChecksum(readLockId, col.get());
  if (!referenceChecksum.ok()) {
    generateError(std::move(referenceChecksum).result());
    return;
  }

  LOG_TOPIC("40b17", DEBUG, Logger::REPLICATION)
      << "Compare Leader: " << referenceChecksum.get()
      << " == Follower: " << checksumSlice.copyString();
  if (!checksumSlice.isEqualString(referenceChecksum.get())) {
    LOG_TOPIC("94ebe", DEBUG, Logger::REPLICATION)
        << followerId << " is not yet in sync with " << _vocbase.name() << "/"
        << col->name();
    std::string const checksum = checksumSlice.copyString();
    LOG_TOPIC("592ef", WARN, Logger::REPLICATION)
        << "Cannot add follower " << followerId << " for shard "
        << _vocbase.name() << "/" << col->name() 
        << ", mismatching checksums. "
        << "Expected (leader): " << referenceChecksum.get() << ", actual (follower): " << checksum;
    generateError(rest::ResponseCode::BAD, TRI_ERROR_REPLICATION_WRONG_CHECKSUM,
                  "'checksum' is wrong. Expected (leader): " + referenceChecksum.get() +
                      ". actual (follower): " + checksum);
    return;
  }

  Result res = col->followers()->add(followerId);

  if (res.fail()) {
    // this will create an error response with the appropriate message
    THROW_ARANGO_EXCEPTION(res);
  }

  {  // untrack the (async) replication client, so the WAL may be cleaned
    arangodb::ServerId const serverId{StringUtils::uint64(
        basics::VelocyPackHelper::getStringValue(body, "serverId", ""))};
    SyncerId const syncerId = SyncerId{StringUtils::uint64(
        basics::VelocyPackHelper::getStringValue(body, "syncerId", ""))};
    std::string const clientInfo =
        basics::VelocyPackHelper::getStringValue(body, "clientInfo", "");

    _vocbase.replicationClients().untrack(SyncerId{syncerId}, serverId, clientInfo);
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add(StaticStrings::Error, VPackValue(false));
  }

  LOG_TOPIC("c13d4", DEBUG, Logger::REPLICATION)
      << followerId << " is now following on shard " << _vocbase.name() << "/"
      << col->name();
  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a follower of a shard from the list of followers
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRemoveFollower() {
  TRI_ASSERT(ServerState::instance()->isDBServer());

  bool success = false;
  VPackSlice const body = this->parseVPackBody(success);
  if (!success) {
    // error already created
    return;
  }
  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an object with attributes 'followerId' "
                  "and 'shard'");
    return;
  }
  VPackSlice const followerId = body.get("followerId");
  VPackSlice const shard = body.get("shard");
  if (!followerId.isString() || !shard.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'followerId' and 'shard' attributes must be strings");
    return;
  }

  auto col = _vocbase.lookupCollection(shard.copyString());

  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "did not find collection");
    return;
  }

  Result res = col->followers()->remove(followerId.copyString());

  if (res.fail()) {
    // this will create an error response with the appropriate message
    THROW_ARANGO_EXCEPTION(res);
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add(StaticStrings::Error, VPackValue(false));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief update the leader of a shard
//////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandSetTheLeader() {
  TRI_ASSERT(ServerState::instance()->isDBServer());

  bool success = false;
  VPackSlice const body = this->parseVPackBody(success);
  if (!success) {
    // error already created
    return;
  }
  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an object with attributes 'leaderId' "
                  "and 'shard'");
    return;
  }
  VPackSlice const leaderIdSlice = body.get("leaderId");
  VPackSlice const oldLeaderIdSlice = body.get("oldLeaderId");
  VPackSlice const shard = body.get("shard");
  if (!leaderIdSlice.isString() || !shard.isString() || !oldLeaderIdSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'leaderId' and 'shard' attributes must be strings");
    return;
  }

  std::string leaderId = leaderIdSlice.copyString();
  auto col = _vocbase.lookupCollection(shard.copyString());

  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "did not find collection");
    return;
  }

  std::string currentLeader = col->followers()->getLeader();
  if (currentLeader == arangodb::maintenance::ResignShardLeadership::LeaderNotYetKnownString) {
    // We have resigned, check that we are the old leader
    currentLeader = ServerState::instance()->getId();
  }

  if (leaderId != currentLeader) {
    Result res = checkPlanLeaderDirect(col, leaderId);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    if (!oldLeaderIdSlice.isEqualString(currentLeader)) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                    "old leader not as expected");
      return;
    }

    col->followers()->setTheLeader(leaderId);
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add(StaticStrings::Error, VPackValue(false));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hold a read lock on a collection to stop writes temporarily
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandHoldReadLockCollection() {
  TRI_ASSERT(ServerState::instance()->isDBServer());
  bool success = false;
  VPackSlice const body = this->parseVPackBody(success);
  if (!success) {
    // error already created
    return;
  }

  RebootId rebootId(0);
  std::string serverId;

  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an object with attributes 'collection', "
                  "'ttl' and 'id'");
    return;
  }

  VPackSlice collection = body.get("collection");
  VPackSlice ttlSlice = body.get("ttl");
  VPackSlice idSlice = body.get("id");

  if (body.hasKey(StaticStrings::RebootId)) {
    if (body.get(StaticStrings::RebootId).isInteger()) {
      if (body.hasKey("serverId") && body.get("serverId").isString()) {
        rebootId = RebootId(body.get(StaticStrings::RebootId).getNumber<uint64_t>());
        serverId = body.get("serverId").copyString();
      } else {
        generateError(
            rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
            "'rebootId' must be accompanied by string attribute 'serverId'");
        return;
      }
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "'rebootId' must be an integer attribute");
      return;
    }
  }

  if (!collection.isString() || !ttlSlice.isNumber() || !idSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'collection' must be a string and 'ttl' a number and "
                  "'id' a string");
    return;
  }

  TransactionId id = ExtractReadlockId(idSlice);
  auto col = _vocbase.lookupCollection(collection.copyString());

  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "did not find collection");
    return;
  }

  // 0.0 means using the default timeout (whatever that is)
  double ttl = VelocyPackHelper::getNumericValue(ttlSlice, 0.0);

  if (col->getStatusLocked() != TRI_VOC_COL_STATUS_LOADED) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED,
                  "collection not loaded");
    return;
  }

  // This is an optional parameter, it may not be set (backwards compatible)
  // If it is not set it will default to a hard-lock, otherwise we do a
  // potentially faster soft-lock synchronization with a smaller hard-lock
  // phase.

  bool doSoftLock = VelocyPackHelper::getBooleanValue(body, StaticStrings::ReplicationSoftLockOnly, false);
  AccessMode::Type lockType = AccessMode::Type::READ;
  if (!doSoftLock) {
    // With not doSoftLock we trigger RocksDB to stop writes on this shard.
    // With a softLock we only stop the WAL from being collected,
    // but still allow writes.
    // This has potential to never ever finish, so we need a short
    // hard lock for the final sync.
    lockType = AccessMode::Type::EXCLUSIVE;
  }

  LOG_TOPIC("4fac2", DEBUG, Logger::REPLICATION)
      << "Attempt to create a Lock: " << id << " for shard: " << _vocbase.name()
      << "/" << col->name() << " of type: " << (doSoftLock ? "soft" : "hard");
  Result res = createBlockingTransaction(id, *col, ttl, lockType, rebootId, serverId);
  if (!res.ok()) {
    generateError(res);
    return;
  }

  TRI_ASSERT(isLockHeld(id).ok());
  TRI_ASSERT(isLockHeld(id).get() == true);

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add(StaticStrings::Error, VPackValue(false));
  }

  LOG_TOPIC("61a9d", DEBUG, Logger::REPLICATION)
      << "Shard: " << _vocbase.name() << "/" << col->name()
      << " is now locked with type: " << (doSoftLock ? "soft" : "hard")
      << " lock id: " << id;
  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check the holding of a read lock on a collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandCheckHoldReadLockCollection() {
  TRI_ASSERT(ServerState::instance()->isDBServer());

  bool success = false;
  VPackSlice const body = this->parseVPackBody(success);
  if (!success) {
    // error already created
    return;
  }

  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an object with attribute 'id'");
    return;
  }
  VPackSlice const idSlice = body.get("id");
  if (!idSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'id' needs to be a string");
    return;
  }
  TransactionId id = ExtractReadlockId(idSlice);
  LOG_TOPIC("05a75", DEBUG, Logger::REPLICATION) << "Test if Lock " << id << " is still active.";
  auto res = isLockHeld(id);
  if (!res.ok()) {
    generateError(std::move(res).result());
    return;
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add(StaticStrings::Error, VPackValue(false));
    b.add("lockHeld", VPackValue(res.get()));
  }
  LOG_TOPIC("ca554", DEBUG, Logger::REPLICATION)
      << "Lock " << id << " is " << (res.get() ? "still active." : "gone.");
  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the holding of a read lock on a collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandCancelHoldReadLockCollection() {
  TRI_ASSERT(ServerState::instance()->isDBServer());

  bool success = false;
  VPackSlice const body = this->parseVPackBody(success);
  if (!success) {
    // error already created
    return;
  }

  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an object with attribute 'id'");
    return;
  }
  VPackSlice const idSlice = body.get("id");
  if (!idSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'id' needs to be a string");
    return;
  }
  TransactionId id = ExtractReadlockId(idSlice);
  LOG_TOPIC("9a5e3", DEBUG, Logger::REPLICATION) << "Attempt to cancel Lock: " << id;

  auto res = cancelBlockingTransaction(id);
  if (!res.ok()) {
    LOG_TOPIC("9caf7", DEBUG, Logger::REPLICATION)
        << "Lock " << id << " not canceled because of: " << res.errorMessage();
    generateError(std::move(res).result());
    return;
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add(StaticStrings::Error, VPackValue(false));
    b.add("lockHeld", VPackValue(res.get()));
  }

  LOG_TOPIC("a58e5", DEBUG, Logger::REPLICATION)
      << "Lock: " << id << " is now canceled, "
      << (res.get() ? "it is still in use." : "it is gone.");
  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get ID for a read lock job
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandGetIdForReadLockCollection() {
  TRI_ASSERT(ServerState::instance()->isDBServer());

  std::string id = std::to_string(transaction::Context::makeTransactionId().id());
  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("id", VPackValue(id));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_return_state
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerState() {
  TRI_ASSERT(server().hasFeature<EngineSelectorFeature>());
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();

  VPackBuilder builder;
  auto res = engine.createLoggerState(&_vocbase, builder);

  if (res.fail()) {
    LOG_TOPIC("c7471", DEBUG, Logger::REPLICATION)
        << "failed to create logger-state" << res.errorMessage();
    generateError(rest::ResponseCode::BAD, res.errorNumber(), res.errorMessage());
    return;
  }

  generateResult(rest::ResponseCode::OK, builder.slice());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the first tick available in a logfile
/// @route GET logger-first-tick
/// @caller js/client/modules/@arangodb/replication.js
/// @response VPackObject with minTick of LogfileManager->ranges()
//////////////////////////////////////////////////////////////////////////////
void RestReplicationHandler::handleCommandLoggerFirstTick() {
  TRI_voc_tick_t tick = UINT64_MAX;
  Result res = server().getFeature<EngineSelectorFeature>().engine().firstTick(tick);

  VPackBuilder b;
  b.add(VPackValue(VPackValueType::Object));
  if (tick == UINT64_MAX || res.fail()) {
    b.add("firstTick", VPackValue(VPackValueType::Null));
  } else {
    auto tickString = std::to_string(tick);
    b.add("firstTick", VPackValue(tickString));
  }
  b.close();
  generateResult(rest::ResponseCode::OK, b.slice());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the available logfile range
/// @route GET logger-tick-ranges
/// @caller js/client/modules/@arangodb/replication.js
/// @response VPackArray, containing info about each datafile
///           * filename
///           * status
///           * tickMin - tickMax
//////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerTickRanges() {
  TRI_ASSERT(server().hasFeature<EngineSelectorFeature>());
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  VPackBuilder b;
  Result res = engine.createTickRanges(b);
  if (res.ok()) {
    generateResult(rest::ResponseCode::OK, b.slice());
  } else {
    generateError(res);
  }
}

bool RestReplicationHandler::prepareRevisionOperation(RevisionOperationContext& ctx) {
  auto& selector = server().getFeature<EngineSelectorFeature>();
  if (!selector.isRocksDB()) {
    generateError(
        rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED,
        "this storage engine does not support revision-based replication");
    return false;
  }

  LOG_TOPIC("253e2", TRACE, arangodb::Logger::REPLICATION)
      << "enter prepareRevisionOperation";

  bool found = false;

  // get collection Name
  ctx.cname = _request->value("collection");
  if (ctx.cname.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return false;
  }

  // get batchId
  std::string const& batchIdString = _request->value("batchId", found);
  if (found) {
    ctx.batchId = StringUtils::uint64(batchIdString);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "replication revision tree - request misses batchId");
    return false;
  }

  // get resume
  std::string const& resumeString = _request->value("resume", found);
  if (found) {
    ctx.resume = RevisionId::fromString(resumeString);
  }

  // print request
  LOG_TOPIC("2b70f", TRACE, arangodb::Logger::REPLICATION)
      << "requested revision tree for collection '" << ctx.cname
      << "' using contextId '" << ctx.batchId << "'";

  ExecContextSuperuserScope escope(ExecContext::current().isAdminUser());

  if (!ExecContext::current().canUseCollection(_vocbase.name(), ctx.cname, auth::Level::RO)) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    return false;
  }

  ctx.collection = _vocbase.lookupCollection(ctx.cname);
  if (!ctx.collection) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    return false;
  }
  if (!ctx.collection->syncByRevision()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED,
                  "this collection doesn't support revision-based replication");
    return false;
  }

  ctx.iter =
      ctx.collection->getPhysical()->getReplicationIterator(ReplicationIterator::Ordering::Revision,
                                                            ctx.batchId);
  if (!ctx.iter) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "could not get replication iterator for collection '" +
                      ctx.cname + "'");
    return false;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the revision tree for a given collection, if available
/// @response serialized revision tree, binary
//////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRevisionTree() {
  RevisionOperationContext ctx;
  if (!prepareRevisionOperation(ctx)) {
    return;
  }

  auto tree = ctx.collection->getPhysical()->revisionTree(ctx.batchId);
  if (!tree) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "could not generate revision tree");
    return;
  }
  VPackBuffer<uint8_t> buffer;
  VPackBuilder result(buffer);
  tree->serialize(result);

  generateResult(rest::ResponseCode::OK, std::move(buffer));
}

void RestReplicationHandler::handleCommandRebuildRevisionTree() {
  RevisionOperationContext ctx;
  if (!prepareRevisionOperation(ctx)) {
    return;
  }

  Result res = ctx.collection->getPhysical()->rebuildRevisionTree();
  if (res.fail()) {
    generateError(res);
    return;
  }

  generateResult(rest::ResponseCode::NO_CONTENT, VPackSlice::nullSlice());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the requested revision ranges for a given collection, if
///        available
/// @response VPackObject, containing
///           * ranges, VPackArray of VPackArray of revisions
///           * resume, optional, if response is chunked; revision resume
///                     point to specify on subsequent requests
//////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRevisionRanges() {
  RevisionOperationContext ctx;
  if (!prepareRevisionOperation(ctx)) {
    return;
  }

  bool success = false;
  std::size_t current = 0;
  VPackSlice const body = this->parseVPackBody(success);
  if (!success) {
    // error already created
    return;
  }
  bool badFormat = !body.isArray();
  if (!badFormat) {
    std::size_t i = 0;
    RevisionId previousRight = RevisionId::none();
    for (VPackSlice entry : VPackArrayIterator(body)) {
      if (!entry.isArray() || entry.length() != 2) {
        badFormat = true;
        break;
      }
      VPackSlice first = entry.at(0);
      VPackSlice second = entry.at(1);
      if (!first.isString() || !second.isString()) {
        badFormat = true;
        break;
      }
      RevisionId left = RevisionId::fromSlice(first);
      RevisionId right = RevisionId::fromSlice(second);
      if (left == RevisionId::max() ||
          right == RevisionId::max() || left >= right ||
          left < previousRight) {
        badFormat = true;
        break;
      }
      if (left <= ctx.resume || (i > 0 && previousRight < ctx.resume)) {
        current = std::max(current, i);
      }
      previousRight = right;
      ++i;
    }
  }
  if (badFormat) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an array of pairs of revisions");
    return;
  }

  RevisionReplicationIterator& it =
      *static_cast<RevisionReplicationIterator*>(ctx.iter.get());
  it.seek(ctx.resume);

  std::size_t constexpr limit = 64 * 1024;  // ~2MB data
  std::size_t total = 0;
  VPackBuilder response;
  {
    VPackObjectBuilder obj(&response);
    RevisionId resumeNext = ctx.resume;
    VPackSlice range;
    RevisionId left;
    RevisionId right;
    auto setRange = [&body, &range, &left, &right](std::size_t index) -> void {
      range = body.at(index);
      left = RevisionId::fromSlice(range.at(0));
      right = RevisionId::fromSlice(range.at(1));
    };
    setRange(current);

    char ridBuffer[arangodb::basics::maxUInt64StringSize];
    {
      TRI_ASSERT(response.isOpenObject());
      VPackArrayBuilder rangesGuard(&response, StaticStrings::RevisionTreeRanges);
      TRI_ASSERT(response.isOpenArray());
      bool subOpen = false;
      while (total < limit && current < body.length()) {
        if (!it.hasMore() || it.revision() <= left || it.revision() > right ||
            (!subOpen && it.revision() <= right)) {
          auto target = std::max(ctx.resume, left);
          if (it.hasMore() && it.revision() < target) {
            it.seek(target);
          }
          TRI_ASSERT(!subOpen);
          response.openArray();
          subOpen = true;
          resumeNext = std::max(ctx.resume.next(), left);
        }

        if (it.hasMore() && it.revision() >= left && it.revision() <= right) {
          response.add(it.revision().toValuePair(ridBuffer));
          ++total;
          resumeNext = it.revision().next();
          it.next();
        }

        if (!it.hasMore() || it.revision() > right) {
          TRI_ASSERT(response.isOpenArray());
          TRI_ASSERT(subOpen);
          subOpen = false;
          response.close();
          TRI_ASSERT(response.isOpenArray());
          resumeNext = right.next();
          ++current;
          if (current < body.length()) {
            setRange(current);
          }
        }
      }
      if (subOpen) {
        TRI_ASSERT(response.isOpenArray());
        response.close();
        subOpen = false;
        TRI_ASSERT(response.isOpenArray());
      }
      TRI_ASSERT(!subOpen);
      TRI_ASSERT(response.isOpenArray());
      // exit scope closes "ranges"
    }

    TRI_ASSERT(response.isOpenObject());

    if (body.length() >= 1) {
      setRange(body.length() - 1);
      if (body.length() > 0 && resumeNext <= right) {
        response.add(StaticStrings::RevisionTreeResume, resumeNext.toValuePair(ridBuffer));
      }
    }
  }

  generateResult(rest::ResponseCode::OK, response.slice());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the requested documents from a given collection, if
///        available
/// @response VPackArray, containing VPackObject documents or errors
//////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRevisionDocuments() {
  RevisionOperationContext ctx;
  if (!prepareRevisionOperation(ctx)) {
    return;
  }

  bool success = false;
  VPackSlice const body = this->parseVPackBody(success);
  if (!success) {
    // error already created
    return;
  }
  bool badFormat = !body.isArray();
  if (!badFormat) {
    for (VPackSlice entry : VPackArrayIterator(body)) {
      if (!entry.isString()) {
        badFormat = true;
        break;
      }
      RevisionId rev = RevisionId::fromSlice(entry);
      if (rev == RevisionId::max()) {
        badFormat = true;
        break;
      }
    }
  }
  if (badFormat) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an array of revisions");
    return;
  }

  constexpr std::size_t sizeLimit = 16 * 1024 * 1024;
  std::size_t chunkSize = sizeLimit;

  bool found;
  std::string const& value = _request->value("chunkSize", found);

  if (found) {
    // query parameter "chunkSize" was specified
    chunkSize = StringUtils::uint64(value);
  }
  chunkSize = std::min(chunkSize, sizeLimit);

  std::size_t size = 0;  // running total, approximation
  RevisionReplicationIterator& it =
      *static_cast<RevisionReplicationIterator*>(ctx.iter.get());

  VPackBuffer<std::uint8_t> buffer;
  VPackBuilder response(buffer);
  {
    VPackArrayBuilder docs(&response);

    for (VPackSlice entry : VPackArrayIterator(body)) {
      RevisionId rev = RevisionId::fromSlice(entry);
      if (it.hasMore() && it.revision() < rev) {
        it.seek(rev);
      }
      VPackSlice res =
          it.hasMore() ? it.document() : velocypack::Slice::emptyObjectSlice();

      auto byteSize = res.byteSize();
      if (size + byteSize > chunkSize && size > 0) {
        break;
      }
      response.add(res);
      size += byteSize;
    }
  }

  auto ctxt = transaction::StandaloneContext::Create(_vocbase);
  generateResult(rest::ResponseCode::OK, std::move(buffer), ctxt);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection, based on the VelocyPack provided
////////////////////////////////////////////////////////////////////////////////

ErrorCode RestReplicationHandler::createCollection(VPackSlice slice,
                                                   arangodb::LogicalCollection** dst) {
  TRI_ASSERT(dst != nullptr);

  if (!slice.isObject()) {
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  std::string const name =
      arangodb::basics::VelocyPackHelper::getStringValue(slice, "name", "");

  if (name.empty()) {
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  std::string const uuid =
      arangodb::basics::VelocyPackHelper::getStringValue(slice,
                                                         StaticStrings::DataSourceGuid, "");

  TRI_col_type_e const type = static_cast<TRI_col_type_e>(
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(slice, "type",
                                                               int(TRI_COL_TYPE_DOCUMENT)));
  std::shared_ptr<arangodb::LogicalCollection> col;

  if (!uuid.empty()) {
    col = _vocbase.lookupCollectionByUuid(uuid);
  }

  if (col != nullptr) {
    col = _vocbase.lookupCollection(name);
  }

  if (col != nullptr && col->type() == type) {
    // TODO
    // collection already exists. TODO: compare attributes
    return TRI_ERROR_NO_ERROR;
  }

  // always use current version number when restoring a collection,
  // because the collection is effectively NEW
  VPackBuilder patch;
  patch.openObject();
  patch.add(StaticStrings::Version, VPackSlice::nullSlice());
  patch.add(StaticStrings::DataSourceSystem, VPackValue(TRI_vocbase_t::IsSystemName(name)));
  if (!uuid.empty()) {
    bool valid = false;
    NumberUtils::atoi_positive<uint64_t>(uuid.data(), uuid.data() + uuid.size(), valid);
    if (valid) {
      // globallyUniqueId is only numeric. This causes ambiguities later
      // and can only happen for collections created with v3.3.0 (the GUID
      // generation process was changed in v3.3.1 already to fix this issue).
      // remove the globallyUniqueId so a new one will be generated server.side
      patch.add(StaticStrings::DataSourceGuid, VPackSlice::nullSlice());
    }
  }
  patch.add(StaticStrings::ObjectId, VPackSlice::nullSlice());
  patch.add("cid", VPackSlice::nullSlice());
  patch.add(StaticStrings::DataSourceId, VPackSlice::nullSlice());
  patch.close();

  VPackBuilder builder =
      VPackCollection::merge(slice, patch.slice(),
                             /*mergeValues*/ true, /*nullMeansRemove*/ true);
  slice = builder.slice();

  col = _vocbase.createCollection(slice);

  if (col == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  /* Temporary ASSERTS to prove correctness of new constructor */
  TRI_ASSERT(col->system() == (name[0] == '_'));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  DataSourceId planId = DataSourceId::none();
  VPackSlice const planIdSlice = slice.get("planId");
  if (planIdSlice.isNumber()) {
    planId = DataSourceId{planIdSlice.getNumericValue<DataSourceId::BaseType>()};
  } else if (planIdSlice.isString()) {
    std::string tmp = planIdSlice.copyString();
    planId = DataSourceId{StringUtils::uint64(tmp)};
  } else if (planIdSlice.isNone()) {
    // There is no plan ID it has to be equal to collection id
    planId = col->id();
  }

  TRI_ASSERT(col->planId() == planId);
#endif

  if (dst != nullptr) {
    *dst = col.get();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the chunk size
////////////////////////////////////////////////////////////////////////////////

uint64_t RestReplicationHandler::determineChunkSize() const {
  // determine chunk size
  uint64_t chunkSize = _defaultChunkSize;

  bool found;
  std::string const& value = _request->value("chunkSize", found);

  if (found) {
    // query parameter "chunkSize" was specified
    chunkSize = StringUtils::uint64(value);

    // don't allow overly big allocations
    if (chunkSize > _maxChunkSize) {
      chunkSize = _maxChunkSize;
    }
  }

  return chunkSize;
}

ReplicationApplier* RestReplicationHandler::getApplier(bool& global) {
  global = _request->parsedValue("global", false);

  if (global && _request->databaseName() != StaticStrings::SystemDatabase) {
    generateError(
        rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
        "global inventory can only be created from within _system database");
    return nullptr;
  }

  if (global) {
    auto& replicationFeature = _vocbase.server().getFeature<ReplicationFeature>();
    return replicationFeature.globalReplicationApplier();
  } else {
    return _vocbase.replicationApplier();
  }
}

namespace {
struct RebootCookie : public arangodb::TransactionState::Cookie {
  RebootCookie(CallbackGuard&& g) : guard(std::move(g)) {}
  ~RebootCookie() = default;
  CallbackGuard guard;
};
}

Result RestReplicationHandler::createBlockingTransaction(
    TransactionId id, LogicalCollection& col, double ttl, AccessMode::Type access,
    RebootId const& rebootId, std::string const& serverId) {
  // This is a constant JSON structure for Queries.
  // we actually do not need a plan, as we only want the query registry to have
  // a hold of our transaction
  auto planBuilder = std::make_shared<VPackBuilder>(VPackSlice::emptyObjectSlice());
  
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);
  
  std::vector<std::string> read;
  std::vector<std::string> exc;
  if (access == AccessMode::Type::READ) {
    read.push_back(col.name());
  } else {
    TRI_ASSERT(access == AccessMode::Type::EXCLUSIVE);
    exc.push_back(col.name());
  }
  
  TRI_ASSERT(isLockHeld(id).is(TRI_ERROR_HTTP_NOT_FOUND));
  
  transaction::Options opts;
  opts.lockTimeout = ttl; // not sure if appropriate ?
  Result res = mgr->ensureManagedTrx(_vocbase, id, read, {}, exc, opts, ttl);

  if (res.fail()) {
    return res;
  }

  ClusterInfo& ci = server().getFeature<ClusterFeature>().clusterInfo();

  std::string vn = _vocbase.name();
  try {
    if (!serverId.empty()) {
      std::string comment = std::string("SynchronizeShard from ") + serverId +
                            " for " + col.name() + " access mode " +
                            AccessMode::typeString(access);
    
      std::function<void(void)> f = [=]() {
        try {
          // Code does not matter, read only access, so we can roll back.
          transaction::Manager* mgr = transaction::ManagerFeature::manager();
          if (mgr) {
            mgr->abortManagedTrx(id, vn);
          }
        } catch (...) {
          // All errors that show up here can only be
          // triggered if the query is destroyed in between.
        }
      };
    
      auto rGuard = std::make_unique<RebootCookie>(
        ci.rebootTracker().callMeOnChange(RebootTracker::PeerState(serverId, rebootId),
                                          std::move(f), std::move(comment)));
      auto ctx = mgr->leaseManagedTrx(id, AccessMode::Type::WRITE);
    
      if (!ctx) {
        // Trx does not exist. So we assume it got cancelled.
        return {TRI_ERROR_TRANSACTION_INTERNAL, "read transaction was cancelled"};
      }

      transaction::Methods trx{ctx};

      void* key = this; // simon: not important to get it again
      trx.state()->cookie(key, std::move(rGuard));
    }

  } catch (...) {
    // For compatibility we only return this error
    return {TRI_ERROR_TRANSACTION_INTERNAL, "cannot begin read transaction"};
  }

  if (isTombstoned(id)) {
    try {
      return mgr->abortManagedTrx(id, vn);
    } catch (...) {
      // Maybe thrown in shutdown.
    }
    // DO NOT LOCK in this case, pointless
    return {TRI_ERROR_TRANSACTION_INTERNAL, "transaction already cancelled"};
  }

  TRI_ASSERT(isLockHeld(id).ok());
  TRI_ASSERT(isLockHeld(id).get() == true);

  return Result();
}

ResultT<bool> RestReplicationHandler::isLockHeld(TransactionId id) const {
  // The query is only hold for long during initial locking
  // there it should return false.
  // In all other cases it is released quickly.
  if (_vocbase.isDropped()) {
    return ResultT<bool>::error(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  transaction::Status stats = mgr->getManagedTrxStatus(id, _vocbase.name());
  if (stats == transaction::Status::UNDEFINED) {
    return ResultT<bool>::error(TRI_ERROR_HTTP_NOT_FOUND,
                                "no hold read lock job found for id " + std::to_string(id.id()));
  }
  
  return ResultT<bool>::success(true);
}

ResultT<bool> RestReplicationHandler::cancelBlockingTransaction(TransactionId id) const {
  // This lookup is only required for API compatibility,
  // otherwise an unconditional destroy() would do.
  auto res = isLockHeld(id);
  if (res.ok()) {
    transaction::Manager* mgr = transaction::ManagerFeature::manager();
    if (mgr) {
      auto isAborted = mgr->abortManagedTrx(id, _vocbase.name());
      if (isAborted.ok()) { // lock was held
        return ResultT<bool>::success(true);
      }
      return ResultT<bool>::error(isAborted);
    }
  } else {
    registerTombstone(id);
  }
  return res;
}

ResultT<std::string> RestReplicationHandler::computeCollectionChecksum(
    TransactionId id, LogicalCollection* col) const {
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  if (!mgr) {
    return ResultT<std::string>::error(TRI_ERROR_SHUTTING_DOWN);
  }

  try {
    auto ctx = mgr->leaseManagedTrx(id, AccessMode::Type::READ);
    if (!ctx) {
      // Trx does not exist. So we assume it got cancelled.
      return ResultT<std::string>::error(TRI_ERROR_TRANSACTION_INTERNAL,
                                         "read transaction was cancelled");
    }
    
    transaction::Methods trx(ctx);
    TRI_ASSERT(trx.status() == transaction::Status::RUNNING);
    
    uint64_t num = col->numberDocuments(&trx, transaction::CountType::Normal);
    return ResultT<std::string>::success(std::to_string(num));
  } catch (...) {
    // Query exists, but is in use.
    // So in Locking phase
    return ResultT<std::string>::error(TRI_ERROR_TRANSACTION_INTERNAL,
                                       "Read lock not yet acquired!");
  }
}

static std::string IdToTombstoneKey(TRI_vocbase_t& vocbase, TransactionId id) {
  return vocbase.name() + "/" + StringUtils::itoa(id.id());
}

void RestReplicationHandler::timeoutTombstones() const {
  std::unordered_set<std::string> toDelete;
  {
    READ_LOCKER(readLocker, RestReplicationHandler::_tombLock);
    if (RestReplicationHandler::_tombstones.empty()) {
      // Fast path
      return;
    }
    auto now = std::chrono::steady_clock::now();
    for (auto const& it : RestReplicationHandler::_tombstones) {
      if (it.second < now) {
        toDelete.emplace(it.first);
      }
    }
    // Release read lock.
    // If someone writes now we do not realy care.
  }
  if (toDelete.empty()) {
    // nothing todo
    return;
  }
  WRITE_LOCKER(writeLocker, RestReplicationHandler::_tombLock);
  for (auto const& it : toDelete) {
    try {
      RestReplicationHandler::_tombstones.erase(it);
    } catch (...) {
      // erase should not throw.
      TRI_ASSERT(false);
    }
  }
}

bool RestReplicationHandler::isTombstoned(TransactionId id) const {
  std::string key = IdToTombstoneKey(_vocbase, id);
  bool isDead = false;
  {
    READ_LOCKER(readLocker, RestReplicationHandler::_tombLock);
    isDead = RestReplicationHandler::_tombstones.find(key) !=
             RestReplicationHandler::_tombstones.end();
  }
  if (!isDead) {
    // Clear Tombstone
    WRITE_LOCKER(writeLocker, RestReplicationHandler::_tombLock);
    try {
      RestReplicationHandler::_tombstones.erase(key);
    } catch (...) {
      // Just ignore, tombstone will be removed by timeout, and IDs are unique
      // anyways
      TRI_ASSERT(false);
    }
  }
  return isDead;
}

void RestReplicationHandler::registerTombstone(TransactionId id) const {
  std::string key = IdToTombstoneKey(_vocbase, id);
  {
    WRITE_LOCKER(writeLocker, RestReplicationHandler::_tombLock);
    RestReplicationHandler::_tombstones.try_emplace(
        key, std::chrono::steady_clock::now() + RestReplicationHandler::_tombstoneTimeout);
  }
  timeoutTombstones();
}
