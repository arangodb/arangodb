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
let db = arangodb.db;
let ERRORS = arangodb.errors;
const isEnterprise = require("internal").isEnterprise();
const communityCompat = "community-compat";
const enterpriseCompat = "enterprise-compat";
const defaultSharding = isEnterprise ? enterpriseCompat : communityCompat;

function DocumentShardingSuite() {
  let name1 = "UnitTestsCollection1";
  let name2 = "UnitTestsCollection2";

  return {

    setUp : function () {
      db._drop(name2);
      db._drop(name1);
    },

    tearDown : function () {
      db._drop(name2);
      db._drop(name1);
    },

    testCreateWithInvalidSharding : function () {
      try {
        db._create(name1, { shardingStrategy: "peng", numberOfShards: 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testCreateWithNoneSharding : function () {
      try {
        db._create(name1, { shardingStrategy: "none", numberOfShards: 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testCreateWithEnterpriseSharding : function () {
      let c = db._create(name1, { shardingStrategy: enterpriseCompat, numberOfShards: 5 });
      assertEqual(enterpriseCompat, c.properties()["shardingStrategy"]);
      assertEqual(5, c.properties()["numberOfShards"]);
      assertEqual(["_key"], c.properties()["shardKeys"]);

      for (let i = 0; i < 1000; ++i) {
        c.insert({ _key: "test" + i, value: i });
      }

      assertEqual([ 188, 192, 198, 204, 218 ], Object.values(c.count(true)).sort());

      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, c.document("test" + i).value);
      }
    },
    
    testCreateWithEnterpriseShardingNonDefaultKeysNumericValues : function () {
      let c = db._create(name1, { shardingStrategy: enterpriseCompat, shardKeys: ["value"], numberOfShards: 5 });
      assertEqual(enterpriseCompat, c.properties()["shardingStrategy"]);
      assertEqual(5, c.properties()["numberOfShards"]);
      assertEqual(["value"], c.properties()["shardKeys"]);

      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(c.insert({ value: i })._key);
      }

      assertEqual([ 0, 0, 0, 0, 1000 ], Object.values(c.count(true)).sort());

      keys.forEach(function(k, i) {
        assertEqual(i, c.document(k).value);
      });
    },

    testCreateWithCommunitySharding : function () {
      let c = db._create(name1, { shardingStrategy: communityCompat, numberOfShards: 5 });
      assertEqual(communityCompat, c.properties()["shardingStrategy"]);
      assertEqual(5, c.properties()["numberOfShards"]);
      assertEqual(["_key"], c.properties()["shardKeys"]);

      for (let i = 0; i < 1000; ++i) {
        c.insert({ _key: "test" + i, value: i });
      }

      assertEqual([ 188, 192, 198, 204, 218 ], Object.values(c.count(true)).sort());

      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, c.document("test" + i).value);
      }
    },

    testDistributeShardsLikeWithCommunitySharding : function () {
      let c1 = db._create(name1, { shardingStrategy: communityCompat, numberOfShards: 5 });
      let c2 = db._create(name2, { distributeShardsLike: name1 });
      assertEqual(communityCompat, c1.properties()["shardingStrategy"]);
      assertEqual(communityCompat, c2.properties()["shardingStrategy"]);
      assertEqual(5, c1.properties()["numberOfShards"]);
      assertEqual(5, c2.properties()["numberOfShards"]);
      assertEqual(["_key"], c1.properties()["shardKeys"]);
      assertEqual(["_key"], c2.properties()["shardKeys"]);

      for (let i = 0; i < 1000; ++i) {
        c2.insert({ _key: "test" + i, value: i });
      }

      assertEqual([ 188, 192, 198, 204, 218 ], Object.values(c2.count(true)).sort());

      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, c2.document("test" + i).value);
      }
    },

    testDistributeShardsLikeWithDiffentKeyAttribute : function () {
      let c1 = db._create(name1, { shardingStrategy: communityCompat, numberOfShards: 5, shardKeys: ["one"] });
      let c2 = db._create(name2, { distributeShardsLike: name1, shardKeys: ["two"] });
      assertEqual(communityCompat, c1.properties()["shardingStrategy"]);
      assertEqual(communityCompat, c2.properties()["shardingStrategy"]);
      assertEqual(5, c1.properties()["numberOfShards"]);
      assertEqual(5, c2.properties()["numberOfShards"]);
      assertEqual(["one"], c1.properties()["shardKeys"]);
      assertEqual(["two"], c2.properties()["shardKeys"]);

      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(c2.insert({ two: i })._key);
      }

      assertEqual([ 179, 192, 200, 207, 222 ], Object.values(c2.count(true)).sort());

      keys.forEach(function(k, i) {
        assertEqual(i, c2.document(k).two);
      });
    },

    testDistributeShardsLikeWithDiffentKeyAttributes : function () {
      let c1 = db._create(name1, { shardingStrategy: communityCompat, numberOfShards: 5, shardKeys: ["one", "two"] });
      let c2 = db._create(name2, { distributeShardsLike: name1, shardKeys: ["three", "four"] });
      assertEqual(communityCompat, c1.properties()["shardingStrategy"]);
      assertEqual(communityCompat, c2.properties()["shardingStrategy"]);
      assertEqual(5, c1.properties()["numberOfShards"]);
      assertEqual(5, c2.properties()["numberOfShards"]);
      assertEqual(["one", "two"], c1.properties()["shardKeys"]);
      assertEqual(["three", "four"], c2.properties()["shardKeys"]);

      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(c2.insert({ three: i, four: i + 1 })._key);
      }

      assertEqual([ 187, 188, 203, 208, 214 ], Object.values(c2.count(true)).sort());

      keys.forEach(function(k, i) {
        assertEqual(i, c2.document(k).three);
      });
    },

    testDistributeShardsLikeWithDiffentNumberOfKeyAttributes : function () {
      let c1 = db._create(name1, { shardingStrategy: communityCompat, numberOfShards: 5, shardKeys: ["one", "two"] });
      try {
        db._create(name2, { distributeShardsLike: name1, shardKeys: ["three"] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testCreateWithNonDefaultKeysMissingValues : function () {
      let c = db._create(name1, { shardingStrategy: communityCompat, shardKeys: ["value"], numberOfShards: 5 });
      assertEqual(communityCompat, c.properties()["shardingStrategy"]);
      assertEqual(5, c.properties()["numberOfShards"]);
      assertEqual(["value"], c.properties()["shardKeys"]);

      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(c.insert({ })._key);
      }

      assertEqual([ 0, 0, 0, 0, 1000 ], Object.values(c.count(true)).sort());

      keys.forEach(function(k, i) {
        assertUndefined(c.document(k).value);
      });
    },

    testCreateWithNonDefaultKeysNumericValues : function () {
      let c = db._create(name1, { shardingStrategy: communityCompat, shardKeys: ["value"], numberOfShards: 5 });
      assertEqual(communityCompat, c.properties()["shardingStrategy"]);
      assertEqual(5, c.properties()["numberOfShards"]);
      assertEqual(["value"], c.properties()["shardKeys"]);

      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(c.insert({ value: i })._key);
      }

      assertEqual([ 179, 192, 200, 207, 222 ], Object.values(c.count(true)).sort());

      keys.forEach(function(k, i) {
        assertEqual(i, c.document(k).value);
      });
    },

    testCreateWithNonDefaultKeysNullValues : function () {
      let c = db._create(name1, { shardingStrategy: communityCompat, shardKeys: ["value"], numberOfShards: 5 });
      assertEqual(communityCompat, c.properties()["shardingStrategy"]);
      assertEqual(5, c.properties()["numberOfShards"]);
      assertEqual(["value"], c.properties()["shardKeys"]);

      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(c.insert({ value: null })._key);
      }

      assertEqual([ 0, 0, 0, 0, 1000 ], Object.values(c.count(true)).sort());

      keys.forEach(function(k, i) {
        assertNull(c.document(k).value);
      });
    },

    testDistributeShardsLikeWithNonDefaultKeys : function () {
      let c1 = db._create(name1, { shardingStrategy: communityCompat, shardKeys: ["value"], numberOfShards: 5 });
      let c2 = db._create(name2, { distributeShardsLike: name1, shardKeys: ["value"] });
      assertEqual(communityCompat, c1.properties()["shardingStrategy"]);
      assertEqual(communityCompat, c2.properties()["shardingStrategy"]);
      assertEqual(5, c1.properties()["numberOfShards"]);
      assertEqual(5, c2.properties()["numberOfShards"]);
      assertEqual(["value"], c1.properties()["shardKeys"]);
      assertEqual(["value"], c2.properties()["shardKeys"]);

      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(c2.insert({ value: i })._key);
      }

      assertEqual([ 179, 192, 200, 207, 222 ], Object.values(c2.count(true)).sort());

      keys.forEach(function(k, i) {
        assertEqual(i, c2.document(k).value);
      });
    }

  };
}

