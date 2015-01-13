////////////////////////////////////////////////////////////////////////////////
/// @brief replication request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestReplicationHandler.h"

#include "Basics/JsonHelper.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/logging.h"
#include "HttpServer/HttpServer.h"
#include "Replication/InitialSyncer.h"
#include "Rest/HttpRequest.h"
#include "Utils/CollectionGuard.h"
#include "Utils/transactions.h"
#include "VocBase/compactor.h"
#include "VocBase/replication-applier.h"
#include "VocBase/replication-dump.h"
#include "VocBase/server.h"
#include "VocBase/update-policy.h"
#include "VocBase/index.h"
#include "Wal/LogfileManager.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ClusterComm.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                       initialise static variables
// -----------------------------------------------------------------------------

const uint64_t RestReplicationHandler::defaultChunkSize = 128 * 1024;

const uint64_t RestReplicationHandler::maxChunkSize = 128 * 1024 * 1024;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestReplicationHandler::RestReplicationHandler (HttpRequest* request)
  : RestVocbaseBaseHandler(request) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestReplicationHandler::~RestReplicationHandler () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Handler::status_t RestReplicationHandler::execute () {
  // extract the request type
  const HttpRequest::HttpRequestType type = _request->requestType();

  vector<string> const& suffix = _request->suffix();

  const size_t len = suffix.size();

  if (len >= 1) {
    const string& command = suffix[0];

    if (command == "logger-state") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandLoggerState();
    }
    else if (command == "logger-follow") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandLoggerFollow();
    }
    else if (command == "batch") {

      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      }
      else {
        handleCommandBatch();
      }
    }
    else if (command == "inventory") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      }
      else {
        handleCommandInventory();
      }
    }
    else if (command == "dump") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }

      if (ServerState::instance()->isCoordinator()) {
        handleTrampolineCoordinator();
      }
      else {
        handleCommandDump();
      }
    }
    else if (command == "restore-collection") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreCollection();
    }
    else if (command == "restore-indexes") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      handleCommandRestoreIndexes();
    }
    else if (command == "restore-data") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      if (ServerState::instance()->isCoordinator()) {
        handleCommandRestoreDataCoordinator();
      }
      else {
        handleCommandRestoreData();
      }
    }
    else if (command == "sync") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(Handler::HANDLER_DONE);
      }

      handleCommandSync();
    }
    else if (command == "server-id") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandServerId();
    }
    else if (command == "applier-config") {
      if (type == HttpRequest::HTTP_REQUEST_GET) {
        handleCommandApplierGetConfig();
      }
      else {
        if (type != HttpRequest::HTTP_REQUEST_PUT) {
          goto BAD_CALL;
        }
        handleCommandApplierSetConfig();
      }
    }
    else if (command == "applier-start") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(Handler::HANDLER_DONE);
      }

      handleCommandApplierStart();
    }
    else if (command == "applier-stop") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }

      if (isCoordinatorError()) {
        return status_t(Handler::HANDLER_DONE);
      }

      handleCommandApplierStop();
    }
    else if (command == "applier-state") {
      if (type == HttpRequest::HTTP_REQUEST_DELETE) {
        handleCommandApplierDeleteState();
      }
      else {
        if (type != HttpRequest::HTTP_REQUEST_GET) {
          goto BAD_CALL;
        }
        handleCommandApplierGetState();
      }
    }
    else if (command == "clusterInventory") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      if (! ServerState::instance()->isCoordinator()) {
        generateError(HttpResponse::FORBIDDEN,
                      TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
      }
      else {
        handleCommandClusterInventory();
      }
    }
    else {
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid command");
    }

    return status_t(Handler::HANDLER_DONE);
  }

