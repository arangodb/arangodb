////////////////////////////////////////////////////////////////////////////////
/// @brief replication request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestReplicationHandler.h"

#include "build.h"
#include "Basics/JsonHelper.h"
#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "Logger/Logger.h"
#include "HttpServer/HttpServer.h"
#include "Replication/InitialSyncer.h"
#include "Rest/HttpRequest.h"
#include "VocBase/replication-applier.h"
#include "VocBase/replication-dump.h"
#include "VocBase/replication-logger.h"
#include "VocBase/server-id.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

  
const uint64_t RestReplicationHandler::minChunkSize = 512 * 1024;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestReplicationHandler::RestReplicationHandler (HttpRequest* request, 
                                                TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestReplicationHandler::~RestReplicationHandler () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestReplicationHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestReplicationHandler::queue () const {
  static string const client = "STANDARD";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Handler::status_e RestReplicationHandler::execute() {
  // extract the request type
  const HttpRequest::HttpRequestType type = _request->requestType();
  
  vector<string> const& suffix = _request->suffix();

  const size_t len = suffix.size();

  if (len == 1) {
    const string& command = suffix[0];

    if (command == "logger-start") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }
      handleCommandLoggerStart();
    }
    else if (command == "logger-stop") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }
      handleCommandLoggerStop();
    }
    else if (command == "logger-state") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandLoggerState();
    }
    else if (command == "logger-config") {
      if (type == HttpRequest::HTTP_REQUEST_GET) {
        handleCommandLoggerGetConfig();
      }
      else {
        if (type != HttpRequest::HTTP_REQUEST_PUT) {
          goto BAD_CALL;
        }
        handleCommandLoggerSetConfig();
      }
    }
    else if (command == "logger-follow") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandLoggerFollow(); 
    }
    else if (command == "inventory") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandInventory();
    }
    else if (command == "dump") {
      if (type != HttpRequest::HTTP_REQUEST_GET) {
        goto BAD_CALL;
      }
      handleCommandDump(); 
    }
    else if (command == "sync") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
      }
      handleCommandSync(); 
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
      handleCommandApplierStart();
    }
    else if (command == "applier-stop") {
      if (type != HttpRequest::HTTP_REQUEST_PUT) {
        goto BAD_CALL;
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
    else {
      generateError(HttpResponse::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid command");
    }
      
    return Handler::HANDLER_DONE;
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
  
  return Handler::HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             public static methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

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

  if (TRI_ExcludeCollectionReplication(collection->_name)) {
    // collection is excluded
    return false;
  }

  // all other cases should be included
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief insert the applier action into an action list
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::insertClient () {
  bool found;
  char const* value;

  value = _request->value("serverId", found);

  if (found) {
    TRI_server_id_t serverId = (TRI_server_id_t) StringUtils::uint64(value);

    if (serverId > 0) {
      TRI_UpdateClientReplicationLogger(_vocbase->_replicationLogger, serverId, _request->fullUrl().c_str());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the minimum chunk size
////////////////////////////////////////////////////////////////////////////////
  
uint64_t RestReplicationHandler::determineChunkSize () const {
  // determine chunk size
  uint64_t chunkSize = minChunkSize;

  bool found;
  const char* value = _request->value("chunkSize", found);

  if (found) {
    // url parameter "chunkSize" specified
    chunkSize = (uint64_t) StringUtils::uint64(value);
  }
  else {
    // not specified, use default
    chunkSize = minChunkSize;
  }

  return chunkSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the replication logger
///
/// @RESTHEADER{PUT /_api/replication/logger-start,starts the replication logger}
///
/// @RESTDESCRIPTION
/// Starts the server's replication logger. Will do nothing if the replication
/// logger is already running.
///
/// The body of the response contains a JSON object with the following
/// attributes:
///
/// - `running`: will contain `true`
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the logger was started successfully, or was already running.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if the logger could not be started.
///
/// @EXAMPLES
///
/// Starts the replication logger.
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerStart}
///     var url = "/_api/replication/logger-start";
///
///     var response = logCurlRequest('PUT', url, "");
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerStart () {
  assert(_vocbase->_replicationLogger != 0);

  int res = TRI_StartReplicationLogger(_vocbase->_replicationLogger);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
    return;
  }

  TRI_json_t result;
    
  TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "running", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, true));

  generateResult(&result);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the replication logger
///
/// @RESTHEADER{PUT /_api/replication/logger-stop,stops the replication logger}
///
/// @RESTDESCRIPTION
/// Stops the server's replication logger. Will do nothing if the replication
/// logger is not running.
///
/// The body of the response contains a JSON object with the following
/// attributes:
///
/// - `running`: will contain `false`
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the logger was stopped successfully, or was not running
/// before.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if the logger could not be stopped.
///
/// @EXAMPLES
///
/// Starts the replication logger.
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerStop}
///     var url = "/_api/replication/logger-stop";
///
///     var response = logCurlRequest('PUT', url, "");
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerStop () {
  assert(_vocbase->_replicationLogger != 0);

  int res = TRI_StopReplicationLogger(_vocbase->_replicationLogger);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
    return;
  }

  TRI_json_t result;
    
  TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "running", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, false));

  generateResult(&result);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the state of the replication logger
///
/// @RESTHEADER{GET /_api/replication/logger-state,returns the replication logger state}
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
/// - `state`: the current logger state as a JSON hash array with the following
///   sub-attributes:
///
///   - `running`: whether or not the logger is running
///
///   - `lastLogTick`: the tick value of the latest tick the logger has logged. 
///     This value can be used for incremental fetching of log data.
///
///   - `time`: the current date and time on the logger server
///
/// - `server`: a JSON hash with the following sub-attributes:
///
///   - `version`: the logger server's version
///
///   - `serverId`: the logger server's id
///
/// - `clients`: a list of all replication clients that ever connected to
///   the logger since it was started. This list can be used to determine 
///   approximately how much data the individual clients have already fetched 
///   from the logger server.
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
/// Returns the state of an inactive replication logger.
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerStateInactive}
///     var re = require("org/arangodb/replication");
///     re.logger.stop();
///
///     var url = "/_api/replication/logger-state";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Returns the state of an active replication logger.
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
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerState () {
  assert(_vocbase->_replicationLogger != 0);

  TRI_json_t* json = TRI_JsonReplicationLogger(_vocbase->_replicationLogger);

  if (json == 0) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
  
  generateResult(json);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the configuration of the replication logger
///
/// @RESTHEADER{GET /_api/replication/logger-config,returns the configuration of the replication logger}
///
/// @RESTDESCRIPTION
/// Returns the configuration of the replication logger.
///
/// The body of the response is a JSON hash with the configuration. The
/// following attributes may be present in the configuration:
///
/// - `logRemoteChanges`: whether or not externally created changes should be
///    logged by the local logger
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
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerGetConfig}
///     var url = "/_api/replication/logger-config";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerGetConfig () {
  assert(_vocbase->_replicationLogger != 0);
    
  TRI_replication_logger_configuration_t config;
    
  TRI_ReadLockReadWriteLock(&_vocbase->_replicationLogger->_statusLock);
  TRI_CopyConfigurationReplicationLogger(&_vocbase->_replicationLogger->_configuration, &config);
  TRI_ReadUnlockReadWriteLock(&_vocbase->_replicationLogger->_statusLock);
  
  TRI_json_t* json = TRI_JsonConfigurationReplicationLogger(&config);
    
  if (json == 0) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
    
  generateResult(json);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the configuration of the replication logger
