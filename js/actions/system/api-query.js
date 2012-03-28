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
////////////////////////////////////////////////////////////////////////////////

function postQuery(req, res) {
  if (req.suffix.length != 0) {
    actions.resultError (req, res, 404, actions.errorInvalidRequest, "Invalid request");
    return;
  }

  try {
    var json = JSON.parse(req.requestBody);
      
    if (!json || !(json instanceof Object) || json.query == undefined) {
      actions.resultError (req, res, 400, actions.errorQuerySpecificationInvalid, "Query specification invalid");
      return;
    }

    var result = AQL_PARSE(json.query);
    if (result instanceof AvocadoQueryError) {
      actions.resultError (req, res, 404, result.code, result.message);
      return;
    }

    result = { "bindVars" : result };

    actions.resultOk (req, res, 200, result);
  }
  catch (e) {
    actions.resultError (req, res, 404, actions.errorJavascriptException, "Javascript exception");
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
      case ("POST") : 
        postQuery(req, res); 
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
