/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertFalse */

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
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();
  var i;

  db._drop('UnitTestsRecovery1');
  db._drop('UnitTestsRecovery2');
  db._create('UnitTestsRecovery1');

  for (i = 0; i < 1000; ++i) {
    db.UnitTestsRecovery1.save({ a: i });
  }

  db.UnitTestsRecovery1.rename('UnitTestsRecovery2');

  db._create('UnitTestsRecovery1');

  for (i = 0; i < 99; ++i) {
    db.UnitTestsRecovery1.save({ a: i });
  }

  db.UnitTestsRecovery1.save({ _key: 'foo' }, true);

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
    // / @brief test whether rename and recreate works
    // //////////////////////////////////////////////////////////////////////////////

    testCollectionRenameRecreate: function () {
      var c, prop;

      c = db._collection('UnitTestsRecovery1');
      assertEqual(100, c.count());

      c = db._collection('UnitTestsRecovery2');
      prop = c.properties();
      assertEqual(1000, c.count());
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
