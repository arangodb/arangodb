////////////////////////////////////////////////////////////////////////////////
/// @brief query explain actions
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

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

var actions = require("actions");

////////////////////////////////////////////////////////////////////////////////
/// @brief explain a query and return information about it
///
/// @RESTHEADER{POST /_api/explain,explains a query}
///
/// @REST{POST /_api/explain}
///
/// To explain how a query would be executed (without actually executing it), 
/// the query string can be passed to the server via an HTTP POST request.
///
/// These query string needs to be passed in the attribute @LIT{query} of a JSON
/// object as the body of the POST request. If the query references any bind 
/// variables, these must also be passed in the attribute @LIT{bindVars}.
///
/// If the query is valid, the server will respond with @LIT{HTTP 200} and
/// return a list of the individual query execution steps in the @LIT{"plan"}
/// attribute of the response.
///
/// The server will respond with @LIT{HTTP 400} in case of a malformed request,
/// or if the query contains a parse error. The body of the response will
/// contain the error details embedded in a JSON object.
/// Omitting bind variables if the query references any will result also result
/// in an @LIT{HTTP 400} error.
///
/// @EXAMPLES
///
/// Valid query:
///
/// @verbinclude api-explain-valid
///
/// Invalid query:
///
/// @verbinclude api-explain-invalid
////////////////////////////////////////////////////////////////////////////////

function POST_api_explain (req, res) {
  if (req.suffix.length != 0) {
    actions.resultNotFound(req, res, actions.ERROR_HTTP_NOT_FOUND);
    return;
  }

  var json = actions.getJsonBody(req, res);

  if (json === undefined) {
    return;
  }

  try {
    var result = AHUACATL_EXPLAIN(json.query, json.bindVars);

    if (result instanceof ArangoError) {
      actions.resultBad(req, res, result.errorNum, result.errorMessage);
      return;
    }

    result = { "plan" : result };

    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief explain gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/explain",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case (actions.POST) : 
        POST_api_explain(req, res); 
        break;

      default:
        actions.resultUnsupported(req, res);
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
