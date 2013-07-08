/*jslint indent: 2, nomen: true, maxlen: 150, sloppy: true, vars: true, white: true, plusplus: true, stupid: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief querying information about the current database
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");

var API = "_api/current-database";

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the current database
///
/// @RESTHEADER{GET /_api/current-database,returns information about the current database}
///
/// @RESTURLPARAMETERS
///
/// @RESTDESCRIPTION
/// The result is an object describing the properties of the current database 
/// (i.e. the database the request was sent to). The result has the following
/// attributes:
///
/// - `name`: The name of the database.
///
/// - `path`: The database path.
///
/// - `isSystem`: Whether or not the current database is the system database.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// The server will respond with `HTTP 200` in case of success. 
///
/////////////////////////////////////////////////////////////////////////////

function get_api_current_database (req, res) {
  if (req.suffix.length !== 0) { 
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var db = arangodb.db;
    
  var result = {
    name: db._name(),
    path: db._path(),
    isSystem: db._isSystem()
  };

  actions.resultOk(req, res, actions.HTTP_OK, { result : result });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a database request
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API,
  context : "api",

  callback : function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        get_api_current_database(req, res);
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
