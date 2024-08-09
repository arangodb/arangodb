/* jshint globalstrict:false, strict:false, unused: false */
/* global runSetup assertNull, assertNotNull */
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

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';

  // write some documents with autoincrement keys
  db._drop('UnitTestsRecovery1');
  let c = db._createEdgeCollection('UnitTestsRecovery1');
  let docs = [];
  for (let i = 0; i < 100000; i++) {
    docs.push({ _from: "test/1", _to: "test/" + i, value: i });
    if (docs.length === 10000) {
      c.insert(docs);
      docs = [];
    }
  }

  c.ensureIndex({ type: "hash", fields: ["value"] });
  c.ensureIndex({ type: "hash", fields: ["value", "_to"], unique: true });
 
  // should trigger range deletion
  db._drop('UnitTestsRecovery1'); 

  c = db._create('UnitTestsRecovery2');
  c.save({ }, { waitForSync: true });

  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    testRangeDeleteCollectionDropIndexes: function () {
      assertNull(db._collection('UnitTestsRecovery1'));
      assertNotNull(db._collection('UnitTestsRecovery2'));
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
