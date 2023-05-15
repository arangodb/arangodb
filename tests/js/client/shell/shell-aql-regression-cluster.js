////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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

'use strict';

const jsunity = require("jsunity");
const {assertEqual, assertTypeOf} = jsunity.jsUnity.assertions;

const arangodb = require("@arangodb");
const db = arangodb.db;
const _ = require('lodash');

function ShellAqlRegressionSuite () {
  const colName = 'UnitTestsCollection';
  const shardKeys = [];

  return {
    setUpAll: function () {
      // Create two shards, in one of them all `.value`s are smaller than all in the other.
      const numberOfShards = 2;
      const col = db._create(colName, {numberOfShards, shardKeys: ["shardKey"]});
      const shardToKey = new Map();
      for (let shardKey = 0; shardToKey.size < numberOfShards; ++shardKey) {
        shardToKey.set(col.getResponsibleShard({shardKey}), shardKey);
      }
      let value = 0;
      for (const [shard, shardKey] of shardToKey) {
        shardKeys.push(shardKey);
        const docs = Array.from({length: 1001}).map(_ => ({shardKey, value: value++}));
        col.insert(docs);
      }
    },

    tearDownAll: function () {
      db._drop(colName);
    },

    testConstrainedSortingGatherSoftLimit: function () {
      const limit = 10;
      const query = `FOR d IN @@col SORT d.value LIMIT @limit RETURN d`;
      const bindVars = {'@col': colName, limit};
      const batchSize = 1;
      const options = {
        optimizer: {rules: ['-parallelize-gather']},
        stream: true,
      };
      const data = {query, bindVars, batchSize, options};
      const stmt = db._createStatement(data);
      const {plan} = stmt.explain();
      assertEqual([
          'SingletonNode',
          'EnumerateCollectionNode',
          'CalculationNode',
          'SortNode',
          'LimitNode',
          'RemoteNode',
          'GatherNode',
          'LimitNode',
          'ReturnNode',
        ],
        plan.nodes.map(node => node.type),
      );
      const sortNode = plan.nodes[3];
      assertEqual('SortNode', sortNode.type);
      assertEqual("constrained-heap", sortNode.strategy);
      const gatherNode = plan.nodes[6];
      assertEqual('GatherNode', gatherNode.type);
      assertEqual("undefined", gatherNode.parallelism);
      // It's important to check the limit, it's only in the constrained case
      assertEqual(limit, gatherNode.limit);
      const cursor = stmt.execute();
      try {
        const results = [];
        while (cursor.hasNext()) {
          results.push(cursor.next());
        }
        assertEqual(_.range(limit), results.map(doc => doc.value));
        assertTypeOf('number', results[0].shardKey);
        // key of the first shard, containing the smaller elements
        const firstShardsKey = shardKeys[0];
        assertEqual(_.fill(Array(limit), firstShardsKey), results.map(doc => doc.shardKey));
      } finally {
        cursor.dispose();
      }
    },
  };
}

function ShellAqlParallelGatherSuite() {
  const colName = 'UnitTestsCollection';
  return {
    setUpAll: function () {
      const numberOfShards = 2;
      const col = db._create(colName, {numberOfShards});
      col.insert({});
    },

    tearDownAll: function () {
      db._drop(colName);
    },

    testLongRunningUpstream: function () {
      // Regression test for https://arangodb.atlassian.net/browse/BTS-1400
      // Previously we would only wait for up to 30s when trying to open an engine.
      // If this failed, we would return with this "query ID xxx not found" error.
      // However, there is no good reason to give up after 30s. In fact, there are
      // enough cases where the upstream operation can take longer than 30s, so we
      // have to wait for as long as it takes.
      assertEqual(1, db[colName].count);
      db._query("FOR d in @@col  LET x = SLEEP(35) INSERT {} INTO @@col", { "@col": colName });
      assertEqual(2, db[colName].count);

      // The explain output for this query looks as follows:
      // Execution plan:
      //  Id   NodeType          Site  Est.   Comment
      //   1   SingletonNode     DBS      1   * ROOT
      //   4   CalculationNode   DBS      1     - LET #3 = {  }   /* json expression */   /* const assignment */
      //  14   IndexNode         DBS      1     - FOR d IN UnitTestsCollection   /* primary index scan, scan only, 2 shard(s) */    
      //   3   CalculationNode   DBS      1       - LET x = SLEEP(35)   /* simple expression */
      //  12   RemoteNode        COOR     1       - REMOTE
      //  13   GatherNode        COOR     1       - GATHER   /* parallel, unsorted */
      //  15   CalculationNode   COOR     1       - LET #5 = MAKE_DISTRIBUTE_INPUT_WITH_KEY_CREATION(#3, null, { "allowSpecifiedKeys" : false, "ignoreErrors" : false, "collection" : "UnitTestsCollection" })   /* simple expression */
      //   6   DistributeNode    COOR     1       - DISTRIBUTE #5
      //   7   RemoteNode        DBS      1       - REMOTE
      //   5   InsertNode        DBS      0       - INSERT #5 IN UnitTestsCollection 
      //   8   RemoteNode        COOR     0       - REMOTE
      //   9   GatherNode        COOR     0       - GATHER   /* parallel, unsorted */

      // In the first parallel gather we fan out to the db servers, which then go
      // back to the coordinator. These requests to the coordinator may come in
      // concurrently. However, only the first one will be able to open the engine
      // and then perform the next request to the db server. The engine will not
      // be released while we wait for this response. Since the db server sleeps
      // for 35s this can take a while during which the other requests have to
      // wait and must not abort the query.
      // This test basically just checks that the query does not fail.
    },
  };
}

jsunity.run(ShellAqlRegressionSuite);
jsunity.run(ShellAqlParallelGatherSuite);
return jsunity.done();
