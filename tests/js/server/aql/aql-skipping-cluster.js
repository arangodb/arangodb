/*jshint globalstrict:true, strict:true, esnext: true */

'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const internal = require("internal");
const jsunity = require("jsunity");
const {assertEqual} = jsunity.jsUnity.assertions;

const db = internal.db;


function aqlSkippingClusterTestsuite () {
  const colName = 'UnitTestsAhuacatlSkipCluster';
  const numberOfShards = 16;
  let col;

  return {
    setUpAll: function () {
      col = db._create(colName, {numberOfShards});
    },
    tearDownAll: function () {
      col.drop();
    },

    /**
     * Regression test for PR https://github.com/arangodb/arangodb/pull/10190.
     * This bug was never in a released version.
     * In an AQL cluster query, when gathering unsorted data in combination with a LIMIT with non-zero offset,
     * if this offset exactly matches the number of documents in the first shards consumed, the rest of the documents
     * was not returned.
     * The test is undeterministic, but has a high chance to detect this problem.
     * This can only trigger when we skip a shard with the exact number of documents left in it,
     * AND the shard returning DONE with that skip.
     * Because of this, this problem didn't occur in 3.5, as the UnsortingGather dependencies were always RemoteNodes,
     * and the RemoteNodes always reported HASMORE when at least one document was skipped.
     * If we just used EnumerateCollection, the RocksDB iterator would also report HASMORE with the last document.
     * Thus we use a FILTER here, which overfetches - the test relies on this and will not work without it, but I see
     * no other way without writing a plan manually.
     */
    testSkipExactDocsInShard: function () {
      const query = 'FOR doc IN @@col FILTER doc._key != "" LIMIT 1, null RETURN doc';
      const bind = {'@col': colName};
      for (let i = 0; i < 2*numberOfShards; ++i) {
        col.insert({});
        const res = db._query(query, bind);
        const n = res.toArray().length;
        assertEqual(i, n);
      }
    },
  };
}

jsunity.run(aqlSkippingClusterTestsuite);

return jsunity.done();