function EdgeShardingSuite() {
  let name1 = "UnitTestsCollection1";
  let name2 = "UnitTestsCollection2";

  return {

    setUp : function () {
      db._drop(name2);
      db._drop(name1);
    },

    tearDown : function () {
      db._drop(name2);
      db._drop(name1);
    },

    testEdgeCreateWithInvalidSharding : function () {
      try {
        db._createEdgeCollection(name1, { shardingStrategy: "peng", numberOfShards: 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testEdgeCreateWithNoneSharding : function () {
      try {
        db._createEdgeCollection(name1, { shardingStrategy: "none", numberOfShards: 5 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testEdgeCreateWithCommunitySharding : function () {
      let c = db._createEdgeCollection(name1, { shardingStrategy: communityCompat, numberOfShards: 5 });
      assertEqual(communityCompat, c.properties()["shardingStrategy"]);
      assertEqual(5, c.properties()["numberOfShards"]);
      assertEqual(["_key"], c.properties()["shardKeys"]);

      for (let i = 0; i < 1000; ++i) {
        c.insert({ _key: "test" + i, value: i, _from: "v/test" + i, _to: "v/test" + i });
      }

      assertEqual([ 188, 192, 198, 204, 218 ], Object.values(c.count(true)).sort());

      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, c.document("test" + i).value);
      }
    },

    testEdgeCreateWithEnterpriseSharding : function () {
      if (!isEnterprise) {
        try {
          db._createEdgeCollection(name1, { shardingStrategy: enterpriseCompat, numberOfShards: 5 });
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      }
    },

    testEdgeCreateWithNonDefaultKeysMissingValues : function () {
      let c = db._createEdgeCollection(name1, { shardingStrategy: communityCompat, shardKeys: ["value"], numberOfShards: 5 });
      assertEqual(communityCompat, c.properties()["shardingStrategy"]);
      assertEqual(5, c.properties()["numberOfShards"]);
      assertEqual(["value"], c.properties()["shardKeys"]);

      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(c.insert({ _from: "v/test" + i, _to: "v/test" + i })._key);
      }

      assertEqual([ 0, 0, 0, 0, 1000 ], Object.values(c.count(true)).sort());

      keys.forEach(function(k, i) {
        assertUndefined(c.document(k).value);
      });
    },

    testEdgeCreateWithNonDefaultKeysNumericValues : function () {
      let c = db._createEdgeCollection(name1, { shardingStrategy: communityCompat, shardKeys: ["value"], numberOfShards: 5 });
      assertEqual(communityCompat, c.properties()["shardingStrategy"]);
      assertEqual(5, c.properties()["numberOfShards"]);
      assertEqual(["value"], c.properties()["shardKeys"]);

      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(c.insert({ value: i, _from: "v/test" + i, _to: "v/test" + i })._key);
      }

      assertEqual([ 179, 192, 200, 207, 222 ], Object.values(c.count(true)).sort());

      keys.forEach(function(k, i) {
        assertEqual(i, c.document(k).value);
      });
    },

    testEdgeCreateWithNonDefaultKeysNullValues : function () {
      let c = db._createEdgeCollection(name1, { shardingStrategy: communityCompat, shardKeys: ["value"], numberOfShards: 5 });
      assertEqual(communityCompat, c.properties()["shardingStrategy"]);
      assertEqual(5, c.properties()["numberOfShards"]);
      assertEqual(["value"], c.properties()["shardKeys"]);

      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(c.insert({ value: null, _from: "v/test" + i, _to: "v/test" + i })._key);
      }

      assertEqual([ 0, 0, 0, 0, 1000 ], Object.values(c.count(true)).sort());

      keys.forEach(function(k, i) {
        assertNull(c.document(k).value);
      });
    },

    testEdgeDistributeShardsLikeWithNonDefaultKeys : function () {
      let c1 = db._createEdgeCollection(name1, { shardingStrategy: communityCompat, shardKeys: ["value"], numberOfShards: 5 });
      let c2 = db._createEdgeCollection(name2, { distributeShardsLike: name1, shardKeys: ["value"] });
      assertEqual(communityCompat, c1.properties()["shardingStrategy"]);
      assertEqual(communityCompat, c2.properties()["shardingStrategy"]);
      assertEqual(5, c1.properties()["numberOfShards"]);
      assertEqual(5, c2.properties()["numberOfShards"]);
      assertEqual(["value"], c1.properties()["shardKeys"]);
      assertEqual(["value"], c2.properties()["shardKeys"]);

      let keys = [];
      for (let i = 0; i < 1000; ++i) {
        keys.push(c2.insert({ value: i, _from: "v/test" + i, _to: "v/test" + i })._key);
      }

      assertEqual([ 179, 192, 200, 207, 222 ], Object.values(c2.count(true)).sort());

      keys.forEach(function(k, i) {
        assertEqual(i, c2.document(k).value);
      });
    }

  };
}

jsunity.run(DocumentShardingSuite);
jsunity.run(EdgeShardingSuite);

return jsunity.done();
