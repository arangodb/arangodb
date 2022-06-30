/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for inventory
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Julia Puget
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const request = require("@arangodb/request");
const {deriveTestSuite, waitForShardsInSync} = require('@arangodb/test-helper');

const cn = "UnitTestsCollection";

const {
  getCoordinators,
  getDBServers
} = require('@arangodb/test-helper');

let clearFailurePoints = function(failurePointsToKeep) {
  getDBServers().forEach((server) => {
    // clear all failure points
    request({method: "DELETE", url: server.url + "/_admin/debug/failat"});
    failurePointsToKeep.forEach((fp) => {
      getDBServers().forEach((server) => {
        let result = request({
          method: "PUT",
          url: server.url + "/_admin/debug/failat/" + encodeURIComponent(fp),
          body: {}
        });
        assertEqual(200, result.status);
      });
    });
  });
};

function BaseTestConfig() {
  'use strict';

  return {
    testFailureOnLeaderNoManagedTrx: function() {
      let c = db._create(cn, {
        numberOfShards: 1, computedValues: [{
          name: "value2",
          expression: "RETURN CONCAT(@doc.value1, '+', RAND())",
          override: false
        }]
      });
      for (let i = 0; i < 100; ++i) {
        c.insert({value1: "test" + i});
      }
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      let compuetdValuesBefore = [];
      let result = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      result.forEach(el => {
        assertTrue(el.hasOwnProperty("value2"));
        compuetdValuesBefore.push(el.value2);
      });


      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];
      let leader = shardInfo[shard][0];
      let leaderUrl = servers.filter((server) => server.id === leader)[0].url;

      // break leaseManagedTrx on the leader, so it will return a nullptr
      result = request({method: "PUT", url: leaderUrl + "/_admin/debug/failat/leaseManagedTrxFail", body: {}});
      assertEqual(200, result.status);

      // add a follower. this will kick off the getting-in-sync protocol,
      // which will eventually call the holdReadLockCollection API, which then
      // will call leaseManagedTrx and get the nullptr back
      c.properties({numberOfShards: 1, replicationFactor: 2});

      waitForShardsInSync(cn, 60);

      // wait until we have an in-sync follower
      let tries = 0;
      while (tries++ < 120) {
        shardInfo = c.shards(true);
        servers = shardInfo[shard];
        if (tries > 20) {
          if (servers.length === 2 && c.count() === 100) {
            break;
          }
        } else if (tries === 20) {
          // wait several seconds so we can be sure the
          clearFailurePoints([]);
        }
        require("internal").sleep(0.5);
      }
      assertEqual(2, servers.length);
      assertEqual(100, c.count());
      assertEqual(100, c.toArray().length);

      tries = 0;
      let total;
      while (tries++ < 120) {
        total = 0;
        getDBServers().forEach((server) => {
          if (servers.indexOf(server.id) === -1) {
            return;
          }
          let result = request({method: "GET", url: server.url + "/_api/collection/" + shard + "/count"});
          assertEqual(200, result.status);
          total += result.json.count;

          result = request({method: "GET", url: server.url + "/_api/collection/" + shard + "/properties"});
          let jsonRes = result.json;
          assertTrue(jsonRes.hasOwnProperty("computedValues"));
          assertEqual(jsonRes.computedValues.length, 1);
          assertTrue(jsonRes.computedValues[0].hasOwnProperty("name"));
          assertEqual(jsonRes.computedValues[0].name, "value2");

        });

        if (total === 2 * 100) {
          break;
        }

        require("internal").sleep(0.5);
      }

         assertEqual(2 * 100, total);
      result = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      result.forEach((el, index) => {
        assertTrue(el.hasOwnProperty("value2"));
        assertEqual(compuetdValuesBefore[index], el.value2);
      });

    },
  };
}

function collectionCountsSuiteOldFormat() {
  'use strict';

  let suite = {
    setUp: function() {
      // this disables usage of the new collection format
      clearFailurePoints(["disableRevisionsAsDocumentIds"]);
      db._drop(cn);
    },

    tearDown: function() {
      clearFailurePoints([]);
      db._drop(cn);
    }
  };

  deriveTestSuite(BaseTestConfig(), suite, '_OldFormat');
  return suite;
}

function collectionCountsSuiteNewFormat() {
  'use strict';

  let suite = {
    setUp: function() {
      clearFailurePoints([]);
      db._drop(cn);
    },

    tearDown: function() {
      clearFailurePoints([]);
      db._drop(cn);
    }
  };

  deriveTestSuite(BaseTestConfig(), suite, '_NewFormat');
  return suite;
}

let res = request({method: "GET", url: getCoordinators()[0].url + "/_admin/debug/failat"});
if (res.body === "true") {
  jsunity.run(collectionCountsSuiteOldFormat);
  jsunity.run(collectionCountsSuiteNewFormat);
}
return jsunity.done();
