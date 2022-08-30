/* jshint globalstrict:false, strict:false, unused : false */
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const errors = internal.errors;
const jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  let c = db._create('UnitTestsRecovery1');
  c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });

  c = db._create('UnitTestsRecovery2');
  c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });

  // set failure point
  internal.debugSetFailAt("ArangoSearch::FailOnCommit");

  db['UnitTestsRecovery1'].insert({ value: 'testi' });

  internal.sleep(5);

  // wait for synchronization of links
  try {
    db._query("FOR doc IN UnitTestsRecovery1 OPTIONS {indexHint: 'inverted', waitForSync: true} FILTER doc.value == '1' RETURN doc");
  } catch (err) {
  }

  // remove failure point 
  internal.debugClearFailAt();

  db['UnitTestsRecovery2'].insert({ value: 'testi' });
  db._query("FOR doc IN UnitTestsRecovery2 OPTIONS {indexHint: 'inverted', waitForSync: true} FILTER doc.value == '1' RETURN doc");

  internal.debugTerminate('crashing server');
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testIndexHasFailedFlag: function () {
      let idx = db['UnitTestsRecovery1'].indexes()[1];
      print(idx);
      assertTrue(idx.hasOwnProperty('failed'));
      assertTrue(idx.failed);
  
      // queries must fail because index is marked as failed
      try {
        db._query("FOR doc IN UnitTestsRecovery1 OPTIONS {indexHint: 'inverted', waitForSync: true} FILTER doc.value == '1' RETURN doc");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC.code, err.errorNum);
      }

      let result = db._query("FOR doc IN UnitTestsRecovery2 OPTIONS {indexHint: 'inverted', waitForSync: true} FILTER doc.value == '1' RETURN doc").toArray();
      assertEqual(1, result.length);
    }

  };
}

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
