/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse */
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
  db._drop('UnitTestsRecoveryDummy2');

  var c = db._create('UnitTestsRecoveryDummy');
  var i1 = c.ensureIndex({ type: "inverted", name: "i1", fields: [ "a", "b", "c" ] });
  var meta = { indexes: [ { index: i1.name, collection: c.name() } ] };

  db._dropView('UnitTestsRecovery1');
  var v = db._createView('UnitTestsRecovery1', 'search-alias', {});
  v.properties(meta);

  internal.wal.flush(true, true);
  internal.debugSetFailAt("RocksDBBackgroundThread::run");
  internal.wait(2); // make sure failure point takes effect

  c.drop();

  c = db._create('UnitTestsRecoveryDummy2');
  c.save({ name: 'crashme' }, true);

}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    testIResearchLinkDropCollectionNoFlushThread: function () {
      let checkView = function(viewName) {
        let v = db._view(viewName);
        assertEqual(v.name(), viewName);
        assertEqual(v.type(), 'search-alias');
        let indexes = v.properties().indexes;
        assertEqual(0, indexes.length);
      };
      checkView("UnitTestsRecovery1");
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
