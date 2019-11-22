/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertNull, assertTrue, assertUndefined, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

let jsunity = require("jsunity");
let arangodb = require("@arangodb");
let internal = require("internal");
let db = arangodb.db;

function ResponsibleShardSuite() {
  const cn = "UnitTestsCollection1";

  return {

    setUp : function () {
      db._drop(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },
    
    testCheckMissingParameter : function () {
      let c = db._create(cn, { numberOfShards: 5, shardKeys: ["_key"] });

      let key = c.insert({})._key;

      let shardCounts = c.count(true);

      let total = 0;
      let expected = null;
      assertEqual(5, Object.keys(shardCounts).length);
      Object.keys(shardCounts).forEach(function(key) {
        let count = shardCounts[key];
        total += count;
        if (count > 0) {
          expected = key;
        }
      });

      assertEqual(1, total);
      assertEqual(expected, c.getResponsibleShard(key));
      assertEqual(expected, c.getResponsibleShard({ _key: key }));
    },
    
    testCheckBrokenParameter : function () {
      let c = db._create(cn, { numberOfShards: 5, shardKeys: ["_key"] });

      assertNotEqual("", c.getResponsibleShard(12345));
    },
    
    testCheckBrokenParameterArray : function () {
      let c = db._create(cn, { numberOfShards: 5, shardKeys: ["_key"] });

      try {
        c.getResponsibleShard([]);
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCheckKeyNotGiven : function () {
      let c = db._create(cn, { numberOfShards: 5, shardKeys: ["_key"] });

      try {
        c.getResponsibleShard({ test: "foo" });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN.code, err.errorNum);
      }
    },

    testCheckWithShardKeyKey : function () {
      let c = db._create(cn, { numberOfShards: 5, shardKeys: ["_key"] });

      c.insert({ _key: "meow" });

      let shardCounts = c.count(true);

      let total = 0;
      let expected = null;
      assertEqual(5, Object.keys(shardCounts).length);
      Object.keys(shardCounts).forEach(function(key) {
        let count = shardCounts[key];
        total += count;
        if (count > 0) {
          expected = key;
        }
      });

      assertEqual(1, total);
      assertEqual(expected, c.getResponsibleShard("meow"));
      assertEqual(expected, c.getResponsibleShard({ _key: "meow" }));
    },
    
    testCheckWithShardKeyOther : function () {
      let c = db._create(cn, { numberOfShards: 5, shardKeys: ["test"] });

      c.insert({ test: "abc" });

      let shardCounts = c.count(true);

      let total = 0;
      let expected = null;
      assertEqual(5, Object.keys(shardCounts).length);
      Object.keys(shardCounts).forEach(function(key) {
        let count = shardCounts[key];
        total += count;
        if (count > 0) {
          expected = key;
        }
      });

      assertEqual(1, total);
      assertEqual(expected, c.getResponsibleShard({ test: "abc" }));

      try {
        c.getResponsibleShard({});
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN.code, err.errorNum);
      }
    },
    
    testCheckWithShardKeysMultiple : function () {
      let c = db._create(cn, { numberOfShards: 5, shardKeys: ["test1", "test2"] });

      c.insert({ test1: "abc", test2: "nnsn" });

      let shardCounts = c.count(true);

      let total = 0;
      let expected = null;
      assertEqual(5, Object.keys(shardCounts).length);
      Object.keys(shardCounts).forEach(function(key) {
        let count = shardCounts[key];
        total += count;
        if (count > 0) {
          expected = key;
        }
      });

      assertEqual(1, total);
      assertEqual(expected, c.getResponsibleShard({ test1: "abc", test2: "nnsn" }));
    },
    
    testCheckWithShardKeysPartial : function () {
      let c = db._create(cn, { numberOfShards: 5, shardKeys: ["test1", "test2"] });

      c.insert({ test1: "abc" });

      let shardCounts = c.count(true);

      let total = 0;
      let expected = null;
      assertEqual(5, Object.keys(shardCounts).length);
      Object.keys(shardCounts).forEach(function(key) {
        let count = shardCounts[key];
        total += count;
        if (count > 0) {
          expected = key;
        }
      });

      assertEqual(1, total);
      assertEqual(expected, c.getResponsibleShard({ test1: "abc", test2: null }));
      try {
        c.getResponsibleShard({ test1: "abc" });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN.code, err.errorNum);
      }
    },
    
    testCheckWithShardKeysPartialOther : function () {
      let c = db._create(cn, { numberOfShards: 5, shardKeys: ["test1", "test2"] });

      c.insert({ test2: "abc", test1: null });

      let shardCounts = c.count(true);

      let total = 0;
      let expected = null;
      assertEqual(5, Object.keys(shardCounts).length);
      Object.keys(shardCounts).forEach(function(key) {
        let count = shardCounts[key];
        total += count;
        if (count > 0) {
          expected = key;
        }
      });

      assertEqual(1, total);
      assertEqual(expected, c.getResponsibleShard({ test2: "abc", test1: null }));
      try {
        c.getResponsibleShard({ test2: "abc" });
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN.code, err.errorNum);
      }
    },

  };
}

jsunity.run(ResponsibleShardSuite);

return jsunity.done();
