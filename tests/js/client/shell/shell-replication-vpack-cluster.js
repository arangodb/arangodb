/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertNotEqual, assertTrue, assertFalse, arango, print */

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
// / @author Max Neunhöffer
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const db = arangodb.db;
let IM = global.instanceManager;
const { waitForShardsInSync } = require("@arangodb/test-helper");

function replicationSuite() {
  'use strict';
  const cn = 'UnitTestsCollection';

  return {

    setUp: function () {
      db._drop(cn);
      db._create(cn, { numberOfShards: 1, replicationFactor: 1 });
    },

    tearDown: function () {
      arango.forceJson(true);
      db._drop(cn);
    },
    
    testReplication: function() {
      // First insert 10000 documents using AQL to ensure that their
      // representation of the numbers is double:
      db._query(`FOR i IN 0..4999 INSERT { value: i+1 } INTO ${cn}`);
      let c = db._collection(cn);
      c.properties({replicationFactor:2});
      for (let i = 0; i < 1000; ++i) {
        db._query(`FOR j in ${i*10}..${i*10+9} INSERT { value: j+1 } INTO ${cn}`);
      }
      waitForShardsInSync(cn, 120, 1);

      let keys = db._query(`FOR doc IN ${cn} RETURN doc._key`).toArray();

      // Now verify that the data is there and contains doubles:
      arango.forceJson(false);

      let checkKeys = function() {
        print("Checking keys...");
        let count = 0;
        for (let key of keys) {
          ++count;
          if (count % 1000 === 0) {
            print("Done with " + count + " keys");
          }

          // Get the document as VelocyPack:
          let doc = arango.GET_RAW(`/_api/document/${cn}/${key}`, {accept:"application/x-velocypack"});
          assertEqual(200, doc.code);
          let body = [];
          for (let number of doc.body.values()) {
            body.push(number);
          }
          // Look for "value" and check the byte after that:
          let found = false;
          for (let pos = 0; pos < body.length - 6; ++pos) {
            if (body[pos]   === "v".charCodeAt(0) &&
                body[pos+1] === "a".charCodeAt(0) &&
                body[pos+2] === "l".charCodeAt(0) &&
                body[pos+3] === "u".charCodeAt(0) &&
                body[pos+4] === "e".charCodeAt(0)) {
              let byte = body[pos+5];
              assertEqual(0x1b, byte);    // VelocyPack double marker
              found = true;
              break;
            }
          }
          assertTrue(found);
        }
      };

      checkKeys();

      const [shard, servers] = Object.entries(c.shards(true))[0];
      // trigger move shard to change leadership:
      let body = {database: "_system", collection: cn, shard, 
                  fromServer: servers[0]  , toServer: servers[1]};
      let result = arango.POST("/_admin/cluster/moveShard", body);
      assertFalse(result.error);
      assertEqual(202, result.code);

      let done = false;
      for (let i = 0; i < 120; ++i) {
        const job = arango.GET(`/_admin/cluster/queryAgencyJob?id=${result.id}`);
        print("Waiting for moveShard job to finish:", job.status);
        if (job.error === false && job.status === "Finished") {
          done = true;
          break;
        }
        require('internal').wait(0.5);
      }
      if (!done) {
        throw new Error("moveShard did not finish in time");
      }

      checkKeys();
    }

  };
}

jsunity.run(replicationSuite);
return jsunity.done();
