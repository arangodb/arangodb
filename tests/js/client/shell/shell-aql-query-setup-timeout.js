/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief timeouts during query setup
// /
// / DISCLAIMER
// /
// / Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
let arangodb = require('@arangodb');
let db = arangodb.db;
let { getEndpointsByType,
      getEndpointById,
      debugCanUseFailAt,
      debugClearFailAt,
      debugSetFailAt,
      waitForShardsInSync
    } = require('@arangodb/test-helper');
const ERRORS = arangodb.errors;
      
function aqlQuerySetupTimeout() {
  'use strict';
  const cn = 'UnitTestsReplication';

  return {

    setUp: function () {
      getEndpointsByType("coordinator").forEach((ep) => debugClearFailAt(ep));
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
    },

    tearDown: function () {
      getEndpointsByType("coordinator").forEach((ep) => debugClearFailAt(ep));
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
    },
    
    testSetupTimeout: function() {
      let c = db._create(cn, { numberOfShards: 1, replicationFactor: 2 });
     
      // these failure points will make the query setup sleep for 3 seconds
      // on the db server, and set the setup timeout for query on the coordinator
      // to 0.1 seconds.
      // that means the first setup request will instantly fail with "timeout
      // in cluster operation". the coordinator will then send a cleanup request,
      // which will add a tombstone for the query
      // once the DB server will execute the (delayed) query, it will see the
      // tombstone and fail
      getEndpointsByType("coordinator").forEach((ep) => debugSetFailAt(ep, "Query::setupTimeoutFailSequence"));
      getEndpointsByType("dbserver").forEach((ep) => debugSetFailAt(ep, "Query::setupTimeoutFailSequence"));

      try {
        db._query("FOR i IN 1..1000 INSERT {} INTO " + cn);
        fail();
      } catch (e) {
        assertTrue(e.errorNum === ERRORS.ERROR_CLUSTER_TIMEOUT.code ||
                   e.errorNum === ERRORS.ERROR_CLUSTER_CONNECTION_LOST.code);
      }

      assertEqual(0, c.count());

      // make sure that query isn't executed after the sleep period on the db server
      require("internal").sleep(5);
      assertEqual(0, c.count());
    },
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length && debugCanUseFailAt(ep[0])) {
  // only execute if failure tests are available
  jsunity.run(aqlQuerySetupTimeout);
}
return jsunity.done();