BAD_CALL:
  if (len != 1) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "expecting URL /_api/replication/<command>");
  }
  else {
    generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }

  return status_t(Handler::HANDLER_DONE);
}

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator to sort collections
/// sort order is by collection type first (vertices before edges, this is
/// because edges depend on vertices being there), then name
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::sortCollections (const void* l,
                                             const void* r) {
  TRI_json_t const* left  = JsonHelper::getObjectElement(static_cast<TRI_json_t const*>(l), "parameters");
  TRI_json_t const* right = JsonHelper::getObjectElement(static_cast<TRI_json_t const*>(r), "parameters");

  int leftType  = JsonHelper::getNumericValue<int>(left,  "type", (int) TRI_COL_TYPE_DOCUMENT);
  int rightType = JsonHelper::getNumericValue<int>(right, "type", (int) TRI_COL_TYPE_DOCUMENT);

  if (leftType != rightType) {
    return leftType - rightType;
  }

  string leftName  = JsonHelper::getStringValue(left,  "name", "");
  string rightName = JsonHelper::getStringValue(right, "name", "");

  return strcasecmp(leftName.c_str(), rightName.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief filter a collection based on collection attributes
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::filterCollection (TRI_vocbase_col_t* collection,
                                               void* data) {
  if (collection->_type != (TRI_col_type_t) TRI_COL_TYPE_DOCUMENT &&
      collection->_type != (TRI_col_type_t) TRI_COL_TYPE_EDGE) {
    // invalid type
    return false;
  }

  bool includeSystem = *((bool*) data);

  if (! includeSystem && collection->_name[0] == '_') {
    // exclude all system collections
    return false;
  }

  if (TRI_ExcludeCollectionReplication(collection->_name, includeSystem)) {
    // collection is excluded from replication
    return false;
  }

  // all other cases should be included
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an error if called on a coordinator server
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::isCoordinatorError () {
  if (_vocbase->_type == TRI_VOCBASE_TYPE_COORDINATOR) {
    generateError(HttpResponse::NOT_IMPLEMENTED,
                  TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "replication API is not supported on a coordinator");
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert the applier action into an action list
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::insertClient (TRI_voc_tick_t lastServedTick) {
  bool found;
  char const* value;

  value = _request->value("serverId", found);

  if (found) {
    TRI_server_id_t serverId = (TRI_server_id_t) StringUtils::uint64(value);

    if (serverId > 0) {
      // TODO: FIXME!!
//      TRI_UpdateClientReplicationLogger(_vocbase->_replicationLogger, serverId, lastServedTick);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the chunk size
////////////////////////////////////////////////////////////////////////////////

uint64_t RestReplicationHandler::determineChunkSize () const {
  // determine chunk size
  uint64_t chunkSize = defaultChunkSize;

  bool found;
  const char* value = _request->value("chunkSize", found);

  if (found) {
    // url parameter "chunkSize" was specified
    chunkSize = StringUtils::uint64(value);

    // don't allow overly big allocations
    if (chunkSize > maxChunkSize) {
      chunkSize = maxChunkSize;
    }
  }

  return chunkSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_replication_logger_return_state
/// @brief returns the state of the replication logger
///
/// @RESTHEADER{GET /_api/replication/logger-state, Return replication logger state}
///
/// @RESTDESCRIPTION
/// Returns the current state of the server's replication logger. The state will
/// include information about whether the logger is running and about the last
/// logged tick value. This tick value is important for incremental fetching of
/// data.
///
/// The state API can be called regardless of whether the logger is currently
/// running or not.
///
/// The body of the response contains a JSON object with the following
/// attributes:
///
/// - *state*: the current logger state as a JSON object with the following
///   sub-attributes:
///
///   - *running*: whether or not the logger is running
///
///   - *lastLogTick*: the tick value of the latest tick the logger has logged.
///     This value can be used for incremental fetching of log data.
///
///   - *totalEvents*: total number of events logged since the server was started.
///     The value is not reset between multiple stops and re-starts of the logger.
///
///   - *time*: the current date and time on the logger server
///
/// - *server*: a JSON object with the following sub-attributes:
///
///   - *version*: the logger server's version
///
///   - *serverId*: the logger server's id
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the logger state could be determined successfully.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if the logger state could not be determined.
///
/// @EXAMPLES
///
/// Returns the state of the replication logger.
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerStateActive}
///     var re = require("org/arangodb/replication");
///     re.logger.start();
///
///     var url = "/_api/replication/logger-state";
///
///     var response = logCurlRequest('GET', url);
///     re.logger.stop();
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerState () {
  TRI_json_t* json = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);

  if (json == nullptr) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  // "state" part 
  TRI_json_t* state = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);

  if (state == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
  
  triagens::wal::LogfileManagerState const&& s = triagens::wal::LogfileManager::instance()->state();
  std::string const lastTickString(StringUtils::itoa(s.lastTick));

  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, state, "running", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, true));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, state, "lastLogTick", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, lastTickString.c_str(), lastTickString.size()));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, state, "totalEvents", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (double) s.numEvents));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, state, "time", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, s.timeString.c_str(), s.timeString.size()));
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "state", state);

  // "server" part
  TRI_json_t* server = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);

  if (server == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, server, "version", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, TRI_VERSION, strlen(TRI_VERSION)));
  char* serverIdString = TRI_StringUInt64(TRI_GetIdServer());
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, server, "serverId", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, serverIdString, strlen(serverIdString)));
  TRI_FreeString(TRI_CORE_MEM_ZONE, serverIdString);
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "server", server);
 
  // clients
  TRI_json_t* clients = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  if (clients != nullptr) {
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "clients", clients);
  }

  generateResult(json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle a dump batch command
///
/// @RESTHEADER{POST /_api/replication/batch, Create new dump batch}
///
/// @RESTBODYPARAM{body,json,required}
/// A JSON object with the batch configuration.
///
/// @RESTDESCRIPTION
/// Creates a new dump batch and returns the batch's id.
///
/// The body of the request must be a JSON object with the following attributes:
///
/// - *ttl*: the time-to-live for the new batch (in seconds)
///
/// The response is a JSON object with the following attributes:
///
/// - *id*: the id of the batch
///
/// **Note**: on a coordinator, this request must have the URL parameter
/// *DBserver* which must be an ID of a DBserver.
/// The very same request is forwarded synchronously to that DBserver.
/// It is an error if this attribute is not bound in the coordinator case.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{204}
/// is returned if the batch was created successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the ttl value is invalid or if *DBserver* attribute
/// is not specified or illegal on a coordinator.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief handle a dump batch command
///
/// @RESTHEADER{PUT /_api/replication/batch/{id}, Prolong existing dump batch}
///
/// @RESTBODYPARAM{body,json,required}
/// A JSON object with the batch configration.
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{id,string,required}
/// The id of the batch.
///
/// @RESTDESCRIPTION
/// Extends the ttl of an existing dump batch, using the batch's id and
/// the provided ttl value.
///
/// The body of the request must be a JSON object with the following attributes:
///
/// - *ttl*: the time-to-live for the batch (in seconds)
///
/// If the batch's ttl can be extended successully, the response is empty.
///
/// **Note**: on a coordinator, this request must have the URL parameter
/// *DBserver* which must be an ID of a DBserver.
/// The very same request is forwarded synchronously to that DBserver.
/// It is an error if this attribute is not bound in the coordinator case.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{204}
/// is returned if the batch's ttl was extended successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the ttl value is invalid or the batch was not found.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief handle a dump batch command
///
/// @RESTHEADER{DELETE /_api/replication/batch/{id}, Deletes an existing dump batch}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{id,string,required}
/// The id of the batch.
///
/// @RESTDESCRIPTION
/// Deletes the existing dump batch, allowing compaction and cleanup to resume.
///
/// **Note**: on a coordinator, this request must have the URL parameter
/// *DBserver* which must be an ID of a DBserver.
/// The very same request is forwarded synchronously to that DBserver.
/// It is an error if this attribute is not bound in the coordinator case.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{204}
/// is returned if the batch was deleted successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the batch was not found.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandBatch () {
  // extract the request type
  const HttpRequest::HttpRequestType type = _request->requestType();
  vector<string> const& suffix = _request->suffix();
  const size_t len = suffix.size();

  TRI_ASSERT(len >= 1);

  if (type == HttpRequest::HTTP_REQUEST_POST) {
    // create a new blocker

    TRI_json_t* input = _request->toJson(0);

    if (input == nullptr) {
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double expires = JsonHelper::getNumericValue<double>(input, "ttl", 0);

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, input);

    TRI_voc_tick_t id;
    int res = TRI_InsertBlockerCompactorVocBase(_vocbase, expires, &id);

    if (res != TRI_ERROR_NO_ERROR) {
      generateError(HttpResponse::BAD, res);
      return;
    }

    TRI_json_t json;
    TRI_InitObjectJson(TRI_CORE_MEM_ZONE, &json);
    char* idString = TRI_StringUInt64((uint64_t) id);
    TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &json, "id", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, idString, strlen(idString)));

    generateResult(&json);
    TRI_DestroyJson(TRI_CORE_MEM_ZONE, &json);
    return;
  }

  if (type == HttpRequest::HTTP_REQUEST_PUT && len >= 2) {
    // extend an existing blocker
    TRI_voc_tick_t id = (TRI_voc_tick_t) StringUtils::uint64(suffix[1]);

    TRI_json_t* input = _request->toJson(0);

    if (input == nullptr) {
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid JSON");
      return;
    }

    // extract ttl
    double expires = JsonHelper::getNumericValue<double>(input, "ttl", 0);

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, input);

    // now extend the blocker
    int res = TRI_TouchBlockerCompactorVocBase(_vocbase, id, expires);

    if (res == TRI_ERROR_NO_ERROR) {
      _response = createResponse(HttpResponse::NO_CONTENT);
    }
    else {
      generateError(HttpResponse::BAD, res);
    }
    return;
  }

  if (type == HttpRequest::HTTP_REQUEST_DELETE && len >= 2) {
    // delete an existing blocker
    TRI_voc_tick_t id = (TRI_voc_tick_t) StringUtils::uint64(suffix[1]);

    int res = TRI_RemoveBlockerCompactorVocBase(_vocbase, id);

    if (res == TRI_ERROR_NO_ERROR) {
      _response = createResponse(HttpResponse::NO_CONTENT);
    }
    else {
      generateError(HttpResponse::BAD, res);
    }
    return;
  }

  // we get here if anything above is invalid
  generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief forward a command in the coordinator case
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleTrampolineCoordinator () {

  // First check the DBserver component of the body json:
  ServerID DBserver = _request->value("DBserver");
  if (DBserver.empty()) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "need \"DBserver\" parameter");
    return;
  }

  string const& dbname = _request->databaseName();

  map<string, string> headers = triagens::arango::getForwardableRequestHeaders(_request);
  map<string, string> values = _request->values();
  string params;
  map<string, string>::iterator i;
  for (i = values.begin(); i != values.end(); ++i) {
    if (i->first != "DBserver") {
      if (params.empty()) {
        params.push_back('?');
      }
      else {
        params.push_back('&');
      }
      params.append(StringUtils::urlEncode(i->first));
      params.push_back('=');
      params.append(StringUtils::urlEncode(i->second));
    }
  }

  // Set a few variables needed for our work:
  ClusterComm* cc = ClusterComm::instance();

  // Send a synchronous request to that shard using ClusterComm:
  ClusterCommResult* res;
  res = cc->syncRequest("", TRI_NewTickServer(), "server:" + DBserver,
                        _request->requestType(),
                        "/_db/" + StringUtils::urlEncode(dbname) +
                        _request->requestPath() + params,
                        string(_request->body(),_request->bodySize()),
                        headers, 300.0);

  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    delete res;
    generateError(HttpResponse::BAD, TRI_ERROR_CLUSTER_TIMEOUT,
                  "timeout within cluster");
    return;
  }
  if (res->status == CL_COMM_ERROR) {
    // This could be a broken connection or an Http error:
    if (res->result == nullptr || !res->result->isComplete()) {
      // there is no result
      delete res;
      generateError(HttpResponse::BAD, TRI_ERROR_CLUSTER_CONNECTION_LOST,
                    "lost connection within cluster");
      return;
    }
    // In this case a proper HTTP error was reported by the DBserver,
    // we simply forward the result.
    // We intentionally fall through here.
  }

  bool dummy;
  _response = createResponse(static_cast<HttpResponse::HttpResponseCode>
                             (res->result->getHttpReturnCode()));
  _response->setContentType(res->result->getHeaderField("content-type",dummy));
  _response->body().swap(& (res->result->getBody()));
  map<string, string> resultHeaders = res->result->getHeaderFields();
  map<string, string>::iterator it;
  for (it = resultHeaders.begin(); it != resultHeaders.end(); ++it) {
    _response->setHeader(it->first, it->second);
  }
  delete res;
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_replication_logger_returns
/// @RESTHEADER{GET /_api/replication/logger-follow, Returns log entries}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{from,number,optional}
/// Lower bound tick value for results.
///
/// @RESTQUERYPARAM{to,number,optional}
/// Upper bound tick value for results.
///
/// @RESTQUERYPARAM{chunkSize,number,optional}
/// Approximate maximum size of the returned result.
///
/// @RESTQUERYPARAM{includeSystem,boolean,optional}
/// Include system collections in the result. The default value is *true*.
///
/// @RESTDESCRIPTION
/// Returns data from the server's replication log. This method can be called
/// by replication clients after an initial synchronization of data. The method
/// will return all "recent" log entries from the logger server, and the clients
/// can replay and apply these entries locally so they get to the same data
/// state as the logger server.
///
/// Clients can call this method repeatedly to incrementally fetch all changes
/// from the logger server. In this case, they should provide the *from* value so
/// they will only get returned the log events since their last fetch.
///
/// When the *from* URL parameter is not used, the logger server will return log
/// entries starting at the beginning of its replication log. When the *from*
/// parameter is used, the logger server will only return log entries which have
/// higher tick values than the specified *from* value (note: the log entry with a
/// tick value equal to *from* will be excluded). Use the *from* value when
/// incrementally fetching log data.
///
/// The *to* URL parameter can be used to optionally restrict the upper bound of
/// the result to a certain tick value. If used, the result will contain only log events
/// with tick values up to (including) *to*. In incremental fetching, there is no
/// need to use the *to* parameter. It only makes sense in special situations,
/// when only parts of the change log are required.
///
/// The *chunkSize* URL parameter can be used to control the size of the result.
/// It must be specified in bytes. The *chunkSize* value will only be honored
/// approximately. Otherwise a too low *chunkSize* value could cause the server
/// to not be able to put just one log entry into the result and return it.
/// Therefore, the *chunkSize* value will only be consulted after a log entry has
/// been written into the result. If the result size is then bigger than
/// *chunkSize*, the server will respond with as many log entries as there are
/// in the response already. If the result size is still smaller than *chunkSize*,
/// the server will try to return more data if there's more data left to return.
///
/// If *chunkSize* is not specified, some server-side default value will be used.
///
/// The *Content-Type* of the result is *application/x-arango-dump*. This is an
/// easy-to-process format, with all log events going onto separate lines in the
/// response body. Each log event itself is a JSON object, with at least the
/// following attributes:
///
/// - *tick*: the log event tick value
///
/// - *type*: the log event type
///
/// Individual log events will also have additional attributes, depending on the
/// event type. A few common attributes which are used for multiple events types
/// are:
///
/// - *cid*: id of the collection the event was for
///
/// - *tid*: id of the transaction the event was contained in
///
/// - *key*: document key
///
/// - *rev*: document revision id
///
/// - *data*: the original document data
///
/// A more detailed description of the individual replication event types and their
/// data structures can be found in @ref RefManualReplicationEventTypes.
///
/// The response will also contain the following HTTP headers:
///
/// - *x-arango-replication-active*: whether or not the logger is active. Clients
///   can use this flag as an indication for their polling frequency. If the
///   logger is not active and there are no more replication events available, it
///   might be sensible for a client to abort, or to go to sleep for a long time
///   and try again later to check whether the logger has been activated.
///
/// - *x-arango-replication-lastincluded*: the tick value of the last included
///   value in the result. In incremental log fetching, this value can be used
///   as the *from* value for the following request. **Note** that if the result is
///   empty, the value will be *0*. This value should not be used as *from* value
///   by clients in the next request (otherwise the server would return the log
///   events from the start of the log again).
///
/// - *x-arango-replication-lasttick*: the last tick value the logger server has
///   logged (not necessarily included in the result). By comparing the the last
///   tick and last included tick values, clients have an approximate indication of
///   how many events there are still left to fetch.
///
/// - *x-arango-replication-checkmore*: whether or not there already exists more
///   log data which the client could fetch immediately. If there is more log data
///   available, the client could call *logger-follow* again with an adjusted *from*
///   value to fetch remaining log entries until there are no more.
///
///   If there isn't any more log data to fetch, the client might decide to go
///   to sleep for a while before calling the logger again.
///
/// **Note**: this method is not supported on a coordinator in a cluster.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully, and there are log
/// events available for the requested range. The response body will not be empty
/// in this case.
///
/// @RESTRETURNCODE{204}
/// is returned if the request was executed successfully, but there are no log
/// events available for the requested range. The response body will be empty
/// in this case.
///
/// @RESTRETURNCODE{400}
/// is returned if either the *from* or *to* values are invalid.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred while assembling the response.
///
/// @RESTRETURNCODE{501}
/// is returned when this operation is called on a coordinator in a cluster.
///
/// @EXAMPLES
///
/// No log events available:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerFollowEmpty}
///     var re = require("org/arangodb/replication");
///     var lastTick = re.logger.state().state.lastLogTick;
///
///     var url = "/_api/replication/logger-follow?from=" + lastTick;
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 204);
///
///     logRawResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// A few log events:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerFollowSome}
///     var re = require("org/arangodb/replication");
///     db._drop("products");
///
///     var lastTick = re.logger.state().state.lastLogTick;
///
///     db._create("products");
///     db.products.save({ "_key": "p1", "name" : "flux compensator" });
///     db.products.save({ "_key": "p2", "name" : "hybrid hovercraft", "hp" : 5100 });
///     db.products.remove("p1");
///     db.products.update("p2", { "name" : "broken hovercraft" });
///     db.products.drop();
///
///     require("internal").wait(1);
///     var url = "/_api/replication/logger-follow?from=" + lastTick;
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logRawResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// More events than would fit into the response:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerFollowBufferLimit}
///     var re = require("org/arangodb/replication");
///     db._drop("products");
///
///     var lastTick = re.logger.state().state.lastLogTick;
///
///     db._create("products");
///     db.products.save({ "_key": "p1", "name" : "flux compensator" });
///     db.products.save({ "_key": "p2", "name" : "hybrid hovercraft", "hp" : 5100 });
///     db.products.remove("p1");
///     db.products.update("p2", { "name" : "broken hovercraft" });
///     db.products.drop();
///
///     require("internal").wait(1);
///     var url = "/_api/replication/logger-follow?from=" + lastTick + "&chunkSize=400";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logRawResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerFollow () {
  // determine start and end tick
  triagens::wal::LogfileManagerState state = triagens::wal::LogfileManager::instance()->state();
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd   = state.lastDataTick;

  bool found;
  char const* value;

  value = _request->value("from", found);
  if (found) {
    tickStart = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
  }

  // determine end tick for dump
  value = _request->value("to", found);
  if (found) {
    tickEnd = static_cast<TRI_voc_tick_t>(StringUtils::uint64(value));
  }

  if (found && (tickStart > tickEnd || tickEnd == 0)) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }
  
  bool includeSystem = true;
  value = _request->value("includeSystem", found);

  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  int res = TRI_ERROR_NO_ERROR;

  try {
    // initialise the dump container
    TRI_replication_dump_t dump(_vocbase, (size_t) determineChunkSize(), includeSystem);

    // and dump
    res = TRI_DumpLogReplication(&dump, tickStart, tickEnd, false);

    if (res == TRI_ERROR_NO_ERROR) {
      bool const checkMore = (dump._lastFoundTick > 0 && dump._lastFoundTick != state.lastDataTick);

      // generate the result
      size_t const length = TRI_LengthStringBuffer(dump._buffer);

      if (length == 0) {
        _response = createResponse(HttpResponse::NO_CONTENT);
      }
      else {
        _response = createResponse(HttpResponse::OK);
      }

      _response->setContentType("application/x-arango-dump; charset=utf-8");

      // set headers
      _response->setHeader(TRI_REPLICATION_HEADER_CHECKMORE,
                           strlen(TRI_REPLICATION_HEADER_CHECKMORE),
                           checkMore ? "true" : "false");

      _response->setHeader(TRI_REPLICATION_HEADER_LASTINCLUDED,
                           strlen(TRI_REPLICATION_HEADER_LASTINCLUDED),
                           StringUtils::itoa(dump._lastFoundTick));

      _response->setHeader(TRI_REPLICATION_HEADER_LASTTICK,
                           strlen(TRI_REPLICATION_HEADER_LASTTICK),
                           StringUtils::itoa(state.lastTick));

      _response->setHeader(TRI_REPLICATION_HEADER_ACTIVE,
                           strlen(TRI_REPLICATION_HEADER_ACTIVE), 
                           "true");

      if (length > 0) {
        // transfer ownership of the buffer contents
        _response->body().set(dump._buffer);

        // to avoid double freeing
        TRI_StealStringBuffer(dump._buffer);
      }

      insertClient(dump._lastFoundTick);
    }
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_replication_inventory
/// @RESTHEADER{GET /_api/replication/inventory, Return inventory of collections and indexes}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{includeSystem,boolean,optional}
/// Include system collections in the result. The default value is *true*.
///
/// @RESTDESCRIPTION
/// Returns the array of collections and indexes available on the server. This
/// array can be used by replication clients to initiate an initial sync with the
/// server.
///
/// The response will contain a JSON object with the *collection* and *state* and
/// *tick* attributes.
///
/// *collections* is a array of collections with the following sub-attributes:
///
/// - *parameters*: the collection properties
///
/// - *indexes*: a array of the indexes of a the collection. Primary indexes and edges indexes
///    are not included in this array.
///
/// The *state* attribute contains the current state of the replication logger. It
/// contains the following sub-attributes:
///
/// - *running*: whether or not the replication logger is currently active. Note:
///   since ArangoDB 2.2, the value will always be *true*
///
/// - *lastLogTick*: the value of the last tick the replication logger has written
///
/// - *time*: the current time on the server
///
/// Replication clients should note the *lastLogTick* value returned. They can then
/// fetch collections' data using the dump method up to the value of lastLogTick, and
/// query the continuous replication log for log events after this tick value.
///
/// To create a full copy of the collections on the server, a replication client
/// can execute these steps:
///
/// - call the */inventory* API method. This returns the *lastLogTick* value and the
///   array of collections and indexes from the server.
///
/// - for each collection returned by */inventory*, create the collection locally and
///   call */dump* to stream the collection data to the client, up to the value of
///   *lastLogTick*.
///   After that, the client can create the indexes on the collections as they were
///   reported by */inventory*.
///
/// If the clients wants to continuously stream replication log events from the logger
/// server, the following additional steps need to be carried out:
///
/// - the client should call */logger-follow* initially to fetch the first batch of
///   replication events that were logged after the client's call to */inventory*.
///
///   The call to */logger-follow* should use a *from* parameter with the value of the
///   *lastLogTick* as reported by */inventory*. The call to */logger-follow* will return the
///   *x-arango-replication-lastincluded* which will contain the last tick value included
///   in the response.
///
/// - the client can then continuously call */logger-follow* to incrementally fetch new
///   replication events that occurred after the last transfer.
///
///   Calls should use a *from* parameter with the value of the *x-arango-replication-lastincluded*
///   header of the previous response. If there are no more replication events, the
///   response will be empty and clients can go to sleep for a while and try again
///   later.
///
/// **Note**: on a coordinator, this request must have the URL parameter
/// *DBserver* which must be an ID of a DBserver.
/// The very same request is forwarded synchronously to that DBserver.
/// It is an error if this attribute is not bound in the coordinator case.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred while assembling the response.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationInventory}
///     var url = "/_api/replication/inventory";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// With some additional indexes:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationInventoryIndexes}
///     db._drop("IndexedCollection1");
///     var c1 = db._create("IndexedCollection1");
///     c1.ensureHashIndex("name");
///     c1.ensureUniqueSkiplist("a", "b");
///     c1.ensureCapConstraint(500);
///
///     db._drop("IndexedCollection2");
///     var c2 = db._create("IndexedCollection2");
///     c2.ensureFulltextIndex("text", 10);
///     c2.ensureSkiplist("a");
///     c2.ensureCapConstraint(0, 1048576);
///
///     var url = "/_api/replication/inventory";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///     logJsonResponse(response);
///
///     db._flushCache();
///     db._drop("IndexedCollection1");
///     db._drop("IndexedCollection2");
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandInventory () {
  TRI_voc_tick_t tick = TRI_CurrentTickServer();

  // include system collections?
  bool includeSystem = true;
  bool found;
  const char* value = _request->value("includeSystem", found);

  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  // collections and indexes
  TRI_json_t* collections = TRI_InventoryCollectionsVocBase(_vocbase, tick, &filterCollection, (void*) &includeSystem);

  if (collections == nullptr) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  TRI_ASSERT(JsonHelper::isArray(collections));

  // sort collections by type, then name
  size_t const n = collections->_value._objects._length;

  if (n > 1) {
    // sort by collection type (vertices before edges), then name
    qsort(collections->_value._objects._buffer, n, sizeof(TRI_json_t), &sortCollections);
  }


  TRI_json_t json;
  TRI_InitObjectJson(TRI_CORE_MEM_ZONE, &json);

  char* tickString = TRI_StringUInt64(tick);

  // add collections data
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &json, "collections", collections);

  // "state"
  TRI_json_t* state = TRI_CreateObjectJson(TRI_CORE_MEM_ZONE);

  if (state == nullptr) {
    TRI_DestroyJson(TRI_CORE_MEM_ZONE, &json);
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  triagens::wal::LogfileManagerState const&& s = triagens::wal::LogfileManager::instance()->state();

  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, state, "running", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));
  char* logTickString = TRI_StringUInt64(s.lastTick);
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, state, "lastLogTick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, logTickString, strlen(logTickString)));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, state, "totalEvents", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) s.numEvents));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, state, "time", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, s.timeString.c_str(), s.timeString.size()));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &json, "state", state);

  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &json, "tick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, tickString, strlen(tickString)));

  generateResult(&json);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &json);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_replication_cluster_inventory
