/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue, assertFalse, assertNull, fail, print, arango */
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
/// @author Andrey Abramov
/// @author Copyright 2022, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop('UnitTestsRecoveryDummy');
  var c = db._create('UnitTestsRecoveryDummy');

  db._dropView('UnitTestsRecoveryView');
  db._createView('UnitTestsRecoveryView', 'search-alias', {});

  var i1 = c.ensureIndex({ type: "inverted", name: "i1", fields: [ "a", "b", "c" ] });
  var meta = { indexes: [ { index: i1.name, collection: c.name() } ] };
  db._view('UnitTestsRecoveryView').properties(meta);

  internal.wal.flush(true, true);
  internal.debugSetFailAt("FlushCrashBeforeSyncingMinTick");

  if (global.hasOwnProperty('arango')) {
    // we intend to crash, so we should get to know quickly:
    print('setting short timeout');
    arango.timeout(10);
  }
  try {
    for (let i = 0; i < 100000; i++) {
      c.save({ a: "foo_" + i, b: "bar_" + i, c: i });
    }
    c.save({ name: 'crashme' }, { waitForSync: true });
  } catch (ex) {
    if ((ex.errorNum !== internal.errors.ERROR_HTTP_REQUEST_TIMEOUT.code) &&
        (ex.errorNum !== internal.errors.ERROR_CLUSTER_CONNECTION_LOST.code)) {
      print(ex);
      throw ex;
    }
  }
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    testIResearchLinkPopulateNoCallbacks: function () {
      let checkView = function(viewName, indexName) {
        let v = db._view(viewName);
        assertEqual(v.name(), viewName);
        assertEqual(v.type(), 'search-alias');
        let indexes = v.properties().indexes;
        assertEqual(1, indexes.length);
        assertEqual(indexName, indexes[0].index);
        assertEqual("UnitTestsRecoveryDummy", indexes[0].collection);
      };
      checkView("UnitTestsRecoveryView", "i1");

      var result = db._query("FOR doc IN UnitTestsRecoveryView SEARCH doc.c >= 0 COLLECT WITH COUNT INTO length RETURN length").toArray();
      var expectedResult = db._query("FOR doc IN UnitTestsRecoveryDummy FILTER doc.c >= 0 COLLECT WITH COUNT INTO length RETURN length").toArray();
      assertEqual(expectedResult[0], result[0]);
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
