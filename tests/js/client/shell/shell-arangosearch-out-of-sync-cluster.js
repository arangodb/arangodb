/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, fail, assertTrue, assertFalse, assertEqual, assertNotEqual */
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const request = require("@arangodb/request");
const errors = require("internal").errors;
  
const {
  getCoordinators,
  getDBServers
} = require('@arangodb/test-helper');

let clearFailurePoints = function () {
  getDBServers().forEach((server) => {
    // clear all failure points
    request({ method: "DELETE", url: server.url + "/_admin/debug/failat" });
  });
};

function ArangoSearchOutOfSyncSuite () {
  'use strict';
  
  return {
    setUp : function () {
      clearFailurePoints([]);
      db._dropView('UnitTestsRecoveryView1');
      db._dropView('UnitTestsRecoveryView2');
      db._drop('UnitTestsRecovery1');
      db._drop('UnitTestsRecovery2');
    },

    tearDown : function () {
      clearFailurePoints([]);
      db._dropView('UnitTestsRecoveryView1');
      db._dropView('UnitTestsRecoveryView2');
      db._drop('UnitTestsRecovery1');
      db._drop('UnitTestsRecovery2');
    },

    testMarkLinksAsOutOfSync : function () {
      let c = db._create('UnitTestsRecovery1', { numberOfShards: 5 });

      let v = db._createView('UnitTestsRecoveryView1', 'arangosearch', {});
      v.properties({ links: { UnitTestsRecovery1: { includeAllFields: true } } });

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shards = Object.keys(shardInfo);

      shards.forEach((s) => {
        let leader = shardInfo[s][0];
        let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

        // break search commit on the DB servers
        let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/" + encodeURIComponent("ArangoSearch::FailOnCommit"), body: {} });
        assertEqual(200, result.status);
      });
      
      c.insert({});
  
      // wait for synchronization of links
      try {
        db._query("FOR doc IN UnitTestsRecoveryView1 OPTIONS {waitForSync: true} RETURN doc");
      } catch (err) {
      }
 
      clearFailurePoints();
      
      c = db._create('UnitTestsRecovery2', { numberOfShards: 5 });
      c.insert({});
      
      v = db._createView('UnitTestsRecoveryView2', 'arangosearch', {});
      v.properties({ links: { UnitTestsRecovery2: { includeAllFields: true } } });
     
      db._query("FOR doc IN UnitTestsRecoveryView2 OPTIONS {waitForSync: true} RETURN doc");

      let p = db._view('UnitTestsRecoveryView1').properties();
      // in a cluster, the coordinator doesn't make the outOfSync flag available
      assertFalse(p.links.UnitTestsRecovery1.hasOwnProperty('outOfSync'));
      
      shards.forEach((s) => {
        let leader = shardInfo[s][0];
        let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

        // break search commit on the DB servers
        let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/" + encodeURIComponent("ArangoSearch::FailQueriesOnOutOfSync"), body: {} });
        assertEqual(200, result.status);
      });
      
      // query must fail because links are marked as out of sync
      try {
        db._query("FOR doc IN UnitTestsRecoveryView1 OPTIONS {waitForSync: true} RETURN doc");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC.code, err.errorNum);
      }
      
      p = db._view('UnitTestsRecoveryView2').properties();
      assertFalse(p.links.UnitTestsRecovery2.hasOwnProperty('outOfSync'));
      
      // query must not fail
      let result = db._query("FOR doc IN UnitTestsRecoveryView2 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(1, result.length);
    
      // clear all failure points
      clearFailurePoints();
      
      // queries must not fail now because we removed the failure point
      result = db._query("FOR doc IN UnitTestsRecoveryView1 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(1, result.length);
        
      result = db._query("FOR doc IN UnitTestsRecoveryView2 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(1, result.length);
    },

  };
}

let res = request({ method: "GET", url: getCoordinators()[0].url + "/_admin/debug/failat" });
if (res.body === "true") {
  jsunity.run(ArangoSearchOutOfSyncSuite);
}
return jsunity.done();
