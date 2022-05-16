/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / @brief recovery test for cacheEnabled flag for indexes
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
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');

const cn = 'UnitTestsRecovery';

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop(cn);
  let c = db._create(cn);
  c.ensureIndex({ type: "persistent", fields: ["value1"], cacheEnabled: true, name: "idx-1" });
  c.ensureIndex({ type: "persistent", fields: ["value2"], cacheEnabled: false, name: "idx-2" });

  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ value1: i, value2: i });
  }
  c.insert(docs);

  c.insert({ _key: 'crashme' }, true);

  internal.debugTerminate('crashing server');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testIndexRecovery: function() {
      let c = db._collection(cn);
      let indexes = c.indexes();
      assertEqual(3, indexes.length);
      assertEqual('primary', indexes[0].type);
      assertEqual('persistent', indexes[1].type);
      assertEqual('idx-1', indexes[1].name);
      assertTrue(indexes[1].cacheEnabled);
      assertEqual('persistent', indexes[2].type);
      assertEqual('idx-2', indexes[2].name);
      assertFalse(indexes[2].cacheEnabled);
    },

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
