/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertTrue, assertFalse */
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
/// @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');
  
const cn = 'UnitTestsRecovery';

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  let c1 = db._create(cn + '1');
  c1.ensureIndex({ type: "persistent", fields: ["a"], estimates: true });
  
  let c2 = db._create(cn + '2');
  c2.ensureIndex({ type: "persistent", fields: ["a"], estimates: false });
  
  let c3 = db._create(cn + '3');
  c3.ensureIndex({ type: "persistent", fields: ["a"] });

  c1.insert({ _key: 'crashme' }, true);

  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {

    testIndexEstimates: function () {
      let c = db._collection(cn + '1');
      let indexes = c.indexes();
      assertEqual("persistent", indexes[1].type);
      assertTrue(indexes[1].hasOwnProperty('estimates'));
      assertTrue(indexes[1].estimates);
      assertTrue(indexes[1].hasOwnProperty('selectivityEstimate'));

      c = db._collection(cn + '2');
      indexes = c.indexes();
      assertEqual("persistent", indexes[1].type);
      assertTrue(indexes[1].hasOwnProperty('estimates'));
      assertFalse(indexes[1].estimates);
      assertFalse(indexes[1].hasOwnProperty('selectivityEstimate'));
      
      c = db._collection(cn + '3');
      indexes = c.indexes();
      assertEqual("persistent", indexes[1].type);
      assertTrue(indexes[1].hasOwnProperty('estimates'));
      assertTrue(indexes[1].estimates);
      assertTrue(indexes[1].hasOwnProperty('selectivityEstimate'));
    }

  };
}

jsunity.run(recoverySuite);
return jsunity.done();
