/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, evil: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");

var db = arangodb.db;

var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_get_api_endpoint
/// @brief returns a list of all endpoints
///
/// @RESTHEADER{GET /_api/endpoint,returns a list of all endpoints}
///
/// @RESTDESCRIPTION
/// Returns a list of all configured endpoints the server is listening on. For
/// each endpoint, the list of allowed databases is returned too if set.
///
/// The result is a JSON hash which has the endpoints as keys, and the list of
/// mapped database names as values for each endpoint.
///
/// If a list of mapped databases is empty, it means that all databases can be
/// accessed via the endpoint. If a list of mapped databases contains more than
/// one database name, this means that any of the databases might be accessed
/// via the endpoint, and the first database in the list will be treated as
/// the default database for the endpoint. The default database will be used 
/// when an incoming request does not specify a database name in the request 
/// explicitly.
///
/// Note: retrieving the list of all endpoints is allowed in the system database
/// only. Calling this action in any other database will make the server return
/// an error.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned when the list of endpoints can be determined successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the action is not carried out in the system database.
///
/// @RESTRETURNCODE{405}
/// The server will respond with `HTTP 405` if an unsupported HTTP method is used.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestEndpointGet}
///     var url = "/_api/endpoint";
///     var endpoint = "tcp://127.0.0.1:8532";
///     var body = {
///       endpoint: endpoint,
///       databases: [ "mydb1", "mydb2" ]
///     };
///     curlRequest('POST', url, JSON.stringify(body));
///
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 200);
/// 
///     logJsonResponse(response);
///     curlRequest('DELETE', url + '/' + encodeURIComponent(endpoint));
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_post_api_endpoint
/// @brief connects a new endpoint or reconfigures an existing endpoint
///
/// @RESTHEADER{POST /_api/endpoint,adds a new endpoint or reconfigures an existing endpoint}
///
/// @RESTBODYPARAM{description,json,required}
/// A JSON object describing the endpoint.
///
/// @RESTDESCRIPTION
/// The request body must be JSON hash with the following attributes:
///
/// - `endpoint`: the endpoint specification, e.g. `tcp://127.0.0.1:8530`
///
/// - `databases`: a list of database names the endpoint is responsible for.
///
/// If `databases` is an empty list, all databases present in the server will
/// become accessible via the endpoint, with the `_system` database being the
/// default database. 
///
/// If `databases` is non-empty, only the specified databases will become 
/// available via the endpoint. The first database name in the `databases` 
/// list will also become the default database for the endpoint. The default 
/// database will always be used if a request coming in on the endpoint does 
/// not specify the database name explicitly.
///
/// Note: adding or reconfiguring endpoints is allowed in the system database 
/// only. Calling this action in any other database will make the server 
/// return an error.
///
/// Adding SSL endpoints at runtime is only supported if the server was started 
/// with SSL properly configured (e.g. `--server.keyfile` must have been set).
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned when the endpoint was added or changed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the request is malformed or if the action is not carried out 
/// in the system database.
///
/// @RESTRETURNCODE{405}
/// The server will respond with `HTTP 405` if an unsupported HTTP method is used.
///
/// @EXAMPLES
/// Adding an endpoint `tcp://127.0.0.1:8532` with two mapped databases 
/// (`mydb1` and `mydb2`). `mydb1` will become the default database for the
/// endpoint.
///
/// @EXAMPLE_ARANGOSH_RUN{RestEndpointPostOne}
///     var url = "/_api/endpoint";
///     var endpoint = "tcp://127.0.0.1:8532";
///     var body = {
///       endpoint: endpoint,
///       databases: [ "mydb1", "mydb2" ]
///     };
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
/// 
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     curlRequest('DELETE', url + '/' + encodeURIComponent(endpoint));
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Adding an endpoint `tcp://127.0.0.1:8533` with no database names specified.
/// This will allow access to all databases on this endpoint. The `_system`
/// database will become the default database for requests that come in on this
/// endpoint and do not specify the database name explicitly.
///
/// @EXAMPLE_ARANGOSH_RUN{RestEndpointPostTwo}
///     var url = "/_api/endpoint";
///     var endpoint = "tcp://127.0.0.1:8533";
///     var body = {
///       endpoint: endpoint,
///       databases: [ ]
///     };
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
/// 
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     curlRequest('DELETE', url + '/' + encodeURIComponent(endpoint));
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Adding an endpoint `tcp://127.0.0.1:8533` without any databases first,
/// and then updating the databases for the endpoint to `testdb1`, `testdb2`, and
/// `testdb3`.
/// 
/// @EXAMPLE_ARANGOSH_RUN{RestEndpointPostChange}
///     var url = "/_api/endpoint";
///     var endpoint = "tcp://127.0.0.1:8533";
///     var body = {
///       endpoint: endpoint,
///       databases: [ ]
///     };
///     var response = logCurlRequest('POST', url, JSON.stringify(body));
/// 
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///
///     body.database = [ "testdb1", "testdb2", "testdb3" ];
///     response = logCurlRequest('POST', url, JSON.stringify(body));
/// 
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     curlRequest('DELETE', url + '/' + encodeURIComponent(endpoint));
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_delete_api_endpoint
/// @brief disconnects an existing endpoint
///
/// @RESTHEADER{DELETE /_api/endpoint/{endpoint},deletes and disconnects an existing endpoint}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{endpoint,string,required}
/// The endpoint to delete, e.g. `tcp://127.0.0.1:8529`.
///
/// @RESTDESCRIPTION
/// This operation deletes an existing endpoint from the list of all endpoints,
/// and makes the server stop listening on the endpoint.
///
/// Note: deleting and disconnecting an endpoint is allowed in the system 
/// database only. Calling this action in any other database will make the server 
/// return an error.
///
/// Futhermore, the last remaining endpoint cannot be deleted as this would make
/// the server kaputt.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned when the endpoint was deleted and disconnected successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the request is malformed or if the action is not carried out 
/// in the system database.
///
/// @RESTRETURNCODE{404}
/// is returned if the endpoint is not found.
///
/// @RESTRETURNCODE{405}
/// The server will respond with `HTTP 405` if an unsupported HTTP method is used.
///
/// @EXAMPLES
///
/// Deleting an existing endpoint
///
/// @EXAMPLE_ARANGOSH_RUN{RestEndpointDelete}
///     var url = "/_api/endpoint";
///     var endpoint = "tcp://127.0.0.1:8532";
///     var body = {
///       endpoint: endpoint,
///       databases: [ ]
///     };
///     curlRequest('POST', url, JSON.stringify(body));
///     var response = logCurlRequest('DELETE', url + '/' + encodeURIComponent(endpoint));
/// 
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Deleting a non-existing endpoint
///
/// @EXAMPLE_ARANGOSH_RUN{RestEndpointDeleteNonExisting}
///     var url = "/_api/endpoint";
///     var endpoint = "tcp://127.0.0.1:8532";
///     var response = logCurlRequest('DELETE', url + '/' + encodeURIComponent(endpoint));
/// 
///     assert(response.code === 404);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/endpoint",
  context : "admin",
  prefix : true,

  callback : function (req, res) {
    try {
      var result;

      if (req.requestType === actions.GET) {
        actions.resultOk(req, res, actions.HTTP_OK, internal.listEndpoints());
      }

      else if (req.requestType === actions.POST) {
        var body = actions.getJsonBody(req, res);

        if (body === undefined || typeof body.endpoint !== 'string') {
          actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                            "invalid endpoint value");
          return;
        }

        result = internal.configureEndpoint(body.endpoint, body.databases || [ ]);
        actions.resultOk(req, res, actions.HTTP_OK, { result: result }); 
      }

      else if (req.requestType === actions.DELETE) {
        if (req.suffix.length !== 1) {
          actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                            "expected DELETE /" + this.url + "/<endpoint>");
          return;
        }

        var endpoint = decodeURIComponent(req.suffix[0]);
        result = internal.removeEndpoint(endpoint);
        actions.resultOk(req, res, actions.HTTP_OK, { result: result });
      }
      else {
        actions.resultUnsupported(req, res);
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}"
// End:
