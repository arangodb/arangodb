/*jshint strict: false */
/*global require, ArangoAgency */

////////////////////////////////////////////////////////////////////////////////
/// @brief database management
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var cluster = require("org/arangodb/cluster");

var API = "_api/database";

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_database_list
/// @brief retrieves a list of all existing databases
///
/// @RESTHEADER{GET /_api/database, List of databases}
///
/// @RESTDESCRIPTION
/// Retrieves the list of all existing databases
///
/// **Note**: retrieving the list of databases is only possible from within the *_system* database.
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
/// is returned if the request was not executed in the *_system* database.
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
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_database_user
/// @brief retrieves a list of all databases the current user can access
///
/// @RESTHEADER{GET /_api/database/user, List of accessible databases }
///
/// @RESTDESCRIPTION
/// Retrieves the list of all databases the current user can access without
/// specifying a different username or password.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the list of database was compiled successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the request is invalid.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestDatabaseGetUser}
///     var url = "/_api/database/user";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_database_current
/// @brief retrieves information about the current database
///
/// @RESTHEADER{GET /_api/database/current, Information of the database}
///
/// @RESTDESCRIPTION
/// Retrieves information about the current database
///
/// The response is a JSON object with the following attributes:
///
/// - *name*: the name of the current database
///
/// - *id*: the id of the current database
///
/// - *path*: the filesystem path of the current database
///
/// - *isSystem*: whether or not the current database is the *_system* database
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
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function get_api_database (req, res) {
  if (req.suffix.length > 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var result;
  if (req.suffix.length === 0) {
    // list of all databases
    result = arangodb.db._listDatabases();
  }
  else {
    if (req.suffix[0] === 'user') {
      // return all databases for current user
      var username = '', password = '', auth = '';

      if (req.headers.hasOwnProperty('authorization')) {
        auth = req.headers.authorization;
        var header = req.headers.authorization.replace(/^Basic\s+/i, '');
        var decoded = require("internal").base64Decode(header);
        var pos = decoded.indexOf(':');

        if (pos >= 0) {
          username = decoded.substr(0, pos);
          password = decoded.substr(pos + 1, decoded.length - pos - 1);
        }
      }

      result = arangodb.db._listDatabases(username, password, auth);
    }
    else if (req.suffix[0] === 'current') {
      if (cluster.isCoordinator()) {
        // fetch database information from Agency
        var values = ArangoAgency.get("Plan/Databases/" + encodeURIComponent(req.database), false);
        var dbEntry = values["Plan/Databases/" + encodeURIComponent(req.database)];
        result = {
          name: dbEntry.name,
          id: dbEntry.id,
          path: "none",
          isSystem: (dbEntry.name.substr(0, 1) === '_')
        };
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
    }
    else {
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
      return;
    }
  }

  actions.resultOk(req, res, actions.HTTP_OK, { result : result });
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_database_new
/// @brief creates a new database
///
/// @RESTHEADER{POST /_api/database, Create database}
///
/// @RESTBODYPARAM{body,json,required}
/// the body with the name of the database.
///
/// @RESTDESCRIPTION
/// Creates a new database
///
/// The request body must be a JSON object with the attribute *name*. *name* must
/// contain a valid database name.
///
/// The request body can optionally contain an attribute *users*, which then
/// must be a list of user objects to initially create for the new database.
/// Each user object can contain the following attributes:
///
/// - *username*: the user name as a string. This attribute is mandatory.
///
/// - *passwd*: the user password as a string. If not specified, then it defaults
///   to the empty string.
///
/// - *active*: a boolean flag indicating whether the user accout should be
///   actived or not. The default value is *true*.
///
/// - *extra*: an optional JSON object with extra user information. The data
///   contained in *extra* will be stored for the user but not be interpreted
///   further by ArangoDB.
///
/// If *users* is not specified or does not contain any users, a default user
/// *root* will be created with an empty string password. This ensures that the
/// new database will be accessible after it is created.
///
/// The response is a JSON object with the attribute *result* set to *true*.
///
/// **Note**: creating a new database is only possible from within the *_system* database.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the database was created successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the request parameters are invalid or if a database with the
/// specified name already exists.
///
/// @RESTRETURNCODE{403}
/// is returned if the request was not executed in the *_system* database.
///
/// @RESTRETURNCODE{409}
/// is returned if a database with the specified name already exists.
///
/// @EXAMPLES
///
/// Creating a database named *example*.
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
///     assert(response.code === 201);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Creating a database named *mydb* with two users.
///
/// @EXAMPLE_ARANGOSH_RUN{RestDatabaseCreateUsers}
///     var url = "/_api/database";
///     var name = "mydb";
///     try {
///       db._dropDatabase(name);
///     }
///     catch (err) {
///     }
///
///     var data = {
///       name: name,
///       users: [
///         {
///           username : "admin",
///           passwd : "secret",
///           active: true
///         },
///         {
///           username : "tester",
///           passwd : "test001",
///           active: false
///         }
///       ]
///     };
///     var response = logCurlRequest('POST', url, JSON.stringify(data));
///
///     db._dropDatabase(name);
///     assert(response.code === 201);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
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

  var users = json.users;

  if (users === undefined || users === null) {
    users = [ ];
  }
  else if (! Array.isArray(users)) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var i;
  for (i = 0; i < users.length; ++i) {
    var user = users[i];
    if (typeof user !== 'object' ||
        ! user.hasOwnProperty('username') ||
        typeof(user.username) !== 'string') {
      // bad username
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
      return;
    }

    if (! user.hasOwnProperty('passwd')) {
      // default to empty string
      users[i].passwd = '';
    }
    else if (typeof(user.passwd) !== 'string') {
      // bad password type
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
      return;
    }

    if (! user.hasOwnProperty('active')) {
      users[i].active = true;
    }
  }

  var result = arangodb.db._createDatabase(json.name || "", options, users);

  var returnCode = (req.compatibility <= 10400 ? actions.HTTP_OK : actions.HTTP_CREATED);
  actions.resultOk(req, res, returnCode, { result : result });
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_database_delete
/// @brief drop an existing database
///
/// @RESTHEADER{DELETE /_api/database/{database-name}, Drop database}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{database-name,string,required}
/// The name of the database
///
/// @RESTDESCRIPTION
/// Drops the database along with all data stored in it.
///
/// **Note**: dropping a database is only possible from within the *_system* database.
/// The *_system* database itself cannot be dropped.
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
/// is returned if the request was not executed in the *_system* database.
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
/// @endDocuBlock
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
