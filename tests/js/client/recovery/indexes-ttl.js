/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertFalse, assertTrue, assertNotNull */
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

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  let c = db._create('UnitTestsRecovery1');
  c.ensureIndex({ type: "ttl", fields: ["value"], expireAfter: 60 });
  
  c = db._create('UnitTestsRecovery2');
  c.ensureIndex({ type: "ttl", fields: ["value"], expireAfter: 600, estimates: true });

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
    testIndexesTtl: function () {
      let c = db._collection('UnitTestsRecovery1');
      let idx = c.getIndexes()[1];

      assertEqual("ttl", idx.type);
      assertEqual(["value"], idx.fields);
      assertEqual(60, idx.expireAfter);
      assertFalse(idx.estimates);
      
      c = db._collection('UnitTestsRecovery2');
      idx = c.getIndexes()[1];

      assertEqual("ttl", idx.type);
      assertEqual(["value"], idx.fields);
      assertEqual(600, idx.expireAfter);
      assertFalse(idx.estimates);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
