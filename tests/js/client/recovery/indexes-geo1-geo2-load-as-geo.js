/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertFalse, assertTrue, assertNotNull */
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
// / Test: Create a database with traditional index types geo1 and geo2.
// / After recovery, IndexFactory routes geo1/geo2 to geo when loading.
// / This test verifies that the on-disk format is compatible (point 1) -
// / indexes load correctly and geo queries return expected results.
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var simple = require('@arangodb/simple-query');
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  // Create collection with legacy geo1 and geo2 index types (so they are
  // persisted with those types in the collection metadata).
  db._drop('UnitTestsRecoveryGeo1Geo2');
  let c = db._create('UnitTestsRecoveryGeo1Geo2');
  c.ensureIndex({ type: "geo1", fields: ["loc"], geoJson: false });
  c.ensureIndex({ type: "geo2", fields: ["lat", "lon"] });

  let docs = [];
  for (let i = -40; i < 40; ++i) {
    for (let j = -40; j < 40; ++j) {
      docs.push({ loc: [ i, j ], lat: i, lon: j });
    }
  }
  c.insert(docs);

  // geo1 with geoJson: true (single field, [lon, lat] or GeoJSON object).
  db._drop('UnitTestsRecoveryGeo1GeoJson');
  c = db._create('UnitTestsRecoveryGeo1GeoJson');
  c.ensureIndex({ type: "geo1", fields: ["a.loc"], geoJson: true, sparse: true });

  docs = [];
  for (let i = -40; i < 40; ++i) {
    for (let j = -40; j < 40; ++j) {
      docs.push({ a: { loc: [ i, j ] } });
    }
  }
  c.insert(docs);

  // geo2 on nested paths (a.lat, a.lon).
  db._drop('UnitTestsRecoveryGeo2Nested');
  c = db._create('UnitTestsRecoveryGeo2Nested');
  c.ensureIndex({ type: "geo2", fields: ["a.lat", "a.lon"], geoJson: false });

  docs = [];
  for (let i = -40; i < 40; ++i) {
    for (let j = -40; j < 40; ++j) {
      docs.push({ a: { lat: i, lon: j } });
    }
  }
  c.insert(docs);

  // --- Explicit verification: geo1/geo2 must be saved in DB before server restart ---
  // The server serializes indexes via the same path that persists the collection
  // document, so if collection.indexes() reports type "geo1"/"geo2", they are persisted.

  let idx1 = db._collection('UnitTestsRecoveryGeo1Geo2').indexes();
  let geo1Saved = false, geo2Saved = false;
  for (let i = 1; i < idx1.length; ++i) {
    if (idx1[i].type === 'geo1' && idx1[i].fields && idx1[i].fields[0] === 'loc') geo1Saved = true;
    if (idx1[i].type === 'geo2' && idx1[i].fields && idx1[i].fields[0] === 'lat' && idx1[i].fields[1] === 'lon') geo2Saved = true;
  }
  if (!geo1Saved) throw new Error("geo1 index must be saved in DB before restart (server reports type for persisted indexes)");
  if (!geo2Saved) throw new Error("geo2 index must be saved in DB before restart (server reports type for persisted indexes)");

  let idx2 = db._collection('UnitTestsRecoveryGeo1GeoJson').indexes();
  let geo1GeoJsonSaved = false;
  for (let i = 1; i < idx2.length; ++i) {
    if (idx2[i].type === 'geo1' && idx2[i].fields && idx2[i].fields[0] === 'a.loc') { geo1GeoJsonSaved = true; break; }
  }
  if (!geo1GeoJsonSaved) throw new Error("geo1 (a.loc) index must be saved in DB before restart");

  let idx3 = db._collection('UnitTestsRecoveryGeo2Nested').indexes();
  let geo2NestedSaved = false;
  for (let i = 1; i < idx3.length; ++i) {
    if (idx3[i].type === 'geo2' && idx3[i].fields && idx3[i].fields[0] === 'a.lat' && idx3[i].fields[1] === 'a.lon') { geo2NestedSaved = true; break; }
  }
  if (!geo2NestedSaved) throw new Error("geo2 (a.lat, a.lon) index must be saved in DB before restart");

  internal.print("Verified: geo1/geo2 indexes are saved in database before server restart (setup will now kill and restart server).");

  db._drop('test');
  c = db._create('test');
  c.save({ _key: 'crashme' }, true);

  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {

    // ////////////////////////////////////////////////////////////////////////////
    // / @brief After recovery, indexes that were stored as geo1/geo2 are loaded
    // / via the geo factory (routing in IndexFactory). Verify that the on-disk
    // / format is compatible: indexes work and geo queries return correct results.
    // ////////////////////////////////////////////////////////////////////////////

    testGeo1Geo2LoadedAsGeo: function () {
      var c = db._collection('UnitTestsRecoveryGeo1Geo2');
      var idx = c.indexes();
      var geoSingle = null;   // single-field (was geo1)
      var geoDouble = null;   // two-field (was geo2)

      for (var i = 1; i < idx.length; ++i) {
        if (idx[i].fields.length === 1 && idx[i].fields[0] === 'loc') {
          geoSingle = idx[i];
        } else if (idx[i].fields.length === 2 &&
                   idx[i].fields[0] === 'lat' && idx[i].fields[1] === 'lon') {
          geoDouble = idx[i];
        }
      }

      assertNotNull(geoSingle, "single-field geo index (was geo1) should exist");
      assertNotNull(geoDouble, "two-field geo index (was geo2) should exist");
      assertFalse(geoSingle.unique);
      assertTrue(geoSingle.sparse);
      assertFalse(geoSingle.geoJson);
      assertEqual([ 'loc' ], geoSingle.fields);
      assertFalse(geoDouble.unique);
      assertTrue(geoDouble.sparse);
      assertEqual([ 'lat', 'lon' ], geoDouble.fields);

      // Verify geo queries work (on-disk format is compatible after routing).
      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geoSingle.id).limit(100).toArray().length);
      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geoDouble.id).limit(100).toArray().length);
    },

    // ////////////////////////////////////////////////////////////////////////////
    // / @brief geo1 with geoJson: true (nested path a.loc) loads as geo; queries work.
    // ////////////////////////////////////////////////////////////////////////////

    testGeo1GeoJsonLoadedAsGeo: function () {
      var c = db._collection('UnitTestsRecoveryGeo1GeoJson');
      var idx = c.indexes();
      var geoIdx = null;
      for (var i = 1; i < idx.length; ++i) {
        if (idx[i].fields.length === 1 && idx[i].fields[0] === 'a.loc') {
          geoIdx = idx[i];
          break;
        }
      }
      assertNotNull(geoIdx, "geo1 geoJson index (a.loc) should exist");
      assertTrue(geoIdx.geoJson);
      assertTrue(geoIdx.sparse);
      assertEqual([ 'a.loc' ], geoIdx.fields);
      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geoIdx.id).limit(100).toArray().length);
    },

    // ////////////////////////////////////////////////////////////////////////////
    // / @brief geo2 on nested paths (a.lat, a.lon) loads as geo; queries work.
    // ////////////////////////////////////////////////////////////////////////////

    testGeo2NestedPathsLoadedAsGeo: function () {
      var c = db._collection('UnitTestsRecoveryGeo2Nested');
      var idx = c.indexes();
      var geoIdx = null;
      for (var i = 1; i < idx.length; ++i) {
        if (idx[i].fields.length === 2 &&
            idx[i].fields[0] === 'a.lat' && idx[i].fields[1] === 'a.lon') {
          geoIdx = idx[i];
          break;
        }
      }
      assertNotNull(geoIdx, "geo2 nested index (a.lat, a.lon) should exist");
      assertFalse(geoIdx.geoJson);
      assertTrue(geoIdx.sparse);
      assertEqual([ 'a.lat', 'a.lon' ], geoIdx.fields);
      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geoIdx.id).limit(100).toArray().length);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
