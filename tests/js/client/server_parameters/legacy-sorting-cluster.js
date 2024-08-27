/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global getOptions, assertEqual, assertTrue, assertFalse, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

// We need the following options for this test:
//  --rocksdb.force-legacy-comparator true
// This means that the old rocksdb comparator is used. This is necessary
// since we want to test that the migration check detects bad data.

if (getOptions === true) {
  return {
    'rocksdb.force-legacy-comparator': 'true'
  };
}

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;

// This introduces some values into a collection which will be sorted
// incorrectly with the legacy sorting order. Note that we cannot do
// this via JSON, since JavaScript has only double numbers. That is the
// reason why we use the HTTP API directly and use awkward strings.

function poisonCollection(databasename, collname, attrA, attrB) {
  // 1152921504606846976 = 2^60 which does not completely fit into IEEE 754,
  // therefore 2^60+1 and 2^60 are considered equal in the old comparator,
  // if they are presented in different representations in vpack (int, uint,
  // or double).
  // Therefore, the following document with key "X" is considered smaller
  // than the one with key "Y" in the legacy order, whereas in the
  // correct lexicographic order document "X" is be larger.
  let r = 
    arango.POST_RAW(`/_db/${databasename}/_api/document/${collname}`,
      `{"_key":"X","${attrA}":1152921504606846977,"${attrB}":"x","x":1.0,"y":1.0}`);
  assertFalse(r.error);
  assertEqual(202, r.code);
  r = arango.POST_RAW(`/_db/${databasename}/_api/document/${collname}`,
    `{"_key":"Y","${attrA}":1152921504606846976.0,"${attrB}":"y","x":1.0,"y":1.0}`);
  assertEqual(202, r.code);
  assertFalse(r.error);
}

function createVPackIndexes(databasename, collname, attrA, attrB) {
  db._useDatabase(databasename);
  let c = db._collection(collname);
  c.ensureIndex({type:"persistent", fields: [attrA, attrB], unique: false,
                 name:"persistent_non_unique"});
  c.ensureIndex({type:"persistent", fields: [attrA, attrB], unique: true,
                 name:"persistent_unique"});
  c.ensureIndex({type:"hash", fields: [attrA, attrB], unique: false,
                 name:"hash_non_unique"});
  c.ensureIndex({type:"hash", fields: [attrA, attrB], unique: true,
                 name:"hash_unique"});
  c.ensureIndex({type:"skiplist", fields: [attrA, attrB], unique: false,
                 name:"skiplist_non_unique"});
  c.ensureIndex({type:"skiplist", fields: [attrA, attrB], unique: true,
                 name:"skiplist_unique"});
  let nr = 6;
  try {
    c.ensureIndex({type:"mdi-prefixed", fields: ["x", "y"], prefixFields:[attrA, attrB],
                   name:"mdi_non_unique", fieldValueTypes: "double", unique: false});
    c.ensureIndex({type:"mdi-prefixed", fields: ["x", "y"], prefixFields:[attrA, attrB],
                   name:"mdi_unique", fieldValueTypes: "double", unique: true});
    nr += 2;
  } catch(e) {
    // Smile it away on 3.11
  }
  db._useDatabase("_system");
  return nr;
}

