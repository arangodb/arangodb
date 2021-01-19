/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let errors = arangodb.errors;
let { getEndpointById, getEndpointsByType } = require('@arangodb/test-helper');
const request = require('@arangodb/request');

/// @brief set failure point
function debugCanUseFailAt(endpoint) {
  let res = request.get({
    url: endpoint + '/_admin/debug/failat',
  });
  return res.status === 200;
}

/// @brief set failure point
function debugSetFailAt(endpoint, failAt) {
  let res = request.put({
    url: endpoint + '/_admin/debug/failat/' + failAt,
    body: ""
  });
  if (res.status !== 200) {
    throw "Error setting failure point";
  }
}

function debugClearFailAt(endpoint) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat',
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure points";
  }
}

function createDatabaseFailureSuite() {
  'use strict';
  const cn = 'UnitTestsCreateDatabase';

  return {

    setUp: function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      try {
        db._dropDatabase(cn);
      } catch (err) {
      }
    },

    tearDown: function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
    },
    
    // make follower execute intermediate commits (before the leader), but let the
    // transaction succeed
    testCreateDatabaseWithFailure: function () {
      let endpoints = getEndpointsByType('dbserver');
      assertTrue(endpoints.length > 1, endpoints);
      endpoints.forEach((endpoint) => {
        debugSetFailAt(endpoint, "CreateDatabase::first");
      });

      try {
        db._createDatabase(cn);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE.code);
      }
    },
    
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length && debugCanUseFailAt(ep[0])) {
  // only execute if failure tests are available
  jsunity.run(createDatabaseFailureSuite);
}
return jsunity.done();
