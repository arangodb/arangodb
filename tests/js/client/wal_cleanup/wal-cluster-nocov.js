/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global print */

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
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const {assertEqual, assertTrue, assertFalse, assertNotEqual} = jsunity.jsUnity.assertions;
const arangodb = require('@arangodb');
const arango = arangodb.arango;
const db = arangodb.db;
let IM = global.instanceManager;
let { instanceRole } = require('@arangodb/testutils/instance');

const _ = require("lodash");

const {
  versionHas
} = require('@arangodb/test-helper');

const cn = "UnitTestsWalCleanup";

function WalCleanupSuite () {
  'use strict';

  let run = function(insertData, getRanges) {
    let seenInitialDeletion = false;
    let seenNewFileDeletion = false;
    let seenNewFile = false;
    let minArchivedLogNumber = null;
    let maxArchivedLogNumber = null;
    const timeout = versionHas('tsan')? 1200 : 600;
    let time = require("internal").time;
    const start = time();

    while (true) {
      insertData();
    
      let ranges = getRanges();
      if (ranges.length) {
        if (minArchivedLogNumber === null) {
          minArchivedLogNumber = ranges[0];
          maxArchivedLogNumber = ranges[ranges.length - 1];
          require("console").warn("minimum logfile number found:", minArchivedLogNumber, "maximum logfile number found:", maxArchivedLogNumber);
        }
        if (!seenNewFile && ranges[ranges.length - 1] > maxArchivedLogNumber) {
          // we have seen a new logfile in the archive
          seenNewFile = true;
          maxArchivedLogNumber = ranges[ranges.length - 1];
        } else if (seenNewFile && ranges[0] > maxArchivedLogNumber) {
          seenNewFileDeletion = true;
        }
        if (ranges[0] > minArchivedLogNumber) {
          // we have seen the deletion of at least one logfile
          seenInitialDeletion = true;
        }
      }

      if (seenInitialDeletion && seenNewFileDeletion) {
        break;
      }

      assertFalse(time() - start > timeout, "time's up for this test!");
    }
  };

  return {
    setUpAll: function() {
      IM.rememberConnection();
    },
    setUp: function() {
      IM.reconnectMe();
    },

    tearDown: function() {
      IM.reconnectMe();
    },

    testAgent: function() {
      const huge = Array(128).join("XYZ");
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ huge });
      }
    
      let getRanges = function() {
        return require("@arangodb/replication").logger.tickRanges().filter(function(r) {
          return r.status === 'collected';
        }).map(function(r) {
          return parseInt(r.datafile.replace(/^.*?(\d+)\.log$/, "$1"));
        });
      };
      
      let insertData = function() {
        let c = db._collection(cn);
        // require("console").warn("inserting more data");
        for (let i = 0; i < 200; ++i) {
          c.insert(docs);
        }
      };

      const agents = IM.getInstancesRole(instanceRole.agent);
      assertTrue(agents.length > 0, "no agents found");
      const agent = agents[0];
      print("connecting to ", agent.name);
      agent.toThisInstance(() => {
        try {
          db._drop(cn);
          db._create(cn);
          run(insertData, getRanges);
        } finally {
          db._drop(cn);
        }
      });
    },
    
    testDBServer: function() {
      const huge = Array(128).join("XYZ");
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ huge });
      }
        
      const coordinators = IM.getInstancesRole(instanceRole.coordinator);
      assertTrue(coordinators.length > 0, "no coordinators found");
      const coordinator = coordinators[0];
      
      const dbservers = IM.getInstancesRole(instanceRole.dbserver);
      assertTrue(dbservers.length > 0, "no dbservers found");
      const dbserver = dbservers[0];
        
      require("console").warn("connecting to dbserver", dbserver.name);
      
      let insertData = function() {
        coordinator.toThisInstance(() => {
          let c = db._collection(cn);
          // require("console").warn("inserting more data");
          for (let i = 0; i < 250; ++i) {
            c.insert(docs);
          }
        });
      };
      
      let getRanges = function() {
        return dbserver.toThisInstance(() => {
          return require("@arangodb/replication").logger.tickRanges().filter(function(r) {
            return r.status === 'collected';
          }).map(function(r) {
            return parseInt(r.datafile.replace(/^.*?(\d+)\.log$/, "$1"));
          });
        });
      };

      try {
        coordinator.toThisInstance(() => {
          db._drop(cn);
          db._create(cn, { numberOfShards: dbservers.length }); // we must make sure that we insert data to each db server
          run(insertData, getRanges);
        });
      } finally {
        IM.reconnectMe();
        db._drop(cn);
      }
    },

  };
}

jsunity.run(WalCleanupSuite);

return jsunity.done();
