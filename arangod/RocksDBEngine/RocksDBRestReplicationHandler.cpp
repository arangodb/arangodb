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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/RocksDBRestReplicationHandler.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/FollowerInfo.h"
#include "GeneralServer/GeneralServer.h"
#include "Indexes/Index.h"
#include "Logger/Logger.h"
#include "Replication/InitialSyncer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Context.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionKeysRepository.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/PhysicalCollection.h"
#include "VocBase/replication-applier.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::rocksutils;

RocksDBRestReplicationHandler::RocksDBRestReplicationHandler(
    GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response),
      _manager(globalRocksEngine()->replicationManager()) {}

RocksDBRestReplicationHandler::~RocksDBRestReplicationHandler() {}

RestStatus RocksDBRestReplicationHandler::execute() {
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
      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandBatch();
      }
    } else if (command == "inventory") {
      if (type != rest::RequestType::GET) {
        goto BAD_CALL;
      }
      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      } else {
        handleCommandInventory();
      }
    } else if (command == "keys") {
      if (type != rest::RequestType::GET && type != rest::RequestType::POST &&
          type != rest::RequestType::PUT &&
          type != rest::RequestType::DELETE_REQ) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return RestStatus::DONE;
      }

      if (type == rest::RequestType::POST) {
        handleCommandCreateKeys();
      } else if (type == rest::RequestType::GET) {
        handleCommandGetKeys();
      } else if (type == rest::RequestType::PUT) {
        handleCommandFetchKeys();
      } else if (type == rest::RequestType::DELETE_REQ) {
        handleCommandRemoveKeys();
      }
    } else if (command == "dump") {
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
                    "invalid command");
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
/// @brief creates an error if called on a coordinator server
////////////////////////////////////////////////////////////////////////////////

