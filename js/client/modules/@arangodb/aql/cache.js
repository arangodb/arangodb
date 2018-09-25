'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief AQL query cache management
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

var internal = require('internal');
var arangosh = require('@arangodb/arangosh');

// / @brief clears the query cache
exports.clear = function () {
  var db = internal.db;

  var requestResult = db._connection.DELETE('/_api/query-cache');
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// / @brief fetches or sets the query cache properties
exports.properties = function (properties) {
  var db = internal.db;
  var requestResult;

  if (properties !== undefined) {
    requestResult = db._connection.PUT('/_api/query-cache/properties', properties);
  } else {
    requestResult = db._connection.GET('/_api/query-cache/properties');
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief fetches the list of cached queries from the cache
// //////////////////////////////////////////////////////////////////////////////

exports.toArray = function (properties) {
  var db = internal.db;
  var requestResult = db._connection.GET('/_api/query-cache/entries');

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};
