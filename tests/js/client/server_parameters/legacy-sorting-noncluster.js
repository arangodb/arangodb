/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global getOptions, assertEqual, assertNotEqual, assertTrue, assertFalse, arango */

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
  assertFalse(r.error);
  assertEqual(202, r.code);
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
      let c = db._create(cn);
      c.insert(l);
      c = db._create(cn2);
      c.insert(l);

      db._createDatabase(dn);
      db._useDatabase(dn);
      c = db._create(cn);
      c.insert(l);
      c = db._create(cn2);
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
      assertEqual("LEGACY", res.result.next);
      assertEqual("LEGACY", res.result.current);

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

      // Now check that we found all the indexes and not too many:
      assertFalse(r.error);
      assertEqual(200, r.code);
      assertTrue(r.result.error);
      assertEqual(1242, r.result.errorCode);
      assertTrue(Array.isArray(r.result.affected));
      assertEqual(2*nr, r.result.affected.length);
      for (let o of r.result.affected) {
        assertEqual(o.collection, cn);
        assertTrue(names.indexOf(o.indexName) >= 0);
        assertNotEqual(0, o.indexId);   // Not the primary index!
        if (o.database === "_system") {
          assertTrue(ids_system.indexOf(cn + "/" + o.indexId) >= 0);
        } else if (o.database === dn) {
          assertTrue(ids_dn.indexOf(cn + "/" + o.indexId) >= 0);
        } else {
          assertTrue(false, "Wrong database name: " + o.database);
        }
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

      // And check again:
      r = arango.GET("/_admin/cluster/vpackSortMigration/check");

      // Now check that we found all the indexes and not too many:
      assertFalse(r.error);
      assertEqual(200, r.code);
      assertFalse(r.result.error);
      assertEqual(0, r.result.errorCode);
      assertTrue(Array.isArray(r.result.affected));
      assertEqual(0, r.result.affected.length);

      // Migrate:
      res = arango.PUT("/_admin/cluster/vpackSortMigration/migrate", {});
      assertFalse(res.error);
      assertEqual(200, res.code);

      // Check info API:
      res = arango.GET("/_admin/cluster/vpackSortMigration/status");
      assertFalse(res.error);
      assertEqual(200, res.code);
      assertEqual("CORRECT", res.result.next);
      assertEqual("LEGACY", res.result.current);
    }
  };
}

jsunity.run(legacySortingTestSuite);

return jsunity.done();
