/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertNull */

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

  var c, i, j;
  for (i = 0; i < 10; ++i) {
    db._drop('UnitTestsRecovery' + i);
    // use fake collection ids
    c = db._create('UnitTestsRecovery' + i, { 'id': `${9999990 + i}` });
    for (j = 0; j < 10; ++j) {
      c.save({ _key: 'test' + j, value: j });
    }
  }

  // drop 'em all
  for (i = 0; i < 10; ++i) {
    db._drop('UnitTestsRecovery' + i);
  }
  internal.wait(5);

  for (i = 0; i < 10; ++i) {
    c = db._create('UnitTestsRecoveryX' + i, { 'id': `${9999990 + i}` });
    for (j = 0; j < 10; ++j) {
      c.save({ _key: 'test' + j, value: 'X' + j });
    }
  }

  // drop 'em all
  for (i = 0; i < 10; ++i) {
    db._drop('UnitTestsRecoveryX' + i);
  }
  internal.wait(5);

  for (i = 0; i < 10; ++i) {
    c = db._create('UnitTestsRecoveryY' + i, { 'id': `${9999990 + i}` });
    for (j = 0; j < 10; ++j) {
      c.save({ _key: 'test' + j, value: 'peterY' + j });
    }
  }

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

    testCollectionsReuse: function () {
      var c, i, j, doc;

      for (i = 0; i < 10; ++i) {
        assertNull(db._collection('UnitTestsRecovery' + i));
        assertNull(db._collection('UnitTestsRecoverX' + i));

        c = db._collection('UnitTestsRecoveryY' + i);
        assertEqual(10, c.count());

        for (j = 0; j < 10; ++j) {
          doc = c.document('test' + j);
          assertEqual('peterY' + j, doc.value);
        }
      }
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
