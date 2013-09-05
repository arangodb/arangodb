/*jslint indent: 2, nomen: true, maxlen: 150, sloppy: true, vars: true, white: true, plusplus: true, stupid: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief database management
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

var API = "_api/database";

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief retrieves a list of all existing databases
///
/// @RESTHEADER{GET /_api/database,retrieves a list of all existing databases}
///
/// @RESTDESCRIPTION
/// Retrieves a list of all existing databases
///
/// Note: retrieving the list of databases is only possible from within the `_system` database.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the database was dropped successfully.
///
/// @RESTRETURNCODE{404}
/// is returned if the database could not be found.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestDatabaseGet}
///     var url = "/_api/database";
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 200);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function get_api_database (req, res) {
  if (req.suffix.length !== 0) { 
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var result = arangodb.db._listDatabases();

  actions.resultOk(req, res, actions.HTTP_OK, { result : result });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new database
///
/// @RESTHEADER{POST /_api/database,creates a new database}
///
/// @RESTDESCRIPTION
/// Creates a new database
///
/// Note: creating a new database is only possible from within the `_system` database.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the database was created successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the request parameters are invalid or if a database with the 
/// specified name or path already exists.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestDatabaseCreate}
///     var url = "/_api/database";
///     var data = {
///       name: "example"
///     };
///     var response = logCurlRequest('POST', url, JSON.stringify(data));
///
///     db._dropDatabase("example"); 
///     assert(response.code === 200);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function post_api_database (req, res) {
  if (req.suffix.length !== 0) { 
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }
  
  var json = actions.getJsonBody(req, res);
  
  if (json === undefined) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var options = json.options;
  
  if (options === undefined) {
    options = { };
  }

  if (typeof options !== 'object') {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var result = arangodb.db._createDatabase(json.name || "", json.path || "", options);

  actions.resultOk(req, res, actions.HTTP_OK, { result : result });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop an existing database
///
/// @RESTHEADER{DELETE /_api/database/`database-name`,drops an existing database}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{database-name,string,required}
/// The name of the database
///
/// @RESTDESCRIPTION
/// Deletes the database along with all data stored in it.
///
/// Note: dropping a database is only possible from within the `_system` database.
/// The `_system` database itself cannot be dropped.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the database was dropped successfully.
///
/// @RESTRETURNCODE{404}
/// is returned if the database could not be found.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestDatabaseDrop}
///     var url = "/_api/database";
///     var name = "example";
///
///     try {
///       db._dropDatabase(name);
///     }
///     catch (err) {
///     }
///
///     db._createDatabase(name);
///     var response = logCurlRequest('DELETE', url + "/" + name);
/// 
///     assert(response.code === 200);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function delete_api_database (req, res) {
  if (req.suffix.length !== 1) { 
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }
  
  var result = arangodb.db._dropDatabase(req.suffix[0]);

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
        get_api_database(req, res);
      }
      else if (req.requestType === actions.POST) {
        post_api_database(req, res);
      }
      else if (req.requestType === actions.DELETE) {
        delete_api_database(req, res);
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
