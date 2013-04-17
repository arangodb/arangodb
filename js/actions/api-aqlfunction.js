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
/// @brief fetch a user
///
/// @RESTHEADER{GET /_api/aqlfunction,returns registered AQL user functions}
///
/// @REST{GET /_api/aqlfunction}
///
/// Returns all registered AQL user functions.
///
/// @REST{GET /_api/aqlfunction?namespace=@FA{namespace}}
///
/// Returns all registered AQL user functions from namespace @FA{namespace}.
///
/// The call will return a JSON list with all user functions found. Each user
/// function will at least have the following attributes:
///
/// - @LIT{name}: The fully qualified name of the user function
///
/// - @LIT{code}: A string representation of the function body
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
/// @REST{POST /_api/aqlfunction}
///
/// The following data need to be passed in a JSON representation in the body of
/// the POST request:
///
/// - @LIT{name}: the fully qualified name of the user functions.
///
/// - @LIT{code}: a string representation of the function body.
///
/// If the function can be registered by the server, the server will respond with 
/// @LIT{HTTP 201}. If the function already existed and was replaced by the
/// call, the server will respond with @LIT{HTTP 200}.
///
/// In case of success, the returned JSON object has the following properties:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{false}
///   in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// If the JSON representation is malformed or mandatory data is missing from the
/// request, the server will respond with @LIT{HTTP 400}.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{true} in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// - @LIT{errorNum}: the server error number
///
/// - @LIT{errorMessage}: a descriptive error message
///
/// @EXAMPLES
///
/// @verbinclude api-aqlfunction-create
////////////////////////////////////////////////////////////////////////////////

function POST_api_aqlfunction (req, res) {
  var json = actions.getJsonBody(req, res, actions.HTTP_BAD);

  if (json === undefined) {
    return;
  }

  var result = aqlfunctions.register(json.name, json.code);

  actions.resultOk(req, res, result ? actions.HTTP_OK : actions.HTTP_CREATED, { });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an existing AQL user function
///
/// @RESTHEADER{DELETE /_api/aqlfunction,remove an existing AQL user function}
///
/// @REST{DELETE /_api/aqlfunction/@FA{name}}
///
/// Removes an existing AQL user function, identified by @FA{name}. 
/// The function name provided in @FA{name} must be fully qualified, including
/// any namespaces.
///
/// If the function can be removed by the server, the server will respond with 
/// @LIT{HTTP 200}. 
///
/// In case of success, the returned JSON object has the following properties:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{false}
///   in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// If the JSON representation is malformed or mandatory data is missing from the
/// request, the server will respond with @LIT{HTTP 400}. If the specified user
/// does not exist, the server will respond with @LIT{HTTP 404}.
///
/// The body of the response will contain a JSON object with additional error
/// details. The object has the following attributes:
///
/// - @LIT{error}: boolean flag to indicate that an error occurred (@LIT{true} in this case)
///
/// - @LIT{code}: the HTTP status code
///
/// - @LIT{errorNum}: the server error number
///
/// - @LIT{errorMessage}: a descriptive error message
////////////////////////////////////////////////////////////////////////////////

function DELETE_api_aqlfunction (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  try {
    aqlfunctions.unregister(name);
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
      actions.resultException(req, res, err);
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
