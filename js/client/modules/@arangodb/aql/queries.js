'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief AQL query management
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2012 triagens GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let internal = require('internal');
let arangosh = require('@arangodb/arangosh');

let allDatabases = function(params) {
  let result = false;
  if (typeof params === 'boolean') {
    result = Boolean(params);
  } else if (typeof params === 'object') {
    result = Boolean(params.all);
  }
  return String(result);
};

// / @brief clears the slow query log
exports.clearSlow = function (params) {
  let db = internal.db;
  let requestResult = db._connection.DELETE('/_api/query/slow?all=' + allDatabases(params));
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// / @brief returns the slow queries
exports.slow = function (params) {
  let db = internal.db;
  let requestResult = db._connection.GET('/_api/query/slow?all=' + allDatabases(params), '');
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// / @brief returns the current queries
exports.current = function (params) {
  let db = internal.db;
  let requestResult = db._connection.GET('/_api/query/current?all=' + allDatabases(params), '');
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// / @brief configures the query tracking properties
exports.properties = function (config) {
  let db = internal.db;

  let requestResult;
  if (config === undefined) {
    requestResult = db._connection.GET('/_api/query/properties');
  } else {
    requestResult = db._connection.PUT('/_api/query/properties', config);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// / @brief kills a query
exports.kill = function (params) {
  let id = params;
  if (typeof params === 'object' && params.hasOwnProperty('id')) {
    id = id.id;
  }

  let db = internal.db;
  let requestResult = db._connection.DELETE('/_api/query/' + encodeURIComponent(id) + '?all=' + allDatabases(params));
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};
