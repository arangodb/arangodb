/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertFalse, assertEqual, assertNotEqual */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const request = require("@arangodb/request");
const _ = require("lodash");
const { waitForShardsInSync } = require('@arangodb/test-helper');
  
const cn = "UnitTestsCollection";

const {
  getCoordinators,
  getDBServers
} = require('@arangodb/test-helper');

function abortReplicationSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  
  return {
    setUp : function () {
      let c = db._create(cn, { numberOfShards: 5, replicationFactor: 1 });
      let docs = [];
      // populate collection with 2M documents. must be a non-trivial amount
      // because we want to run many fetchCollectionDump requests
      for (let i = 0; i < 2 * 1000 * 1000; ++i) {
        docs.push({ value1: i, value2: "test" + i });
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
    },

    tearDown : function () {
      db._drop(cn);
    },

    testAbortReplication : function () {
      let c = db._collection(cn);
      const isCov = require("@arangodb/test-helper").versionHas('coverage');
      let factor = (isCov) ? 4 : 1;

      const servers = getDBServers();
      assertTrue(servers.length >= 2, servers);

      try {
        servers.forEach((server) => {
          // set failure point on each DB server, which will trigger an error in replication
          let result = request({ method: "PUT", url: server.url + "/_admin/debug/failat/Replication%3A%3AforceCheckCancellation", body: {} });
          assertEqual(200, result.status);
        });

        // now increase replicationFactor from 1 to whatever number of DB servers we have
        c.properties({ replicationFactor: servers.length });

        // give servers some time to run into the problem
        console.warn("waiting for servers to run into problems...");
        require("internal").sleep(45);

        // clear the failure points
        servers.forEach((server) => {
          console.warn("clearing failure points on " + server.url);
          request({ method: "DELETE", url: server.url + "/_admin/debug/failat" });
        });
      
        // wait for shards to get into sync - this really can take long on a slow CI
        waitForShardsInSync(cn, 180 * factor, servers.length - 1);

      } finally {
        servers.forEach((server) => {
          request({ method: "DELETE", url: server.url + "/_admin/debug/failat" });
        });
      }
    },

  };
}

let res = request({ method: "GET", url: getCoordinators()[0].url + "/_admin/debug/failat" });
if (res.body === "true") {
  jsunity.run(abortReplicationSuite);
}
return jsunity.done();
