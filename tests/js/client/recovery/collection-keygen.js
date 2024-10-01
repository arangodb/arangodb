/* jshint globalstrict:false, strict:false, unused: false */
/* global runSetup assertTrue, assertFalse, assertEqual, assertMatch */
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
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();
  var c;

  db._drop('UnitTestsRecovery1');
  c = db._create('UnitTestsRecovery1', { keyOptions: { type: 'autoincrement',
    offset: 0, increment: 1 } } );
  c.save({ name: 'a' });
  c.save({ name: 'b' });
  c.save({ name: 'c' });

  db._drop('UnitTestsRecovery2');
  c = db._create('UnitTestsRecovery2', { keyOptions: { type: 'autoincrement',
    offset: 10, increment: 5 } } );
  c.save({ name: 'a' });
  c.save({ name: 'b' });
  c.save({ name: 'c' });
  
  db._drop('UnitTestsRecovery3');
  c = db._create('UnitTestsRecovery3', { keyOptions: { type: 'uuid' } });
  c.save({ name: 'a' });
  c.save({ name: 'b' });
  c.save({ name: 'c' });
  
  db._drop('UnitTestsRecovery4');
  c = db._create('UnitTestsRecovery4', { keyOptions: { type: 'padded' } });
  c.save({ name: 'a' });
  c.save({ name: 'b' });
  c.save({ name: 'c' }, { waitForSync: true });

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

    testCollectionKeyGen: function () {
      var c, d;

      c = db._collection('UnitTestsRecovery1');
      assertEqual("autoincrement", c.properties()["keyOptions"]["type"]);
      assertEqual(["1", "2", "3"], c.toArray().map(function(doc) { return doc._key; }).sort());
      d = c.save({ name: "d"});
      assertEqual("4", d._key);

      c = db._collection('UnitTestsRecovery2');
      assertEqual("autoincrement", c.properties()["keyOptions"]["type"]);
      assertEqual(["10", "15", "20"], c.toArray().map(function(doc) { return doc._key; }).sort());
      d = c.save({ name: "d"});
      assertEqual("25", d._key);
      
      c = db._collection('UnitTestsRecovery3');
      assertEqual("uuid", c.properties()["keyOptions"]["type"]);
      c.toArray().forEach(function(doc) { 
        assertMatch(/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/, doc._key);
      });
      
      c = db._collection('UnitTestsRecovery4');
      assertEqual("padded", c.properties()["keyOptions"]["type"]);
      c.toArray().forEach(function(doc) { 
        assertMatch(/^[0-9a-f]{16}$/, doc._key);
      });
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
