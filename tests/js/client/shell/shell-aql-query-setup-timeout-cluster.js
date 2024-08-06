/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, arango */

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
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let arangodb = require('@arangodb');
let db = arangodb.db;
const ERRORS = arangodb.errors;
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

function aqlQuerySetupTimeout() {
  'use strict';
  const cn = 'UnitTestsReplication';

  return {

    setUp: function () {
      IM.debugClearFailAt();
      db._drop(cn);
    },

    tearDown: function () {
      IM.debugClearFailAt();
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
      IM.debugSetFailAt("Query::setupTimeoutFailSequence", instanceRole.dbServer);
      IM.debugSetFailAt("Query::setupTimeoutFailSequence", instanceRole.coordinator);

      try {
        db._query("FOR i IN 1..1000 INSERT {} INTO " + cn);
        fail();
      } catch (e) {
        assertTrue(e.errorNum === ERRORS.ERROR_CLUSTER_TIMEOUT.code ||
                   e.errorNum === ERRORS.ERROR_CLUSTER_CONNECTION_LOST.code, JSON.stringify(e));
      }

      assertEqual(0, c.count());

      // make sure that query isn't executed after the sleep period on the db server
      require("internal").sleep(5);
      assertEqual(0, c.count());
    },
  };
}

if (IM.debugCanUseFailAt()) {
  // only execute if failure tests are available
  jsunity.run(aqlQuerySetupTimeout);
}
return jsunity.done();
