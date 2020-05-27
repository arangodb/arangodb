/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue, assertFalse */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for dump/reload
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
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop('UnitTestsRecovery1');
  var c = db._create('UnitTestsRecovery1', {
    waitForSync: true,
    journalSize: 8 * 1024 * 1024,
    doCompact: false
  });
  c.save({ value1: 1, value2: [ 'the',
      'quick',
      'brown',
      'foxx',
      'jumped',
      'over',
      'the',
      'lazy',
      'dog',
    'xxxxxxxxxxx' ] });
  c.ensureHashIndex('value1');
  c.ensureSkiplist('value2');

  db._drop('UnitTestsRecovery2');
  c = db._create('UnitTestsRecovery2', {
    waitForSync: false,
    journalSize: 16 * 1024 * 1024,
    doCompact: true,
  });
  c.save({ value1: { 'some': 'rubbish' } });
  c.ensureSkiplist('value1');

  db._drop('UnitTestsRecovery3');
  c = db._createEdgeCollection('UnitTestsRecovery3', {
    waitForSync: false,
    journalSize: 32 * 1024 * 1024,
    doCompact: true
  });

  c.save('UnitTestsRecovery1/foo', 'UnitTestsRecovery2/bar', { value1: { 'some': 'rubbish' } });
  c.ensureUniqueSkiplist('value1');

  db._drop('_UnitTestsRecovery4');
  c = db._create('_UnitTestsRecovery4', { isSystem: true });

  c.save({ value42: 42 });
  c.ensureUniqueConstraint('value42');
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

    testCreateCollections: function () {
      var c, idx, prop;

      c = db._collection('UnitTestsRecovery1');
      assertEqual(1, c.count());
      prop = c.properties();
      assertTrue(prop.waitForSync);
      assertEqual(2, c.type());
      assertFalse(prop.isSystem);
      idx = c.getIndexes();
      assertEqual(3, idx.length);

      c = db._collection('UnitTestsRecovery2');
      assertEqual(1, c.count());
      prop = c.properties();
      assertFalse(prop.waitForSync);
      assertEqual(2, c.type());
      assertFalse(prop.isSystem);
      idx = c.getIndexes();
      assertEqual(2, idx.length);
      assertEqual('skiplist', idx[1].type);
      assertFalse(idx[1].unique);

      c = db._collection('UnitTestsRecovery3');

      assertEqual(1, c.count());
      prop = c.properties();
      assertFalse(prop.waitForSync);
      assertEqual(3, c.type());
      assertFalse(prop.isSystem);
      idx = c.getIndexes();
      assertEqual(3, idx.length);
      assertEqual('edge', idx[1].type);
      assertEqual('skiplist', idx[2].type);
      assertTrue(idx[2].unique);

      c = db._collection('_UnitTestsRecovery4');
      assertEqual(2, c.count());
      prop = c.properties();
      assertEqual(2, c.type());
      assertTrue(prop.isSystem);
      idx = c.getIndexes();
      assertEqual(2, idx.length);
      assertEqual('hash', idx[1].type);
      assertTrue(idx[1].unique);
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
