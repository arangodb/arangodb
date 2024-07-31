'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const arangosh = require('@arangodb/arangosh');

// / @brief clears the query plan cache
exports.clear = function () {
  let requestResult = db._connection.DELETE('/_api/query-plan-cache');
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief fetches the list of cached plans from the cache
// //////////////////////////////////////////////////////////////////////////////

exports.toArray = function (properties) {
  let requestResult = db._connection.GET('/_api/query-plan-cache/entries');
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};