/// @RESTHEADER{GET /_api/replication/clusterInventory, Return cluster inventory of collections and indexes}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{includeSystem,boolean,optional}
/// Include system collections in the result. The default value is *true*.
///
/// @RESTDESCRIPTION
/// Returns the array of collections and indexes available on the cluster.
///
/// The response will be an array of JSON objects, one for each collection.
/// Each collection containscontains exactly two keys "parameters" and "indexes". This
/// information comes from Plan/Collections/<DB-Name>/* in the agency,
/// just that the *indexes* attribute there is relocated to adjust it to
/// the data format of arangodump.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred while assembling the response.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandClusterInventory () {
  string const& dbName = _request->databaseName();
  string value;
  bool found;
  bool includeSystem = true;

  value = _request->value("includeSystem", found);
  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  AgencyComm _agency;
  AgencyCommResult result;

  {
    string prefix("Plan/Collections/");
    prefix.append(dbName);

    AgencyCommLocker locker("Plan", "READ");
    if (! locker.successful()) {
      generateError(HttpResponse::SERVER_ERROR,
                    TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN);
    }
    else {
      result = _agency.getValues(prefix, false);
      if (! result.successful()) {
        generateError(HttpResponse::SERVER_ERROR,
                      TRI_ERROR_CLUSTER_READING_PLAN_AGENCY);
      }
      else {
        if (! result.parse(prefix + "/", false)) {
          generateError(HttpResponse::SERVER_ERROR,
                        TRI_ERROR_CLUSTER_READING_PLAN_AGENCY);
        }
        else {
          map<string, AgencyCommResultEntry>::iterator it;
          TRI_json_t json;
          TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &json, result._values.size());
          for (it = result._values.begin();
               it != result._values.end(); ++it) {
            if (TRI_IsObjectJson(it->second._json)) {
              TRI_json_t const* sub
                  = TRI_LookupObjectJson(it->second._json, "isSystem");
              if (includeSystem ||
                  (TRI_IsBooleanJson(sub) && ! sub->_value._boolean)) {
                TRI_json_t coll;
                TRI_InitObjectJson(TRI_CORE_MEM_ZONE, &coll, 2);
                sub = TRI_LookupObjectJson( it->second._json, "indexes");
                TRI_InsertObjectJson(TRI_CORE_MEM_ZONE,&coll,"indexes", sub);
                TRI_DeleteObjectJson(TRI_UNKNOWN_MEM_ZONE, it->second._json,
                                    "indexes");
                // This makes a copy to the CORE memory zone:
                TRI_InsertObjectJson(TRI_CORE_MEM_ZONE, &coll,
                                     "parameters", it->second._json);
                TRI_PushBack2ArrayJson(&json, &coll);
              }
            }
          }

          // Wrap the result:
          TRI_json_t wrap;
          TRI_InitObjectJson(TRI_CORE_MEM_ZONE, &wrap, 3);
          TRI_Insert2ObjectJson(TRI_CORE_MEM_ZONE,&wrap,"collections", &json);
          TRI_voc_tick_t tick = TRI_CurrentTickServer();
          char* tickString = TRI_StringUInt64(tick);
          char const* stateStatic = "unused";
          char* state = TRI_DuplicateString2(stateStatic, strlen(stateStatic));
          TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &wrap, "tick",
                      TRI_CreateStringJson(TRI_CORE_MEM_ZONE, tickString, strlen(tickString)));
          TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &wrap, "state",
                      TRI_CreateStringJson(TRI_CORE_MEM_ZONE, state, strlen(state)));

          generateResult(HttpResponse::OK, &wrap);
          TRI_DestroyJson(TRI_CORE_MEM_ZONE, &wrap);
        }
      }
    }
  }

}


