/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, print, fail, arango */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////
"use strict";

const jsunity = require("jsunity");
const {aql, db, errors} = require("@arangodb");
const internal = require("internal");

function aqlSatelliteMaterializeRegressionTestSuite() {
  const database = "UnitTestsSatelliteMaterializeRegression";

  const cn1 = "UnitTestSatelliteCollection1";
  const cn2 = "UnitTestSatelliteCollection2";

  return {
    setUpAll: function() {
      db._createDatabase(database);
      db._useDatabase(database);
      let c1 = db._create(cn1, {replicationFactor: "satellite", numberOfShards: 1});
      let c2 = db._create(cn2, {replicationFactor: "satellite", numberOfShards: 1});

      let c1Shards = c1.shards(true);
      let c1Servers = c1Shards[Object.keys(c1Shards)[0]];

      let c2Shards = c2.shards(true);
      let c2Servers = c2Shards[Object.keys(c2Shards)[0]];

      // The bug we test for is triggered by 2 satellite collections
      // having different leaders, so if the leaders are the same,
      // try to moveShard one of the collections' leaders around
      if(c1Servers[0] === c2Servers[0]) {
        // Schedule MoveShard operation
        const moveShardResult = arango.POST("/_admin/cluster/moveShard", {
          database: database,
          collection: cn1,
          shard: Object.keys(c1),
          fromServer: c1Servers[0],
          toServer: c1Servers[1]
        });
        const moveShardId = moveShardResult.id;

        let moveShardNotFinished = false;
        let maxWait = 30; // 30 seconds timeout
        while (maxWait > 0) {
          const jobStatus = arango.GET(`/_admin/cluster/queryAgencyJob?id=${moveShardId}`);
          if (jobStatus.status === "Finished") {
            moveShardNotFinished = true;
            break;
          }
          internal.wait(1);
          maxWait--;
        }
      }

      db._query(`FOR i IN 1..1000
                   INSERT {_key: CONCAT("${cn1}", i), i}
                   INTO ${cn1}`);
      db._query(`FOR i IN 1..1000
                   INSERT {_key: CONCAT("${cn2}",i), i}
                   INTO ${cn2}`);

      
    },
    tearDownAll: function() {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testSatelliteMaterializeRegression: function() {
      let query = `
        FOR se IN ${cn1}
          FOR sv IN ${cn2}
            FILTER sv._id == se._id 
            RETURN [se, sv]`;

      // this crashes in maintainer mode prior to the patch
      // introduced in PR #22776
      // if the two satellite collections created above have
      // different leaders.
      db._query(query);
    }
  };
}

jsunity.run(aqlSatelliteMaterializeRegressionTestSuite);
return jsunity.done();