///
/// @RESTHEADER{PUT /_api/replication/logger-config,adjusts the configuration of the replication logger}
///
/// @RESTBODYPARAM{configuration,json,required}
/// A JSON representation of the configuration.
///
/// @RESTDESCRIPTION
/// Sets the configuration of the replication logger. 
///
/// The body of the request must be JSON hash with the configuration. The
/// following attributes are allowed for the configuration:
///
/// - `logRemoteChanges`: whether or not externally created changes should be
///    logged by the local logger
///
/// In case of success, the body of the response is a JSON hash with the updated
/// configuration.
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
/// is returned if an error occurred while assembling the response.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerSetConfig}
///     var re = require("org/arangodb/replication");
///     re.applier.stop();
///
///     var url = "/_api/replication/logger-config";
///     var body = { 
///       logRemoteChanges: true
///     };
///
///     var response = logCurlRequest('PUT', url, JSON.stringify(body));
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerSetConfig () {
  assert(_vocbase->_replicationLogger != 0);
  
  TRI_replication_logger_configuration_t config;
  // copy previous config
  TRI_ReadLockReadWriteLock(&_vocbase->_replicationLogger->_statusLock);
  TRI_CopyConfigurationReplicationLogger(&_vocbase->_replicationLogger->_configuration, &config);
  TRI_ReadUnlockReadWriteLock(&_vocbase->_replicationLogger->_statusLock);
  
  TRI_json_t* json = parseJsonBody();

  if (json == 0) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  TRI_json_t const* value;

  value = JsonHelper::getArrayElement(json, "logRemoteChanges");
  if (JsonHelper::isBoolean(value)) {
    config._logRemoteChanges = value->_value._boolean;
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  int res = TRI_ConfigureReplicationLogger(_vocbase->_replicationLogger, &config);
  
  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_REPLICATION_INVALID_CONFIGURATION) {
      generateError(HttpResponse::BAD, res);
    }
    else {
      generateError(HttpResponse::SERVER_ERROR, res);
    }
    return;
  }
 
  handleCommandLoggerGetConfig();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns ranged data from the replication log