function legacySortingTestSuite() {
  'use strict';
  const dn = "UnitTestsDatabase";
  const cn = 'UnitTestsLegacySortingBad';
  const cn2 = 'UnitTestsLegacySortingGood';

  return {
    setUp: function() {
      let l = [];
      for (let i = 0; i < 1000; ++i) {
        l.push({Hallo:i, a:i, b: i, x: i * 1.234567, y : i * 1.234567});
      }
      // Note: We must only use one shard, since otherwise the incriminating
      // documents might be distributed to more than one dbserver, removing
      // the problem!
      let c = db._create(cn, {numberOfShards:1, replicationFactor:2});
      c.insert(l);
      c = db._create(cn2, {numberOfShards:1, replicationFactor:2});
      c.insert(l);

      db._createDatabase(dn);
      db._useDatabase(dn);
      c = db._create(cn, {numberOfShards:1, replicationFactor:2});
      c.insert(l);
      c = db._create(cn2, {numberOfShards:1, replicationFactor:2});
      c.insert(l);
      db._useDatabase("_system");
    },

    tearDown: function () {
      db._drop(cn);
      db._drop(cn2);
      db._dropDatabase(dn);
    },

    testMigrationCheck: function() {
      let nr = createVPackIndexes("_system", cn, "a", "b");
      createVPackIndexes(dn, cn, "a", "b");
      createVPackIndexes("_system", cn2, "a", "b");
      createVPackIndexes(dn, cn2, "a", "b");
      poisonCollection("_system", cn, "a", "b");
      poisonCollection(dn, cn, "a", "b");

      // Check info API:
      let res = arango.GET("/_admin/cluster/vpackSortMigration/status");
      assertFalse(res.error);
      assertEqual(200, res.code);
      assertEqual("object", typeof(res.result));
      for (let dbserver in res.result) {
        let oneResult = res.result[dbserver];
        assertFalse(oneResult.error);
        assertEqual(200, oneResult.code);
        assertEqual("LEGACY", oneResult.result.next);
        assertEqual("LEGACY", oneResult.result.current);
      }

      let c = db._collection(cn);
      let indexes = c.getIndexes();
      let names = indexes.map(x => x.name);
      let ids_system = indexes.map(x => x.id);
      let r = arango.GET("/_admin/cluster/vpackSortMigration/check");

      db._useDatabase(dn);
      c = db._collection(cn);
      indexes = c.getIndexes();
      let ids_dn = indexes.map(x => x.id);
      db._useDatabase("_system");

      r = arango.GET("/_admin/cluster/vpackSortMigration/check");

      // Now check that we found all the indexes and not too many.
      // We do not check the details of the output, since we do this
      // in the single server case and we never know which shards land
      // on which dbserver. We do not even know
      assertFalse(r.error);
      assertEqual(200, r.code);
      assertEqual("object", typeof(r.result));
      for (let dbserver in r.result) {
        let oneResult = r.result[dbserver];
        assertFalse(oneResult.error);
        assertEqual(200, oneResult.code);
        assertTrue(oneResult.result.error);
        assertEqual(1242, oneResult.result.errorCode);
        assertTrue(Array.isArray(oneResult.result.affected));
        assertTrue(oneResult.result.affected.length > 0);
      }

      // Now get rid of indexes:
      for (let d of ["_system", dn]) {
        db._useDatabase(d);
        c = db._collection(cn);
        indexes = c.getIndexes();
        for (let i of indexes) {
          if (i.id !== cn + "/0") {   // primary index
            c.dropIndex(i.id);
          }
        }
        db._useDatabase("_system");
      }

      // Now check that we find no more bad indexes:
      // Unfortunately, it is possible that some followers might not yet
      // have dropped their indexes. Therefore, we conduct the following
      // test potentially repeatedly and only assume that it is successful
      // after at most 60s:
      let tester = function(r) {
        if (r.error !== false) { return false; }
        if (r.code !== 200) { return false; }
        if (typeof(r.result) !== "object") { return false; }
        for (let dbserver in r.result) {
          let oneResult = r.result[dbserver];
          if (oneResult.error !== false) { return false; }
          if (oneResult.code !== 200) { return false; }
          if (oneResult.result.error !== false) { return false; }
          if (oneResult.result.errorCode !== 0) { return false; }
          if (!Array.isArray(oneResult.result.affected)) { return false; }
          if (oneResult.result.affected.length !== 0) { return false; }
        }
        return true;
      };
      let count = 0;
      while (true) {
        r = arango.GET("/_admin/cluster/vpackSortMigration/check");
        let res = tester(r);
        if (res === true) {
          break;
        }
        console.error("Bad result:", JSON.stringify(r));
        require("internal").wait(1.0);
        count += 1;
        if (count > 60) {
          assertTrue(false, "Test not good after 60s.");
        }
      }

      // Migrate:
      res = arango.PUT("/_admin/cluster/vpackSortMigration/migrate", {});
      assertFalse(res.error);
      assertEqual(200, res.code);

      // Check info API:
      res = arango.GET("/_admin/cluster/vpackSortMigration/status");
      assertFalse(res.error);
      assertEqual(200, res.code);
      assertEqual("object", typeof(res.result));
      for (let dbserver in res.result) {
        let oneResult = res.result[dbserver];
        assertFalse(oneResult.error);
        assertEqual(200, oneResult.code);
        assertEqual("CORRECT", oneResult.result.next);
        assertEqual("LEGACY", oneResult.result.current);
      }
    }
  };
}

jsunity.run(legacySortingTestSuite);

return jsunity.done();