////////////////////////////////////////////////////////////////////////////////
/// @brief extract the collection id from JSON TODO: move
////////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t RestReplicationHandler::getCid (TRI_json_t const* json) const {
  if (! JsonHelper::isObject(json)) {
    return 0;
  }

  TRI_json_t const* id = JsonHelper::getObjectElement(json, "cid");

  if (JsonHelper::isString(id)) {
    return StringUtils::uint64(id->_value._string.data, id->_value._string.length - 1);
  }
  else if (JsonHelper::isNumber(id)) {
    return (TRI_voc_cid_t) id->_value._number;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection, based on the JSON provided TODO: move
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::createCollection (TRI_json_t const* json,
                                              TRI_vocbase_col_t** dst,
                                              bool reuseId) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  if (! JsonHelper::isObject(json)) {
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  const string name = JsonHelper::getStringValue(json, "name", "");

  if (name.empty()) {
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  TRI_voc_cid_t cid = 0;

  if (reuseId) {
    cid = getCid(json);

    if (cid == 0) {
      return TRI_ERROR_HTTP_BAD_PARAMETER;
    }
  }

  const TRI_col_type_e type = (TRI_col_type_e) JsonHelper::getNumericValue<int>(json, "type", (int) TRI_COL_TYPE_DOCUMENT);

  TRI_vocbase_col_t* col = nullptr;

  if (cid > 0) {
    col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);
  }

  if (col != nullptr &&
      (TRI_col_type_t) col->_type == (TRI_col_type_t) type) {
    // collection already exists. TODO: compare attributes
    return TRI_ERROR_NO_ERROR;
  }


  TRI_json_t* keyOptions = nullptr;

  if (JsonHelper::isObject(JsonHelper::getObjectElement(json, "keyOptions"))) {
    keyOptions = TRI_CopyJson(TRI_CORE_MEM_ZONE, JsonHelper::getObjectElement(json, "keyOptions"));
  }

  TRI_col_info_t params;
  TRI_InitCollectionInfo(_vocbase,
                         &params,
                         name.c_str(),
                         type,
                         (TRI_voc_size_t) JsonHelper::getNumericValue<int64_t>(json, "maximalSize", (int64_t) TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
                         keyOptions);

  if (keyOptions != nullptr) {
    TRI_FreeJson(TRI_CORE_MEM_ZONE, keyOptions);
  }

  params._doCompact   = JsonHelper::getBooleanValue(json, "doCompact", true);
  params._waitForSync = JsonHelper::getBooleanValue(json, "waitForSync", _vocbase->_settings.defaultWaitForSync);
  params._isVolatile  = JsonHelper::getBooleanValue(json, "isVolatile", false);
  params._isSystem    = (name[0] == '_');
  params._planId      = 0;

  TRI_voc_cid_t planId = JsonHelper::stringUInt64(json, "planId");
  if (planId > 0) {
    params._planId = planId;
  }

  if (cid > 0) {
    // wait for "old" collection to be dropped
    char* dirName = TRI_GetDirectoryCollection(_vocbase->_path,
                                               name.c_str(),
                                               type,
                                               cid);

    if (dirName != nullptr) {
      char* parameterName = TRI_Concatenate2File(dirName, TRI_VOC_PARAMETER_FILE);

      if (parameterName != nullptr) {
        int iterations = 0;

        // TODO: adjust sleep timer & maxiterations
        while (TRI_IsDirectory(dirName) && TRI_ExistsFile(parameterName) && iterations++ < 1200) {
          usleep(100 * 1000);
        }

        TRI_FreeString(TRI_CORE_MEM_ZONE, parameterName);
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, dirName);
    }
  }

  col = TRI_CreateCollectionVocBase(_vocbase, &params, cid, true);
  TRI_FreeCollectionInfoOptions(&params);

  if (col == nullptr) {
    return TRI_errno();
  }

  if (dst != nullptr) {
    *dst = col;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection TODO
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreCollection () {
  TRI_json_t* json = _request->toJson(0);

  if (json == nullptr) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return;
  }

  bool found;
  char const* value;

  bool overwrite = false;
  value = _request->value("overwrite", found);

  if (found) {
    overwrite = StringUtils::boolean(value);
  }

  bool recycleIds = false;
  value = _request->value("recycleIds", found);
  if (found) {
    recycleIds = StringUtils::boolean(value);
  }

  bool force = false;
  value = _request->value("force", found);
  if (found) {
    force = StringUtils::boolean(value);
  }

  string errorMsg;
  int res;
  if (ServerState::instance()->isCoordinator()) {
    res = processRestoreCollectionCoordinator(json, overwrite, recycleIds,
                                              force, errorMsg);
  }
  else {
    res = processRestoreCollection(json, overwrite, recycleIds, force, errorMsg);
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
  else {
    TRI_json_t result;

    TRI_InitObjectJson(TRI_CORE_MEM_ZONE, &result);
    TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &result, "result", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));

    generateResult(&result);
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection TODO
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreIndexes () {
  TRI_json_t* json = _request->toJson(nullptr);

  if (json == nullptr) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid JSON");
    return;
  }

  bool found;
  bool force = false;
  char const* value = _request->value("force", found);
  if (found) {
    force = StringUtils::boolean(value);
  }

  string errorMsg;
  int res;
  if (ServerState::instance()->isCoordinator()) {
    res = processRestoreIndexesCoordinator(json, force, errorMsg);
  }
  else {
    res = processRestoreIndexes(json, force, errorMsg);
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
  else {
    TRI_json_t result;

    TRI_InitObjectJson(TRI_CORE_MEM_ZONE, &result);
    TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &result, "result", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));

    generateResult(&result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreCollection (TRI_json_t const* collection,
                                                      bool dropExisting,
                                                      bool reuseId,
                                                      bool force,
                                                      string& errorMsg) {
  if (! JsonHelper::isObject(collection)) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  TRI_json_t const* parameters = JsonHelper::getObjectElement(collection, "parameters");

  if (! JsonHelper::isObject(parameters)) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  TRI_json_t const* indexes = JsonHelper::getObjectElement(collection, "indexes");

  if (! JsonHelper::isArray(indexes)) {
    errorMsg = "collection indexes declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  const string name = JsonHelper::getStringValue(parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (JsonHelper::getBooleanValue(parameters, "deleted", false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  TRI_vocbase_col_t* col = nullptr;

  if (reuseId) {
    TRI_json_t const* idString = JsonHelper::getObjectElement(parameters, "cid");

    if (! JsonHelper::isString(idString)) {
      errorMsg = "collection id is missing";

      return TRI_ERROR_HTTP_BAD_PARAMETER;
    }

    TRI_voc_cid_t cid = StringUtils::uint64(idString->_value._string.data, idString->_value._string.length - 1);

    // first look up the collection by the cid
    col = TRI_LookupCollectionByIdVocBase(_vocbase, cid);
  }


  if (col == nullptr) {
    // not found, try name next
    col = TRI_LookupCollectionByNameVocBase(_vocbase, name.c_str());
  }

  // drop an existing collection if it exists
  if (col != nullptr) {
    if (dropExisting) {
      int res = TRI_DropCollectionVocBase(_vocbase, col, true);

      if (res == TRI_ERROR_FORBIDDEN) {
        // some collections must not be dropped

        // instead, truncate them
        SingleCollectionWriteTransaction<UINT64_MAX> trx(new StandaloneTransactionContext(), _vocbase, col->_cid);

        res = trx.begin();
        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

        res = trx.truncate(false);
        res = trx.finish(res);

        return res;
      }

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "unable to drop collection '" + name + "': " + string(TRI_errno_string(res));

        return res;
      }
    }
    else {
      int res = TRI_ERROR_ARANGO_DUPLICATE_NAME;

      errorMsg = "unable to create collection '" + name + "': " + string(TRI_errno_string(res));

      return res;
    }
  }

  // now re-create the collection
  int res = createCollection(parameters, &col, reuseId);

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg = "unable to create collection: " + string(TRI_errno_string(res));

    return res;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the structure of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreCollectionCoordinator (
                 TRI_json_t const* collection,
                 bool dropExisting,
                 bool reuseId,
                 bool force,
                 string& errorMsg) {

  if (! JsonHelper::isObject(collection)) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  TRI_json_t* parameters = JsonHelper::getObjectElement(collection, "parameters");

  if (! JsonHelper::isObject(parameters)) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  string const name = JsonHelper::getStringValue(parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (JsonHelper::getBooleanValue(parameters, "deleted", false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  string dbName = _vocbase->_name;

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  shared_ptr<CollectionInfo> col = ci->getCollection(dbName, name);

  // drop an existing collection if it exists
  if (! col->empty()) {
    if (dropExisting) {
      int res = ci->dropCollectionCoordinator(dbName, col->id_as_string(),
                                              errorMsg, 0.0);
      if (res == TRI_ERROR_FORBIDDEN) {
        // some collections must not be dropped
        res = truncateCollectionOnCoordinator(dbName, name);
        if (res != TRI_ERROR_NO_ERROR) {
          errorMsg = "unable to truncate collection (dropping is forbidden): "+
                     name;
          return res;
        }
      }

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "unable to drop collection '" + name + "': " + string(TRI_errno_string(res));

        return res;
      }
    }
    else {
      int res = TRI_ERROR_ARANGO_DUPLICATE_NAME;

      errorMsg = "unable to create collection '" + name + "': " + string(TRI_errno_string(res));

      return res;
    }
  }

  // now re-create the collection
  // dig out number of shards:
  uint64_t numberOfShards = 1;
  TRI_json_t const* shards = JsonHelper::getObjectElement(parameters, "shards");
  if (nullptr != shards && TRI_IsObjectJson(shards)) {
    numberOfShards = TRI_LengthVector(&shards->_value._objects)/2;
  }
  // We take one shard if "shards" was not given

  TRI_voc_tick_t new_id_tick = ci->uniqid(1);
  string new_id = StringUtils::itoa(new_id_tick);
  TRI_ReplaceObjectJson(TRI_UNKNOWN_MEM_ZONE, parameters, "id",
                       TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE,
                                                new_id.c_str(), new_id.size()));

  // Now put in the primary and an edge index if needed:
  TRI_json_t* indexes = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (indexes == nullptr) {
    errorMsg = "out of memory";
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // create a dummy primary index
  TRI_index_t* idx = TRI_CreatePrimaryIndex(0);

  if (idx == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, indexes);
    errorMsg = "out of memory";
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_json_t* idxJson = idx->json(idx);
  TRI_FreeIndex(idx);

  TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, indexes, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, idxJson));
  TRI_FreeJson(TRI_CORE_MEM_ZONE, idxJson);

  TRI_json_t* type = TRI_LookupObjectJson(parameters, "type");
  TRI_col_type_e collectionType;
  if (TRI_IsNumberJson(type)) {
    collectionType = (TRI_col_type_e) ((int) type->_value._number);
  }
  else {
    errorMsg = "collection type not given or wrong";
    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (collectionType == TRI_COL_TYPE_EDGE) {
    // create a dummy edge index
    idx = TRI_CreateEdgeIndex(0, new_id_tick);

    if (idx == nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, indexes);
      errorMsg = "cannot create edge index";
      return TRI_ERROR_INTERNAL;
    }

    idxJson = idx->json(idx);
    TRI_FreeIndex(idx);

    TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, indexes, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, idxJson));
    TRI_FreeJson(TRI_CORE_MEM_ZONE, idxJson);
  }

  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, parameters, "indexes", indexes);

  int res = ci->createCollectionCoordinator(dbName, new_id, numberOfShards,
                                            parameters, errorMsg, 0.0);

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg = "unable to create collection: " + string(TRI_errno_string(res));

    return res;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreIndexes (TRI_json_t const* collection,
                                                   bool force,
                                                   string& errorMsg) {
  if (! JsonHelper::isObject(collection)) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  TRI_json_t const* parameters = JsonHelper::getObjectElement(collection, "parameters");

  if (! JsonHelper::isObject(parameters)) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  TRI_json_t const* indexes = JsonHelper::getObjectElement(collection, "indexes");

  if (! JsonHelper::isArray(indexes)) {
    errorMsg = "collection indexes declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  size_t const n = indexes->_value._objects._length;
  if (n == 0) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  string const name = JsonHelper::getStringValue(parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (JsonHelper::getBooleanValue(parameters, "deleted", false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }
    
  TRI_ReadLockReadWriteLock(&_vocbase->_inventoryLock);

  // look up the collection
  try {
    CollectionGuard guard(_vocbase, name.c_str());

    TRI_document_collection_t* document = guard.collection()->_collection;

    SingleCollectionWriteTransaction<UINT64_MAX> trx(new StandaloneTransactionContext(), _vocbase, document->_info._cid); 

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "unable to start transaction: " + string(TRI_errno_string(res));
      THROW_ARANGO_EXCEPTION(res);
    }
 
    for (size_t i = 0; i < n; ++i) {
      TRI_json_t const* idxDef = static_cast<TRI_json_t const*>(TRI_AtVector(&indexes->_value._objects, i));
      TRI_index_t* idx = nullptr;

      // {"id":"229907440927234","type":"hash","unique":false,"fields":["x","Y"]}

      res = TRI_FromJsonIndexDocumentCollection(document, idxDef, &idx);

      if (res != TRI_ERROR_NO_ERROR) {
        errorMsg = "could not create index: " + string(TRI_errno_string(res));
        break;
      }
      else {
        TRI_ASSERT(idx != nullptr);

        res = TRI_SaveIndex(document, idx, true);

        if (res != TRI_ERROR_NO_ERROR) {
          errorMsg = "could not save index: " + string(TRI_errno_string(res));
          break;
        }
      }
    }
  }
  catch (triagens::arango::Exception const& ex) {
    errorMsg = "could not create index: " + string(TRI_errno_string(ex.code()));
  }
  catch (...) {
    errorMsg = "could not create index: unknown error";
  }

  TRI_ReadUnlockReadWriteLock(&_vocbase->_inventoryLock);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the indexes of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreIndexesCoordinator (
                 TRI_json_t const* collection,
                 bool force,
                 string& errorMsg) {

  if (! JsonHelper::isObject(collection)) {
    errorMsg = "collection declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  TRI_json_t const* parameters = JsonHelper::getObjectElement(collection, "parameters");

  if (! JsonHelper::isObject(parameters)) {
    errorMsg = "collection parameters declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  TRI_json_t const* indexes = JsonHelper::getObjectElement(collection, "indexes");

  if (! JsonHelper::isArray(indexes)) {
    errorMsg = "collection indexes declaration is invalid";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  const size_t n = indexes->_value._objects._length;
  if (n == 0) {
    // nothing to do
    return TRI_ERROR_NO_ERROR;
  }

  const string name = JsonHelper::getStringValue(parameters, "name", "");

  if (name.empty()) {
    errorMsg = "collection name is missing";

    return TRI_ERROR_HTTP_BAD_PARAMETER;
  }

  if (JsonHelper::getBooleanValue(parameters, "deleted", false)) {
    // we don't care about deleted collections
    return TRI_ERROR_NO_ERROR;
  }

  string dbName = _vocbase->_name;

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  shared_ptr<CollectionInfo> col = ci->getCollection(dbName, name);

  if (col->empty()) {
    errorMsg = "could not find collection '" + name + "'";
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  int res = TRI_ERROR_NO_ERROR;
  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* idxDef = static_cast<TRI_json_t const*>(TRI_AtVector(&indexes->_value._objects, i));
    TRI_json_t* res_json = nullptr;
    res = ci->ensureIndexCoordinator(dbName, col->id_as_string(), idxDef,
                     true, IndexComparator, res_json, errorMsg, 3600.0);
    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "could not create index: " + string(TRI_errno_string(res));
      break;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the data from a collection dump or the continuous log
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::applyCollectionDumpMarker (CollectionNameResolver const& resolver,
                                                       TRI_transaction_collection_t* trxCollection,
                                                       TRI_replication_operation_e type,
                                                       const TRI_voc_key_t key,
                                                       const TRI_voc_rid_t rid,
                                                       TRI_json_t const* json,
                                                       string& errorMsg) {

  if (type == REPLICATION_MARKER_DOCUMENT || 
      type == REPLICATION_MARKER_EDGE) {
    // {"type":2400,"key":"230274209405676","data":{"_key":"230274209405676","_rev":"230274209405676","foo":"bar"}}

    TRI_ASSERT(json != nullptr);

    TRI_document_collection_t* document = trxCollection->_collection->_collection;
    TRI_memory_zone_t* zone = document->getShaper()->_memoryZone;  // PROTECTED by trx in trxCollection
    TRI_shaped_json_t* shaped = TRI_ShapedJsonJson(document->getShaper(), json, true);  // PROTECTED by trx in trxCollection
    
    if (shaped == nullptr) {
      errorMsg = TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);

      return TRI_ERROR_OUT_OF_MEMORY;
    }

    try {
      TRI_doc_mptr_copy_t mptr;

      int res = TRI_ReadShapedJsonDocumentCollection(trxCollection, key, &mptr, false);

      if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // insert

        if (type == REPLICATION_MARKER_EDGE) {
          // edge
          if (document->_info._type != TRI_COL_TYPE_EDGE) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
          }
          else {
            res = TRI_ERROR_NO_ERROR;

            string const from = JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_FROM, "");
            string const to   = JsonHelper::getStringValue(json, TRI_VOC_ATTRIBUTE_TO, "");


            // parse _from
            TRI_document_edge_t edge;
            if (! DocumentHelper::parseDocumentId(resolver, from.c_str(), edge._fromCid, &edge._fromKey)) {
              res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
            }

            // parse _to
            if (! DocumentHelper::parseDocumentId(resolver, to.c_str(), edge._toCid, &edge._toKey)) {
              res = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
            }

            if (res == TRI_ERROR_NO_ERROR) {
              res = TRI_InsertShapedJsonDocumentCollection(trxCollection, key, rid, nullptr, &mptr, shaped, &edge, false, false, true);
            }
          }
        }
        else {
          // document
          if (document->_info._type != TRI_COL_TYPE_DOCUMENT) {
            res = TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID;
          }
          else {
            res = TRI_InsertShapedJsonDocumentCollection(trxCollection, key, rid, nullptr, &mptr, shaped, nullptr, false, false, true);
          }
        }
      }
      else {
        // update

        // init the update policy
        TRI_doc_update_policy_t policy(TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr);
        res = TRI_UpdateShapedJsonDocumentCollection(trxCollection, key, rid, nullptr, &mptr, shaped, &policy, false, false);
      }

      TRI_FreeShapedJson(zone, shaped);

      return res;
    }
    catch (triagens::arango::Exception const& ex) {
      TRI_FreeShapedJson(zone, shaped);
      return ex.code();
    }
    catch (...) {
      TRI_FreeShapedJson(zone, shaped);
      return TRI_ERROR_INTERNAL;
    }
  }

  else if (type == REPLICATION_MARKER_REMOVE) {
    // {"type":2402,"key":"592063"}
    // init the update policy
    TRI_doc_update_policy_t policy(TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr);

    int res = TRI_ERROR_INTERNAL;

    try {
      res = TRI_RemoveShapedJsonDocumentCollection(trxCollection, key, rid, nullptr, &policy, false, false);

      if (res != TRI_ERROR_NO_ERROR && res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // ignore this error
        res = TRI_ERROR_NO_ERROR;
      }
    }
    catch (triagens::arango::Exception const& ex) {
      res = ex.code();
    }
    catch (...) {
      res = TRI_ERROR_INTERNAL;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      errorMsg = "document removal operation failed: " + string(TRI_errno_string(res));
    }

    return res;
  }

  else {
    errorMsg = "unexpected marker type " + StringUtils::itoa(type);

    return TRI_ERROR_REPLICATION_UNEXPECTED_MARKER;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO MOVE
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreDataBatch (CollectionNameResolver const& resolver,
                                                     TRI_transaction_collection_t* trxCollection,
                                                     bool useRevision,
                                                     bool force,
                                                     std::string& errorMsg) {
  string const invalidMsg = "received invalid JSON data for collection " +
                            StringUtils::itoa(trxCollection->_cid);

  char const* ptr = _request->body();
  char const* end = ptr + _request->bodySize();

  while (ptr < end) {
    char const* pos = strchr(ptr, '\n');

    if (pos == nullptr) {
      pos = end;
    }
    else {
      *((char*) pos) = '\0';
    }

    if (pos - ptr > 1) {
      // found something
      TRI_json_t* json = TRI_JsonString(TRI_CORE_MEM_ZONE, ptr);

      if (! JsonHelper::isObject(json)) {
        if (json != nullptr) {
          TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
        }

        errorMsg = invalidMsg;

        return TRI_ERROR_HTTP_CORRUPTED_JSON;
      }

      TRI_replication_operation_e type = REPLICATION_INVALID;
      const char* key       = nullptr;
      TRI_voc_rid_t rid     = 0;
      TRI_json_t const* doc = nullptr;

      const size_t n = json->_value._objects._length;

      for (size_t i = 0; i < n; i += 2) {
        TRI_json_t const* element = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));

        if (! JsonHelper::isString(element)) {
          TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
          errorMsg = invalidMsg;

          return TRI_ERROR_HTTP_CORRUPTED_JSON;
        }

        const char* attributeName = element->_value._string.data;
        TRI_json_t const* value = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i + 1));

        if (TRI_EqualString(attributeName, "type")) {
          if (JsonHelper::isNumber(value)) {
            type = (TRI_replication_operation_e) (int) value->_value._number;
          }
        }

        else if (TRI_EqualString(attributeName, "key")) {
          if (JsonHelper::isString(value)) {
            key = value->_value._string.data;
          }
        }

        else if (useRevision && TRI_EqualString(attributeName, "rev")) {
          if (JsonHelper::isString(value)) {
            rid = StringUtils::uint64(value->_value._string.data, value->_value._string.length - 1);
          }
        }

        else if (TRI_EqualString(attributeName, "data")) {
          if (JsonHelper::isObject(value)) {
            doc = value;
          }
        }
      }

      // key must not be 0, but doc can be 0!
      if (key == nullptr) {
        TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
        errorMsg = invalidMsg;

        return TRI_ERROR_HTTP_BAD_PARAMETER;
      }

      int res = applyCollectionDumpMarker(resolver, trxCollection, type, (const TRI_voc_key_t) key, rid, doc, errorMsg);

      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

      if (res != TRI_ERROR_NO_ERROR && ! force) {
        return res;
      }
    }

    ptr = pos + 1;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO
////////////////////////////////////////////////////////////////////////////////

int RestReplicationHandler::processRestoreData (CollectionNameResolver const& resolver,
                                                TRI_voc_cid_t cid,
                                                bool useRevision,
                                                bool force,
                                                string& errorMsg) {

  SingleCollectionWriteTransaction<UINT64_MAX> trx(new StandaloneTransactionContext(), _vocbase, cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    errorMsg = "unable to start transaction: " + string(TRI_errno_string(res));

    return res;
  }

  TRI_transaction_collection_t* trxCollection = trx.trxCollection();

  if (trxCollection == nullptr) {
    res = TRI_ERROR_INTERNAL;
    errorMsg = "unable to start transaction: " + string(TRI_errno_string(res));
  }
  else {
    // TODO: waitForSync disabled here. use for initial replication, too
    // sync at end of trx
    trxCollection->_waitForSync = false;

    // create a fake transaction to avoid assertion failures. TODO: use proper transaction here
    res = processRestoreDataBatch(resolver, trxCollection, useRevision, force, errorMsg);
  }

  res = trx.finish(res);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection TODO
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreData () {
  char const* value = _request->value("collection");

  if (value == nullptr) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  CollectionNameResolver resolver(_vocbase);

  TRI_voc_cid_t cid = resolver.getCollectionId(value);

  if (cid == 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  bool recycleIds = false;
  value = _request->value("recycleIds");
  if (value != nullptr) {
    recycleIds = StringUtils::boolean(value);
  }

  bool force = false;
  value = _request->value("force");
  if (value != nullptr) {
    force = StringUtils::boolean(value);
  }

  string errorMsg;

  int res = processRestoreData(resolver, cid, recycleIds, force, errorMsg);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
  else {
    TRI_json_t result;

    TRI_InitObjectJson(TRI_CORE_MEM_ZONE, &result);
    TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &result, "result", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));

    generateResult(&result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the data of a collection, coordinator case
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandRestoreDataCoordinator () {
  char const* name = _request->value("collection");

  if (name == nullptr) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  string dbName = _vocbase->_name;
  string errorMsg;

  // in a cluster, we only look up by name:
  ClusterInfo* ci = ClusterInfo::instance();
  shared_ptr<CollectionInfo> col = ci->getCollection(dbName, name);

  if (col->empty()) {
    generateError(HttpResponse::BAD, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }

  // We need to distribute the documents we get over the shards:
  map<ShardID, ServerID> shardIdsMap = col->shardIds();
  map<string, size_t> shardTab;
  vector<string> shardIds;
  map<ShardID, ServerID>::iterator it;
  map<string, size_t>::iterator it2;
  for (it = shardIdsMap.begin(); it != shardIdsMap.end(); ++it) {
    shardTab.insert(make_pair(it->first,shardIds.size()));
    shardIds.push_back(it->first);
  }
  vector<StringBuffer*> bufs;
  size_t j;
  for (j = 0; j < shardIds.size(); j++) {
    bufs.push_back(new StringBuffer(TRI_UNKNOWN_MEM_ZONE));
  }

  const string invalidMsg = string("received invalid JSON data for collection ")
                            + name;

  char const* ptr = _request->body();
  char const* end = ptr + _request->bodySize();

  int res = TRI_ERROR_NO_ERROR;

  while (ptr < end) {
    char const* pos = strchr(ptr, '\n');

    if (pos == 0) {
      pos = end;
    }
    else {
      *((char*) pos) = '\0';
    }

    if (pos - ptr > 1) {
      // found something
      TRI_json_t* json = TRI_JsonString(TRI_CORE_MEM_ZONE, ptr);

      if (! JsonHelper::isObject(json)) {
        if (json != nullptr) {
          TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
        }

        errorMsg = invalidMsg;

        res = TRI_ERROR_HTTP_CORRUPTED_JSON;
        break;
      }

      const char* key       = nullptr;
      TRI_json_t const* doc = nullptr;
      TRI_replication_operation_e type = REPLICATION_INVALID;

      const size_t n = json->_value._objects._length;

      for (size_t i = 0; i < n; i += 2) {
        TRI_json_t const* element = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));

        if (! JsonHelper::isString(element)) {
          TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
          errorMsg = invalidMsg;

          res = TRI_ERROR_HTTP_CORRUPTED_JSON;
          break;
        }

        const char* attributeName = element->_value._string.data;
        TRI_json_t const* value = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i + 1));

        if (TRI_EqualString(attributeName, "type")) {
          if (JsonHelper::isNumber(value)) {
            type = (TRI_replication_operation_e) (int) value->_value._number;
          }
        }

        else if (TRI_EqualString(attributeName, "key")) {
          if (JsonHelper::isString(value)) {
            key = value->_value._string.data;
          }
        }

        else if (TRI_EqualString(attributeName, "data")) {
          if (JsonHelper::isObject(value)) {
            doc = value;
          }
        }
      }
      if (res != TRI_ERROR_NO_ERROR) {
        break;
      }

      // key must not be 0, but doc can be 0!
      if (key == nullptr) {
        TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
        errorMsg = invalidMsg;

        res = TRI_ERROR_HTTP_BAD_PARAMETER;
        break;
      }

      if (nullptr != doc && type != REPLICATION_MARKER_REMOVE) {
        ShardID responsibleShard;
        bool usesDefaultSharding;
        res = ci->getResponsibleShard(col->id_as_string(), doc, true,
                                      responsibleShard, usesDefaultSharding);
        if (res != TRI_ERROR_NO_ERROR) {
          TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
          errorMsg = "error during determining responsible shard";
          res = TRI_ERROR_INTERNAL;
          break;
        }
        else {
          it2 = shardTab.find(responsibleShard);
          if (it2 == shardTab.end()) {
            TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
            errorMsg = "cannot find responsible shard";
            res = TRI_ERROR_INTERNAL;
            break;
          }
          else {
            bufs[it2->second]->appendText(ptr, pos-ptr);
            bufs[it2->second]->appendText("\n");
          }
        }
      }
      else if (type == REPLICATION_MARKER_REMOVE) {
        // A remove marker, this has to be appended to all!
        for (j = 0; j < bufs.size(); j++) {
          bufs[j]->appendText(ptr, pos-ptr);
          bufs[j]->appendText("\n");
        }
      }
      else {
        // How very strange!
        TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
        errorMsg = invalidMsg;

        res = TRI_ERROR_HTTP_BAD_PARAMETER;
        break;
      }

      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    }

    ptr = pos + 1;
  }

  if (res == TRI_ERROR_NO_ERROR) {
    // Set a few variables needed for our work:
    ClusterComm* cc = ClusterComm::instance();

    // Send a synchronous request to that shard using ClusterComm:
    ClusterCommResult* result;
    CoordTransactionID coordTransactionID = TRI_NewTickServer();

    char const* value;
    string forceopt;
    value = _request->value("force");
    if (value != nullptr) {
      bool force = StringUtils::boolean(value);
      if (force) {
        forceopt = "&force=true";
      }
    }

    for (it = shardIdsMap.begin(); it != shardIdsMap.end(); ++it) {
      map<string, string>* headers = new map<string, string>;
      it2 = shardTab.find(it->first);
      if (it2 == shardTab.end()) {
        errorMsg = "cannot find shard";
        res = TRI_ERROR_INTERNAL;
      }
      else {
        j = it2->second;
        result = cc->asyncRequest("", coordTransactionID, "shard:" + it->first,
                               triagens::rest::HttpRequest::HTTP_REQUEST_PUT,
                               "/_db/" + StringUtils::urlEncode(dbName) +
                               "/_api/replication/restore-data?collection=" +
                               it->first + forceopt,
                               new string(bufs[j]->c_str(), bufs[j]->length()),
                               true, headers, NULL, 300.0);
        delete result;
      }
    }

    // Now listen to the results:
    unsigned int count;
    unsigned int nrok = 0;
    for (count = (int) shardIdsMap.size(); count > 0; count--) {
      result = cc->wait( "", coordTransactionID, 0, "", 0.0);
      if (result->status == CL_COMM_RECEIVED) {
        if (result->answer_code == triagens::rest::HttpResponse::OK ||
            result->answer_code == triagens::rest::HttpResponse::CREATED) {
          TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE,
                                            result->answer->body());

          if (JsonHelper::isObject(json)) {
            TRI_json_t const* r = TRI_LookupObjectJson(json, "result");
            if (TRI_IsBooleanJson(r)) {
              if (r->_value._boolean) {
                nrok++;
              }
            }
            else {
              TRI_json_t const* m = TRI_LookupObjectJson(json, "errorMessage");
              if (TRI_IsStringJson(m)) {
                errorMsg.append(m->_value._string.data,
                                m->_value._string.length);
                errorMsg.push_back(':');
              }
            }
          }

          if (json != nullptr) {
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          }
        }
        else if (result->answer_code == triagens::rest::HttpResponse::SERVER_ERROR) {
          TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE,
                                            result->answer->body());

          if (JsonHelper::isObject(json)) {
            TRI_json_t const* m = TRI_LookupObjectJson(json, "errorMessage");
            if (TRI_IsStringJson(m)) {
              errorMsg.append(m->_value._string.data,
                              m->_value._string.length);
              errorMsg.push_back(':');
            }
          }

          if (json != nullptr) {
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          }
        }

      }
      delete result;
    }

    if (nrok != shardIdsMap.size()) {
      errorMsg.append("some shard(s) produced error(s)");
      res = TRI_ERROR_INTERNAL;
    }
  }

  // Free all the string buffers:
  for (j = 0; j < shardIds.size(); j++) {
    delete bufs[j];
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::BAD, res, errorMsg);
    return;
  }

  TRI_json_t result;

  TRI_InitObjectJson(TRI_CORE_MEM_ZONE, &result);
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &result, "result", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));

  generateResult(&result);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_replication_dump
/// @RESTHEADER{GET /_api/replication/dump, Return data of a collection}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The name or id of the collection to dump.
///
/// @RESTQUERYPARAM{from,number,optional}
/// Lower bound tick value for results.
///
/// @RESTQUERYPARAM{to,number,optional}
/// Upper bound tick value for results.
///
/// @RESTQUERYPARAM{chunkSize,number,optional}
/// Approximate maximum size of the returned result.
///
/// @RESTQUERYPARAM{includeSystem,boolean,optional}
/// Include system collections in the result. The default value is *true*.
///
/// @RESTQUERYPARAM{ticks,boolean,optional}
/// Whether or not to include tick values in the dump. Default value is *true*.
///
/// @RESTDESCRIPTION
/// Returns the data from the collection for the requested range.
///
/// When the *from* URL parameter is not used, collection events are returned from
/// the beginning. When the *from* parameter is used, the result will only contain
/// collection entries which have higher tick values than the specified *from* value
/// (note: the log entry with a tick value equal to *from* will be excluded).
///
/// The *to* URL parameter can be used to optionally restrict the upper bound of
/// the result to a certain tick value. If used, the result will only contain
/// collection entries with tick values up to (including) *to*.
///
/// The *chunkSize* URL parameter can be used to control the size of the result.
/// It must be specified in bytes. The *chunkSize* value will only be honored
/// approximately. Otherwise a too low *chunkSize* value could cause the server
/// to not be able to put just one entry into the result and return it.
/// Therefore, the *chunkSize* value will only be consulted after an entry has
/// been written into the result. If the result size is then bigger than
/// *chunkSize*, the server will respond with as many entries as there are
/// in the response already. If the result size is still smaller than *chunkSize*,
/// the server will try to return more data if there's more data left to return.
///
/// If *chunkSize* is not specified, some server-side default value will be used.
///
/// The *Content-Type* of the result is *application/x-arango-dump*. This is an
/// easy-to-process format, with all entries going onto separate lines in the
/// response body.
///
/// Each line itself is a JSON object, with at least the following attributes:
///
/// - *tick*: the operation's tick attribute
///
/// - *key*: the key of the document/edge or the key used in the deletion operation
///
/// - *rev*: the revision id of the document/edge or the deletion operation
///
/// - *data*: the actual document/edge data for types 2300 and 2301. The full
///   document/edge data will be returned even for updates.
///
/// - *type*: the type of entry. Possible values for *type* are:
///
///   - 2300: document insertion/update
///
///   - 2301: edge insertion/update
///
///   - 2302: document/edge deletion
///
/// **Note**: there will be no distinction between inserts and updates when calling this method.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if either the *from* or *to* values are invalid.
///
/// @RESTRETURNCODE{404}
/// is returned when the collection could not be found.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred while assembling the response.
///
/// @EXAMPLES
///
/// Empty collection:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationDumpEmpty}
///     db._drop("testCollection");
///     var c = db._create("testCollection");
///     var url = "/_api/replication/dump?collection=" + c.name();
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 204);
///     logRawResponse(response);
///
///     c.drop();
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Non-empty collection:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationDump}
///     db._drop("testCollection");
///     var c = db._create("testCollection");
///     c.save({ "test" : true, "a" : "abc", "_key" : "abcdef" });
///     c.save({ "b" : 1, "c" : false, "_key" : "123456" });
///     c.update("123456", { "d" : "additional value" });
///     c.save({ "_key": "foobar" });
///     c.remove("foobar");
///     c.remove("abcdef");
///
///     var url = "/_api/replication/dump?collection=" + c.name();
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///     logRawResponse(response);
///
///     c.drop();
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandDump () {
  char const* collection = _request->value("collection");

  if (collection == nullptr) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }

  // determine start tick for dump
  TRI_voc_tick_t tickStart    = 0;
  TRI_voc_tick_t tickEnd      = static_cast<TRI_voc_tick_t>(UINT64_MAX);
  bool flush                  = true; // flush WAL before dumping?
  bool withTicks              = true;
  bool translateCollectionIds = true;

  bool found;
  char const* value;
 
  // determine flush WAL value 
  value = _request->value("flush", found);

  if (found) {
    flush = StringUtils::boolean(value);
  }

  // determine start tick for dump
  value = _request->value("from", found);

  if (found) {
    tickStart = (TRI_voc_tick_t) StringUtils::uint64(value);
  }

  // determine end tick for dump
  value = _request->value("to", found);

  if (found) {
    tickEnd = (TRI_voc_tick_t) StringUtils::uint64(value);
  }

  if (tickStart > tickEnd || tickEnd == 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid from/to values");
    return;
  }
  
  bool includeSystem = true;

  value = _request->value("includeSystem", found);
  if (found) {
    includeSystem = StringUtils::boolean(value);
  }

  value = _request->value("ticks", found);

  if (found) {
    withTicks = StringUtils::boolean(value);
  }

  value = _request->value("translateIds", found);

  if (found) {
    translateCollectionIds = StringUtils::boolean(value);
  }

  TRI_vocbase_col_t* c = TRI_LookupCollectionByNameVocBase(_vocbase, collection);

  if (c == nullptr) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }

  LOG_TRACE("requested collection dump for collection '%s', tickStart: %llu, tickEnd: %llu",
            collection,
            (unsigned long long) tickStart,
            (unsigned long long) tickEnd);

  int res = TRI_ERROR_NO_ERROR;

  try {
    if (flush) {
      triagens::wal::LogfileManager::instance()->flush(true, true, false);
    }

    triagens::arango::CollectionGuard guard(_vocbase, c->_cid, false);

    TRI_vocbase_col_t* col = guard.collection();
    TRI_ASSERT(col != nullptr);
  
    // initialise the dump container
    TRI_replication_dump_t dump(_vocbase, (size_t) determineChunkSize(), includeSystem);

    res = TRI_DumpCollectionReplication(&dump, col, tickStart, tickEnd, withTicks, translateCollectionIds);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    // generate the result
    size_t const length = TRI_LengthStringBuffer(dump._buffer);

    if (length == 0) {
      _response = createResponse(HttpResponse::NO_CONTENT);
    }
    else {
      _response = createResponse(HttpResponse::OK);
    }

    _response->setContentType("application/x-arango-dump; charset=utf-8");

    // set headers
    _response->setHeader(TRI_REPLICATION_HEADER_CHECKMORE,
                         strlen(TRI_REPLICATION_HEADER_CHECKMORE),
                         ((dump._hasMore || dump._bufferFull) ? "true" : "false"));

    _response->setHeader(TRI_REPLICATION_HEADER_LASTINCLUDED,
                         strlen(TRI_REPLICATION_HEADER_LASTINCLUDED),
                         StringUtils::itoa(dump._lastFoundTick));

    // transfer ownership of the buffer contents
    _response->body().set(dump._buffer);

    // avoid double freeing
    TRI_StealStringBuffer(dump._buffer);
  }
  catch (triagens::arango::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }
    
  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_replication_synchronize
/// @RESTHEADER{PUT /_api/replication/sync, Synchronize data from a remote endpoint}
///
/// @RESTBODYPARAM{configuration,json,required}
/// A JSON representation of the configuration.
///
/// @RESTDESCRIPTION
/// Starts a full data synchronization from a remote endpoint into the local
/// ArangoDB database.
///
/// The *sync* method can be used by replication clients to connect an ArangoDB database 
/// to a remote endpoint, fetch the remote list of collections and indexes, and collection
/// data. It will thus create a local backup of the state of data at the remote ArangoDB 
/// database. *sync* works on a per-database level. 
///
/// *sync* will first fetch the list of collections and indexes from the remote endpoint.
/// It does so by calling the *inventory* API of the remote database. It will then purge
/// data in the local ArangoDB database, and after start will transfer collection data 
/// from the remote database to the local ArangoDB database. It will extract data from the
/// remote database by calling the remote database's *dump* API until all data are fetched.
///
/// The body of the request must be JSON object with the configuration. The
/// following attributes are allowed for the configuration:
///
/// - *endpoint*: the endpoint to connect to (e.g. "tcp://192.168.173.13:8529").
///
/// - *database*: the database name on the master (if not specified, defaults to the
///   name of the local current database).
///
/// - *username*: an optional ArangoDB username to use when connecting to the endpoint.
///
/// - *password*: the password to use when connecting to the endpoint.
///
/// - *includeSystem*: whether or not system collection operations will be applied
///
/// - *restrictType*: an optional string value for collection filtering. When
///    specified, the allowed values are *include* or *exclude*.
///
/// - *restrictCollections*: an optional array of collections for use with
///   *restrictType*. If *restrictType* is *include*, only the specified collections
///    will be sychronised. If *restrictType* is *exclude*, all but the specified
///    collections will be synchronized.
///
/// In case of success, the body of the response is a JSON object with the following
/// attributes:
///
/// - *collections*: an array of collections that were transferred from the endpoint
///
/// - *lastLogTick*: the last log tick on the endpoint at the time the transfer
///   was started. Use this value as the *from* value when starting the continuous
///   synchronization later.
///
/// WARNING: calling this method will sychronise data from the collections found
/// on the remote endpoint to the local ArangoDB database. All data in the local
/// collections will be purged and replaced with data from the endpoint.
///
/// Use with caution!
///
/// **Note**: this method is not supported on a coordinator in a cluster.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the configuration is incomplete or malformed.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred during sychronisation.
///
/// @RESTRETURNCODE{501}
/// is returned when this operation is called on a coordinator in a cluster.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandSync () {
  TRI_json_t* json = parseJsonBody();

  if (json == nullptr) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  const string endpoint = JsonHelper::getStringValue(json, "endpoint", "");
  const string database = JsonHelper::getStringValue(json, "database", _vocbase->_name);
  const string username = JsonHelper::getStringValue(json, "username", "");
  const string password = JsonHelper::getStringValue(json, "password", "");


  if (endpoint.empty()) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, "<endpoint> must be a valid endpoint");
    return;
  }
  
  bool includeSystem = JsonHelper::getBooleanValue(json, "includeSystem", true);

  std::unordered_map<string, bool> restrictCollections;
  TRI_json_t* restriction = JsonHelper::getObjectElement(json, "restrictCollections");

  if (restriction != nullptr && restriction->_type == TRI_JSON_ARRAY) {
    size_t const n = restriction->_value._objects._length;

    for (size_t i = 0; i < n; ++i) {
      TRI_json_t const* cname = static_cast<TRI_json_t const*>(TRI_AtVector(&restriction->_value._objects, i));

      if (JsonHelper::isString(cname)) {
        restrictCollections.insert(pair<string, bool>(string(cname->_value._string.data, cname->_value._string.length - 1), true));
      }
    }
  }

  string restrictType = JsonHelper::getStringValue(json, "restrictType", "");

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if ((restrictType.empty() && ! restrictCollections.empty()) ||
      (! restrictType.empty() && restrictCollections.empty()) ||
      (! restrictType.empty() && restrictType != "include" && restrictType != "exclude")) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, "invalid value for <restrictCollections> or <restrictType>");
    return;
  }

  TRI_replication_applier_configuration_t config;
  TRI_InitConfigurationReplicationApplier(&config);
  config._endpoint = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, endpoint.c_str(), endpoint.size());
  config._database = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, database.c_str(), database.size());
  config._username = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, username.c_str(), username.size());
  config._password = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, password.c_str(), password.size());
  config._includeSystem = includeSystem;

  InitialSyncer syncer(_vocbase, &config, restrictCollections, restrictType, false);
  TRI_DestroyConfigurationReplicationApplier(&config);

  int res = TRI_ERROR_NO_ERROR;
  string errorMsg = "";

  try {
    res = syncer.run(errorMsg);
  }
  catch (...) {
    errorMsg = "caught an exception";
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res, errorMsg);
    return;
  }

  TRI_json_t result;

  TRI_InitObjectJson(TRI_CORE_MEM_ZONE, &result);

  TRI_json_t* jsonCollections = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  if (jsonCollections != nullptr) {
    map<TRI_voc_cid_t, string>::const_iterator it;
    const map<TRI_voc_cid_t, string>& c = syncer.getProcessedCollections();

    for (it = c.begin(); it != c.end(); ++it) {
      const string cidString = StringUtils::itoa((*it).first);

      TRI_json_t* ci = TRI_CreateObjectJson(TRI_CORE_MEM_ZONE, 2);

      if (ci != nullptr) {
        TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE,
                             ci,
                             "id",
                             TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, cidString.c_str(), cidString.size()));

        TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE,
                             ci,
                             "name",
                             TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, (*it).second.c_str(), (*it).second.size()));

        TRI_PushBack3ArrayJson(TRI_CORE_MEM_ZONE, jsonCollections, ci);
      }
    }

    TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &result, "collections", jsonCollections);
  }

  char* tickString = TRI_StringUInt64(syncer.getLastLogTick());
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, &result, "lastLogTick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, tickString, strlen(tickString)));

  generateResult(&result);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_replication_serverID