///
/// @RESTHEADER{GET /_api/replication/logger-follow,returns recent log entries from the replication log}
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
/// @RESTDESCRIPTION
/// Returns data from the server's replication log. This method can be called
/// by replication clients after an initial synchronisation of data. The method 
/// will return all "recent" log entries from the logger server, and the clients
/// can replay and apply these entries locally so they get to the same data
/// state as the logger server.
///
/// Clients can call this method repeatedly to incrementally fetch all changes
/// from the logger server. In this case, they should provide the `from` value so
/// they will only get returned the log events since their last fetch.
///
/// When the `from` URL parameter is not used, the logger server will return log 
/// entries starting at the beginning of its replication log. When the `from`
/// parameter is used, the logger server will only return log entries which have 
/// higher tick values than the specified `from` value (note: the log entry with a
/// tick value equal to `from` will be excluded). Use the `from` value when
/// incrementally fetching log data.
///
/// The `to` URL parameter can be used to optionally restrict the upper bound of
/// the result to a certain tick value. If used, the result will contain only log events
/// with tick values up to (including) `to`. In incremental fetching, there is no
/// need to use the `to` parameter. It only makes sense in special situations, 
/// when only parts of the change log are required.
///
/// The `chunkSize` URL parameter can be used to control the size of the result. 
/// It must be specified in bytes. The `chunkSize` value will only be honored 
/// approximately. Otherwise a too low `chunkSize` value could cause the server 
/// to not be able to put just one log entry into the result and return it. 
/// Therefore, the `chunkSize` value will only be consulted after a log entry has
/// been written into the result. If the result size is then bigger than 
/// `chunkSize`, the server will respond with as many log entries as there are
/// in the response already. If the result size is still smaller than `chunkSize`,
/// the server will try to return more data if there's more data left to return.
///
/// If `chunkSize` is not specified, some server-side default value will be used.
///
/// The `Content-Type` of the result is `application/x-arango-dump`. This is an 
/// easy-to-process format, with all log events going onto separate lines in the
/// response body. Each log event itself is a JSON hash, with at least the 
/// following attributes:
///
/// - `tick`: the log event tick value
///
/// - `type`: the log event type
///
/// Individual log events will also have additional attributes, depending on the
/// event type. A few common attributes which are used for multiple events types
/// are:
///
/// - `cid`: id of the collection the event was for
///
/// - `tid`: id of the transaction the event was contained in
/// 
/// - `key`: document key
///
/// - `rev`: document revision id
///
/// - `data`: the original document data
/// 
/// A more detailed description of the individual replication event types and their 
/// data structures can be found in @ref RefManualReplicationEventTypes.
///
/// The response will also contain the following HTTP headers:
///
/// - `x-arango-replication-active`: whether or not the logger is active. Clients
///   can use this flag as an indication for their polling frequency. If the 
///   logger is not active and there are no more replication events available, it
///   might be sensible for a client to abort, or to go to sleep for a long time
///   and try again later to check whether the logger has been activated.
///
/// - `x-arango-replication-lastincluded`: the tick value of the last included
///   value in the result. In incremental log fetching, this value can be used 
///   as the `from` value for the following request. Note that if the result is
///   empty, the value will be `0`. This value should not be used as `from` value
///   by clients in the next request (otherwise the server would return the log
///   events from the start of the log again).
///
/// - `x-arango-replication-lasttick`: the last tick value the logger server has
///   logged (not necessarily included in the result). By comparing the the last
///   tick and last included tick values, clients have an approximate indication of
///   how many events there are still left to fetch.
///
/// - `x-arango-replication-checkmore`: whether or not there already exists more
///   log data which the client could fetch immediately. If there is more log data
///   available, the client could call `logger-follow` again with an adjusted `from`
///   value to fetch remaining log entries until there are no more.
///
///   If there isn't any more log data to fetch, the client might decide to go
///   to sleep for a while before calling the logger again. 
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
/// is returned if either the `from` or `to` values are invalid.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @RESTRETURNCODE{500}
/// is returned if an error occurred while assembling the response.
///
/// @EXAMPLES
///
/// No log events available:
///
/// @EXAMPLE_ARANGOSH_RUN{RestReplicationLoggerFollowEmpty}
///     var re = require("org/arangodb/replication");
///     re.logger.start();
///     var lastTick = re.logger.state().state.lastLogTick;
///
///     var url = "/_api/replication/logger-follow?from=" + lastTick;
///     var response = logCurlRequest('GET', url);
///
///     re.logger.stop();
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
///     re.logger.start();
///     var lastTick = re.logger.state().state.lastLogTick;
///
///     db._create("products"); 
///     db.products.save({ "_key": "p1", "name" : "flux compensator" });
///     db.products.save({ "_key": "p2", "name" : "hybrid hovercraft", "hp" : 5100 });
///     db.products.remove("p1");
///     db.products.update("p2", { "name" : "broken hovercraft" });
///     db.products.drop();
///
///     var url = "/_api/replication/logger-follow?from=" + lastTick;
///     var response = logCurlRequest('GET', url);
///
///     re.logger.stop();
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
///     re.logger.start();
///     var lastTick = re.logger.state().state.lastLogTick;
///
///     db._create("products"); 
///     db.products.save({ "_key": "p1", "name" : "flux compensator" });
///     db.products.save({ "_key": "p2", "name" : "hybrid hovercraft", "hp" : 5100 });
///     db.products.remove("p1");
///     db.products.update("p2", { "name" : "broken hovercraft" });
///     db.products.drop();
///
///     var url = "/_api/replication/logger-follow?from=" + lastTick + "&chunkSize=400";
///     var response = logCurlRequest('GET', url);
///
///     re.logger.stop();
///     assert(response.code === 200);
///
///     logRawResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandLoggerFollow () {
  // determine start tick
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd   = (TRI_voc_tick_t) UINT64_MAX;
  bool found;
  char const* value;
  
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
  
  const uint64_t chunkSize = determineChunkSize(); 
  
  // initialise the dump container
  TRI_replication_dump_t dump; 
  TRI_InitDumpReplication(&dump);
  dump._buffer = TRI_CreateSizedStringBuffer(TRI_CORE_MEM_ZONE, (size_t) minChunkSize);

  if (dump._buffer == 0) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  int res = TRI_DumpLogReplication(_vocbase, &dump, tickStart, tickEnd, chunkSize);
  
  TRI_replication_logger_state_t state;

  if (res == TRI_ERROR_NO_ERROR) {
    res = TRI_StateReplicationLogger(_vocbase->_replicationLogger, &state);
  }
 
  if (res == TRI_ERROR_NO_ERROR) { 
    const bool checkMore = (dump._lastFoundTick > 0 && dump._lastFoundTick != state._lastLogTick);

    // generate the result
    const size_t length = TRI_LengthStringBuffer(dump._buffer);

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
                         StringUtils::itoa(state._lastLogTick));
    
    _response->setHeader(TRI_REPLICATION_HEADER_ACTIVE, 
                         strlen(TRI_REPLICATION_HEADER_ACTIVE), 
                         state._active ? "true" : "false");

    if (length > 0) {
      // transfer ownership of the buffer contents
      _response->body().appendText(TRI_BeginStringBuffer(dump._buffer), length);
      // avoid double freeing
      TRI_StealStringBuffer(dump._buffer);
    }
    
    insertClient();
  }
  else {
    generateError(HttpResponse::SERVER_ERROR, res);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, dump._buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server inventory
///
/// @RESTHEADER{GET /_api/replication/inventory,returns an inventory of collections and indexes}
///
/// @RESTDESCRIPTION
/// Returns the list of collections and indexes available on the server. This
/// list can be used by replication clients to initiate an initial sync with the
/// server.
///
/// The response will contain a JSON hash array with the `collection` and `state`
/// attributes.
///
/// `collections` is a list of collections with the following sub-attributes:
///
/// - `parameters`: the collection properties
///
/// - `indexes`: a list of the indexes of a the collection. Primary indexes and edges indexes
///    are not included in this list.
///
/// The `state` attribute contains the current state of the replication logger. It
/// contains the following sub-attributes:
///
/// - `running`: whether or not the replication logger is currently active
///
/// - `lastLogTick`: the value of the last tick the replication logger has written  
///
/// - `time`: the current time on the server
///
/// Replication clients should note the `lastLogTick` value returned. They can then
/// fetch collections' data using the dump method up to the value of lastLogTick, and
/// query the continuous replication log for log events after this tick value.
///
/// To create a full copy of the collections on the logger server, a replication client
/// can execute these steps:
///
/// - call the `/inventory` API method. This returns the `lastLogTick` value and the 
///   list of collections and indexes from the logger server.
///
/// - for each collection returned by `/inventory`, create the collection locally and 
///   call `/dump` to stream the collection data to the client, up to the value of 
///   `lastLogTick`. 
///   After that, the client can create the indexes on the collections as they were 
///   reported by `/inventory`.
///
/// If the clients wants to continuously stream replication log events from the logger
/// server, the following additional steps need to be carried out:
///
/// - the client should call `/logger-follow` initially to fetch the first batch of 
///   replication events that were logged after the client's call to `/inventory`.
/// 
///   The call to `/logger-follow` should use a `from` parameter with the value of the
///   `lastLogTick` as reported by `/inventory`. The call to `/logger-follow` will return the 
///   `x-arango-replication-lastincluded` which will contain the last tick value included 
///   in the response.
///
/// - the client can then continuously call `/logger-follow` to incrementally fetch new 
///   replication events that occurred after the last transfer.
///
///   Calls should use a `from` parameter with the value of the `x-arango-replication-lastincluded`
///   header of the previous response. If there are no more replication events, the 
///   response will be empty and clients can go to sleep for a while and try again
///   later.  
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
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandInventory () {
  assert(_vocbase->_replicationLogger != 0);

  TRI_voc_tick_t tick = TRI_CurrentTickVocBase();
  
  // collections
  TRI_json_t* collections = TRI_InventoryCollectionsVocBase(_vocbase, tick, &filterCollection, NULL);

  if (collections == 0) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }

  TRI_replication_logger_state_t state;

  int res = TRI_StateReplicationLogger(_vocbase->_replicationLogger, &state);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_CORE_MEM_ZONE, collections);

    generateError(HttpResponse::SERVER_ERROR, res);
    return;
  }
    
  TRI_json_t json;
  
  TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &json);

  // add collections data
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &json, "collections", collections);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &json, "state", TRI_JsonStateReplicationLogger(&state)); 
  
  generateResult(&json);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &json);
    
  insertClient();
}
    
