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
      const gatherNode = plan.nodes[5];
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

jsunity.run(ShellAqlRegressionSuite);
return jsunity.done();
