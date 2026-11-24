/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup */

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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const { assertEqual, assertNotEqual, assertTrue } = jsunity.jsUnity.assertions;
const helper = require('@arangodb/testutils/agency-restart-helper');
const agencyMgr = global.instanceManager.agencyMgr;

const count = 50000;

if (runSetup === true) {
  helper.writeTestEntries(agencyMgr, count);
  helper.waitForCompactionsToSettle();
  helper.syncToDisk();
  return 0;
}

function recoverySuite () {
  'use strict';

  return {
    testRestart: function () {
      let compactionIndex = helper.latestCompactionIndex();
      assertNotEqual(0, compactionIndex);
      let log = helper.persistedLog(agencyMgr);
      // the persisted log keeps entries before the compaction point
      assertTrue(log[0].index <= compactionIndex);
      assertTrue(compactionIndex <= log[log.length - 1].index);
      helper.assertContiguousLog(log);
      helper.testEntries(log).forEach(helper.assertWellFormedTestEntry);
      helper.assertTestValuesPresent(agencyMgr, count);
    }
  };
}

jsunity.run(recoverySuite);
return jsunity.done();
