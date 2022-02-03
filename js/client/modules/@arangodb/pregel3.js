/* jshint strict: false */
// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
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
// / @author Markus Pfeiffer
// / @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var arangosh = require('@arangodb/arangosh');
var arangodb = require('@arangodb');
var throwBadParameter = arangodb.throwBadParameter;
var db = internal.db;

// TODO: there needs to be a safe way to make these paths
const api_root = '/_api/pregel3/';
const api_query = '/_api/pregel3/query/';

const api_query_verb = function(queryId, verb) {
  return `/_api/pregel3/query/${queryId}/${verb}`;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a pregel query
// //////////////////////////////////////////////////////////////////////////////
const createQuery = function(graphSpec) {
  let requestResult = db._connection.POST(api_query, { graphSpec: graphSpec });
  arangosh.checkRequestResult(requestResult);
  return requestResult;
};

const loadGraph = function(queryId) {
  let requestResult = db._connection.GET(api_query_verb(queryId, 'loadGraph'));
  arangosh.checkRequestResult(requestResult);
  return requestResult;
}

exports.createQuery = createQuery;
exports.loadGraph = loadGraph;