////////////////////////////////////////////////////////////////////////////////
/// @brief dumps the data of a collection
///
/// @RESTHEADER{GET /_api/replication/dump,returns the data of a collection}
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
/// @RESTDESCRIPTION
/// Returns the data from the collection for the requested range.
///
/// When the `from` URL parameter is not used, collection events are returned from
/// the beginning. When the `from` parameter is used, the result will only contain
/// collection entries which have higher tick values than the specified `from` value 
/// (note: the log entry with a tick value equal to `from` will be excluded). 
///
/// The `to` URL parameter can be used to optionally restrict the upper bound of
/// the result to a certain tick value. If used, the result will only contain
/// collection entries with tick values up to (including) `to`. 
///
/// The `chunkSize` URL parameter can be used to control the size of the result. 
/// It must be specified in bytes. The `chunkSize` value will only be honored 
/// approximately. Otherwise a too low `chunkSize` value could cause the server 
/// to not be able to put just one entry into the result and return it. 
/// Therefore, the `chunkSize` value will only be consulted after an entry has
/// been written into the result. If the result size is then bigger than 
/// `chunkSize`, the server will respond with as many entries as there are
/// in the response already. If the result size is still smaller than `chunkSize`,
/// the server will try to return more data if there's more data left to return.
///
/// If `chunkSize` is not specified, some server-side default value will be used.
///
/// The `Content-Type` of the result is `application/x-arango-dump`. This is an 
/// easy-to-process format, with all entries going onto separate lines in the
/// response body. 
///
/// Each line itself is a JSON hash, with at least the following attributes:
///
/// - `type`: the type of entry. Possible values for `type` are:
///
///   - 2300: document insertion/update
///
///   - 2301: edge insertion/update
///
///   - 2302: document/edge deletion
///
/// - `key`: the key of the document/edge or the key used in the deletion operation
///
/// - `rev`: the revision id of the document/edge or the deletion operation
///
/// - `data`: the actual document/edge data for types 2300 and 2301. The full
///   document/edge data will be returned even for updates.
///
/// A more detailed description of the different entry types and their 
/// data structures can be found in @ref RefManualReplicationEventTypes.
///
/// Note: there will be no distinction between inserts and updates when calling this method.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the request was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if either the `from` or `to` values are invalid.
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
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandDump () {
  char const* collection = _request->value("collection");
    
  if (collection == 0) {
    generateError(HttpResponse::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid collection parameter");
    return;
  }
  
  // determine start tick for dump
  TRI_voc_tick_t tickStart = 0;
  TRI_voc_tick_t tickEnd   = (TRI_voc_tick_t) UINT64_MAX;
  bool found;
  char const* value;
  
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
  
  const uint64_t chunkSize = determineChunkSize(); 

  TRI_vocbase_col_t* c = TRI_LookupCollectionByNameVocBase(_vocbase, collection);

  if (c == 0) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }

  const TRI_voc_cid_t cid = c->_cid;

  LOGGER_DEBUG("request collection dump for collection '" << collection << "', "
               "tickStart: " << tickStart << ", tickEnd: " << tickEnd);

  TRI_vocbase_col_t* col = TRI_UseCollectionByIdVocBase(_vocbase, cid);

  if (col == 0) {
    generateError(HttpResponse::NOT_FOUND, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return;
  }
  

  // initialise the dump container
  TRI_replication_dump_t dump; 
  TRI_InitDumpReplication(&dump);
  dump._buffer = TRI_CreateSizedStringBuffer(TRI_CORE_MEM_ZONE, (size_t) minChunkSize);

  if (dump._buffer == 0) {
    TRI_ReleaseCollectionVocBase(_vocbase, col);
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);

    return;
  }

  int res = TRI_DumpCollectionReplication(&dump, col, tickStart, tickEnd, chunkSize);
  
  TRI_ReleaseCollectionVocBase(_vocbase, col);

  if (res == TRI_ERROR_NO_ERROR) {
    // generate the result
    const size_t length = TRI_LengthStringBuffer(dump._buffer);

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
    _response->body().appendText(TRI_BeginStringBuffer(dump._buffer), length);
    // avoid double freeing
    TRI_StealStringBuffer(dump._buffer);
    
    insertClient();
  }
  else {
    generateError(HttpResponse::SERVER_ERROR, res);
  }

  TRI_FreeStringBuffer(TRI_CORE_MEM_ZONE, dump._buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief synchronises data from a remote endpoint
///
/// @RESTHEADER{PUT /_api/replication/sync,synchronises data from a remote endpoint}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTBODYPARAM{configuration,json,required}
/// A JSON representation of the configuration.
///
/// @RESTDESCRIPTION
/// Starts a full data synchronisation from a remote endpoint into the local
/// ArangoDB database.
///
/// The body of the request must be JSON hash with the configuration. The
/// following attributes are allowed for the configuration:
///
/// - `endpoint`: the endpoint to connect to (e.g. "tcp://192.168.173.13:8529").
///
/// - `username`: an optional ArangoDB username to use when connecting to the endpoint.
///
/// - `password`: the password to use when connecting to the endpoint.
///
/// - `restrictType`: an optional string value for collection filtering. When
///    specified, the allowed values are `include` or `exclude`.
///
/// - `restrictCollections`: an optional list of collections for use with 
///   `restrictType`. If `restrictType` is `include`, only the specified collections
///    will be sychronised. If `restrictType` is `exclude`, all but the specified
///    collections will be synchronised.
///
/// In case of success, the body of the response is a JSON hash with the following
/// attributes: 
///
/// - `collections`: a list of collections that were transferred from the endpoint 
///
/// - `lastLogTick`: the last log tick on the endpoint at the time the transfer
///   was started. Use this value as the `from` value when starting the continuous 
///   synchronisation later.
///
/// WARNING: calling this method will sychronise data from the collections found
/// on the remote endpoint to the local ArangoDB database. All data in the local
/// collections will be purged and replaced with data from the endpoint. 
///
/// Use with caution!
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
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandSync () {
  TRI_json_t* json = parseJsonBody();

  if (json == 0) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }
  
  const string endpoint = JsonHelper::getStringValue(json, "endpoint", "");
  const string username = JsonHelper::getStringValue(json, "username", "");
  const string password = JsonHelper::getStringValue(json, "password", "");
  

  if (endpoint.empty()) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, "<endpoint> must be a valid endpoint");
    return;
  }

  map<string, bool> restrictCollections;
  TRI_json_t* restriction = JsonHelper::getArrayElement(json, "restrictCollections");

  if (restriction != 0 && restriction->_type == TRI_JSON_LIST) {
    size_t i;
    const size_t n = restriction->_value._objects._length;

    for (i = 0; i < n; ++i) {
      TRI_json_t const* cname = (TRI_json_t const*) TRI_AtVector(&restriction->_value._objects, i);

      if (JsonHelper::isString(cname)) {
        restrictCollections.insert(pair<string, bool>(string(cname->_value._string.data, cname->_value._string.length - 1), true)); 
      }
    }
  }

  string restrictType = JsonHelper::getStringValue(json, "restrictType", "");
  
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if ((restrictType.empty() && restrictCollections.size() > 0) ||
      (! restrictType.empty() && restrictCollections.size() == 0) ||
      (! restrictType.empty() && restrictType != "include" && restrictType != "exclude")) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, "invalid value for <restrictCollections> or <restrictType>");
    return;
  }
  
  TRI_replication_applier_configuration_t config;
  TRI_InitConfigurationReplicationApplier(&config);
  config._endpoint = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, endpoint.c_str(), endpoint.size());
  config._username = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, username.c_str(), username.size());
  config._password = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, password.c_str(), password.size());

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
    
  TRI_InitArrayJson(TRI_CORE_MEM_ZONE, &result);

  TRI_json_t* jsonCollections = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

  if (jsonCollections != 0) {
    map<TRI_voc_cid_t, string>::const_iterator it;
    const map<TRI_voc_cid_t, string>& c = syncer.getProcessedCollections();

    for (it = c.begin(); it != c.end(); ++it) {
      const string cidString = StringUtils::itoa((*it).first);

      TRI_json_t* ci = TRI_CreateArray2Json(TRI_CORE_MEM_ZONE, 2);

      if (ci != 0) {
        TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, 
                             ci, 
                             "id",
                             TRI_CreateString2CopyJson(TRI_CORE_MEM_ZONE, cidString.c_str(), cidString.size()));

        TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE,
                             ci,
                             "name",
                             TRI_CreateString2CopyJson(TRI_CORE_MEM_ZONE, (*it).second.c_str(), (*it).second.size()));
      
        TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, jsonCollections, ci);
      }
    }
  
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "collections", jsonCollections);
  }

  char* tickString = TRI_StringUInt64(syncer.getLastLogTick());
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, &result, "lastLogTick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, tickString));

  generateResult(&result);
  TRI_DestroyJson(TRI_CORE_MEM_ZONE, &result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the configuration of the replication applier
///
/// @RESTHEADER{GET /_api/replication/applier-config,returns the configuration of the replication applier}
///
/// @RESTDESCRIPTION
/// Returns the configuration of the replication applier.
///
/// The body of the response is a JSON hash with the configuration. The
/// following attributes may be present in the configuration:
///
/// - `endpoint`: the logger server to connect to (e.g. "tcp://192.168.173.13:8529").
///
/// - `username`: an optional ArangoDB username to use when connecting to the endpoint.
///
/// - `password`: the password to use when connecting to the endpoint.
///
/// - `connectTimeout`: the timeout (in seconds) when connecting to the endpoint.
///
/// - `requestTimeout`: the timeout (in seconds) for individual requests to the endpoint.
///
/// - `autoStart`: whether or not to auto-start the replication applier on
///   (next and following) server starts
///
/// - `adaptivePolling`: whether or not the replication applier will use
///   adaptive polling.
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
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierGetConfig () {
  assert(_vocbase->_replicationApplier != 0);
    
  TRI_replication_applier_configuration_t config;
  TRI_InitConfigurationReplicationApplier(&config);
    
  TRI_ReadLockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);
  TRI_CopyConfigurationReplicationApplier(&_vocbase->_replicationApplier->_configuration, &config);
  TRI_ReadUnlockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);
  
  TRI_json_t* json = TRI_JsonConfigurationReplicationApplier(&config);
  TRI_DestroyConfigurationReplicationApplier(&config);
    
  if (json == 0) {
    generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);
    return;
  }
    
  generateResult(json);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the configuration of the replication applier
