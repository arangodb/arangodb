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
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

uint64_t const RestReplicationHandler::_defaultChunkSize = 128 * 1024;
uint64_t const RestReplicationHandler::_maxChunkSize = 128 * 1024 * 1024;

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

    std::string const attributeName = pair.key.copyString();

    if (attributeName == "type") {
      if (pair.value.isNumber()) {
        int v = pair.value.getNumericValue<int>();
        if (v == 2301) {
          // pre-3.0 type for edges
          type = REPLICATION_MARKER_DOCUMENT;
        } else {
          type = static_cast<TRI_replication_operation_e>(v);
        }
      }
    }

    else if (attributeName == "data") {
      if (pair.value.isObject()) {
        doc = pair.value;

        if (doc.hasKey(StaticStrings::KeyString)) {
          key = doc.get(StaticStrings::KeyString).copyString();
        }
      }
    }

    else if (attributeName == "key") {
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
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
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
  
  bool success;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(success);
  if (!success) {
    // error already created
    return;
  }
 
  std::string databaseName;
  if (!isGlobal) {
    databaseName = _vocbase->name();
  }
  
  ReplicationApplierConfiguration configuration = ReplicationApplierConfiguration::fromVelocyPack(parsedBody->slice(), databaseName);
  configuration._skipCreateDrop = false;

  // will throw if invalid
  configuration.validate();

  std::unique_ptr<InitialSyncer> syncer;
  if (isGlobal) {
    syncer.reset(new GlobalInitialSyncer(configuration));
  } else {
    syncer.reset(new DatabaseInitialSyncer(_vocbase, configuration));
  }

  // forget about any existing replication applier configuration
  applier->forget();

  // start initial synchronization
  TRI_voc_tick_t barrierId = 0;
  TRI_voc_tick_t lastLogTick = 0;
  
  try {
    Result r = syncer->run(configuration._incremental);
    if (r.fail()) {
      THROW_ARANGO_EXCEPTION(r);
    }
    lastLogTick = syncer->getLastLogTick();
    // steal the barrier from the syncer
    barrierId = syncer->stealBarrier();
  } catch (basics::Exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(ex.code(), std::string("caught exception during slave creation: ") + ex.what());
  } catch (std::exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::string("caught exception during slave creation: ") + ex.what());
  } catch (...) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "caught unknown exception during slave creation");
  }

  applier->reconfigure(configuration);
  applier->start(lastLogTick, true, barrierId);

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
    generateError(rest::ResponseCode::BAD, TRI_ERROR_SHUTTING_DOWN,
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
    res = cc->syncRequest("", TRI_NewTickServer(), "server:" + DBserver,
                          _request->requestType(),
                          "/_db/" + StringUtils::urlEncode(dbname) +
                              _request->requestPath() + params,
                          httpRequest->body(), *headers, 300.0);
  } else {
    // do we need to handle multiple payloads here - TODO
    // here we switch from vst to http?!
    res = cc->syncRequest("", TRI_NewTickServer(), "server:" + DBserver,
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
  bool found;
  bool includeSystem = true;

  std::string const& value = _request->value("includeSystem", found);

  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  ClusterInfo* ci = ClusterInfo::instance();
  std::vector<std::shared_ptr<LogicalCollection>> cols =
      ci->getCollections(dbName);

  VPackBuilder resultBuilder;
  resultBuilder.openObject();
  resultBuilder.add(VPackValue("collections"));
  resultBuilder.openArray();
  for (auto const& c : cols) {
    // We want to check if the collection is usable and all followers
    // are in sync:
    auto shardMap = c->shardIds();
    // shardMap is an unordered_map from ShardId (string) to a vector of
    // servers (strings), wrapped in a shared_ptr
    auto cic =
        ci->getCollectionCurrent(dbName, basics::StringUtils::itoa(c->cid()));
    // Check all shards:
    bool isReady = true;
    for (auto const& p : *shardMap) {
      auto currentServerList = cic->servers(p.first /* shardId */);
      if (!ClusterHelpers::compareServerLists(p.second, currentServerList)) {
        isReady = false;
      }
    }
    c->toVelocyPackForClusterInventory(resultBuilder, includeSystem, isReady);
  }
  resultBuilder.close();  // collections
  TRI_voc_tick_t tick = TRI_CurrentTickServer();
  auto tickString = std::to_string(tick);
  resultBuilder.add("tick", VPackValue(tickString));
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

  bool overwrite = false;

  bool found;
  std::string const& value1 = _request->value("overwrite", found);

  if (found) {
    overwrite = StringUtils::boolean(value1);
  }

  bool force = false;
  std::string const& value3 = _request->value("force", found);

  if (found) {
    force = StringUtils::boolean(value3);
  }

  std::string const& value9 =
      _request->value("ignoreDistributeShardsLikeErrors", found);
  bool ignoreDistributeShardsLikeErrors =
      found ? StringUtils::boolean(value9) : false;

  uint64_t numberOfShards = 0;
  std::string const& value4 = _request->value("numberOfShards", found);

  if (found) {
    numberOfShards = StringUtils::uint64(value4);
  }

  uint64_t replicationFactor = 1;
  std::string const& value5 = _request->value("replicationFactor", found);

  if (found) {
    replicationFactor = StringUtils::uint64(value5);
  }

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

  bool found;
  bool force = false;
  std::string const& value = _request->value("force", found);

  if (found) {
    force = StringUtils::boolean(value);
  }

  std::string errorMsg;
  int res;
  if (ServerState::instance()->isCoordinator()) {
    res = processRestoreIndexesCoordinator(slice, force, errorMsg);
  } else {
    res = processRestoreIndexes(slice, force, errorMsg);
  }

  if (res != TRI_ERROR_NO_ERROR) {
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
    VPackSlice const& collection, bool dropExisting, bool force) {
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
  LogicalCollection* col = _vocbase->lookupCollection(name);

  // drop an existing collection if it exists
  if (col != nullptr) {
    if (dropExisting) {
      Result res = _vocbase->dropCollection(col, true, -1.0);

      if (res.errorNumber() == TRI_ERROR_FORBIDDEN) {
        // some collections must not be dropped

        // instead, truncate them
        auto ctx = transaction::StandaloneContext::Create(_vocbase);
        SingleCollectionTransaction trx(ctx, col->cid(),
                                        AccessMode::Type::EXCLUSIVE);
        // to turn off waitForSync!
        trx.addHint(transaction::Hints::Hint::RECOVERY);
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
    AuthenticationFeature *auth = AuthenticationFeature::INSTANCE;
    auth->authInfo()->updateUser(exe->user(), [&](AuthUserEntry& entry) {
      entry.grantCollection(_vocbase->name(), col->name(), AuthLevel::RW);
    });
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

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return Result();
  }

  std::string dbName = _vocbase->name();

  ClusterInfo* ci = ClusterInfo::instance();

  try {
    // in a cluster, we only look up by name:
    std::shared_ptr<LogicalCollection> col = ci->getCollection(dbName, name);

    // drop an existing collection if it exists
    if (dropExisting) {
      std::string errorMsg;
      int res = ci->dropCollectionCoordinator(dbName, col->cid_as_string(),
                                              errorMsg, 0.0);
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
    LOG_TOPIC(DEBUG, Logger::FIXME) << "processRestoreCollectionCoordinator "
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
  if (!replFactorSlice.isInteger()) {
    if (replicationFactor == 0) {
      replicationFactor = 1;
    }
    TRI_ASSERT(replicationFactor > 0);
    toMerge.add("replicationFactor", VPackValue(replicationFactor));
  }

  // always use current version number when restoring a collection,
  // because the collection is effectively NEW
  toMerge.add("version", VPackValue(LogicalCollection::VERSION_31));
  if (!name.empty() && name[0] == '_' && !parameters.hasKey("isSystem")) {
    // system collection?
    toMerge.add("isSystem", VPackValue(true));
  }
  toMerge.close();  // TopLevel

  VPackSlice const type = parameters.get("type");
  if (!type.isNumber()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "collection type not given or wrong");
  }

  TRI_col_type_e collectionType = static_cast<TRI_col_type_e>(type.getNumericValue<int>());

  VPackSlice const sliceToMerge = toMerge.slice();
  VPackBuilder mergedBuilder =
      VPackCollection::merge(parameters, sliceToMerge, false);
  VPackSlice const merged = mergedBuilder.slice();

  try {
    bool createWaitsForSyncReplication =
        application_features::ApplicationServer::getFeature<ClusterFeature>(
            "Cluster")
            ->createWaitsForSyncReplication();
    // in the replication case enforcing the replication factor is absolutely
    // not desired, so it is hardcoded to false
    auto col = ClusterMethods::createCollectionOnCoordinator(
        collectionType, _vocbase, merged, ignoreDistributeShardsLikeErrors,
        createWaitsForSyncReplication, false);
    TRI_ASSERT(col != nullptr);
    
    ExecContext const* exe = ExecContext::CURRENT;
    if (name[0] != '_' && exe != nullptr && !exe->isSuperuser()) {
      AuthenticationFeature *auth = AuthenticationFeature::INSTANCE;
      auth->authInfo()->updateUser(ExecContext::CURRENT->user(),
                     [&](AuthUserEntry& entry) {
                       entry.grantCollection(dbName, col->name(), AuthLevel::RW);
                     });
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
  grantTemporaryRights();
  
  if (colName == "_users") {
    // We need to handle the _users in a special way
    return processRestoreUsersBatch(colName);
  }
  auto ctx =
      transaction::StandaloneContext::Create(_vocbase);
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
        auto it = latest.find(key);
        if (it != latest.end()) {
          // Already found, overwrite:
          it->second = currentPos;
        } else {
          latest.emplace(std::make_pair(key, currentPos));
        }
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
    VPackSlice const typeSlice = marker.get("type");
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
      VPackSlice const doc = marker.get("data");
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

  arangodb::aql::Query query(false, _vocbase, arangodb::aql::QueryString(aql),
                             bindVars, nullptr, arangodb::aql::PART_MAIN);

  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
  TRI_ASSERT(queryRegistry != nullptr);

  auto queryResult = query.execute(queryRegistry);
  return Result{queryResult.code};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection
////////////////////////////////////////////////////////////////////////////////

Result RestReplicationHandler::processRestoreDataBatch(
    transaction::Methods& trx, std::string const& collectionName) {
  VPackBuilder builder;

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
      VPackSlice const typeSlice = marker.get("type");
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
    OperationResult opRes =
        trx.remove(collectionName, oldBuilder.slice(), options);
    if (opRes.fail()) {
      return opRes.result;
    }
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }

  // Now try to insert all keys for which the last marker was a document
  // marker, note that these could still be replace markers!
  builder.clear();
  if (ServerState::instance()->isCoordinator() && collectionName == "_users") {
    // Special-case for _users, we need to remove the _key and _id from the
    // marker
    VPackArrayBuilder guard(&builder);

    for (auto const& p : latest) {
      VPackSlice const marker = allMarkersSlice.at(p.second);
      VPackSlice const typeSlice = marker.get("type");
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
        VPackSlice const doc = marker.get("data");
        TRI_ASSERT(doc.isObject());
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
        builder.add(doc);
      }
    }
  } else {
    VPackArrayBuilder guard(&builder);

    for (auto const& p : latest) {
      VPackSlice const marker = allMarkersSlice.at(p.second);
      VPackSlice const typeSlice = marker.get("type");
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
        VPackSlice const doc = marker.get("data");
        TRI_ASSERT(doc.isObject());
        builder.add(doc);
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
    opRes = trx.insert(collectionName, requestSlice, options);
    if (opRes.fail()) {
      return opRes.result;
    }
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }

  // Now go through the individual results and check each error, if it was
  // TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED, then we have to call
  // replace on the document:
  VPackSlice resultSlice = opRes.slice();
  VPackBuilder replBuilder;  // documents for replace operation
  {
    VPackArrayBuilder guard(&oldBuilder);
    VPackArrayBuilder guard2(&replBuilder);
    VPackArrayIterator itRequest(requestSlice);
    VPackArrayIterator itResult(resultSlice);

    while (itRequest.valid()) {
      VPackSlice result = *itResult;
      VPackSlice error = result.get("error");
      if (error.isTrue()) {
        error = result.get("errorNum");
        if (error.isNumber()) {
          int code = error.getNumericValue<int>();
          if (code == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
            replBuilder.add(*itRequest);
          } else {
            return code;
          }
        } else {
          return Result(TRI_ERROR_INTERNAL);
        }
      }
      itRequest.next();
      itResult.next();
    }
  }
  try {
    OperationOptions options;
    options.silent = true;
    options.ignoreRevs = true;
    options.isRestore = true;
    options.waitForSync = false;
    opRes = trx.replace(collectionName, replBuilder.slice(), options);
    if (opRes.fail()) {
      return opRes.result;
    }
  } catch (arangodb::basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL);
  }

  return Result();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreIndexes(VPackSlice const& collection,
                                                  bool force,
                                                  std::string& errorMsg) {
  if (!collection.isObject()) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice const indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    errorMsg = "collection indexes declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackValueLength const n = indexes.length();

  if (n == 0) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  std::string const name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_NO_ERROR;

  grantTemporaryRights();
  READ_LOCKER(readLocker, _vocbase->_inventoryLock);
  
  // look up the collection
  try {
    CollectionGuard guard(_vocbase, name.c_str());

    LogicalCollection* collection = guard.collection();

    auto ctx = transaction::StandaloneContext::Create(_vocbase);
    SingleCollectionTransaction trx(ctx, collection->cid(),
                                    AccessMode::Type::EXCLUSIVE);
    Result res = trx.begin();

    if (!res.ok()) {
      errorMsg = "unable to start transaction: " + res.errorMessage();
      res.reset(res.errorNumber(), errorMsg);
      THROW_ARANGO_EXCEPTION(res);
    }

    auto physical = collection->getPhysical();
    TRI_ASSERT(physical != nullptr);
    for (VPackSlice const& idxDef : VPackArrayIterator(indexes)) {
      std::shared_ptr<arangodb::Index> idx;

      // {"id":"229907440927234","type":"hash","unique":false,"fields":["x","Y"]}

      res = physical->restoreIndex(&trx, idxDef, idx);

      if (res.errorNumber() == TRI_ERROR_NOT_IMPLEMENTED) {
        continue;
      }

      if (res.fail()) {
        errorMsg = "could not create index: " + res.errorMessage();
        res.reset(res.errorNumber(), errorMsg);
        break;
      }
      TRI_ASSERT(idx != nullptr);
    }

    if (res.fail()) {
      return res.errorNumber();
    }
    res = trx.commit();

  } catch (arangodb::basics::Exception const& ex) {
    // fix error handling
    errorMsg =
        "could not create index: " + std::string(TRI_errno_string(ex.code()));
  } catch (...) {
    errorMsg = "could not create index: unknown error";
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreIndexesCoordinator(
    VPackSlice const& collection, bool force, std::string& errorMsg) {
  if (!collection.isObject()) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }
  VPackSlice const parameters = collection.get("parameters");

  if (!parameters.isObject()) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  VPackSlice indexes = collection.get("indexes");

  if (!indexes.isArray()) {
    errorMsg = "collection indexes declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  size_t const n = static_cast<size_t>(indexes.length());

  if (n == 0) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  std::string name = arangodb::basics::VelocyPackHelper::getStringValue(
      parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (arangodb::basics::VelocyPackHelper::getBooleanValue(parameters, "deleted",
                                                          false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  std::string dbName = _vocbase->name();

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<LogicalCollection> col;
  try {
    col = ci->getCollection(dbName, name);
  } catch (...) {
    errorMsg = "could not find collection '" + name + "'";
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  TRI_ASSERT(col != nullptr);

  int res = TRI_ERROR_NO_ERROR;
  for (VPackSlice const& idxDef : VPackArrayIterator(indexes)) {
    VPackSlice type = idxDef.get("type");
    if (type.isString() &&
        (type.copyString() == "primary" || type.copyString() == "edge")) {
      // must ignore these types of indexes during restore
      continue;
    }

    VPackBuilder tmp;
    res = ci->ensureIndexCoordinator(dbName, col->cid_as_string(), idxDef, true,
                                     arangodb::Index::Compare, tmp, errorMsg,
                                     3600.0);
    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg =
          "could not create index: " + std::string(TRI_errno_string(res));
      break;
    }
  }

  return res;
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
  
  bool success;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(success);
  if (!success) {
    // error already created
    return;
  }

  VPackSlice const body = parsedBody->slice();
  std::string const endpoint =
      VelocyPackHelper::getStringValue(body, "endpoint", "");
  if (endpoint.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "<endpoint> must be a valid endpoint");
    return;
  }
  
  std::string dbname = isGlobal ? "" : _vocbase->name();
  auto config = ReplicationApplierConfiguration::fromVelocyPack(body, dbname);
  // will throw if invalid
  config.validate();
  
  double waitForSyncTimeout = VelocyPackHelper::getNumericValue(body, "waitForSyncTimeout", 5.0);

  // wait until all data in current logfile got synced
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);
  engine->waitForSyncTimeout(waitForSyncTimeout);

  TRI_ASSERT(!config._skipCreateDrop);
  std::unique_ptr<InitialSyncer> syncer;
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

  bool success;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(success);

  if (!success) {
    // error already created
    return;
  }
    
  std::string databaseName;
  if (!isGlobal) {
    databaseName = _vocbase->name();
  } 
  
  auto config = ReplicationApplierConfiguration::fromVelocyPack(applier->configuration(),
                                                                parsedBody->slice(), databaseName);
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

  applier->start(initialTick, useTick, barrierId);
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
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(success);
  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();
  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "body needs to be an object with attributes 'followerId' "
                  "and 'shard'");
    return;
  }
  VPackSlice const followerId = body.get("followerId");
  VPackSlice const readLockId = body.get("readLockId");
  VPackSlice const shard = body.get("shard");
  if (!followerId.isString() || !shard.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "'followerId' and 'shard' attributes must be strings");
    return;
  }

  auto col = _vocbase->lookupCollection(shard.copyString());

  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                  "did not find collection");
    return;
  }

  if (readLockId.isNone()) {
    // Short cut for the case that the collection is empty
    auto ctx = transaction::StandaloneContext::Create(_vocbase);
    SingleCollectionTransaction trx(ctx, col->cid(),
                                    AccessMode::Type::EXCLUSIVE);

    auto res = trx.begin();
    if (res.ok()) {
      auto countRes = trx.count(col->name(), false);
      if (countRes.ok()) {
        VPackSlice nrSlice = countRes.slice();
        uint64_t nr = nrSlice.getNumber<uint64_t>();
        if (nr == 0) {
          col->followers()->add(followerId.copyString());

          VPackBuilder b;
          {
            VPackObjectBuilder bb(&b);
            b.add("error", VPackValue(false));
          }

          generateResult(rest::ResponseCode::OK, b.slice());

          return;
        }  
      }
    }
    // If we get here, we have to report an error:
    generateError(rest::ResponseCode::FORBIDDEN,
                  TRI_ERROR_REPLICATION_SHARD_NONEMPTY,
                  "shard not empty");
    return;
  }

  VPackSlice const checksum = body.get("checksum");
  // optional while introducing this bugfix. should definitely be required with
  // 3.4
  // and throw a 400 then when no checksum is provided
  if (checksum.isString() && readLockId.isString()) {
    std::string referenceChecksum;
    {
      CONDITION_LOCKER(locker, _condVar);
      auto it = _holdReadLockJobs.find(readLockId.copyString());
      if (it == _holdReadLockJobs.end()) {
        // Entry has been removed since, so we cancel the whole thing
        // right away and generate an error:
        generateError(rest::ResponseCode::SERVER_ERROR,
                      TRI_ERROR_TRANSACTION_INTERNAL,
                      "read transaction was cancelled");
        return;
      }

      auto trx = it->second;
      if (!trx) {
        generateError(rest::ResponseCode::SERVER_ERROR,
                      TRI_ERROR_TRANSACTION_INTERNAL,
                      "Read lock not yet acquired!");
        return;
      }

      // referenceChecksum is the stringified number of documents in the
      // collection
      uint64_t num = col->numberDocuments(trx.get());
      referenceChecksum = std::to_string(num);
    }

    auto result = col->compareChecksums(checksum, referenceChecksum);

    if (result.fail()) {
      auto errorNumber = result.errorNumber();
      rest::ResponseCode code;
      if (errorNumber == TRI_ERROR_REPLICATION_WRONG_CHECKSUM ||
          errorNumber == TRI_ERROR_REPLICATION_WRONG_CHECKSUM_FORMAT) {
        code = rest::ResponseCode::BAD;
      } else {
        code = rest::ResponseCode::SERVER_ERROR;
      }
      generateError(code, errorNumber, result.errorMessage());
      return;
    }
  }

  col->followers()->add(followerId.copyString());

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a follower of a shard from the list of followers
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRemoveFollower() {
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(success);
  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();
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

  auto col = _vocbase->lookupCollection(shard.copyString());

  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                  "did not find collection");
    return;
  }
  col->followers()->remove(followerId.copyString());

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hold a read lock on a collection to stop writes temporarily
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandHoldReadLockCollection() {
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(success);
  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();
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
  std::string id = idSlice.copyString();

  auto col = _vocbase->lookupCollection(collection.copyString());

  if (col == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                  "did not find collection");
    return;
  }

  double ttl = 0.0;
  if (ttlSlice.isInteger()) {
    try {
      ttl = static_cast<double>(ttlSlice.getInt());
    } catch (...) {
    }
  } else {
    try {
      ttl = ttlSlice.getDouble();
    } catch (...) {
    }
  }

  if (col->getStatusLocked() != TRI_VOC_COL_STATUS_LOADED) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED,
                  "collection not loaded");
    return;
  }

  {
    CONDITION_LOCKER(locker, _condVar);
    _holdReadLockJobs.emplace(
        id, std::shared_ptr<SingleCollectionTransaction>(nullptr));
  }

  AccessMode::Type access = AccessMode::Type::READ;
  if (StringRef(EngineSelectorFeature::ENGINE->typeName()) == "rocksdb") {
    // we need to lock in EXCLUSIVE mode here, because simply locking
    // in READ mode will not stop other writers in RocksDB. In order
    // to stop other writers, we need to fetch the EXCLUSIVE lock
    access = AccessMode::Type::EXCLUSIVE;
  }

  auto ctx =
      transaction::StandaloneContext::Create(_vocbase);
  auto trx =
      std::make_shared<SingleCollectionTransaction>(ctx, col->cid(), access);
  trx->addHint(transaction::Hints::Hint::LOCK_ENTIRELY);
  Result res = trx->begin();
  if (!res.ok()) {
    generateError(rest::ResponseCode::SERVER_ERROR,
                  TRI_ERROR_TRANSACTION_INTERNAL,
                  "cannot begin read transaction");
    return;
  }

  {
    CONDITION_LOCKER(locker, _condVar);
    auto it = _holdReadLockJobs.find(id);
    if (it == _holdReadLockJobs.end()) {
      // Entry has been removed since, so we cancel the whole thing
      // right away and generate an error:
      generateError(rest::ResponseCode::SERVER_ERROR,
                    TRI_ERROR_TRANSACTION_INTERNAL,
                    "read transaction was cancelled");
      return;
    }
    it->second = trx;  // mark the read lock as acquired
  }

  double now = TRI_microtime();
  double startTime = now;
  double endTime = startTime + ttl;
  bool stopping = false;

  {
    CONDITION_LOCKER(locker, _condVar);
    while (now < endTime) {
      _condVar.wait(100000);
      auto it = _holdReadLockJobs.find(id);
      if (it == _holdReadLockJobs.end()) {
        break;
      }
      if (application_features::ApplicationServer::isStopping()) {
        stopping = true;
        break;
      }
      now = TRI_microtime();
    }
    auto it = _holdReadLockJobs.find(id);
    if (it != _holdReadLockJobs.end()) {
      _holdReadLockJobs.erase(it);
    }
  }

  if (stopping) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_SHUTTING_DOWN);
    return;
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check the holding of a read lock on a collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandCheckHoldReadLockCollection() {
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(success);
  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();
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
  std::string id = idSlice.copyString();

  bool lockHeld = false;

  {
    CONDITION_LOCKER(locker, _condVar);
    auto it = _holdReadLockJobs.find(id);
    if (it == _holdReadLockJobs.end()) {
      generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                    "no hold read lock job found for 'id'");
      return;
    }
    if (it->second) {
      lockHeld = true;
    }
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
    b.add("lockHeld", VPackValue(lockHeld));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the holding of a read lock on a collection
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandCancelHoldReadLockCollection() {
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(success);
  if (!success) {
    // error already created
    return;
  }
  VPackSlice const body = parsedBody->slice();
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
  std::string id = idSlice.copyString();

  bool lockHeld = false;
  {
    CONDITION_LOCKER(locker, _condVar);
    auto it = _holdReadLockJobs.find(id);
    if (it != _holdReadLockJobs.end()) {
      // Note that this approach works if the lock has been acquired
      // as well as if we still wait for the read lock, in which case
      // it will eventually be acquired but immediately released:
      if (it->second) {
        lockHeld = true;
      }
      _holdReadLockJobs.erase(it);
      _condVar.broadcast();
    }
  }

  VPackBuilder b;
  {
    VPackObjectBuilder bb(&b);
    b.add("error", VPackValue(false));
    b.add("lockHeld", VPackValue(lockHeld));
  }

  generateResult(rest::ResponseCode::OK, b.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get ID for a read lock job
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandGetIdForReadLockCollection() {
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
  
  engine->waitForSyncTimeout(10.0); // only for mmfiles
  
  VPackBuilder builder;
  auto res = engine->createLoggerState(_vocbase, builder);
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

  arangodb::LogicalCollection* col = nullptr;
  if (!uuid.empty()) {
    col = _vocbase->lookupCollectionByUuid(uuid);
  }
  if (col != nullptr) {
    col = _vocbase->lookupCollection(name);
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
    patch.add("objectId", VPackSlice::nullSlice());
    patch.add("cid", VPackSlice::nullSlice());
    patch.add("id", VPackSlice::nullSlice());
  }
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);
  engine->addParametersForNewCollection(patch, slice);
  patch.close();

  VPackBuilder builder = VPackCollection::merge(slice, patch.slice(),
                        /*mergeValues*/true, /*nullMeansRemove*/true);
  slice = builder.slice();

  col = _vocbase->createCollection(slice);

  if (col == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  /* Temporary ASSERTS to prove correctness of new constructor */
  TRI_ASSERT(col->isSystem() == (name[0] == '_'));
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
    planId = col->cid();
  }

  TRI_ASSERT(col->planId() == planId);
#endif

  if (dst != nullptr) {
    *dst = col;
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
    if (ExecContext::CURRENT->canUseDatabase(_vocbase->name(), AuthLevel::RW) ) {
      // If you have administrative access on this database,
      // we grant you everything for restore.
      ExecContext::CURRENT = nullptr;
    }
  }
}

ReplicationApplier* RestReplicationHandler::getApplier(bool& global) {
  global = false;
  bool found = false;
  std::string const& value = _request->value("global", found);
  if (found) {
    global = StringUtils::boolean(value);
  }
  
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
    return _vocbase->replicationApplier();
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief condition locker to wake up holdReadLockCollection jobs
//////////////////////////////////////////////////////////////////////////////

arangodb::basics::ConditionVariable RestReplicationHandler::_condVar;

//////////////////////////////////////////////////////////////////////////////
/// @brief global table of flags to cancel holdReadLockCollection jobs, if
/// the flag is set of the ID of a job, the job is cancelled
//////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, std::shared_ptr<SingleCollectionTransaction>>
    RestReplicationHandler::_holdReadLockJobs;
