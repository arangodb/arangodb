/* jshint globalstrict:false, strict:false, unused: false */
/* global assertEqual, assertFalse, assertNotNull, fail */
// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for transactions
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

  db._drop('UnitTestsRecovery1');
  let c = db._createEdgeCollection('UnitTestsRecovery1');
  let docs = [];
  for (let i = 0; i < 100000; i++) {
    docs.push({ _key: "test" + i, _from: "test/1", _to: "test/" + i, value: i });
    if (docs.length === 10000) {
      c.insert(docs);
      docs = [];
    }
  }

  c.ensureIndex({ type: "hash", fields: ["value"] });
  c.ensureIndex({ type: "hash", fields: ["value", "_to"], unique: true });
 
  // should trigger range deletion
  c.truncate();

  c = db._create('UnitTestsRecovery2');
  c.insert({}, { waitForSync: true });

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

    testRangeDeleteTruncateIndexes: function () {
      let c = db._collection('UnitTestsRecovery1');
      assertEqual(0, c.count());
      assertNotNull(db._collection('UnitTestsRecovery2'));
  
      assertEqual([], c.edges("test/1"));
      let query = "FOR doc IN @@collection FILTER doc.value == @value RETURN doc";
      
      for (let i = 0; i < 100000; i += 1000) {
        assertFalse(c.exists("key" + i));
        assertEqual([], db._query(query, { "@collection": c.name(), value: i }).toArray());
        assertEqual([], c.edges("test/" + i));
      }

      internal.waitForEstimatorSync(); // make sure estimates are consistent
      let indexes = c.getIndexes(true);
      for (let i of indexes) {
        switch (i.type) {
          case 'primary':
          case 'hash':
          case 'edge':
            assertEqual(i.selectivityEstimate, 1, JSON.stringify(indexes));
            break;
            default:
            fail();
        }
      }
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
