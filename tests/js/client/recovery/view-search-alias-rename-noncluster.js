/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertNull, assertTrue, assertFalse, assertNotNull */

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
/// @author Andrey Abramov
/// @author Copyright 2022, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();
  var v;

  db._drop('UnitTestsDummy');
  var c = db._create('UnitTestsDummy');
  var i1 = c.ensureIndex({ type: "inverted", name: "i1", fields: [ "a" ] });
  var meta1 = { indexes: [ { index: i1.name, collection: c.name() } ] };
  var i2 = c.ensureIndex({ type: "inverted", name: "i2", fields: [ "b" ] });
  var meta2 = { indexes: [ { index: i2.name, collection: c.name() } ] };

  db._dropView('UnitTestsRecovery1');
  db._dropView('UnitTestsRecovery2');
  v = db._createView('UnitTestsRecovery1', 'search-alias', {});
  v.properties(meta1);
  v.rename('UnitTestsRecovery2');

  db._dropView('UnitTestsRecovery3');
  db._dropView('UnitTestsRecovery4');
  v = db._createView('UnitTestsRecovery3', 'search-alias', meta2);
  v.rename('UnitTestsRecovery4');

  db._dropView('UnitTestsRecovery5');
  db._dropView('UnitTestsRecovery6');
  v = db._createView('UnitTestsRecovery5', 'search-alias', {});
  v.rename('UnitTestsRecovery6');
  v.rename('UnitTestsRecovery5');

  db._dropView('UnitTestsRecovery7');
  db._dropView('UnitTestsRecovery8');
  v = db._createView('UnitTestsRecovery7', 'search-alias', {});
  v.rename('UnitTestsRecovery8');
  db._dropView('UnitTestsRecovery8');
  v = db._createView('UnitTestsRecovery8', 'search-alias', {});

  db._collection('UnitTestsDummy').save({ _key: 'foo' }, { waitForSync: true });

  return 0;
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    testViewRename: function () {
      let checkView = function(viewName, indexName) {
        let v = db._view(viewName);
        assertEqual(v.name(), viewName);
        assertEqual(v.type(), 'search-alias');
        let indexes = v.properties().indexes;
        assertEqual(1, indexes.length);
        assertEqual(indexName, indexes[0].index);
        assertEqual("UnitTestsDummy", indexes[0].collection);
      };

      assertNull(db._view('UnitTestsRecovery1'));
      checkView("UnitTestsRecovery2", "i1");
      assertNull(db._view('UnitTestsRecovery3'));
      checkView("UnitTestsRecovery4", "i2");
      assertNull(db._view('UnitTestsRecovery6'));
      assertNotNull(db._view('UnitTestsRecovery5'));
      assertNull(db._view('UnitTestsRecovery7'));
    }

  };
}

jsunity.run(recoverySuite);
return jsunity.done();
