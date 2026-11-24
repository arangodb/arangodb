/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup, getOptions */

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

if (getOptions === true) {
  // set at startup, so compaction is off before the agent first gets to it,
  // both while writing and right after the restart
  return { 'server.failure-point': 'State::compact' };
}

const jsunity = require('jsunity');
const { assertEqual } = jsunity.jsUnity.assertions;
const helper = require('@arangodb/testutils/agency-restart-helper');
const agencyMgr = global.instanceManager.agencyMgr;

const count = 50000;

if (runSetup === true) {
  helper.writeTestEntries(agencyMgr, count);
  helper.syncToDisk();
  return 0;
}

function recoverySuite () {
  'use strict';

  return {
    testRestart: function () {
      assertEqual(0, helper.latestCompactionIndex());
      let log = helper.persistedLog(agencyMgr);
      helper.assertContiguousLog(log);
      let entries = helper.testEntries(log);
      assertEqual(count, entries.length);
      entries.forEach(helper.assertWellFormedTestEntry);
      helper.assertTestValuesPresent(agencyMgr, count);
    }
  };
}

jsunity.run(recoverySuite);
return jsunity.done();