/// @RESTHEADER{GET /_api/replication/server-id, Return server id}
///
/// @RESTDESCRIPTION
/// Returns the servers id. The id is also returned by other replication API
/// methods, and this method is an easy means of determining a server's id.
///
/// The body of the response is a JSON object with the attribute *serverId*. The
/// server id is returned as a string.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred while assembling the response.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationServerId}
///     var url = "/_api/replication/server-id";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandServerId () {
  TRI_json_t result;

  TRI_InitObjectJson(TRI_CORE_MEM_ZONE, &result);

  const string serverId = StringUtils::itoa(TRI_GetIdServer());
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE,
                       &result,
                       "serverId",
                       TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, serverId.c_str(), serverId.size()));

  generateResult(&result);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_replication_applier
/// @RESTHEADER{GET /_api/replication/applier-config, Return configuration of replication applier}
///
/// @RESTDESCRIPTION
/// Returns the configuration of the replication applier.
///
/// The body of the response is a JSON object with the configuration. The
/// following attributes may be present in the configuration:
///
/// - *endpoint*: the logger server to connect to (e.g. "tcp://192.168.173.13:8529").
///
/// - *database*: the name of the database to connect to (e.g. "_system").
///
/// - *username*: an optional ArangoDB username to use when connecting to the endpoint.
///
/// - *password*: the password to use when connecting to the endpoint.
///
/// - *maxConnectRetries*: the maximum number of connection attempts the applier
///   will make in a row. If the applier cannot establish a connection to the
///   endpoint in this number of attempts, it will stop itself.
///
/// - *connectTimeout*: the timeout (in seconds) when attempting to connect to the
///   endpoint. This value is used for each connection attempt.
///
/// - *requestTimeout*: the timeout (in seconds) for individual requests to the endpoint.
///
/// - *chunkSize*: the requested maximum size for log transfer packets that
///   is used when the endpoint is contacted.
///
/// - *autoStart*: whether or not to auto-start the replication applier on
///   (next and following) server starts
///
/// - *adaptivePolling*: whether or not the replication applier will use
///   adaptive polling.
///
/// - *includeSystem*: whether or not system collection operations will be applied
///
/// - *restrictType*: the configuration for *restrictCollections*
///
/// - *restrictCollections*: the optional array of collections to include or exclude,
///   based on the setting of *restrictType*
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred while assembling the response.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationApplierGetConfig}
///     var url = "/_api/replication/applier-config";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierGetConfig () {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

  TRI_replication_applier_configuration_t config;
  TRI_InitConfigurationReplicationApplier(&config);

  TRI_ReadLockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);
  TRI_CopyConfigurationReplicationApplier(&_vocbase->_replicationApplier->_configuration, &config);
  TRI_ReadUnlockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);

  TRI_json_t* json = TRI_JsonConfigurationReplicationApplier(&config);
  TRI_DestroyConfigurationReplicationApplier(&config);

  if (json == nullptr) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  generateResult(json);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_replication_applier_adjust
