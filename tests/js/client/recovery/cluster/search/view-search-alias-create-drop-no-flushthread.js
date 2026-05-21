/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertNull, assertTrue, assertFalse, assertNotNull */

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
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db;
var internal = require('internal');
var jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();
  var v;

  db._dropView('UnitTestsRecovery');
  v = db._createView('UnitTestsRecovery', 'search-alias', {});

  internal.wal.flush(true, true);
  global.instanceManager.debugSetFailAt("RocksDBBackgroundThread::run");
  internal.wait(2); // make sure failure point takes effect

  db._dropView('UnitTestsRecovery');

  return 0;
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    testIResearchViewCreateDropNoFlushThread: function () {
      assertNull(db._view('UnitTestsRecovery'));
    }
  };
}

jsunity.run(recoverySuite);
return jsunity.done();
