/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue */

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
const arangodb = require('@arangodb');
const db = arangodb.db;
const { instanceRole } = require('@arangodb/testutils/instance');

const IM = global.instanceManager;

const srcName = 'UnitTestsIntermediateCommitSrc';
const dstName = 'UnitTestsIntermediateCommitDst';
const numDocs = 20000;
const numberOfShards = 4;
const numIterations = 5;

function sumIntermediateCommits() {
  return IM.arangods
    .filter((arangod) => arangod.isRole(instanceRole.dbServer))
    .reduce((sum, arangod) => sum + arangod.getMetric('arangodb_intermediate_commits_total'), 0);
}

// BTS-2456: Data race between snippets of a single  query on dbserver
function transactionIntermediateCommitConcurrentSnippetsSuite() {
  'use strict';

  return {
    setUpAll: function () {
      const src = db._create(srcName, { numberOfShards });
      db._create(dstName, { numberOfShards, replicationFactor: 2 });

      let docs = [];
      for (let i = 0; i < numDocs; ++i) {
        docs.push({ value: i });
        if (docs.length === 5000) {
          src.insert(docs);
          docs = [];
        }
      }
      if (docs.length > 0) {
        src.insert(docs);
      }
    },

    tearDownAll: function () {
      db._drop(dstName);
      db._drop(srcName);
    },

    testCopyWithIntermediateCommitsAndReplication: function () {
      const dst = db._collection(dstName);
      for (let i = 0; i < numIterations; ++i) {
        dst.truncate();
        const before = sumIntermediateCommits();
        // we will do 5000/100 = 50 intermediate commits, meaning we have 50 chances to catch
        // tsan race
        db._query(`FOR d IN ${srcName} INSERT { value: d.value } INTO ${dstName}`, {},
                  { intermediateCommitCount: 100 });

        assertEqual(numDocs, dst.count());
        assertTrue(sumIntermediateCommits() > before, 'expected intermediate commits to happen');
      }
    },
  };
}

jsunity.run(transactionIntermediateCommitConcurrentSnippetsSuite);
return jsunity.done();
