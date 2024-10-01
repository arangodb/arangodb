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
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var simple = require('@arangodb/simple-query');
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop('UnitTestsRecovery1');
  let c = db._create('UnitTestsRecovery1');
  c.ensureIndex({ type: "geo", fields: ["loc"] });
  c.ensureIndex({ type: "geo", fields: ["lat", "lon"] });

  let docs = [];
  for (let i = -40; i < 40; ++i) {
    for (let j = -40; j < 40; ++j) {
      docs.push({ loc: [ i, j ], lat: i, lon: j });
    }
  }
  c.insert(docs);

  db._drop('UnitTestsRecovery2');
  c = db._create('UnitTestsRecovery2');
  c.ensureIndex({ type: "geo", fields: ["a.loc"], geoJson: true, sparse: true });

  docs = [];
  for (let i = -40; i < 40; ++i) {
    for (let j = -40; j < 40; ++j) {
      docs.push({ a: { loc: [ i, j ] } });
    }
  }
  c.insert(docs);

  db._drop('UnitTestsRecovery3');
  c = db._create('UnitTestsRecovery3');
  c.ensureIndex({ type: "geo", fields: ["a.lat", "a.lon"], geoJson: false });

  docs = [];
  for (let i = -40; i < 40; ++i) {
    for (let j = -40; j < 40; ++j) {
      docs.push({ a: { lat: i, lon: j } });
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


    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testIndexesGeo: function () {
      var c = db._collection('UnitTestsRecovery1'), idx, i;
      var geo1 = null, geo2 = null;
      idx = c.getIndexes();

      for (i = 1; i < idx.length; ++i) {
        if (idx[i].type === 'geo1' || (idx[i].type === 'geo' && idx[i].fields.length === 1)) {
          geo1 = idx[i];
          assertFalse(geo1.unique);
          assertTrue(geo1.sparse);
          assertFalse(geo1.geoJson);
          assertEqual([ 'loc' ], geo1.fields);
        } else if (idx[i].type === 'geo2' || (idx[i].type === 'geo' && idx[i].fields.length === 2)) {
          geo2 = idx[i];
          assertFalse(geo2.unique);
          assertTrue(geo2.sparse);
          assertEqual([ 'lat', 'lon' ], geo2.fields);
        }
      }

      assertNotNull(geo1);
      assertNotNull(geo2);

      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geo1.id).limit(100).toArray().length);
      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geo2.id).limit(100).toArray().length);

      c = db._collection('UnitTestsRecovery2');
      geo1 = c.getIndexes()[1];
      assertFalse(geo1.unique);
      assertTrue(geo1.sparse);
      assertTrue(geo1.geoJson);
      assertEqual([ 'a.loc' ], geo1.fields);

      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geo1.id).limit(100).toArray().length);

      c = db._collection('UnitTestsRecovery3');
      geo1 = c.getIndexes()[1];
      assertFalse(geo1.unique);
      assertTrue(geo1.sparse);
      assertFalse(geo1.geoJson);
      assertEqual([ 'a.lat', 'a.lon' ], geo1.fields);

      assertEqual(100, new simple.SimpleQueryNear(c, 0, 0, geo1.id).limit(100).toArray().length);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