///
/// @RESTHEADER{PUT /_api/replication/applier-config,adjusts the configuration of the replication applier}
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
/// The body of the request must be JSON hash with the configuration. The
/// following attributes are allowed for the configuration:
///
/// - `endpoint`: the logger server to connect to (e.g. "tcp://192.168.173.13:8529").
///   The endpoint must be specified.
///
/// - `username`: an optional ArangoDB username to use when connecting to the endpoint.
///
/// - `password`: the password to use when connecting to the endpoint.
///
/// - `connectTimeout`: the timeout (in seconds) when connecting to the endpoint.
///
/// - `requestTimeout`: the timeout (in seconds) for individual requests to the endpoint.
///
/// - `autoStart`: whether or not to auto-start the replication applier on
///   (next and following) server starts
///
/// - `adaptivePolling`: if set to `true`, the replication applier will fall
///   to sleep for an increasingly long period in case the logger server at the
///   endpoint does not have any more replication events to apply. Using
///   adaptive polling is thus useful to reduce the amount of work for both the
///   applier and the logger server for cases when there are only infrequent
///   changes. The downside is that when using adaptive polling, it might take
///   longer for the replication applier to detect that there are new replication
///   events on the logger server.
///
///   Setting `adaptivePolling` to false will make the replication applier 
///   contact the logger server in a constant interval, regardless of whether
///   the logger server provides updates frequently or seldomly.
///
/// In case of success, the body of the response is a JSON hash with the updated
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
///     re.applier.stop();
///
///     var url = "/_api/replication/applier-config";
///     var body = { 
///       endpoint: "tcp://127.0.0.1:8529",
///       username: "replicationApplier",
///       password: "applier1234@foxx",
///       autoStart: false,
///       adaptivePolling: true
///     };
///
///     var response = logCurlRequest('PUT', url, JSON.stringify(body));
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierSetConfig () {
  assert(_vocbase->_replicationApplier != 0);
  
  TRI_replication_applier_configuration_t config;
  TRI_InitConfigurationReplicationApplier(&config);
  
  TRI_json_t* json = parseJsonBody();

  if (json == 0) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return;
  }
  
  TRI_ReadLockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);
  TRI_CopyConfigurationReplicationApplier(&_vocbase->_replicationApplier->_configuration, &config);
  TRI_ReadUnlockReadWriteLock(&_vocbase->_replicationApplier->_statusLock);

  TRI_json_t const* value;
  const string endpoint = JsonHelper::getStringValue(json, "endpoint", "");

  if (! endpoint.empty()) {
    if (config._endpoint != 0) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, config._endpoint);
    }
    config._endpoint = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, endpoint.c_str(), endpoint.size());
  }
  
  value = JsonHelper::getArrayElement(json, "username");
  if (JsonHelper::isString(value)) {
    if (config._username != 0) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, config._username);
    }
    config._username = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, value->_value._string.data, value->_value._string.length - 1);
  }

  value = JsonHelper::getArrayElement(json, "password");
  if (JsonHelper::isString(value)) {
    if (config._password != 0) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, config._password);
    }
    config._password = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, value->_value._string.data, value->_value._string.length - 1);
  }

  config._requestTimeout    = JsonHelper::getDoubleValue(json, "requestTimeout", config._requestTimeout);
  config._connectTimeout    = JsonHelper::getDoubleValue(json, "connectTimeout", config._connectTimeout);
  config._ignoreErrors      = JsonHelper::getUInt64Value(json, "ignoreErrors", config._ignoreErrors);
  config._maxConnectRetries = JsonHelper::getUInt64Value(json, "maxConnectRetries", config._maxConnectRetries);
  config._autoStart         = JsonHelper::getBooleanValue(json, "autoStart", config._autoStart);
  config._adaptivePolling   = JsonHelper::getBooleanValue(json, "adaptivePolling", config._adaptivePolling);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  int res = TRI_ConfigureReplicationApplier(_vocbase->_replicationApplier, &config);
  
  TRI_DestroyConfigurationReplicationApplier(&config);
    
  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_REPLICATION_INVALID_CONFIGURATION ||
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
/// @brief start the replication applier
///
/// @RESTHEADER{PUT /_api/replication/applier-start,starts the replication applier}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{from,string,optional}
/// The remote `lastLogTick` value from which to start applying. If not specified,
/// the last saved tick from the previous applier run is used.
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
/// `/_api/replication/applier-state` API instead.
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
///     re.applier.stop();
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
///     re.applier.stop();
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierStart () {
  assert(_vocbase->_replicationApplier != 0);
  
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
    if (res == TRI_ERROR_REPLICATION_INVALID_CONFIGURATION ||
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
/// @brief stops the replication applier
///
/// @RESTHEADER{PUT /_api/replication/applier-stop,stops the replication applier}
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
///     re.applier.stop();
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
///     re.applier.stop();
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierStop () {
  assert(_vocbase->_replicationApplier != 0);

  int res = TRI_StopReplicationApplier(_vocbase->_replicationApplier, true);
  
  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
    return;
  }
    
  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the state of the replication applier
///
/// @RESTHEADER{GET /_api/replication/applier-state,returns the state of the replication applier}
///
/// @RESTDESCRIPTION
/// Returns the state of the replication applier, regardless of whether the
/// applier is currently running or not.
///
/// The response is a JSON hash with the following attributes:
///
/// - `state`: a JSON hash with the following sub-attributes:
/// 
///   - `running`: whether or not the applier is active and running
///
///   - `lastAppliedContinuousTick`: the last tick value from the continuous
///     replication log the applier has applied.
///
///   - `lastProcessedContinuousTick`: the last tick value from the continuous
///     replication log the applier has processed. 
///
///     Regularly, the last applied and last processed tick values should be
///     identical. For transactional operations, the replication applier will first
///     process incoming log events before applying them, so the processed tick
///     value might be higher than the applied tick value. This will be the case
///     until the applier encounters the `transaction commit` log event for the
///     transaction.
///
///   - `lastAvailableContinuousTick`: the last tick value the logger server can
///     provide.
///
///   - `time`: the time on the applier server.
///
///   - `progress`: a JSON hash with details about the replication applier progress.
///     It contains the following sub-attributes if there is progress to report:
///
///     - `message`: a textual description of the progress
///
///     - `time`: the date and time the progress was logged
///
///   - `lastError`: a JSON hash with details about the last error that happened on
///     the applier. It contains the following sub-attributes if there was an error:
///
///     - `errorNum`: a numerical error code
///
///     - `errorMessage`: a textual error description
///
///     - `time`: the date and time the error occurred
///
///     In case no error has occurred, `lastError` will be empty.
///
/// - `server`: a JSON hash with the following sub-attributes:
///
///   - `version`: the applier server's version
///
///   - `serverId`: the applier server's id
///  
/// - `endpoint`: the endpoint the applier is connected to (if applier is
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
///     re.applier.stop();
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
///     re.applier.stop();
///     re.applier.start();
///
///     var url = "/_api/replication/applier-state";
///     var response = logCurlRequest('GET', url);
///     re.applier.stop();
///
///     assert(response.code === 200);
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

void RestReplicationHandler::handleCommandApplierGetState () {
  assert(_vocbase->_replicationApplier != 0);

  TRI_json_t* json = TRI_JsonReplicationApplier(_vocbase->_replicationApplier); 
  
  if (json == 0) {  
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
  assert(_vocbase->_replicationApplier != 0);

  int res = TRI_ForgetReplicationApplier(_vocbase->_replicationApplier);
  
  if (res != TRI_ERROR_NO_ERROR) {
    generateError(HttpResponse::SERVER_ERROR, res);
    return;
  }

  handleCommandApplierGetState();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
