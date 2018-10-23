/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue, assertFalse */
// //////////////////////////////////////////////////////////////////////////////
// / @brief recovery tests for views
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
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop('UnitTestsRecoveryDummy');
  db._drop('UnitTestsRecoveryDummy2');

  var c = db._create('UnitTestsRecoveryDummy');

  db._dropView('UnitTestsRecovery1');
  var v = db._createView('UnitTestsRecovery1', 'arangosearch', {});
  var meta = { links: { 'UnitTestsRecoveryDummy': { includeAllFields: true } } };
  v.properties(meta);

  c.rename('UnitTestsRecoveryDummy2');

  c.save({ name: 'crashme' }, true);

  internal.debugSegfault('crashing server');
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

    testIResearchLinkRenameCollection: function () {
      var meta = { links: { 'UnitTestsRecoveryDummy2': { includeAllFields: true } } };
      var v1 = db._view('UnitTestsRecovery1');
      assertEqual(v1.name(), 'UnitTestsRecovery1');
      assertEqual(v1.type(), 'arangosearch');
      var p = v1.properties().links;
      assertTrue(p.hasOwnProperty('UnitTestsRecoveryDummy2'));
      assertTrue(p.UnitTestsRecoveryDummy2.includeAllFields);
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
