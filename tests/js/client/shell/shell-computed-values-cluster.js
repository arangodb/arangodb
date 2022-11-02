/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global assertTrue, assertEqual */

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const request = require("@arangodb/request");

const cn = "UnitTestsCollection";

const {
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
          overwrite: false
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

      // contact DB servers and verify they have have stored the same RAND()
      // values via replication
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
