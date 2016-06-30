/* jshint strict: false */
/* global ArangoAgency */

// //////////////////////////////////////////////////////////////////////////////
// / @brief database management
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb');
var actions = require('@arangodb/actions');
var cluster = require('@arangodb/cluster');

var API = '_api/database';

function get_api_database (req, res) {
  if (req.suffix.length > 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var dbname = arangodb.db._name();
  var isSystem = arangodb.db._isSystem();
  var path = arangodb.db._path();
  var id = arangodb.db._id();

  arangodb.db._useDatabase('_system');

  var result;
  if (req.suffix.length === 0) {
    // list of all databases
    result = arangodb.db._databases();
  } else {
    if (req.suffix[0] === 'user') {
      // fetch all databases for the current user
      // note: req.user may be null if authentication is turned off
      if (req.user === null) {
        result = arangodb.db._databases();
      } else {
        result = arangodb.db._databases(req.user);
      }
    } else if (req.suffix[0] === 'current') {
      if (cluster.isCoordinator()) {
        // fetch database information from Agency
        var values = ArangoAgency.get('Plan/Databases/' + encodeURIComponent(req.database), false);
        var dbEntry = values.arango.Plan.Databases[encodeURIComponent(req.database)];
        result = {
          name: dbEntry.name,
          id: dbEntry.id,
          path: 'none',
          isSystem: (dbEntry.name.substr(0, 1) === '_')
        };
      } else {
        // information about the current database
        result = {
          name: dbname,
          id: id,
          path: path,
          isSystem: isSystem
        };
      }
    } else {
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
      return;
    }
  }

  actions.resultOk(req, res, actions.HTTP_OK, { result: result });
}

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
    users = [];
  } else if (!Array.isArray(users)) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var i;
  for (i = 0; i < users.length; ++i) {
    var user = users[i];
    if (typeof user !== 'object' ||
      !user.hasOwnProperty('username') ||
      typeof (user.username) !== 'string') {
      // bad username
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
      return;
    }

    if (!user.hasOwnProperty('passwd')) {
      // default to empty string
      users[i].passwd = '';
    } else if (typeof (user.passwd) !== 'string') {
      // bad password type
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
      return;
    }

    if (!user.hasOwnProperty('active')) {
      users[i].active = true;
    }
  }

  var result = arangodb.db._createDatabase(json.name || '', options, users);

  actions.resultOk(req, res, actions.HTTP_CREATED, { result: result });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_database_delete
// //////////////////////////////////////////////////////////////////////////////

function delete_api_database (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var result = arangodb.db._dropDatabase(req.suffix[0]);

  actions.resultOk(req, res, actions.HTTP_OK, { result: result });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief handles a database request
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API,
  allowUseDatabase: true,

  callback: function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        get_api_database(req, res);
      } else if (req.requestType === actions.POST) {
        post_api_database(req, res);
      } else if (req.requestType === actions.DELETE) {
        delete_api_database(req, res);
      } else {
        actions.resultUnsupported(req, res);
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});