bool RocksDBRestReplicationHandler::isCoordinatorError() {
  if (_vocbase->type() == TRI_VOCBASE_TYPE_COORDINATOR) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                  TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "replication API is not supported on a coordinator");
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_return_state
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandLoggerState() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_delete_batch_replication
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandBatch() {
  // extract the request type
  auto const type = _request->requestType();
  auto const& suffixes = _request->suffixes();
  size_t const len = suffixes.size();
  
  TRI_ASSERT(len >= 1);
  
  if (type == rest::RequestType::POST) {
    // create a new blocker
    std::shared_ptr<VPackBuilder> input = _request->toVelocyPackBuilderPtr();
    
    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }
    
    // extract ttl
    //double expires =
    //VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);
    
    //TRI_voc_tick_t id;
    //StorageEngine* engine = EngineSelectorFeature::ENGINE;
    //int res = engine->insertCompactionBlocker(_vocbase, expires, id);
    
    RocksDBReplicationContext *ctx = _manager->createContext();
    if (ctx == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_FAILED);
    }
    
    VPackBuilder b;
    b.add(VPackValue(VPackValueType::Object));
    b.add("id", VPackValue(std::to_string(ctx->id())));
    b.close();
    generateResult(rest::ResponseCode::OK, b.slice());
    return;
  }
  
  if (type == rest::RequestType::PUT && len >= 2) {
    // extend an existing blocker
    TRI_voc_tick_t id =
    static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffixes[1]));
    
    auto input = _request->toVelocyPackBuilderPtr();
    
    if (input == nullptr || !input->slice().isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }
    
    // extract ttl
    double expires =
    VelocyPackHelper::getNumericValue<double>(input->slice(), "ttl", 0);
   
    int res = TRI_ERROR_NO_ERROR;
    bool busy;
    RocksDBReplicationContext *ctx = _manager->find(id, busy, expires);
    if (busy) {
      res = TRI_ERROR_CURSOR_BUSY;
    } else if (ctx == nullptr) {
      res = TRI_ERROR_CURSOR_NOT_FOUND;
    } else {
      _manager->release(ctx);
    }
    
    // now extend the blocker
    //StorageEngine* engine = EngineSelectorFeature::ENGINE;
    // res = engine->extendCompactionBlocker(_vocbase, id, expires);
    
    if (res == TRI_ERROR_NO_ERROR) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }
  
  if (type == rest::RequestType::DELETE_REQ && len >= 2) {
    // delete an existing blocker
    TRI_voc_tick_t id =
    static_cast<TRI_voc_tick_t>(StringUtils::uint64(suffixes[1]));
    
    bool found = _manager->remove(id);
    //StorageEngine* engine = EngineSelectorFeature::ENGINE;
    //int res = engine->removeCompactionBlocker(_vocbase, id);
    
    if (found) {
      resetResponse(rest::ResponseCode::NO_CONTENT);
    } else {
      int res = TRI_ERROR_CURSOR_NOT_FOUND;
      generateError(GeneralResponse::responseCode(res), res);
    }
    return;
  }
  
  // we get here if anything above is invalid
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief forward a command in the coordinator case
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleTrampolineCoordinator() {
  bool useVpp = false;
  if (_request->transportType() == Endpoint::TransportType::VPP) {
    useVpp = true;
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
  if (!useVpp) {
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
    // here we switch vorm vst to http?!
    // i am not allowed to change cluster comm!
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

  if (!useVpp) {
    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());
    if (_response == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid response type");
    }
    httpResponse->body().swap(&(res->result->getBody()));
  } else {
    // TODO copy all payloads
    VPackSlice slice = res->result->getBodyVelocyPack()->slice();
    _response->setPayload(slice, true);  // do we need to generate the body?!
  }

  auto const& resultHeaders = res->result->getHeaderFields();
  for (auto const& it : resultHeaders) {
    _response->setHeader(it.first, it.second);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_logger_returns
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandLoggerFollow() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run the command that determines which transactions were open at
/// a given tick value
/// this is an internal method use by ArangoDB's replication that should not
/// be called by client drivers directly
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandDetermineOpenTransactions() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_inventory
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandInventory() {
  RocksDBReplicationContext *ctx = nullptr;
  bool found, busy;
  std::string batchId = _request->value("batchId", found);
  if (found) {
    ctx = _manager->find(StringUtils::uint64(batchId), busy);
  }
  if (!found || busy || ctx == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_CURSOR_NOT_FOUND,
                  "batchId not specified");
  }

  TRI_voc_tick_t tick = TRI_CurrentTickServer();
  
  // include system collections?
  bool includeSystem = true;
  std::string const& value = _request->value("includeSystem", found);
  if (found) {
    includeSystem = StringUtils::boolean(value);
  }
  
  std::pair<RocksDBReplicationResult, std::shared_ptr<VPackBuilder>> result =
    ctx->getInventory(this->_vocbase, includeSystem);
  if (!result.first.ok()) {
    generateError(rest::ResponseCode::BAD,
                  result.first.errorNumber(),
                  "inventory could not be created");
  }

  VPackSlice const collections = result.second->slice();
  TRI_ASSERT(collections.isArray());
  
  VPackBuilder builder;
  builder.openObject();

  // add collections data
  builder.add("collections", collections);
  
  // "state"
  builder.add("state", VPackValue(VPackValueType::Object));
  
  //MMFilesLogfileManagerState const s =
  //MMFilesLogfileManager::instance()->state();
  
  builder.add("running", VPackValue(true));
  builder.add("lastLogTick", VPackValue(std::to_string(ctx->lastTick())));
  builder.add("lastUncommittedLogTick",
              VPackValue(std::to_string(0)));//s.lastAssignedTick
  builder.add("totalEvents", VPackValue(0));// s.numEvents + s.numEventsSync
  builder.add("time", VPackValue(utilities::timeString()));
  builder.close();  // state
  
  std::string const tickString(std::to_string(tick));
  builder.add("tick", VPackValue(tickString));
  builder.close();  // Toplevel
  
  _manager->release(ctx);
  generateResult(rest::ResponseCode::OK, builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_cluster_inventory
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandClusterInventory() {
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
    c->toVelocyPackForClusterInventory(resultBuilder, includeSystem);
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

void RocksDBRestReplicationHandler::handleCommandRestoreCollection() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandRestoreIndexes() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

void RocksDBRestReplicationHandler::handleCommandRestoreData() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief produce list of keys for a specific collection
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandCreateKeys() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all key ranges
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandGetKeys() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns date for a key range
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandFetchKeys() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

void RocksDBRestReplicationHandler::handleCommandRemoveKeys() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_dump
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandDump() {
  
  bool found = false;
  uint64_t contextId = 0;

  // get collection Name
  std::string const& collection = _request->value("collection");
  if (collection.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  // get contextId
  std::string const& contextIdString = _request->value("batchId", found);
  if (found) {
    contextId = StringUtils::uint64(contextIdString);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "replication dump - request misses batchId");
  }

  // acquire context
  bool isBusy = false;
  RocksDBReplicationContext* context = _manager->find(contextId, isBusy);
  if (context == nullptr || isBusy){
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "replication dump - unable to acquire context");
  }


  // print request
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
      << "requested collection dump for collection '" << collection
      << "' using contextId '" << context->id() << "'";


  // TODO needs to generalized || velocypacks needs to support multiple slices per response!
  auto response = dynamic_cast<HttpResponse*>(_response.get());
  StringBuffer& dump = response->body();

  if (response == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid response type");
  }

  context->dump(_vocbase, collection, dump, 1000);

  // generate the result
  if (dump.length() == 0) {
    resetResponse(rest::ResponseCode::NO_CONTENT);
    response->body().reset();
  } else {
    resetResponse(rest::ResponseCode::OK);
    response->setContentType(rest::ContentType::DUMP);
     // set headers
     _response->setHeaderNC(TRI_REPLICATION_HEADER_CHECKMORE,
                            (context->more() ? "true" : "false"));

     //_response->setHeaderNC(TRI_REPLICATION_HEADER_LASTINCLUDED,
     //                       StringUtils::itoa(dump._lastFoundTick));
  }

  _manager->release(context); //release context when done
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_makeSlave
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandMakeSlave() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_synchronize
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandSync() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_serverID
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandServerId() {
  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  std::string const serverId = StringUtils::itoa(ServerIdFeature::getId());
  result.add("serverId", VPackValue(serverId));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierGetConfig() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_adjust
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierSetConfig() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_start
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierStart() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_put_api_replication_applier_stop
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierStop() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_replication_applier_state
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierGetState() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete the state of the replication applier
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandApplierDeleteState() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a follower of a shard to the list of followers
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandAddFollower() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a follower of a shard from the list of followers
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandRemoveFollower() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hold a read lock on a collection to stop writes temporarily
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandHoldReadLockCollection() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check the holding of a read lock on a collection
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandCheckHoldReadLockCollection() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel the holding of a read lock on a collection
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::
    handleCommandCancelHoldReadLockCollection() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get ID for a read lock job
////////////////////////////////////////////////////////////////////////////////

void RocksDBRestReplicationHandler::handleCommandGetIdForReadLockCollection() {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_NOT_YET_IMPLEMENTED,
                "replication API is not fully implemented for RocksDB yet");
}
