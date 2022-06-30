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

function collectionComputedValuesClusterSuite() {
  'use strict';

  return {

    tearDown: function() {
      db._drop(cn);
    },

    testComputedValuesWithRand: function() {
      let c = db._create(cn, {
        numberOfShards: 1, replicationFactor: 2, computedValues: [{
          name: "randValue",
          expression: "RETURN TO_STRING(RAND())",
          override: false
        }]
      });
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({});
      }
      c.insert(docs);

      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shard = Object.keys(shardInfo)[0];

      let randValues = {};

      getDBServers().forEach(server => {
        if (servers.indexOf(server.id) === -1) {
          return;
        }
        randValues[server.id] = {};

        let result = request({method: "GET", url: server.url + "/_api/collection/" + shard + "/properties"});
        let jsonRes = result.json;
        assertTrue(jsonRes.hasOwnProperty("computedValues"));
        assertEqual(jsonRes.computedValues.length, 1);
        assertTrue(jsonRes.computedValues[0].hasOwnProperty("name"));
        assertEqual(jsonRes.computedValues[0].name, "value2");

        c.toArray().forEach(el => {
          result = request({method: "GET", url: server.url + "/_api/document/" + el._id});
          jsonRes = result.json;
          assertTrue(jsonRes.hasOwnProperty("randValue"));
          randValues[server.id][el._key] = el.randValue;
        });

      });
      assertEqual(randValues[servers[0].id], randValues[servers[1].id]);
    },
  };
}

jsunity.run(collectionComputedValuesClusterSuite);
return jsunity.done();
