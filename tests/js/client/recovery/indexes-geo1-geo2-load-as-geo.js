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
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
