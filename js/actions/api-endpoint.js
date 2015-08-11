/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("org/arangodb/actions");
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_endpoint
/// @brief returns a list of all endpoints
///
/// @RESTHEADER{GET /_api/endpoint, Return list of all endpoints}
///
/// @RESTDESCRIPTION
/// Returns an array of all configured endpoints the server is listening on. For
/// each endpoint, the array of allowed databases is returned too if set.
///
/// The result is a JSON object which has the endpoints as keys, and an array of
/// mapped database names as values for each endpoint.
///
/// If an array of mapped databases is empty, it means that all databases can be
/// accessed via the endpoint. If an array of mapped databases contains more than
/// one database name, this means that any of the databases might be accessed
/// via the endpoint, and the first database in the arry will be treated as
/// the default database for the endpoint. The default database will be used
/// when an incoming request does not specify a database name in the request
/// explicitly.
///
/// **Note**: retrieving the array of all endpoints is allowed in the system database
/// only. Calling this action in any other database will make the server return
/// an error.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned when the array of endpoints can be determined successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the action is not carried out in the system database.
///
/// @RESTRETURNCODE{405}
/// The server will respond with *HTTP 405* if an unsupported HTTP method is used.
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
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/endpoint",
  prefix : true,

  callback : function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        actions.resultOk(req, res, actions.HTTP_OK, internal.listEndpoints());
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
