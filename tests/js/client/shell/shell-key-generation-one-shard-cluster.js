/*jshint globalstrict:false, strict:false */
/*global arango, assertEqual, assertTrue, assertFalse, assertMatch */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Test Author Max Neunhoeffer
/// @author Copyright 2025, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require("internal");
const isCluster = internal.isCluster();
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

const cn = "UnitTestsKeyGenerationOneShard";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: key generation in one-shard collection
////////////////////////////////////////////////////////////////////////////////

function KeyGenerationOneShardSuite() {
  'use strict';

  return {

    setUp: function () {
      // Clean up any existing collection
      try {
        db._drop(cn);
      } catch (err) {
        // Collection might not exist, ignore
      }
    },

    tearDown: function () {
      IM.debugClearFailAt('always-fetch-new-cluster-wide-uniqid', instanceRole.dbServer);
      // Clean up the test collection
      try {
        db._drop(cn);
      } catch (err) {
        // Collection might not exist, ignore
      }
    },

    testKeyGenerationOneShardCollection: function () {
      // Create a collection with exactly one shard
      let c = db._create(cn, { 
        numberOfShards: 1,
        keyOptions: { 
          type: "traditional",
          allowUserKeys: false 
        }
      });

      // Verify the collection has exactly one shard
      assertEqual(1, c.shards().length, "Collection should have exactly one shard");
      assertEqual(cn, c.name(), "Collection name should match");

      // Set failure points to always fetch new cluster-wide unique IDs from agency on dbservers:
      IM.debugSetFailAt('always-fetch-new-cluster-wide-uniqid', instanceRole.dbServer);

      let res = arango.PUT("/_admin/cluster/uniqId?minimum=100000000",{});
      assertFalse(res.error);
      assertEqual(200, res.code, `must return HTTP code 200 but got ${res.code}`);

      // Insert documents using the AQL statement
      let query = `
        LET l = [{Hello:1},{Hello:2}]
        FOR doc IN l
          INSERT doc INTO ${cn}
          RETURN NEW
      `;

      let result = db._query(query).toArray();

      // Verify we got 2 results back
      assertEqual(2, result.length, "Should have inserted 2 documents");

      // Check that both documents have automatically generated keys
      result.forEach((doc, index) => {
        assertTrue(doc.hasOwnProperty('_key'), `Document ${index} should have _key property`);
        assertTrue(doc.hasOwnProperty('_id'), `Document ${index} should have _id property`);
        assertTrue(doc.hasOwnProperty('_rev'), `Document ${index} should have _rev property`);
        assertTrue(doc.hasOwnProperty('Hello'), `Document ${index} should have Hello property`);
        
        // Verify the Hello property values
        if (index === 0) {
          assertEqual(1, doc.Hello, `First document should have Hello: 1`);
        } else {
          assertEqual(2, doc.Hello, `Second document should have Hello: 2`);
        }

        // Verify the _key is a valid traditional key (numeric string)
        assertMatch(/^\d+$/, doc._key, `_key should be a numeric string: ${doc._key}`);
        
        // Verify the _id follows the pattern collectionName/_key
        assertMatch(new RegExp(`^${cn}/\\d+$`), doc._id, `_id should follow pattern: ${doc._id}`);
      });

      // Verify the collection count
      assertEqual(2, c.count(), "Collection should contain exactly 2 documents");

      // Verify the documents exist in the collection
      let allDocs = c.toArray();
      assertEqual(2, allDocs.length, "Collection should return exactly 2 documents");

      // Check that the keys are large:
      let keys = allDocs.map(doc => parseInt(doc._key)).sort((a, b) => a - b);
      assertTrue(keys[0] >= 100000000, `First key must be at least 100000000 and not ${keys[0]}`);
      assertTrue(keys[1] >= 100000000, `Second key be must be at leaset 100000000 and not ${keys[1]}`);
    },

    testKeyGenerationOneShardCollectionWithPaddedKeyOptions: function () {
      // Create a collection with exactly one shard and padded key options
      let c = db._create(cn, { 
      numberOfShards: 1,
        keyOptions: { 
          type: "padded",
          allowUserKeys: false,
        }
      });

      // Verify the collection has exactly one shard
      assertEqual(1, c.shards().length, "Collection should have exactly one shard");

      // Set failure points to always fetch new cluster-wide unique IDs from agency on dbservers:
      IM.debugSetFailAt('always-fetch-new-cluster-wide-uniqid', instanceRole.dbServer);

      let res = arango.PUT("/_admin/cluster/uniqId?minimum=100000000",{});
      assertFalse(res.error);
      assertEqual(200, res.code, `must return HTTP code 200 but got ${res.code}`);

      // Insert documents using the AQL statement
      let query = `
        LET l = [{Hello:1},{Hello:2}]
        FOR doc IN l
          INSERT doc INTO ${cn}
          RETURN NEW
      `;

      let result = db._query(query).toArray();

      // Verify we got 2 results back
      assertEqual(2, result.length, "Should have inserted 2 documents");

      // Check that both documents have automatically generated keys
      result.forEach((doc, index) => {
        assertTrue(doc.hasOwnProperty('_key'), `Document ${index} should have _key property`);
        assertTrue(doc.hasOwnProperty('_id'), `Document ${index} should have _id property`);
        assertTrue(doc.hasOwnProperty('_rev'), `Document ${index} should have _rev property`);
        assertTrue(doc.hasOwnProperty('Hello'), `Document ${index} should have Hello property`);
      });

      // Verify the collection count
      assertEqual(2, c.count(), "Collection should contain exactly 2 documents");
      
      // Verify the documents exist in the collection
      let allDocs = c.toArray();
      assertEqual(2, allDocs.length, "Collection should return exactly 2 documents");

      // Check that the keys are large, note that in the padded case we see hex numbers!:
      let keys = allDocs.map(doc => doc._key);
      assertTrue(keys[0] >= "0000000005f5e100", `First key must be at least 0000000005f5e100 and not ${keys[0]}`);
      assertTrue(keys[1] >= "0000000005f5e100", `Second key must be at least 0000000005f5e100 and not ${keys[1]}`);
    },

    testKeyGenerationMultiShardCollectionForced: function () {
      // Create a collection with 3 shards:
      let c = db._create(cn, { 
        numberOfShards: 3,
        shardKeys: ["s"],
        keyOptions: { 
          type: "traditional",
          allowUserKeys: false 
        }
      });

      // Verify the collection has three shards
      assertEqual(3, c.shards().length, "Collection should have three shards");
      assertEqual(cn, c.name(), "Collection name should match");

      // Set failure points to always fetch new cluster-wide unique IDs from agency on dbservers:
      IM.debugSetFailAt('always-fetch-new-cluster-wide-uniqid', instanceRole.dbServer);

      let res = arango.PUT("/_admin/cluster/uniqId?minimum=100000000",{});
      assertFalse(res.error);
      assertEqual(200, res.code, `must return HTTP code 200 but got ${res.code}`);

      // Insert documents using the AQL statement
      let query = `
        LET l = [{Hello:1, s: "a"},{Hello:2, s: "a"}]
        FOR doc IN l
          INSERT doc INTO ${cn}
          RETURN NEW
      `;

      let result = db._query(query, {}, {forceOneShardAttributeValue: "a"}).toArray();

      // Verify we got 2 results back
      assertEqual(2, result.length, "Should have inserted 2 documents");

      // Check that both documents have automatically generated keys
      result.forEach((doc, index) => {
        assertTrue(doc.hasOwnProperty('_key'), `Document ${index} should have _key property`);
        assertTrue(doc.hasOwnProperty('_id'), `Document ${index} should have _id property`);
        assertTrue(doc.hasOwnProperty('_rev'), `Document ${index} should have _rev property`);
        assertTrue(doc.hasOwnProperty('Hello'), `Document ${index} should have Hello property`);
        
        // Verify the Hello property values
        if (index === 0) {
          assertEqual(1, doc.Hello, `First document should have Hello: 1`);
        } else {
          assertEqual(2, doc.Hello, `Second document should have Hello: 2`);
        }

        // Verify the _key is a valid traditional key (numeric string)
        assertMatch(/^\d+$/, doc._key, `_key should be a numeric string: ${doc._key}`);
        
        // Verify the _id follows the pattern collectionName/_key
        assertMatch(new RegExp(`^${cn}/\\d+$`), doc._id, `_id should follow pattern: ${doc._id}`);
      });

      // Verify the collection count
      assertEqual(2, c.count(), "Collection should contain exactly 2 documents");

      // Verify the documents exist in the collection
      let allDocs = c.toArray();
      assertEqual(2, allDocs.length, "Collection should return exactly 2 documents");

      // Check that the keys are large:
      let keys = allDocs.map(doc => parseInt(doc._key)).sort((a, b) => a - b);
      assertTrue(keys[0] >= 100000000, "First key must be at least 100000000");
      assertTrue(keys[1] >= 100000000, "Second key be must be at leaset 100000000");
    },

    testKeyGenerationMultiShardCollectionForcedWithPaddedKeyOptions: function () {
      // Create a collection with exactly one shard and padded key options
      let c = db._create(cn, { 
        numberOfShards: 3,
        shardKeys: ["s"],
        keyOptions: { 
          type: "padded",
          allowUserKeys: false,
        }
      });

      // Verify the collection has exactly one shard
      assertEqual(3, c.shards().length, "Collection should have exactly three shards");

      // Set failure points to always fetch new cluster-wide unique IDs from agency on dbservers:
      IM.debugSetFailAt('always-fetch-new-cluster-wide-uniqid', instanceRole.dbServer);

      let res = arango.PUT("/_admin/cluster/uniqId?minimum=100000000",{});
      assertFalse(res.error);
      assertEqual(200, res.code, `must return HTTP code 200 but got ${res.code}`);

      // Insert documents using the AQL statement
      let query = `
        LET l = [{Hello:1, s: "a"},{Hello:2, s: "a"}]
        FOR doc IN l
          INSERT doc INTO ${cn}
          RETURN NEW
      `;

      let result = db._query(query, {}, {forceOneShardAttributeValue: "a"}).toArray();

      // Verify we got 2 results back
      assertEqual(2, result.length, "Should have inserted 2 documents");

      // Check that both documents have automatically generated keys
      result.forEach((doc, index) => {
        assertTrue(doc.hasOwnProperty('_key'), `Document ${index} should have _key property`);
        assertTrue(doc.hasOwnProperty('_id'), `Document ${index} should have _id property`);
        assertTrue(doc.hasOwnProperty('_rev'), `Document ${index} should have _rev property`);
        assertTrue(doc.hasOwnProperty('Hello'), `Document ${index} should have Hello property`);
      });

      // Verify the collection count
      assertEqual(2, c.count(), "Collection should contain exactly 2 documents");
      
      // Verify the documents exist in the collection
      let allDocs = c.toArray();
      assertEqual(2, allDocs.length, "Collection should return exactly 2 documents");

      // Check that the keys are large, note that in the padded case we see hex numbers!:
      let keys = allDocs.map(doc => doc._key);
      assertTrue(keys[0] >= "0000000005f5e100", `First key must be at least 0000000005f5e100 and not ${keys[0]}`);
      assertTrue(keys[1] >= "0000000005f5e100", `Second key must be at least 0000000005f5e100 and not ${keys[1]}`);
    },

  };
}

// Run the appropriate test suite based on cluster mode
jsunity.run(KeyGenerationOneShardSuite);
return jsunity.done();