/// @RESTHEADER{PUT /_api/replication/applier-config, Adjust configuration of replication applier}
///
/// @RESTBODYPARAM{configuration,json,required}
/// A JSON representation of the configuration.
///
/// @RESTDESCRIPTION
/// Sets the configuration of the replication applier. The configuration can
/// only be changed while the applier is not running. The updated configuration
/// will be saved immediately but only become active with the next start of the
/// applier.
///
/// The body of the request must be JSON object with the configuration. The
/// following attributes are allowed for the configuration:
///
/// - *endpoint*: the logger server to connect to (e.g. "tcp://192.168.173.13:8529").
///   The endpoint must be specified.
///
/// - *database*: the name of the database on the endpoint. If not specified, defaults
///   to the current local database name.
///
/// - *username*: an optional ArangoDB username to use when connecting to the endpoint.
///
/// - *password*: the password to use when connecting to the endpoint.
///
/// - *maxConnectRetries*: the maximum number of connection attempts the applier
///   will make in a row. If the applier cannot establish a connection to the
///   endpoint in this number of attempts, it will stop itself.
///
/// - *connectTimeout*: the timeout (in seconds) when attempting to connect to the
///   endpoint. This value is used for each connection attempt.
///
/// - *requestTimeout*: the timeout (in seconds) for individual requests to the endpoint.
///
/// - *chunkSize*: the requested maximum size for log transfer packets that
///   is used when the endpoint is contacted.
///
/// - *autoStart*: whether or not to auto-start the replication applier on
///   (next and following) server starts
///
/// - *adaptivePolling*: if set to *true*, the replication applier will fall
///   to sleep for an increasingly long period in case the logger server at the
///   endpoint does not have any more replication events to apply. Using
///   adaptive polling is thus useful to reduce the amount of work for both the
///   applier and the logger server for cases when there are only infrequent
///   changes. The downside is that when using adaptive polling, it might take
///   longer for the replication applier to detect that there are new replication
///   events on the logger server.
///
///   Setting *adaptivePolling* to false will make the replication applier
///   contact the logger server in a constant interval, regardless of whether
///   the logger server provides updates frequently or seldomly.
///
/// - *includeSystem*: whether or not system collection operations will be applied
///
/// - *restrictType*: the configuration for *restrictCollections*
///
/// - *restrictCollections*: the optional array of collections to include or exclude,
///   based on the setting of *restrictType*
///
/// In case of success, the body of the response is a JSON object with the updated
/// configuration.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the configuration is incomplete or malformed, or if the
/// replication applier is currently running.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred while assembling the response.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationApplierSetConfig}
///     var re = require("org/arangodb/replication");
///     re.applier.shutdown();
///
///     var url = "/_api/replication/applier-config";
///     var body = {
///       endpoint: "tcp://127.0.0.1:8529",
///       username: "replicationApplier",
///       password: "applier1234@foxx",
///       chunkSize: 4194304,
///       autoStart: false,
///       adaptivePolling: true
///     };
///
///     var response = logCurlRequest('PUT', url, JSON.stringify(body));
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierSetConfig () {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

  TRI_replication_applier_configuration_t config;
  TRI_InitConfigurationReplicationApplier(&config);

  TRI_json_t* json = parseJsonBody();

  if (json == nullptr) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  TRI_ReadLockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);
  TRI_CopyConfigurationReplicationApplier(&_vocbase->_replicationApplier->_configuration, &config);
  TRI_ReadUnlockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);

  TRI_json_t const* value;
  const string endpoint = JsonHelper::getStringValue(json, "endpoint", "");

  if (! endpoint.empty()) {
    if (config._endpoint != nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, config._endpoint);
    }
    config._endpoint = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, endpoint.c_str(), endpoint.size());
  }

  value = JsonHelper::getObjectElement(json, "database");
  if (config._database != nullptr) {
    // free old value
    TRI_FreeString(TRI_CORE_MEM_ZONE, config._database);
  }

  if (JsonHelper::isString(value)) {
    config._database = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, value->_value._string.data, value->_value._string.length - 1);
  }
  else {
    config._database = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, _vocbase->_name);
  }

  value = JsonHelper::getObjectElement(json, "username");
  if (JsonHelper::isString(value)) {
    if (config._username != nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, config._username);
    }
    config._username = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, value->_value._string.data, value->_value._string.length - 1);
  }

  value = JsonHelper::getObjectElement(json, "password");
  if (JsonHelper::isString(value)) {
    if (config._password != nullptr) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, config._password);
    }
    config._password = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, value->_value._string.data, value->_value._string.length - 1);
  }

  config._requestTimeout    = JsonHelper::getNumericValue<double>(json, "requestTimeout", config._requestTimeout);
  config._connectTimeout    = JsonHelper::getNumericValue<double>(json, "connectTimeout", config._connectTimeout);
  config._ignoreErrors      = JsonHelper::getNumericValue<uint64_t>(json, "ignoreErrors", config._ignoreErrors);
  config._maxConnectRetries = JsonHelper::getNumericValue<uint64_t>(json, "maxConnectRetries", config._maxConnectRetries);
  config._sslProtocol       = JsonHelper::getNumericValue<uint32_t>(json, "sslProtocol", config._sslProtocol);
  config._chunkSize         = JsonHelper::getNumericValue<uint64_t>(json, "chunkSize", config._chunkSize);
  config._autoStart         = JsonHelper::getBooleanValue(json, "autoStart", config._autoStart);
  config._adaptivePolling   = JsonHelper::getBooleanValue(json, "adaptivePolling", config._adaptivePolling);
  config._includeSystem     = JsonHelper::getBooleanValue(json, "includeSystem", config._includeSystem);
  config._restrictType      = JsonHelper::getStringValue(json, "restrictType", config._restrictType);

  value = JsonHelper::getObjectElement(json, "restrictCollections");
  if (TRI_IsArrayJson(value)) {
    config._restrictCollections.clear();
    size_t const n = TRI_LengthArrayJson(value);
    for (size_t i = 0; i < n; ++i) {
      TRI_json_t const* collection = TRI_LookupArrayJson(value, i);
      if (TRI_IsStringJson(collection)) {
        config._restrictCollections.emplace(std::make_pair(std::string(collection->_value._string.data), true));
      }
    }
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  int res = TRI_ConfigureReplicationApplier(_vocbase->_replicationApplier, &config);

  TRI_DestroyConfigurationReplicationApplier(&config);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION ||
        res == TRI_ERROR_REPLICATION_RUNNING) {
      generateError(HttpResponse::BAD, res);
    }
    else {
      generateError(HttpResponse::SERVER_ERROR, res);
    }
    return;
  }

  handleCommandApplierGetConfig();
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_replication_applier_start
/// @RESTHEADER{PUT /_api/replication/applier-start, Start replication applier}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{from,string,optional}
/// The remote *lastLogTick* value from which to start applying. If not specified,
/// the last saved tick from the previous applier run is used. If there is no
/// previous applier state saved, the applier will start at the beginning of the
/// logger server's log.
///
/// @RESTDESCRIPTION
/// Starts the replication applier. This will return immediately if the
/// replication applier is already running.
///
/// If the replication applier is not already running, the applier configuration
/// will be checked, and if it is complete, the applier will be started in a
/// background thread. This means that even if the applier will encounter any
/// errors while running, they will not be reported in the response to this
/// method.
///
/// To detect replication applier errors after the applier was started, use the
/// */_api/replication/applier-state* API instead.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the replication applier is not fully configured or the
/// configuration is invalid.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred while assembling the response.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationApplierStart}
///     var re = require("org/arangodb/replication");
///     re.applier.shutdown();
///     re.applier.properties({
///       endpoint: "tcp://127.0.0.1:8529",
///       username: "replicationApplier",
///       password: "applier1234@foxx",
///       autoStart: false,
///       adaptivePolling: true
///     });
///
///     var url = "/_api/replication/applier-start";
///     var response = logCurlRequest('PUT', url, "");
///     re.applier.shutdown();
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierStart () {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

  bool found;
  const char* value = _request->value("from", found);

  TRI_voc_tick_t initialTick = 0;
  if (found) {
    // url parameter "from" specified
    initialTick = (TRI_voc_tick_t) StringUtils::uint64(value);
  }

  int res = TRI_StartReplicationApplier(_vocbase->_replicationApplier,
                                        initialTick,
                                        found);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION ||
        res == TRI_ERROR_REPLICATION_RUNNING) {
      generateError(HttpResponse::BAD, res);
    }
    else {
      generateError(HttpResponse::SERVER_ERROR, res);
    }
    return;
  }

  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_replication_applier_stop
