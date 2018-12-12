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

#include "RestReplicationHandler.h"

#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/RocksDBUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterHelpers.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/FollowerInfo.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Indexes/Index.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/GlobalInitialSyncer.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
std::string const dataString("data");
std::string const keyString("key");
std::string const typeString("type");
}

uint64_t const RestReplicationHandler::_defaultChunkSize = 128 * 1024;
uint64_t const RestReplicationHandler::_maxChunkSize = 128 * 1024 * 1024;
std::chrono::hours const RestReplicationHandler::_tombstoneTimeout = std::chrono::hours(24);


basics::ReadWriteLock RestReplicationHandler::_tombLock;
std::unordered_map<std::string, std::chrono::time_point<std::chrono::steady_clock>> RestReplicationHandler::_tombstones = {};


static aql::QueryId ExtractReadlockId(VPackSlice slice) {
  TRI_ASSERT(slice.isString());
  return StringUtils::uint64(slice.copyString());
}

static bool ignoreHiddenEnterpriseCollection(std::string const& name, bool force) {
#ifdef USE_ENTERPRISE
  if (!force && name[0] == '_') {
    if (strncmp(name.c_str(), "_local_", 7) == 0 ||
        strncmp(name.c_str(), "_from_", 6) == 0 ||
        strncmp(name.c_str(), "_to_", 4) == 0) {
      LOG_TOPIC(WARN, arangodb::Logger::REPLICATION)
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

static Result restoreDataParser(char const* ptr, char const* pos,
                                std::string const& collectionName,
                                std::string& key,
                                VPackBuilder& builder, VPackSlice& doc,
                                TRI_replication_operation_e& type) {
  builder.clear();

  try {
    VPackParser parser(builder);
    parser.parse(ptr, static_cast<size_t>(pos - ptr));
  } catch (VPackException const&) {
    // Could not parse the given string
    return Result{
        TRI_ERROR_HTTP_CORRUPTED_JSON,
        "received invalid JSON data for collection " + collectionName};
  } catch (std::exception const&) {
    // Could not even build the string
    return Result{
        TRI_ERROR_HTTP_CORRUPTED_JSON,
        "received invalid JSON data for collection " + collectionName};
  } catch (...) {
    return Result{TRI_ERROR_INTERNAL};
  }

  VPackSlice const slice = builder.slice();

  if (!slice.isObject()) {
    return Result{
        TRI_ERROR_HTTP_CORRUPTED_JSON,
        "received invalid JSON data for collection " + collectionName};
  }

  type = REPLICATION_INVALID;

  for (auto const& pair : VPackObjectIterator(slice, true)) {
    if (!pair.key.isString()) {
      return Result{
          TRI_ERROR_HTTP_CORRUPTED_JSON,
          "received invalid JSON data for collection " + collectionName};
    }

    if (pair.key.isEqualString(::typeString)) {
      if (pair.value.isNumber()) {
        int v = pair.value.getNumericValue<int>();
        if (v == 2301) {
          // pre-3.0 type for edges
          type = REPLICATION_MARKER_DOCUMENT;
        } else {
          type = static_cast<TRI_replication_operation_e>(v);
        }
      }
    } else if (pair.key.isEqualString(::dataString)) {
      if (pair.value.isObject()) {
        doc = pair.value;

        VPackSlice k = doc.get(StaticStrings::KeyString);
        if (k.isString()) {
          key = k.copyString();
        }
      }
    } else if (pair.key.isEqualString(::keyString)) {
      if (key.empty()) {
        key = pair.value.copyString();
      }
    }
  }

  if (type == REPLICATION_MARKER_DOCUMENT && !doc.isObject()) {
    return Result{TRI_ERROR_HTTP_BAD_PARAMETER,
                  "got document marker without contents"};
  }

  if (key.empty()) {
    return Result{
        TRI_ERROR_HTTP_BAD_PARAMETER,
        "received invalid JSON data for collection " + collectionName};
  }

  return Result{TRI_ERROR_NO_ERROR};
}

RestReplicationHandler::RestReplicationHandler(GeneralRequest* request,
                                               GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestReplicationHandler::~RestReplicationHandler() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error if called on a coordinator server
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::isCoordinatorError() {
  if (_vocbase.type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                  TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "replication API is not supported on a coordinator");

    return true;
  }

  return false;
}

// main function that dispatches the different routes and commands
RestStatus RestReplicationHandler::execute() {
  // extract the request type
  auto const type = _request->requestType();
  auto const& suffixes = _request->suffixes();

  size_t const len = suffixes.size();

  if (len >= 1) {
    std::string const& command = suffixes[0];

    if (command == "logger-state") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandLoggerState();
    } else if (command == "logger-tick-ranges") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }
      handleCommandLoggerTickRanges();
    } else if (command == "logger-first-tick") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }
      handleCommandLoggerFirstTick();
    } else if (command == "logger-follow") {
      if (type != rest::RequestType::GET && type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }
      handleCommandLoggerFollow();
    } else if (command == "determine-open-transactions") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandDetermineOpenTransactions();
    } else if (command == "batch") {
      // access batch context in context manager
      // example call: curl -XPOST --dump - --data '{}'
      // http://localhost:5555/_api/replication/batch
      // the object may contain a "ttl" for the context

      // POST - create batch id / handle
      // PUT  - extend batch id / handle
      // DEL  - delete batchid

      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandBatch();
      }
    } else if (command == "barrier") {
      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }
      handleCommandBarrier();
    } else if (command == "inventory") {
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
        handleTrampolineCoordinator();
      } else {
        handleCommandInventory();
      }
    } else if (command == "keys") {
      // preconditions for calling this route are unclear and undocumented --
      // FIXME
      if (type != rest::RequestType::GET && type != rest::RequestType::POST &&
          type != rest::RequestType::PUT &&
          type != rest::RequestType::DELETE_REQ) {
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
    } else if (command == "dump") {
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
        handleTrampolineCoordinator();
      } else {
        handleCommandDump();
      }
    } else if (command == "restore-collection") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreCollection();
    } else if (command == "restore-indexes") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreIndexes();
    } else if (command == "restore-data") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      handleCommandRestoreData();
    } else if (command == "restore-view") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreView();
    } else if (command == "sync") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandSync();
    } else if (command == "make-slave") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandMakeSlave();
    } else if (command == "server-id") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandServerId();
    } else if (command == "applier-config") {
      if (type == rest::RequestType::GET) {
        handleCommandApplierGetConfig();
      } else {
        if (type != rest::RequestType::PUT) {
          goto BAD_CALL;
        }
        handleCommandApplierSetConfig();
      }
    } else if (command == "applier-start") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandApplierStart();
    } else if (command == "applier-stop") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      handleCommandApplierStop();
    } else if (command == "applier-state") {
      if (type == rest::RequestType::DELETE_REQ) {
        handleCommandApplierDeleteState();
      } else {
        if (type != rest::RequestType::GET) {
          goto BAD_CALL;
        }
        handleCommandApplierGetState();
      }
    } else if (command == "applier-state-all") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      handleCommandApplierGetStateAll();
    } else if (command == "clusterInventory") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isCoordinator()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
      } else {
        handleCommandClusterInventory();
      }
    } else if (command == "addFollower") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        handleCommandAddFollower();
      }
    } else if (command == "removeFollower") {
      if (type != rest::RequestType::PUT) {
        goto BAD_CALL;
      }
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
      } else {
        handleCommandRemoveFollower();
      }
    } else if (command == "holdReadLockCollection") {
      if (!ServerState::instance()->isDBServer()) {
        generateError(rest::ResponseCode::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
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
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }

  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_makeSlave
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandMakeSlave() {
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

  ReplicationApplierConfiguration configuration = ReplicationApplierConfiguration::fromVelocyPack(body, databaseName);
  configuration._skipCreateDrop = false;

  // will throw if invalid
  configuration.validate();

  // allow access to _users if appropriate
  grantTemporaryRights();

  // forget about any existing replication applier configuration
  applier->forget();
  applier->reconfigure(configuration);
  applier->startReplication();

  while(applier->isInitializing()) { // wait for initial sync
    std::this_thread::sleep_for(std::chrono::microseconds(50000));
    if (application_features::ApplicationServer::isStopping()) {
      generateError(Result(TRI_ERROR_SHUTTING_DOWN));
      return;
    }
  }
  //applier->startTailing(lastLogTick, true, barrierId);

  VPackBuilder result;
  result.openObject();
  applier->toVelocyPack(result);
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief forward a command in the coordinator case
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleTrampolineCoordinator() {
  bool useVst = false;
  if (_request->transportType() == Endpoint::TransportType::VST) {
    useVst = true;
  }
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

  std::string const& dbname = _request->databaseName();

  auto headers = std::make_shared<std::unordered_map<std::string, std::string>>(
      arangodb::getForwardableRequestHeaders(_request.get()));
  std::unordered_map<std::string, std::string> values = _request->values();
  std::string params;

  for (auto const& i : values) {
    if (i.first != "DBserver") {
      if (params.empty()) {
        params.push_back('?');
      } else {
        params.push_back('&');
      }
      params.append(StringUtils::urlEncode(i.first));
      params.push_back('=');
      params.append(StringUtils::urlEncode(i.second));
    }
  }

  // Set a few variables needed for our work:
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_SHUTTING_DOWN,
                  "shutting down server");
    return;
  }

  std::unique_ptr<ClusterCommResult> res;
  if (!useVst) {
    HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(_request.get());
    if (httpRequest == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid request type");
    }

    // Send a synchronous request to that shard using ClusterComm:
    res = cc->syncRequest(TRI_NewTickServer(), "server:" + DBserver,
                          _request->requestType(),
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              _request->requestPath() + params,
                          httpRequest->body(), *headers, 300.0);
  } else {
    // do we need to handle multiple payloads here - TODO
    // here we switch from vst to http?!
    res = cc->syncRequest(TRI_NewTickServer(), "server:" + DBserver,
                          _request->requestType(),
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              _request->requestPath() + params,
                          _request->payload().toJson(), *headers, 300.0);
  }

  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    generateError(rest::ResponseCode::BAD, TRI_ERROR_CLUSTER_TIMEOUT,
                  "timeout within cluster");
    return;
  }
  if (res->status == CL_COMM_BACKEND_UNAVAILABLE) {
    // there is no result
    generateError(rest::ResponseCode::BAD, TRI_ERROR_CLUSTER_CONNECTION_LOST,
                  "lost connection within cluster");
    return;
  }
  if (res->status == CL_COMM_ERROR) {
    // This could be a broken connection or an Http error:
    TRI_ASSERT(nullptr != res->result && res->result->isComplete());
    // In this case a proper HTTP error was reported by the DBserver,
    // we simply forward the result.
    // We intentionally fall through here.
  }

  bool dummy;
  resetResponse(
      static_cast<rest::ResponseCode>(res->result->getHttpReturnCode()));

  _response->setContentType(
      res->result->getHeaderField(StaticStrings::ContentTypeHeader, dummy));

  if (!useVst) {
    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
    if (_response == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid response type");
    }
    httpResponse->body().swap(&(res->result->getBody()));
  } else {
    std::shared_ptr<VPackBuilder> builder = res->result->getBodyVelocyPack();
    std::shared_ptr<VPackBuffer<uint8_t>> buf = builder->steal();
    _response->setPayload(std::move(*buf), true);// do we need to generate the body?!
  }

  auto const& resultHeaders = res->result->getHeaderFields();
  for (auto const& it : resultHeaders) {
    _response->setHeader(it.first, it.second);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_cluster_inventory
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandClusterInventory() {
  std::string const& dbName = _request->databaseName();
  bool includeSystem = _request->parsedValue("includeSystem", true);

  ClusterInfo* ci = ClusterInfo::instance();
  std::vector<std::shared_ptr<LogicalCollection>> cols =
      ci->getCollections(dbName);
  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add("collections", VPackValue(VPackValueType::Array));
  for (std::shared_ptr<LogicalCollection> const& c : cols) {
    // We want to check if the collection is usable and all followers
    // are in sync:
    std::shared_ptr<ShardMap> shardMap = c->shardIds();
    // shardMap is an unordered_map from ShardId (string) to a vector of
    // servers (strings), wrapped in a shared_ptr
    auto cic = ci->getCollectionCurrent(dbName, basics::StringUtils::itoa(c->id()));
    // Check all shards:
    bool isReady = true;
    bool allInSync = true;
    for (auto const& p : *shardMap) {
      auto currentServerList = cic->servers(p.first /* shardId */);
      if (currentServerList.size() == 0 || p.second.size() == 0 ||
          currentServerList[0] != p.second[0]) {
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
    LogicalView::enumerate(
      _vocbase,
      [&resultBuilder](LogicalView::ptr const& view)->bool {
        if (view) {
          resultBuilder.openObject();
            view->properties(resultBuilder, true, false); // details, !forPersistence because on restore any datasource ids will differ, so need an end-user representation
          resultBuilder.close();
        }

        return true;
      }
    );
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
  std::shared_ptr<VPackBuilder> parsedRequest;

  try {
    parsedRequest = _request->toVelocyPackBuilderPtr();
  } catch (arangodb::velocypack::Exception const& e) {
    std::string errorMsg = "invalid JSON: ";
    errorMsg += e.what();
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  errorMsg);
    return;
  } catch (...) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return;
  }
  auto pair = rocksutils::stripObjectIds(parsedRequest->slice());
  VPackSlice const slice = pair.first;

  bool overwrite = _request->parsedValue<bool>("overwrite", false);
  bool force = _request->parsedValue<bool>("force", false);;
  bool ignoreDistributeShardsLikeErrors =
      _request->parsedValue<bool>("ignoreDistributeShardsLikeErrors", false);
  uint64_t numberOfShards = _request->parsedValue<uint64_t>("numberOfShards", 0);
  uint64_t replicationFactor = _request->parsedValue<uint64_t>("replicationFactor", 1);

  Result res;
  if (ServerState::instance()->isCoordinator()) {
    res = processRestoreCollectionCoordinator(
        slice, overwrite, force, numberOfShards,
        replicationFactor, ignoreDistributeShardsLikeErrors);
  } else {
    res =
        processRestoreCollection(slice, overwrite, force);
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
  std::shared_ptr<VPackBuilder> parsedRequest;

  try {
    parsedRequest = _request->toVelocyPackBuilderPtr();
  } catch (...) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return;
  }
  VPackSlice const slice = parsedRequest->slice();

  bool force = _request->parsedValue("force", false);

  Result res;
  if (ServerState::instance()->isCoordinator()) {
    res = processRestoreIndexesCoordinator(slice, force);
  } else {
    res = processRestoreIndexes(slice, force);
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
      generateError(GeneralResponse::responseCode(res.errorNumber()),
                    res.errorNumber());
    } else {
      generateError(GeneralResponse::responseCode(res.errorNumber()),
                    res.errorNumber(),
                    std::string(TRI_errno_string(res.errorNumber())) + ": " +
                        res.errorMessage());
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
/// @brief restores the structure of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreCollection(
    VPackSlice const& collection, bool dropExisting, bool /*force*/) {
  if (!collection.isObject()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "collection declaration is invalid");
  }

  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "collection parameters declaration is invalid");
  }

  VPackSlice const indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "collection indexes declaration is invalid");
  }

  std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "collection name is missing");
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return Result();
  }

  grantTemporaryRights();

  auto* col = _vocbase.lookupCollection(name).get();

  // drop an existing collection if it exists
  if (col != nullptr) {
    if (dropExisting) {
      auto res = _vocbase.dropCollection(col->id(), true, -1.0);

      if (res.errorNumber() == TRI_ERROR_FORBIDDEN) {
        // some collections must not be dropped

        // instead, truncate them
        auto ctx = transaction::StandaloneContext::Create(_vocbase);
        SingleCollectionTransaction trx(ctx, *col, AccessMode::Type::EXCLUSIVE);

        // to turn off waitForSync!
        trx.addHint(transaction::Hints::Hint::RECOVERY);
        trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
        trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
        res = trx.begin();

        if (!res.ok()) {
          return res;
        }

        OperationOptions options;
        OperationResult opRes = trx.truncate(name, options);

        return trx.finish(opRes.result);
      }

      if (!res.ok()) {
        return Result(res.errorNumber(), std::string("unable to drop collection '") + name + "': " + res.errorMessage());
      }
      // intentionally falls through
    } else {
      return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME, std::string("unable to create collection '") + name + "': " + TRI_errno_string(TRI_ERROR_ARANGO_DUPLICATE_NAME));
    }
  }

  // now re-create the collection
  int res = createCollection(parameters, &col);

  if (res != TRI_ERROR_NO_ERROR) {
    return Result(res, std::string("unable to create collection: ") + TRI_errno_string(res));
  }

  // might be also called on dbservers
  ExecContext const* exe = ExecContext::CURRENT;
  if (name[0] != '_' && exe != nullptr && !exe->isSuperuser() &&
      ServerState::instance()->isSingleServer()) {
    auth::UserManager* um = AuthenticationFeature::instance()->userManager();
    TRI_ASSERT(um != nullptr); // should not get here
    if (um != nullptr) {
      um->updateUser(exe->user(), [&](auth::User& entry) {
        entry.grantCollection(_vocbase.name(), col->name(), auth::Level::RW);
        return TRI_ERROR_NO_ERROR;
      });
    }
  }

  return Result();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreCollectionCoordinator(
    VPackSlice const& collection, bool dropExisting, bool force,
    uint64_t numberOfShards, uint64_t replicationFactor,
    bool ignoreDistributeShardsLikeErrors) {
  if (!collection.isObject()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "collection declaration is invalid");
  }

  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "collection parameters declaration is invalid");
  }

  std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "collection name is missing");
  }

  if (ignoreHiddenEnterpriseCollection(name, force)) {
    return {TRI_ERROR_NO_ERROR};
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return Result();
  }

  auto& dbName = _vocbase.name();
  ClusterInfo* ci = ClusterInfo::instance();

  try {
    // in a cluster, we only look up by name:
    std::shared_ptr<LogicalCollection> col = ci->getCollection(dbName, name);

    // drop an existing collection if it exists
    if (dropExisting) {
      std::string errorMsg;
      int res = ci->dropCollectionCoordinator(
        dbName, std::to_string(col->id()), errorMsg, 0.0
      );

      if (res == TRI_ERROR_FORBIDDEN ||
          res ==
              TRI_ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE) {
        // some collections must not be dropped
        res = truncateCollectionOnCoordinator(dbName, name);
        if (res != TRI_ERROR_NO_ERROR) {
          return Result(res, std::string("unable to truncate collection (dropping is forbidden): '") + name + "'");
        }
        return Result(res);
      }

      if (res != TRI_ERROR_NO_ERROR) {
        return Result(res, std::string("unable to drop collection '") + name + "': " + TRI_errno_string(res));
      }
    } else {
      return Result(TRI_ERROR_ARANGO_DUPLICATE_NAME, std::string("unable to create collection '") + name + "': " + TRI_errno_string(TRI_ERROR_ARANGO_DUPLICATE_NAME));
    }
  } catch (basics::Exception const& ex) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION)
        << "processRestoreCollectionCoordinator "
        << "could not drop collection: " << ex.what();
  } catch (...) {}

  // now re-create the collection

  // Build up new information that we need to merge with the given one
  VPackBuilder toMerge;
  toMerge.openObject();

  // We always need a new id
  TRI_voc_tick_t newIdTick = ci->uniqid(1);
  std::string newId = StringUtils::itoa(newIdTick);
  toMerge.add("id", VPackValue(newId));

  // Number of shards. Will be overwritten if not existent
  VPackSlice const numberOfShardsSlice = parameters.get("numberOfShards");
  if (!numberOfShardsSlice.isInteger()) {
    // The information does not contain numberOfShards. Overwrite it.
    VPackSlice const shards = parameters.get("shards");
    if (shards.isObject()) {
      numberOfShards = static_cast<uint64_t>(shards.length());
    } else {
      // "shards" not specified
      // now check if numberOfShards property was given
      if (numberOfShards == 0) {
        // We take one shard if no value was given
        numberOfShards = 1;
      }
    }
    TRI_ASSERT(numberOfShards > 0);
    toMerge.add("numberOfShards", VPackValue(numberOfShards));
  }

  // Replication Factor. Will be overwritten if not existent
  VPackSlice const replFactorSlice = parameters.get("replicationFactor");
  bool isValidReplFactorSlice =
      replFactorSlice.isInteger() ||
        (replFactorSlice.isString() && replFactorSlice.isEqualString("satellite"));
  if (!isValidReplFactorSlice) {
    if (replicationFactor == 0) {
      replicationFactor = 1;
    }
    TRI_ASSERT(replicationFactor > 0);
    toMerge.add("replicationFactor", VPackValue(replicationFactor));
  }

  // always use current version number when restoring a collection,
  // because the collection is effectively NEW
  toMerge.add("version", VPackValue(LogicalCollection::VERSION_33));
  if (!name.empty() && name[0] == '_' && !parameters.hasKey("isSystem")) {
    // system collection?
    toMerge.add("isSystem", VPackValue(true));
  }


  // Always ignore `shadowCollections` they were accidentially dumped in arangodb versions
  // earlier than 3.3.6
  toMerge.add("shadowCollections", arangodb::velocypack::Slice::nullSlice());
  toMerge.close();  // TopLevel

  VPackSlice const type = parameters.get("type");
  if (!type.isNumber()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "collection type not given or wrong");
  }

  TRI_col_type_e collectionType = static_cast<TRI_col_type_e>(type.getNumericValue<int>());

  VPackSlice const sliceToMerge = toMerge.slice();
  VPackBuilder mergedBuilder =
      VPackCollection::merge(parameters, sliceToMerge, false, true);
  VPackSlice const merged = mergedBuilder.slice();

  try {
    bool createWaitsForSyncReplication =
        application_features::ApplicationServer::getFeature<ClusterFeature>(
            "Cluster")
            ->createWaitsForSyncReplication();
    // in the replication case enforcing the replication factor is absolutely
    // not desired, so it is hardcoded to false
    auto col = ClusterMethods::createCollectionOnCoordinator(
      collectionType,
      _vocbase,
      merged,
      ignoreDistributeShardsLikeErrors,
      createWaitsForSyncReplication,
      false
    );
    TRI_ASSERT(col != nullptr);

    ExecContext const* exe = ExecContext::CURRENT;
    if (name[0] != '_' && exe != nullptr && !exe->isSuperuser()) {
      auth::UserManager* um = AuthenticationFeature::instance()->userManager();
      TRI_ASSERT(um != nullptr); // should not get here
      if (um != nullptr) {
        um->updateUser(ExecContext::CURRENT->user(),
                       [&](auth::User& entry) {
                         entry.grantCollection(dbName, col->name(), auth::Level::RW);
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

  grantTemporaryRights();

  if (colName == TRI_COL_NAME_USERS) {
    // We need to handle the _users in a special way
    return processRestoreUsersBatch(colName);
  }

  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  SingleCollectionTransaction trx(ctx, colName, AccessMode::Type::WRITE);

  trx.addHint(transaction::Hints::Hint::RECOVERY);  // to turn off waitForSync!

  Result res = trx.begin();

  if (!res.ok()) {
    res.reset(res.errorNumber(), std::string("unable to start transaction (") +
                                     std::string(__FILE__) + std::string(":") +
                                     std::to_string(__LINE__) +
                                     std::string("): ") + res.errorMessage());
    return res;
  }

  res = processRestoreDataBatch(trx, colName);
  res = trx.finish(res);

  return res;
}

Result RestReplicationHandler::parseBatch(
    std::string const& collectionName,
    std::unordered_map<std::string, VPackValueLength>& latest,
    VPackBuilder& allMarkers) {
  VPackBuilder builder;
  allMarkers.clear();

  HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(_request.get());
  if (httpRequest == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request type");
  }

  std::string const& bodyStr = httpRequest->body();
  char const* ptr = bodyStr.c_str();
  char const* end = ptr + bodyStr.size();

  VPackValueLength currentPos = 0;

  // First parse and collect all markers, we assemble everything in one
  // large builder holding an array. We keep for each key the latest
  // entry.

  {
    VPackArrayBuilder guard(&allMarkers);
    std::string key;
    while (ptr < end) {
      char const* pos = strchr(ptr, '\n');

      if (pos == nullptr) {
        pos = end;
      } else {
        *((char*)pos) = '\0';
      }

      if (pos - ptr > 1) {
        // found something
        key.clear();
        VPackSlice doc;
        TRI_replication_operation_e type = REPLICATION_INVALID;

        Result res = restoreDataParser(ptr, pos, collectionName,
                                       key, builder, doc, type);
        if (res.fail()) {
          return res;
        }

        // Put into array of all parsed markers:
        allMarkers.add(builder.slice());
        latest[key] = currentPos;
        ++currentPos;
      }

      ptr = pos + 1;
    }
  }

  return Result{TRI_ERROR_NO_ERROR};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of _users collection
/// Special case, we shall allow to import in this collection,
/// however it is not sharded by key and we need to replace by name instead of
/// by key
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreUsersBatch(
    std::string const& collectionName) {
  std::unordered_map<std::string, VPackValueLength> latest;
  VPackBuilder allMarkers;

  parseBatch(collectionName, latest, allMarkers);

  VPackSlice allMarkersSlice = allMarkers.slice();

  std::string aql(
      "FOR u IN @restored UPSERT {name: u.name} INSERT u REPLACE u "
      "INTO @@collection OPTIONS {ignoreErrors: true, silent: true, "
      "waitForSync: false, isRestore: true}");

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(collectionName));
  bindVars->add(VPackValue("restored"));
  bindVars->openArray();

  // Now loop over the markers and insert the docs, ignoring _key and _id
  for (auto const& p : latest) {
    VPackSlice const marker = allMarkersSlice.at(p.second);
    VPackSlice const typeSlice = marker.get(::typeString);
    TRI_replication_operation_e type = REPLICATION_INVALID;
    if (typeSlice.isNumber()) {
      int typeInt = typeSlice.getNumericValue<int>();
      if (typeInt == 2301) {  // pre-3.0 type for edges
        type = REPLICATION_MARKER_DOCUMENT;
      } else {
        type = static_cast<TRI_replication_operation_e>(typeInt);
      }
    }
    if (type == REPLICATION_MARKER_DOCUMENT) {
      VPackSlice const doc = marker.get(::dataString);
      TRI_ASSERT(doc.isObject());
      // In the _users case we silently remove the _key value.
      bindVars->openObject();
      for (auto const& it : VPackObjectIterator(doc)) {
        if (StringRef(it.key) != StaticStrings::KeyString &&
            StringRef(it.key) != StaticStrings::IdString) {
          bindVars->add(it.key);
          bindVars->add(it.value);
        }
      }
      bindVars->close();
    }
  }

  bindVars->close();  // restored
  bindVars->close();  // bindVars

  arangodb::aql::Query query(
    false,
    _vocbase,
    arangodb::aql::QueryString(aql),
    bindVars,
    nullptr,
    arangodb::aql::PART_MAIN
  );
  auto queryRegistry = QueryRegistryFeature::registry();
  TRI_ASSERT(queryRegistry != nullptr);

  aql::QueryResult queryResult = query.executeSync(queryRegistry);

  // neither agency nor dbserver should get here
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af->userManager() != nullptr);
  if (af->userManager() != nullptr) {
    af->userManager()->triggerLocalReload();
    af->userManager()->triggerGlobalReload();
  }

  return Result{queryResult.code};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreDataBatch(
    transaction::Methods& trx, std::string const& collectionName) {
  std::unordered_map<std::string, VPackValueLength> latest;
  VPackBuilder allMarkers;
  parseBatch(collectionName, latest, allMarkers);

  // First remove all keys of which the last marker we saw was a deletion
  // marker:
  VPackSlice allMarkersSlice = allMarkers.slice();

  VPackBuilder oldBuilder;
  {
    VPackArrayBuilder guard(&oldBuilder);

    for (auto const& p : latest) {
      VPackSlice const marker = allMarkersSlice.at(p.second);
      VPackSlice const typeSlice = marker.get(::typeString);
      TRI_replication_operation_e type = REPLICATION_INVALID;
      if (typeSlice.isNumber()) {
        int typeInt = typeSlice.getNumericValue<int>();
        if (typeInt == 2301) {  // pre-3.0 type for edges
          type = REPLICATION_MARKER_DOCUMENT;
        } else {
          type = static_cast<TRI_replication_operation_e>(typeInt);
        }
      }
      if (type == REPLICATION_MARKER_REMOVE) {
        oldBuilder.add(VPackValue(p.first));  // Add _key
      } else if (type != REPLICATION_MARKER_DOCUMENT) {
        return Result{TRI_ERROR_REPLICATION_UNEXPECTED_MARKER,
                      "unexpected marker type " + StringUtils::itoa(type)};
      }
    }
  }

  // Note that we ignore individual errors here, as long as the main
  // operation did not fail. In particular, we intentionally ignore
  // individual "DOCUMENT NOT FOUND" errors, because they can happen!
  try {
    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;
    double startTime = TRI_microtime();
    OperationResult opRes =
        trx.remove(collectionName, oldBuilder.slice(), options);
    double duration = TRI_microtime() - startTime;
    if (opRes.fail()) {
      LOG_TOPIC(WARN, Logger::CLUSTER)
        << "Could not delete " << oldBuilder.slice().length()
        << " documents for restore: "
        << opRes.result.errorMessage();
      return opRes.result;
    }
    if (duration > 30) {
      LOG_TOPIC(INFO, Logger::PERFORMANCE) << "Restored/deleted "
        << oldBuilder.slice().length() << " documents in time: " << duration
        << " seconds.";
    }
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC(WARN, Logger::CLUSTER)
      << "Could not delete documents for restore exception: "
      << ex.what();
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    LOG_TOPIC(WARN, Logger::CLUSTER)
      << "Could not delete documents for restore exception: "
      << ex.what();
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    LOG_TOPIC(WARN, Logger::CLUSTER)
      << "Could not delete documents for restore exception.";
    return Result(TRI_ERROR_INTERNAL);
  }

  bool const isUsersOnCoordinator = (ServerState::instance()->isCoordinator()
                                     && collectionName == TRI_COL_NAME_USERS);

  // Now try to insert all keys for which the last marker was a document
  // marker, note that these could still be replace markers!
  VPackBuilder builder;
  {
    VPackArrayBuilder guard(&builder);

    for (auto const& p : latest) {
      VPackSlice const marker = allMarkersSlice.at(p.second);
      VPackSlice const typeSlice = marker.get(::typeString);
      TRI_replication_operation_e type = REPLICATION_INVALID;
      if (typeSlice.isNumber()) {
        int typeInt = typeSlice.getNumericValue<int>();
        if (typeInt == 2301) {  // pre-3.0 type for edges
          type = REPLICATION_MARKER_DOCUMENT;
        } else {
          type = static_cast<TRI_replication_operation_e>(typeInt);
        }
      }
      if (type == REPLICATION_MARKER_DOCUMENT) {
        VPackSlice const doc = marker.get(::dataString);
        TRI_ASSERT(doc.isObject());
        if (isUsersOnCoordinator) {
          // In the _users case we silently remove the _key value.
          builder.openObject();
          for (auto const& it : VPackObjectIterator(doc)) {
            if (StringRef(it.key) != StaticStrings::KeyString &&
                StringRef(it.key) != StaticStrings::IdString) {
              builder.add(it.key);
              builder.add(it.value);
            }
          }
          builder.close();
        } else {
          builder.add(doc);
        }
      }
    }
  }

  VPackSlice requestSlice = builder.slice();
  OperationResult opRes;
  try {
    OperationOptions options;
    options.silent = false;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;
    options.overwrite = true;
    double startTime = TRI_microtime();
    opRes = trx.insert(collectionName, requestSlice, options);
    double duration = TRI_microtime() - startTime;
    if (opRes.fail()) {
      LOG_TOPIC(WARN, Logger::CLUSTER)
        << "Could not insert " << requestSlice.length()
        << " documents for restore: "
        << opRes.result.errorMessage();
      return opRes.result;
    }
    if (duration > 30) {
      LOG_TOPIC(INFO, Logger::PERFORMANCE) << "Restored/inserted "
        << requestSlice.length() << " documents in time: " << duration
        << " seconds.";
    }
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC(WARN, Logger::CLUSTER)
      << "Could not insert documents for restore exception: "
      << ex.what();
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    LOG_TOPIC(WARN, Logger::CLUSTER)
      << "Could not insert documents for restore exception: "
      << ex.what();
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    LOG_TOPIC(WARN, Logger::CLUSTER)
      << "Could not insert documents for restore exception.";
    return Result(TRI_ERROR_INTERNAL);
  }

  // Now go through the individual results and check each error, if it was
  // TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED, then we have to call
  // replace on the document:
  builder.clear(); // documents for replace operation
  VPackSlice resultSlice = opRes.slice();
  {
    VPackArrayBuilder guard(&oldBuilder);
    VPackArrayBuilder guard2(&builder);
    VPackArrayIterator itRequest(requestSlice);
    VPackArrayIterator itResult(resultSlice);

    while (itRequest.valid()) {
      VPackSlice result = *itResult;
      VPackSlice error = result.get(StaticStrings::Error);
      if (error.isTrue()) {
        error = result.get(StaticStrings::ErrorNum);
        if (error.isNumber()) {
          int code = error.getNumericValue<int>();
          if (code != TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
            return code;
          }
          builder.add(*itRequest);
        } else {
          return Result(TRI_ERROR_INTERNAL);
        }
      }
      itRequest.next();
      itResult.next();
    }
  }

  if (builder.slice().length() == 0) {
    // no replace operations to perform
    return Result();
  }

  try {
    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;
    double startTime = TRI_microtime();
    opRes = trx.replace(collectionName, builder.slice(), options);
    double duration = TRI_microtime() - startTime;
    if (opRes.fail()) {
      LOG_TOPIC(WARN, Logger::CLUSTER)
        << "Could not replace " << builder.slice().length()
        << " documents for restore: "
        << opRes.result.errorMessage();
      return opRes.result;
    }
    if (duration > 30) {
      LOG_TOPIC(INFO, Logger::PERFORMANCE) << "Restored/replaced "
        << builder.slice().length() << " documents in time: " << duration
        << " seconds.";
    }
  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC(WARN, Logger::CLUSTER)
      << "Could not replace documents for restore exception: "
      << ex.what();
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    LOG_TOPIC(WARN, Logger::CLUSTER)
      << "Could not replace documents for restore exception: "
      << ex.what();
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    LOG_TOPIC(WARN, Logger::CLUSTER)
      << "Could not replace documents for restore exception.";
    return Result(TRI_ERROR_INTERNAL);
  }

  return Result();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreIndexes(VPackSlice const& collection,
                                                  bool force) {
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

  std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    std::string errorMsg = "collection name is missing";
    return {TRI_ERROR_HTTP_BAD_PARAMETER, errorMsg};
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return {};
  }
  
  std::shared_ptr<LogicalCollection> coll = _vocbase.lookupCollection(name);
  if (!coll) {
    return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  Result fres;

  grantTemporaryRights();
  READ_LOCKER(readLocker, _vocbase._inventoryLock);

  // look up the collection
  try {

    auto physical = coll->getPhysical();
    TRI_ASSERT(physical != nullptr);
    
    for (VPackSlice const& idxDef : VPackArrayIterator(indexes)) {
      // {"id":"229907440927234","type":"hash","unique":false,"fields":["x","Y"]}
      arangodb::velocypack::Slice value = idxDef.get(StaticStrings::IndexType);
      if (value.isString()) {
        std::string const typeString = value.copyString();
        if ((typeString == "primary") ||(typeString == "edge")) {
          LOG_TOPIC(DEBUG, Logger::REPLICATION)
              << "processRestoreIndexes silently ignoring primary or edge "
              << "index: " << idxDef.toJson();
          continue;
        }
      }

      std::shared_ptr<arangodb::Index> idx;
      try {
        bool created = false;
        idx = physical->createIndex(idxDef, /*restore*/true, created);
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

Result RestReplicationHandler::processRestoreIndexesCoordinator(
    VPackSlice const& collection, bool force) {
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

  std::string name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    std::string errorMsg = "collection indexes declaration is invalid";
    return {TRI_ERROR_HTTP_BAD_PARAMETER, errorMsg};
  }

  if (ignoreHiddenEnterpriseCollection(name, force)) {
    return {};
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return {};
  }

  auto& dbName = _vocbase.name();

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<LogicalCollection> col = ci->getCollectionNT(dbName, name);
  if (col == nullptr) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        ClusterInfo::getCollectionNotFoundMsg(dbName, name)};
  }

  TRI_ASSERT(col != nullptr);

  auto cluster = application_features::ApplicationServer::getFeature<ClusterFeature>("Cluster");

  Result res;
  for (VPackSlice const& idxDef : VPackArrayIterator(indexes)) {
    VPackSlice type = idxDef.get(StaticStrings::IndexType);
    if (type.isString() &&
        (type.copyString() == "primary" || type.copyString() == "edge")) {
      // must ignore these types of indexes during restore
      continue;
    }

    VPackBuilder tmp;
    std::string errorMsg;
    res = ci->ensureIndexCoordinator( //returns int that gets converted to result
      dbName,
      std::to_string(col->id()),
      idxDef,
      true,
      tmp,
      errorMsg,
      cluster->indexCreationTimeout()
    );

    if (res.fail()) {
      return res.reset(res.errorNumber(), "could not create index: " + res.errorMessage());
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
    return; // error message generated in parseVPackBody
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

  LOG_TOPIC(TRACE, Logger::REPLICATION) << "restoring view: "
    << nameSlice.copyString();

  try {
    CollectionNameResolver resolver(_vocbase);
    auto view = resolver.getView(nameSlice.toString());

    if (view) {
      if (!overwrite) {
        generateError(TRI_ERROR_ARANGO_DUPLICATE_NAME);

        return;
      }

      auto res = view->drop();

      if (!res.ok()) {
        generateError(res);

        return;
      }
    }

    auto res = LogicalView::create(view, _vocbase, slice); // must create() since view was drop()ed

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
  std::string const serverId = StringUtils::itoa(ServerIdFeature::getId());
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
  auto config = ReplicationApplierConfiguration::fromVelocyPack(body, dbname);

  // will throw if invalid
  config.validate();

  TRI_ASSERT(!config._skipCreateDrop);
  std::shared_ptr<InitialSyncer> syncer;

  if (isGlobal) {
    syncer.reset(new GlobalInitialSyncer(config));
  } else {
    syncer.reset(new DatabaseInitialSyncer(_vocbase, config));
  }

  Result r = syncer->run(config._incremental);

  if (r.fail()) {
    LOG_TOPIC(ERR, Logger::REPLICATION)
      << "failed to sync: " << r.errorMessage();
    generateError(r);
    return;
  }

  // FIXME: change response for databases
  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("collections", VPackValue(VPackValueType::Array));
  for (auto const& it : syncer->getProcessedCollections()) {
    std::string const cidString = StringUtils::itoa(it.first);
    // Insert a collection
    result.add(VPackValue(VPackValueType::Object));
    result.add("id", VPackValue(cidString));
    result.add("name", VPackValue(it.second));
    result.close();  // one collection
  }
  result.close();  // collections

  auto tickString = std::to_string(syncer->getLastLogTick());
  result.add("lastLogTick", VPackValue(tickString));

  bool const keepBarrier =
    VelocyPackHelper::getBooleanValue(body, "keepBarrier", false);
  if (keepBarrier) {
    auto barrierId = std::to_string(syncer->stealBarrier());
    result.add("barrierId", VPackValue(barrierId));
  }

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

  auto config = ReplicationApplierConfiguration::fromVelocyPack(applier->configuration(),
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

  TRI_voc_tick_t barrierId = 0;
  std::string const& value2 = _request->value("barrierId", found);
  if (found) {
    // query parameter "barrierId" specified
    barrierId = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value2));
  }

  applier->startTailing(initialTick, useTick, barrierId);
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
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                  "global inventory can only be fetched from within _system database");
    return;
  }
  DatabaseFeature* databaseFeature = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");

  VPackBuilder builder;
  builder.openObject();
  for (auto& name : databaseFeature->getDatabaseNames()) {
    TRI_vocbase_t* vocbase = databaseFeature->lookupDatabase(name);

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
  if (!followerIdSlice.isString() ||
      !shardSlice.isString() ||
      !checksumSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'followerId', 'shard' and 'checksum' attributes must be strings");
    return;
  }

  auto col = _vocbase.lookupCollection(shardSlice.copyString());
  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "did not find collection");
    return;
  }

  const std::string followerId = followerIdSlice.copyString();
  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Attempt to Add Follower: " << followerId << " to shard " << col->name() << " in database: " << _vocbase.name();
  // Short cut for the case that the collection is empty
  if (readLockIdSlice.isNone()) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Try add follower fast-path (no documents)";
    auto ctx = transaction::StandaloneContext::Create(_vocbase);
    SingleCollectionTransaction trx(ctx, *col, AccessMode::Type::EXCLUSIVE);
    auto res = trx.begin();

    if (res.ok()) {
      auto countRes = trx.count(col->name(), transaction::CountType::Normal);

      if (countRes.ok()) {
        VPackSlice nrSlice = countRes.slice();
        uint64_t nr = nrSlice.getNumber<uint64_t>();
        LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Compare with shortCut Leader: " << nr << " == Follower: " << checksumSlice.copyString();
        if (nr == 0 && checksumSlice.isEqualString("0")) {
          col->followers()->add(followerId);

          VPackBuilder b;
          {
            VPackObjectBuilder bb(&b);
            b.add(StaticStrings::Error, VPackValue(false));
          }

          generateResult(rest::ResponseCode::OK, b.slice());
          LOG_TOPIC(DEBUG, Logger::REPLICATION) << followerId << " is now following on shard " << _vocbase.name() << "/" << col->name();
          return;
        }
      }
    }
    // If we get here, we have to report an error:
    generateError(rest::ResponseCode::FORBIDDEN,
                  TRI_ERROR_REPLICATION_SHARD_NONEMPTY,
                  "shard not empty");
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << followerId << " is not yet in sync with " << _vocbase.name() << "/" << col->name();
    return;
  }

  if (!readLockIdSlice.isString() || readLockIdSlice.getStringLength() == 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'readLockId' is not a string or empty");
    return;
  }
  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Try add follower with documents";
  // previous versions < 3.3x might not send the checksum, if mixed clusters
  // get into trouble here we may need to be more lenient
  TRI_ASSERT(checksumSlice.isString() && readLockIdSlice.isString());

  aql::QueryId readLockId = ExtractReadlockId(readLockIdSlice);

  // referenceChecksum is the stringified number of documents in the collection
  ResultT<std::string> referenceChecksum = computeCollectionChecksum(readLockId, col.get());
  if (!referenceChecksum.ok()) {
    generateError(referenceChecksum);
    return;
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Compare Leader: " << referenceChecksum.get() << " == Follower: " << checksumSlice.copyString();
  if (!checksumSlice.isEqualString(referenceChecksum.get())) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << followerId << " is not yet in sync with " << _vocbase.name() << "/" << col->name();
    const std::string checksum = checksumSlice.copyString();
    LOG_TOPIC(WARN, Logger::REPLICATION) << "Cannot add follower, mismatching checksums. "
     << "Expected: " << referenceChecksum.get() << " Actual: " << checksum;
    generateError(rest::ResponseCode::BAD, TRI_ERROR_REPLICATION_WRONG_CHECKSUM,
                  "'checksum' is wrong. Expected: "
                  + referenceChecksum.get()
                  + ". Actual: " + checksum);
    return;
  }

  col->followers()->add(followerId);

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add(StaticStrings::Error, VPackValue(false));
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION) << followerId << " is now following on shard " << _vocbase.name() << "/" << col->name();
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
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "did not find collection");
    return;
  }
  col->followers()->remove(followerId.copyString());

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

  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an object with attributes 'collection', "
                  "'ttl' and 'id'");
    return;
  }

  VPackSlice const collection = body.get("collection");
  VPackSlice const ttlSlice = body.get("ttl");
  VPackSlice const idSlice = body.get("id");

  if (!collection.isString() || !ttlSlice.isNumber() || !idSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'collection' must be a string and 'ttl' a number and "
                  "'id' a string");
    return;
  }

  aql::QueryId id = ExtractReadlockId(idSlice);
  auto col = _vocbase.lookupCollection(collection.copyString());

  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "did not find collection");
    return;
  }

  double ttl = VelocyPackHelper::getNumericValue(ttlSlice, 0.0);

  if (col->getStatusLocked() != TRI_VOC_COL_STATUS_LOADED) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED,
                  "collection not loaded");
    return;
  }

  // This is an optional parameter, it may not be set (backwards compatible)
  // If it is not set it will default to a hard-lock, otherwise we do a
  // potentially faster soft-lock synchronisation with a smaller hard-lock phase.

  bool doSoftLock = VelocyPackHelper::getBooleanValue(body, "doSoftLockOnly", false);
  AccessMode::Type lockType = AccessMode::Type::READ;
  if (!doSoftLock && EngineSelectorFeature::ENGINE->typeName() == "rocksdb") {
    // With not doSoftLock we trigger RocksDB to stop writes on this shard.
    // With a softLock we only stop the WAL from being collected,
    // but still allow writes.
    // This has potential to never ever finish, so we need a short
    // hard lock for the final sync.
    lockType = AccessMode::Type::EXCLUSIVE;
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Attempt to create a Lock: " << id << " for shard: " << _vocbase.name() << "/" << col->name() << " of type: " << (doSoftLock ? "soft" : "hard");
  Result res = createBlockingTransaction(id, *col, ttl, lockType);
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

  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Shard: " << _vocbase.name() << "/" << col->name() << " is now locked with type: " << (doSoftLock ? "soft" : "hard") << " lock id: " << id;
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
  aql::QueryId id = ExtractReadlockId(idSlice);
  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Test if Lock " << id << " is still active.";
  auto res = isLockHeld(id);
  if (!res.ok()) {
    generateError(res);
    return;
  }

   VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add(StaticStrings::Error, VPackValue(false));
    b.add("lockHeld", VPackValue(res.get()));
  }
  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Lock " << id << " is " << (res.get() ? "still active." : "gone.");
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
  aql::QueryId id = ExtractReadlockId(idSlice);
  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Attempt to cancel Lock: " << id;

  auto res = cancelBlockingTransaction(id);
  if (!res.ok()) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Lock " << id << " not canceled because of: " << res.errorMessage();
    generateError(res);
    return;
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add(StaticStrings::Error, VPackValue(false));
    b.add("lockHeld", VPackValue(res.get()));
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "Lock: " << id << " is now canceled, " << (res.get() ? "it is still in use.": "it is gone.");
  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get ID for a read lock job
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandGetIdForReadLockCollection() {
  TRI_ASSERT(ServerState::instance()->isDBServer());

  std::string id = std::to_string(TRI_NewTickServer());
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
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  VPackBuilder builder;
  auto res = engine->createLoggerState(&_vocbase, builder);

  if (res.fail()) {
    LOG_TOPIC(DEBUG, Logger::REPLICATION) << "failed to create logger-state" << res.errorMessage();
    generateError(rest::ResponseCode::BAD, res.errorNumber(),
                  res.errorMessage());
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
  Result res = EngineSelectorFeature::ENGINE->firstTick(tick);

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
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);
  VPackBuilder b;
  Result res = engine->createTickRanges(b);
  if (res.ok()) {
    generateResult(rest::ResponseCode::OK, b.slice());
  } else {
    generateError(res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection, based on the VelocyPack provided
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::createCollection(VPackSlice slice,
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
      arangodb::basics::VelocyPackHelper::getStringValue(slice, "globallyUniqueId", "");

  TRI_col_type_e const type = static_cast<TRI_col_type_e>(
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          slice, "type", int(TRI_COL_TYPE_DOCUMENT)));
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
  patch.add("version", VPackValue(LogicalCollection::VERSION_31));
  if (!name.empty() && name[0] == '_' && !slice.hasKey("isSystem")) {
    // system collection?
    patch.add("isSystem", VPackValue(true));
  }
  patch.add("objectId", VPackSlice::nullSlice());
  patch.add("cid", VPackSlice::nullSlice());
  patch.add("id", VPackSlice::nullSlice());
  patch.close();

  VPackBuilder builder = VPackCollection::merge(slice, patch.slice(),
                        /*mergeValues*/true, /*nullMeansRemove*/true);
  slice = builder.slice();

  col = _vocbase.createCollection(slice);

  if (col == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  /* Temporary ASSERTS to prove correctness of new constructor */
  TRI_ASSERT(col->system() == (name[0] == '_'));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_voc_cid_t planId = 0;
  VPackSlice const planIdSlice = slice.get("planId");
  if (planIdSlice.isNumber()) {
    planId =
        static_cast<TRI_voc_cid_t>(planIdSlice.getNumericValue<uint64_t>());
  } else if (planIdSlice.isString()) {
    std::string tmp = planIdSlice.copyString();
    planId = static_cast<TRI_voc_cid_t>(StringUtils::uint64(tmp));
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

//////////////////////////////////////////////////////////////////////////////
/// @brief Grant temporary restore rights
//////////////////////////////////////////////////////////////////////////////
void RestReplicationHandler::grantTemporaryRights() {
  if (ExecContext::CURRENT != nullptr) {
    if (ExecContext::CURRENT->databaseAuthLevel() == auth::Level::RW) {
      // If you have administrative access on this database,
      // we grant you everything for restore.
      ExecContext::CURRENT = nullptr;
    }
  }
}

ReplicationApplier* RestReplicationHandler::getApplier(bool& global) {
  global = _request->parsedValue("global", false);

  if (global &&
      _request->databaseName() != StaticStrings::SystemDatabase) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                  "global inventory can only be created from within _system database");
    return nullptr;
  }

  if (global) {
    auto replicationFeature = application_features::ApplicationServer::
      getFeature<ReplicationFeature>("Replication");
    return replicationFeature->globalReplicationApplier();
  } else {
    return _vocbase.replicationApplier();
  }
}

Result RestReplicationHandler::createBlockingTransaction(aql::QueryId id,
                                                         LogicalCollection& col,
                                                         double ttl,
                                                         AccessMode::Type access) const {
  // This is a constant JSON structure for Queries.
  // we actually do not need a plan, as we only want the query registry to have
  // a hold of our transaction
  auto planBuilder = std::make_shared<VPackBuilder>(VPackSlice::emptyObjectSlice());

  auto query = std::make_unique<aql::Query>(
    false,
    _vocbase,
    planBuilder,
    nullptr, /* options */
    aql::QueryPart::PART_MAIN /* Do locking */
  );
 // NOTE: The collections are on purpose not locked here.
  // To acquire an EXCLUSIVE lock may require time under load,
  // we want to allow to cancel this operation while waiting
  // for the lock.

  auto queryRegistry = QueryRegistryFeature::registry();
  if (queryRegistry == nullptr) {
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  {
    auto ctx = transaction::StandaloneContext::Create(_vocbase);
    auto trx = std::make_unique<SingleCollectionTransaction>(ctx, col, access);
    query->setTransactionContext(ctx);
    // Inject will take over responsiblilty of transaction, even on error case.
    query->injectTransaction(trx.release());
  }
  auto trx = query->trx();
  TRI_ASSERT(trx != nullptr);
  trx->addHint(transaction::Hints::Hint::LOCK_ENTIRELY);
 
  TRI_ASSERT(isLockHeld(id).is(TRI_ERROR_HTTP_NOT_FOUND));

  try {
    queryRegistry->insert(id, query.get(), ttl, true, true);
  } catch (...) {
    // For compatibility we only return this error
    return {TRI_ERROR_TRANSACTION_INTERNAL, "cannot begin read transaction"};
  }
  // For leak protection in case of errors the unique_ptr
  // is not responsible anymore, it has been handed over to the
  // registry.
  auto q = query.release();
  // Make sure to return the query after we are done
  TRI_DEFER(queryRegistry->close(&_vocbase, id));

  if (isTombstoned(id)) {
    try {
      // Code does not matter, read only access, so we can roll back.
      queryRegistry->destroy(&_vocbase, id, TRI_ERROR_QUERY_KILLED);
    } catch (...) {
      // Maybe thrown in shutdown.
    }
    // DO NOT LOCK in this case, pointless
    return {TRI_ERROR_TRANSACTION_INTERNAL, "transaction already cancelled"};
  }

  TRI_ASSERT(isLockHeld(id).ok());
  TRI_ASSERT(isLockHeld(id).get() == false);

  return q->trx()->begin();
}

ResultT<bool> RestReplicationHandler::isLockHeld(aql::QueryId id) const {
  // The query is only hold for long during initial locking
  // there it should return false.
  // In all other cases it is released quickly.
  auto queryRegistry = QueryRegistryFeature::registry();
  if (queryRegistry == nullptr) {
    return ResultT<bool>::error(TRI_ERROR_SHUTTING_DOWN);
  }
  auto res = queryRegistry->isQueryInUse(&_vocbase, id);
  if (!res.ok()) {
    // API compatibility otherwise just return res...
    return ResultT<bool>::error(TRI_ERROR_HTTP_NOT_FOUND, "no hold read lock job found for 'id'");
  } else {
    // We need to invert the result, because:
    //   if the query is there, but is in use => we are in the process of getting the lock => lock is not held
    //   if the query is there, but is not in use => we are after the process of getting the lock => lock is held
    return ResultT<bool>::success(!res.get());
  }
}

ResultT<bool> RestReplicationHandler::cancelBlockingTransaction(aql::QueryId id) const {
  // This lookup is only required for API compatibility,
  // otherwise an unconditional destroy() would do.
  auto res = isLockHeld(id);
  if (res.ok()) {
    auto queryRegistry = QueryRegistryFeature::registry();
    if (queryRegistry == nullptr) {
      return ResultT<bool>::error(TRI_ERROR_SHUTTING_DOWN);
    }
    try {
      // Code does not matter, read only access, so we can roll back.
      queryRegistry->destroy(&_vocbase, id, TRI_ERROR_QUERY_KILLED);
    } catch (...) {
      // All errors that show up here can only be
      // triggered if the query is destroyed in between.
    }
  } else {
    registerTombstone(id);
  }
  return res;
}

ResultT<std::string> RestReplicationHandler::computeCollectionChecksum(aql::QueryId id, LogicalCollection* col) const {
  auto queryRegistry = QueryRegistryFeature::registry();
  if (queryRegistry == nullptr) {
    return ResultT<std::string>::error(TRI_ERROR_SHUTTING_DOWN);
  }

  try {
    auto query = queryRegistry->open(&_vocbase, id);
    if (query == nullptr) {
      // Query does not exist. So we assume it got cancelled.
      return ResultT<std::string>::error(TRI_ERROR_TRANSACTION_INTERNAL, "read transaction was cancelled");
    }
    TRI_DEFER(queryRegistry->close(&_vocbase, id));

    uint64_t num = col->numberDocuments(query->trx(), transaction::CountType::Normal);
    return ResultT<std::string>::success(std::to_string(num));
  } catch (...) {
    // Query exists, but is in use.
    // So in Locking phase
    return ResultT<std::string>::error(TRI_ERROR_TRANSACTION_INTERNAL, "Read lock not yet acquired!");
  }
}

static std::string IdToTombstoneKey (TRI_vocbase_t& vocbase, aql::QueryId id) {
  return vocbase.name() + "/" + StringUtils::itoa(id);
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
  for (auto const& it: toDelete) {
    try {
      RestReplicationHandler::_tombstones.erase(it);
    } catch (...) {
      // erase should not throw.
      TRI_ASSERT(false);
    }
  }
}

bool RestReplicationHandler::isTombstoned(aql::QueryId id) const {
  std::string key = IdToTombstoneKey(_vocbase, id);
  bool isDead = false;
  {
    READ_LOCKER(readLocker, RestReplicationHandler::_tombLock);
    isDead = RestReplicationHandler::_tombstones.find(key)
          != RestReplicationHandler::_tombstones.end();
  }
  if (!isDead) {
    // Clear Tombstone
    WRITE_LOCKER(writeLocker, RestReplicationHandler::_tombLock);
    try {
      RestReplicationHandler::_tombstones.erase(key);
    } catch (...) {
      // Just ignore, tombstone will be removed by timeout, and IDs are unique anyways
      TRI_ASSERT(false);
    }
  }
  return isDead;
}

void RestReplicationHandler::registerTombstone(aql::QueryId id) const {
  std::string key = IdToTombstoneKey(_vocbase, id);
  {
    WRITE_LOCKER(writeLocker, RestReplicationHandler::_tombLock);
    RestReplicationHandler::_tombstones.emplace(key, std::chrono::steady_clock::now() + RestReplicationHandler::_tombstoneTimeout); 
  }
  timeoutTombstones();
}
