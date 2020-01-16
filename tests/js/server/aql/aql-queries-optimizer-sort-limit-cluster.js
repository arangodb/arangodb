/*jshint globalstrict:true, strict:true, esnext: true */

"use strict";

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

const jsunity = require('jsunity');
const assert = jsunity.jsUnity.assertions;
const internal = require('internal');
const db = internal.db;
const console = require('console');

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerLimitClusterTestSuite() {
  const cn = 'UnitTestsAhuacatlOptimizerLimitCluster';
  const numberOfShards = 9;
  const docCount = 20;
  let col;

  const runWithOneFilledShard = (fun) => {
    try {
      internal.db._drop(cn);
      col = db._create(cn, {numberOfShards});
      const shards = col.shards();
      assert.assertEqual(numberOfShards, shards.length);
      const aShard = shards[0];

      for (let i = 0, inserted = 0; inserted < docCount; i++) {
        const doc = {_key: "test" + i.toString(), value: inserted};

        const shard = col.getResponsibleShard(doc);
        if (shard === aShard) {
          col.save(doc);
          ++inserted;
        }
      }

      for (const [k, v] of Object.entries(col.count(true))) {
        if (k === aShard) {
          assert.assertEqual(docCount, v);
        } else {
          assert.assertEqual(0, v);
        }
      }

      fun();
    } finally {
      internal.db._drop(cn);
    }
  };

  const getSorts = function (plan) {
    return plan.nodes.filter(node => node.type === "SortNode");
  };

  return {
    setUpAll: function () {
    },
    tearDownAll: function () {
    },

    testSortWithFullCountOnOneShard: function () {
      runWithOneFilledShard(() => {
        const query = `FOR c IN ${cn} SORT c.value LIMIT 5, 10 RETURN c`;

        const queryResult = db._query(query, {}, {fullCount: true, profile: 2});
        const extra = queryResult.getExtra();
        const values = queryResult.toArray();

        const fullCount = extra.stats.fullCount;

        assert.assertEqual(10, values.length);

        assert.assertEqual(fullCount, 20);

        const sorts = getSorts(extra.plan);
        assert.assertEqual(sorts.length, 1);
        // Temporarily disabled:
        // assertEqual(15, sorts[0].limit);
        // assertEqual('constrained-heap', sorts[0].strategy);
        assert.assertEqual('standard', sorts[0].strategy);
      });
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimizerLimitClusterTestSuite);

return jsunity.done();
