/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertNull, assertTrue, assertFalse, assertNotNull, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for dump/reload
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2022 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Copyright 2022, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();
  var v;

  db._drop('UnitTestsDummy');
  var c = db._create('UnitTestsDummy');
  var i1 = c.ensureIndex({ type: "inverted", name: "i1", fields: [ "num" ] });
  var meta = { indexes: [ { index: i1.name, collection: c.name() } ] };

  db._dropView('UnitTestsRecovery1');
  db._dropView('UnitTestsRecovery2');
  v = db._createView('UnitTestsRecovery1', 'search-alias', {});
  v.properties(meta);
  db._collection('UnitTestsDummy').save({ _key: 'foo', num: 1 }, { waitForSync: true });

  v.rename('UnitTestsRecovery2');

  db._collection('UnitTestsDummy').save({ _key: 'bar', num: 2 }, { waitForSync: true });

  internal.debugTerminate('crashing server');
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {},
    tearDown: function () {},

    testIResearchViewRename: function () {
      assertNull(db._view('UnitTestsRecovery1'));
      assertNotNull(db._view('UnitTestsRecovery2'));
      var res = db._query('FOR doc IN `UnitTestsRecovery2` SEARCH doc.num > 0 RETURN doc').toArray();
      assertEqual(2, res.length);
    }
  };
}

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
