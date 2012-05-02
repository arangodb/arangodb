////////////////////////////////////////////////////////////////////////////////
/// @brief query actions
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
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

var actions = require("actions");

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a query and return information about it
///
/// @REST{POST /_api/query}
///
/// To validate a query string without executing it, the query string can be 
/// passed to the server via an HTTP POST request.
///
/// These query string needs to be passed in the attribute @LIT{query} of a 
/// JSON object as the body of the POST request.
///
/// The server will respond with @LIT{HTTP 400} or @LIT{HTTP 500} in case of a
/// malformed request or a general error.
/// If the query contains a parse error, the server will respond with an
/// @LIT{HTTP 404} error. 
///
/// The body of the response will contain the error details embedded in a JSON
/// object.
///
/// @verbinclude querypostfail
///
/// If the query is valid, the server will respond with @LIT{HTTP 200} and return
/// the names of the bind parameters it found in the query (if any) in the 
/// @LIT{"bindVars"} attribute of the response. 
///
/// @verbinclude querypost
////////////////////////////////////////////////////////////////////////////////

function POST_api_query (req, res) {
  if (req.suffix.length != 0) {
    actions.resultNotFound(req, res, actions.ERROR_HTTP_NOT_FOUND);
    return;
  }

  try {
    var json = JSON.parse(req.requestBody);
      
    if (!json || !(json instanceof Object) || json.query == undefined) {
      actions.resultBad(req, res, actions.ERROR_QUERY_SPECIFICATION_INVALID, actions.getErrorMessage(actions.ERROR_QUERY_SPECIFICATION_INVALID));
      return;
    }

    var result = AHUACATL_PARSE(json.query);
    if (result instanceof AvocadoError) {
      actions.resultBad(req, res, result.errorNum, result.errorMessage);
      return;
    }

    result = { "bindVars" : result };

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
/// @brief query actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/query",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case (actions.POST) : 
        POST_api_query(req, res); 
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