/// @RESTHEADER{PUT /_api/replication/applier-stop, Stop replication applier}
///
/// @RESTDESCRIPTION
/// Stops the replication applier. This will return immediately if the
/// replication applier is not running.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred while assembling the response.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationApplierStop}
///     var re = require("org/arangodb/replication");
///     re.applier.shutdown();
///     re.applier.properties({
///       endpoint: "tcp://127.0.0.1:8529",
///       username: "replicationApplier",
///       password: "applier1234@foxx",
///       autoStart: false,
///       adaptivePolling: true
///     });
///
///     re.applier.start();
///     var url = "/_api/replication/applier-stop";
///     var response = logCurlRequest('PUT', url, "");
///     re.applier.shutdown();
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierStop () {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

  int res = TRI_StopReplicationApplier(_vocbase->_replicationApplier, true);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
    return;
  }

  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_replication_applier_state
/// @RESTHEADER{GET /_api/replication/applier-state, State of the replication applier}
///
/// @RESTDESCRIPTION
/// Returns the state of the replication applier, regardless of whether the
/// applier is currently running or not.
///
/// The response is a JSON object with the following attributes:
///
/// - *state*: a JSON object with the following sub-attributes:
///
///   - *running*: whether or not the applier is active and running
///
///   - *lastAppliedContinuousTick*: the last tick value from the continuous
///     replication log the applier has applied.
///
///   - *lastProcessedContinuousTick*: the last tick value from the continuous
///     replication log the applier has processed.
///
///     Regularly, the last applied and last processed tick values should be
///     identical. For transactional operations, the replication applier will first
///     process incoming log events before applying them, so the processed tick
///     value might be higher than the applied tick value. This will be the case
///     until the applier encounters the *transaction commit* log event for the
///     transaction.
///
///   - *lastAvailableContinuousTick*: the last tick value the logger server can
///     provide.
///
///   - *time*: the time on the applier server.
///
///   - *totalRequests*: the total number of requests the applier has made to the
///     endpoint.
///
///   - *totalFailedConnects*: the total number of failed connection attempts the
///     applier has made.
///
///   - *totalEvents*: the total number of log events the applier has processed.
///
///   - *totalOperationsExcluded*: the total number of log events excluded because
///     of *restrictCollections*.
///
///   - *progress*: a JSON object with details about the replication applier progress.
///     It contains the following sub-attributes if there is progress to report:
///
///     - *message*: a textual description of the progress
///
///     - *time*: the date and time the progress was logged
///
///     - *failedConnects*: the current number of failed connection attempts
///
///   - *lastError*: a JSON object with details about the last error that happened on
///     the applier. It contains the following sub-attributes if there was an error:
///
///     - *errorNum*: a numerical error code
///
///     - *errorMessage*: a textual error description
///
///     - *time*: the date and time the error occurred
///
///     In case no error has occurred, *lastError* will be empty.
///
/// - *server*: a JSON object with the following sub-attributes:
///
///   - *version*: the applier server's version
///
///   - *serverId*: the applier server's id
///
/// - *endpoint*: the endpoint the applier is connected to (if applier is
///   active) or will connect to (if applier is currently inactive)
///
/// - *database*: the name of the database the applier is connected to (if applier is
///   active) or will connect to (if applier is currently inactive)
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred while assembling the response.
///
/// @EXAMPLES
///
/// Fetching the state of an inactive applier:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationApplierStateNotRunning}
///     var re = require("org/arangodb/replication");
///     re.applier.shutdown();
///
///     var url = "/_api/replication/applier-state";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Fetching the state of an active applier:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationApplierStateRunning}
///     var re = require("org/arangodb/replication");
///     re.applier.shutdown();
///     re.applier.start();
///
///     var url = "/_api/replication/applier-state";
///     var response = logCurlRequest('GET', url);
///     re.applier.shutdown();
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierGetState () {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

  TRI_json_t* json = TRI_JsonReplicationApplier(_vocbase->_replicationApplier);

  if (json == nullptr) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  generateResult(json);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete the state of the replication applier
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierDeleteState () {
  TRI_ASSERT(_vocbase->_replicationApplier != nullptr);

  int res = TRI_ForgetReplicationApplier(_vocbase->_replicationApplier);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
    return;
  }

  handleCommandApplierGetState();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
