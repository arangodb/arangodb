/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual */

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
  for (let i = 0; i < 5; ++i) {
    db._drop('UnitTestsRecovery' + i);
    let c = db._create('UnitTestsRecovery' + i);
    c.save({ _key: 'foo', value1: 'foo', value2: 'bar' });

    c.ensureIndex({ type: "persistent", fields: ["value1"] });
    c.ensureIndex({ type: "persistent", fields: ["value2"] });
  }

  // drop all indexes but primary
  let c;
  for (let i = 0; i < 4; ++i) {
    c = db._collection('UnitTestsRecovery' + i);
    let idx = c.getIndexes();
    for (let j = 1; j < idx.length; ++j) {
      c.dropIndex(idx[j].id);
    }
  }

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
    // / @brief test whether the data are correct after a restart
    // //////////////////////////////////////////////////////////////////////////////

    testDropIndexes: function () {
      var i, c, idx;

      for (i = 0; i < 4; ++i) {
        c = db._collection('UnitTestsRecovery' + i);
        idx = c.getIndexes();
        assertEqual(1, idx.length);
        assertEqual('primary', idx[0].type);
      }

      c = db._collection('UnitTestsRecovery4');
      idx = c.getIndexes();
      assertEqual(3, idx.length);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
