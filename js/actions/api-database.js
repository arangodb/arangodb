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
/// @fn JSF_get_api_database_list
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
/// is returned if the list of database was compiled successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the request is invalid.
///
/// @RESTRETURNCODE{403}
/// is returned if the request was not executed in the `_system` database.
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

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_get_api_database_current
/// @brief retrieves information about the current database
///
/// @RESTHEADER{GET /_api/database/current,retrieves information about the current database}
///
/// @RESTDESCRIPTION
/// Retrieves information about the current database
///
/// The response is a JSON object with the following attributes:
/// 
/// - `name`: the name of the current database
///
/// - `id`: the id of the current database
///
/// - `path`: the filesystem path of the current database
///
/// - `isSystem`: whether or not the current database is the `_system` database
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the information was retrieved successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the request is invalid.
///
/// @RESTRETURNCODE{404}
/// is returned if the database could not be found.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestDatabaseGetInfo}
///     var url = "/_api/database/current";
///     var response = logCurlRequest('GET', url);
/// 
///     assert(response.code === 200);
/// 
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

function get_api_database (req, res) {
  if (req.suffix.length > 1) { 
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }
  
  if (req.suffix.length === 1 && req.suffix[0] !== 'current') {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var result;
  if (req.suffix.length === 0) {
    // list of all databases
    result = arangodb.db._listDatabases();
  }
  else {
    // information about the current database
    result = {
      name: arangodb.db._name(),
      id: arangodb.db._id(),
      path: arangodb.db._path(),
      isSystem: arangodb.db._isSystem()
    };
  }

  actions.resultOk(req, res, actions.HTTP_OK, { result : result });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new database
///
/// @RESTHEADER{POST /_api/database,creates a new database}
///
/// @RESTBODYPARAM{body,json,required}
/// the body with the name of the database.
///
/// @RESTDESCRIPTION
/// Creates a new database
///
/// The request body must be a JSON object with the attribute `name`. `name` must
/// contain a valid @ref DatabaseNames.
///
/// The response is a JSON object with the attribute `result` set to `true`.
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
/// specified name already exists.
///
/// @RESTRETURNCODE{403}
/// is returned if the request was not executed in the `_system` database.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestDatabaseCreate}
///     var url = "/_api/database";
///     var name = "example";
///     try {
///       db._dropDatabase(name);
///     }
///     catch (err) {
///     }
///
///     var data = {
///       name: name
///     };
///     var response = logCurlRequest('POST', url, JSON.stringify(data));
///
///     db._dropDatabase(name);
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

  var result = arangodb.db._createDatabase(json.name || "", options);

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
/// @RESTRETURNCODE{400}
/// is returned if the request is malformed.
///
/// @RESTRETURNCODE{403}
/// is returned if the request was not executed in the `_system` database.
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
///     db._createDatabase(name); 
///     var response = logCurlRequest('DELETE', url + '/' + name);
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
