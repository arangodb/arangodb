////////////////////////////////////////////////////////////////////////////////
/// @brief AQL user functions management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
var aqlfunctions = require("org/arangodb/aql/functions");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all reqistered AQL user functions
///
/// @RESTHEADER{GET /_api/aqlfunction,returns registered AQL user functions}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{namespace,string,optional}
/// Returns all registered AQL user functions from namespace `namespace`.
///
/// @RESTDESCRIPTION
/// Returns all registered AQL user functions.
///
/// The call will return a JSON list with all user functions found. Each user
/// function will at least have the following attributes:
///
/// - `name`: The fully qualified name of the user function
///
/// - `code`: A string representation of the function body
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// if success `HTTP 200` is returned.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestAqlfunctionsGetAll}
///     var url = "/_api/aqlfunction";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function GET_api_aqlfunction (req, res) {
  if (req.suffix.length != 0) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var namespace = undefined;
  if (req.parameters.hasOwnProperty('namespace')) {
    namespace = req.parameters.namespace;
  }

  var result = aqlfunctions.toArray(namespace);
  actions.resultOk(req, res, actions.HTTP_OK, result)
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new AQL user function
///
/// @RESTHEADER{POST /_api/aqlfunction,creates or replaces an AQL user function}
///
/// @RESTBODYPARAM{body,json,required}
/// the body with name and code of the aql user function.
///
/// @RESTDESCRIPTION
///
/// The following data need to be passed in a JSON representation in the body of
/// the POST request:
///
/// - `name`: the fully qualified name of the user functions.
///
/// - `code`: a string representation of the function body.
/// 
/// - `isDeterministic`: an optional boolean value to indicate that the function
///   results are fully deterministic (function return value solely depends on 
///   the input value and return value is the same for repeated calls with same
///   input). The `isDeterministic` attribute is currently not used but may be
///   used later for optimisations.
///
/// In case of success, the returned JSON object has the following properties:
///
/// - `error`: boolean flag to indicate that an error occurred (`false`
///   in this case)
///
/// - `code`: the HTTP status code
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - `error`: boolean flag to indicate that an error occurred (`true` in this case)
///
/// - `code`: the HTTP status code
///
/// - `errorNum`: the server error number
///
/// - `errorMessage`: a descriptive error message
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the function already existed and was replaced by the
/// call, the server will respond with `HTTP 200`.
///
/// @RESTRETURNCODE{201}
/// If the function can be registered by the server, the server will respond with 
/// `HTTP 201`.
///
/// @RESTRETURNCODE{400}
/// If the JSON representation is malformed or mandatory data is missing from the
/// request, the server will respond with `HTTP 400`.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestAqlfunctionCreate}
///     var url = "/_api/aqlfunction";
///     var body = '{ "name" : "myfunctions:temperature:celsiustofahrenheit", "code" : "function (celsius) { return celsius * 1.8 + 32; }" }';
///
///     var response = logCurlRequest('POST', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function POST_api_aqlfunction (req, res) {
  var json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  var result = aqlfunctions.register(json.name, json.code, json.isDeterministic);

  actions.resultOk(req, res, result ? actions.HTTP_OK : actions.HTTP_CREATED, { });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an existing AQL user function
///
/// @RESTHEADER{DELETE /_api/aqlfunction/{name},remove an existing AQL user function}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{name,string,required}
/// the name of the AQL user function.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{group,string,optional}
/// If set to `true`, then the function name provided in `name` is treated as
/// a namespace prefix, and all functions in the specified namespace will be deleted.
/// If set to `false`, the function name provided in `name` must be fully 
/// qualified, including any namespaces.
///
/// @RESTDESCRIPTION
///
/// Removes an existing AQL user function, identified by `name`. 
///
/// In case of success, the returned JSON object has the following properties:
///
/// - `error`: boolean flag to indicate that an error occurred (`false`
///   in this case)
///
/// - `code`: the HTTP status code
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - `error`: boolean flag to indicate that an error occurred (`true` in this case)
///
/// - `code`: the HTTP status code
///
/// - `errorNum`: the server error number
///
/// - `errorMessage`: a descriptive error message
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// If the function can be removed by the server, the server will respond with 
/// `HTTP 200`.
///
/// @RESTRETURNCODE{400}
/// If the user function name is malformed, the server will respond with `HTTP 400`.
///
/// @RESTRETURNCODE{404}
/// If the specified user user function does not exist, the server will respond with `HTTP 404`.
///
/// @EXAMPLES
///
/// deletes a function:
///
/// @EXAMPLE_ARANGOSH_RUN{RestAqlfunctionDelete}
///     var url = "/_api/aqlfunction/square:x:y";
///
///     var body = '{ "name" : "square:x:y", "code" : "function (x) { return x*x; }" }';
///
///     db._connection.POST("/_api/aqlfunction", body);
///     var response = logCurlRequest('DELETE', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// function not found:
///
/// @EXAMPLE_ARANGOSH_RUN{RestAqlfunctionDeleteFails}
///     var url = "/_api/aqlfunction/myfunction:x:y";
///     var response = logCurlRequest('DELETE', url);
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function DELETE_api_aqlfunction (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  try {
    var g = req.parameters['group'];
    if (g === 'true' || g === 'yes' || g === 'y' || g === 'on' || g === '1') {
      aqlfunctions.unregisterGroup(name);
    }
    else {
      aqlfunctions.unregister(name);
    }
    actions.resultOk(req, res, actions.HTTP_OK, { });
  }
  catch (err) {
    if (err.errorNum === arangodb.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code) {
      actions.resultNotFound(req, res, arangodb.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code);
    }
    else {
      throw err;
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/aqlfunction",
  context : "api",

  callback : function (req, res) {
    try {
      switch (req.requestType) {
        case actions.GET: 
          GET_api_aqlfunction(req, res); 
          break;

        case actions.POST: 
          POST_api_aqlfunction(req, res); 
          break;

        case actions.DELETE:  
          DELETE_api_aqlfunction(req, res); 
          break;

        default:
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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
