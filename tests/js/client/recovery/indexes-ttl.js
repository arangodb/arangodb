/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue, assertNotNull */
// //////////////////////////////////////////////////////////////////////////////
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

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  let c = db._create('UnitTestsRecovery1');
  c.ensureIndex({ type: "ttl", fields: ["value"], expireAfter: 60 });
  
  c = db._create('UnitTestsRecovery2');
  c.ensureIndex({ type: "ttl", fields: ["value"], expireAfter: 600, estimates: true });

  c.save({ _key: 'crashme' }, true);

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
