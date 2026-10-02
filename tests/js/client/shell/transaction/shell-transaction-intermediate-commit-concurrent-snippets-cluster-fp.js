/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const db = require('@arangodb').db;
const { instanceRole } = require('@arangodb/testutils/instance');

const IM = global.instanceManager;

const srcName = 'UnitTestsIntermediateCommitSrc';
const dstName = 'UnitTestsIntermediateCommitDst';
const numDocs = 12000;
const numberOfShards = 4;

// BTS-2456: the test is designed to catch a tsan race with intermediate commits interleaving 
// and needing to reaquire the lock.
function transactionIntermediateCommitConcurrentSnippetsSuite() {
  'use strict';

  const failurePoint = 'RocksDBTrxBaseMethods::sleepAfterIntermediateCommitReBegin';

  return {
    setUp: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      db._create(srcName, { numberOfShards, replicationFactor: 2 })
        .ensureIndex({ type: 'persistent', fields: ['value'] });
      db._create(dstName, { distributeShardsLike: srcName });
      db._query(`FOR i IN 1..${numDocs} INSERT { value: i } INTO ${srcName}`);
    },

    tearDown: function () {
      IM.debugClearFailAt('', instanceRole.dbServer);
      db._drop(dstName);
      db._drop(srcName);
    },

    testCopyWithIntermediateCommitsAndReplication: function () {
      const shards = db._collection(dstName).shards(true);
      const leader = IM.getInstanceByID(Object.values(shards)[0][0]);
      leader.debugSetFailAt(failurePoint);

      db._query(`FOR d IN ${srcName} FILTER d.value >= 0 INSERT d INTO ${dstName}`, {},
                { intermediateCommitCount: 100 });
      assertEqual(numDocs, db._collection(dstName).count());
    },
  };
}

jsunity.run(transactionIntermediateCommitConcurrentSnippetsSuite);
return jsunity.done();
