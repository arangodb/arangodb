/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue, assertNotNull */
// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for transactions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var simple = require('@arangodb/simple-query');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop('UnitTestsRecovery1');
  var c = db._create('UnitTestsRecovery1'), i, j;
  c.ensureGeoIndex('loc');
  c.ensureGeoIndex('lat', 'lon');

  for (i = -40; i < 40; ++i) {
    for (j = -40; j < 40; ++j) {
      c.save({ loc: [ i, j ], lat: i, lon: j });
    }
  }

  db._drop('UnitTestsRecovery2');
  c = db._create('UnitTestsRecovery2');
  c.ensureGeoConstraint('a.loc', true, true);

  for (i = -40; i < 40; ++i) {
    for (j = -40; j < 40; ++j) {
      c.save({ a: { loc: [ i, j ] } });
    }
  }

  db._drop('UnitTestsRecovery3');
  c = db._create('UnitTestsRecovery3');
  c.ensureGeoConstraint('a.lat', 'a.lon', false);

  for (i = -40; i < 40; ++i) {
    for (j = -40; j < 40; ++j) {
      c.save({ a: { lat: i, lon: j } });
    }
  }

  db._drop('test');
  c = db._create('test');
  c.save({ _key: 'crashme' }, true);

  internal.debugTerminate('crashing server');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {},
    tearDown: function () {},

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

function main (argv) {
  'use strict';
  if (argv[1] === 'setup') {
    runSetup();
    return 0;
  } else {
    jsunity.run(recoverySuite);
    return jsunity.writeDone().status ? 0 : 1;
  }
}
