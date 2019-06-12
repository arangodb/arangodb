/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, arango, require*/

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");

const db = require("internal").db;
const url = require('url');
const _ = require("lodash");
        
function getServers(role) {
  const matchesRole = (d) => (_.toLower(d.role) === role);
  const instanceInfo = JSON.parse(require('internal').env.INSTANCEINFO);
  return instanceInfo.arangods.filter(matchesRole);
}

const cn = "UnitTestsWalCleanup";
const originalEndpoint = arango.getEndpoint();

function WalCleanupSuite () {
  'use strict';
  
  let run = function(insertData, getRanges) {
    let seenInitialDeletion = false;
    let seenNewFileDeletion = false;
    let seenNewFile = false;
    let minArchivedLogNumber = null;
    let maxArchivedLogNumber = null;

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

      assertFalse(time() - start > 600, "time's up for this test!");
    }
  };

  return {
    setUp: function() {
      arango.reconnect(originalEndpoint, "_system", "root", "");
    },

    tearDown: function() {
      arango.reconnect(originalEndpoint, "_system", "root", "");
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

      const agents = getServers('agent');
      assertTrue(agents.length > 0, "no agents found");
      const agent = agents[0].endpoint;
      require("console").warn("connecting to agent", agent);
      arango.reconnect(agent, "_system", "root", "");
      try {
        db._drop(cn);
        db._create(cn);
        run(insertData, getRanges);
      } finally {
        db._drop(cn);
      }
    },
    
    testDBServer: function() {
      const huge = Array(128).join("XYZ");
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ huge });
      }
        
      const coordinators = getServers("coordinator");
      assertTrue(coordinators.length > 0, "no coordinators found");
      const coordinator = coordinators[0].endpoint;
      
      const dbservers = getServers("dbserver");
      assertTrue(dbservers.length > 0, "no dbservers found");
      const dbserver = dbservers[0].endpoint;
        
      require("console").warn("connecting to dbserver", dbserver);
      
      let insertData = function() {
        arango.reconnect(coordinator, "_system", "root", "");
        let c = db._collection(cn);
        // require("console").warn("inserting more data");
        for (let i = 0; i < 250; ++i) {
          c.insert(docs);
        }
      };
      
      let getRanges = function() {
        arango.reconnect(dbserver, "_system", "root", "");
        return require("@arangodb/replication").logger.tickRanges().filter(function(r) {
          return r.status === 'collected';
        }).map(function(r) {
          return parseInt(r.datafile.replace(/^.*?(\d+)\.log$/, "$1"));
        });
      };

      try {
        arango.reconnect(coordinator, "_system", "root", "");
        db._drop(cn);
        db._create(cn, { numberOfShards: dbservers.length }); // we must make sure that we insert data to each db server
        run(insertData, getRanges);
      } finally {
        arango.reconnect(coordinator, "_system", "root", "");
        db._drop(cn);
      }
    },

  };
}

jsunity.run(WalCleanupSuite);

return jsunity.done();
