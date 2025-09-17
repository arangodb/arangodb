/* jshint strict:true */

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
/// @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const {assertEqual, assertNotEqual} = jsunity.jsUnity.assertions;
const helper = require("@arangodb/aql-helper");
const assertQueryError = helper.assertQueryError;
let IM = global.instanceManager;
const {aql, db, errors} = require('@arangodb');

function AsyncPrefetchFailure () {
  const ruleName = "async-prefetch";

  const cn = "UnitTestsCollection";
  let col;

  return {
    setUpAll : function () {
      col = db._create(cn);
      let docs = [];
      for(let i = 0; i < 10000; ++i) {
       docs.push({val: i});
      }
      col.save(docs);
    },
    
    tearDownAll : function () {
      db._drop(cn);
    },

    tearDown : function () {
      IM.debugClearFailAt();
    },

    // This failure was observed when the execution blocks were not destroyed in
    // topological order, and the async task had outlived its execution block.
    // Regression test for https://arangodb.atlassian.net/browse/BTS-2119
    // (https://github.com/arangodb/arangodb/pull/21709).
    testAsyncPrefetchRegressionBts2119 : function () {
      IM.debugSetFailAt("AsyncPrefetch::blocksDestroyedOutOfOrder");
      // This query is forced to have async prefetch optimization
      const query = aql`FOR d IN ${col} FILTER d.val > 0 LET x = FAIL('failed') RETURN d`;
      query.options = {optimizer: {rules: ["-move-filters-into-enumerate", "-move-filters-into-enumerate2", "-fuse-filters"]}};
      const expectedNodes = [["SingletonNode", false], ["EnumerateCollectionNode", true], ["CalculationNode", true], ["FilterNode", true], ["CalculationNode", false], ["ReturnNode", false]];

      const result = db._createStatement(query).explain();

      const actualNodes = result.plan.nodes.map((n) => [n.type, n.isAsyncPrefetchEnabled === true]);
      assertEqual(expectedNodes, actualNodes, query);

      assertNotEqual(-1, result.plan.rules.indexOf(ruleName));

      assertQueryError(
          errors.ERROR_QUERY_FAIL_CALLED.code,
          query,
          query.bindParams,
          query.options
      );
    },

    // Regression test for https://arangodb.atlassian.net/browse/BTS-2211.
    // There was a race after the prefetch task has written its result, but
    // before the shared_ptr to the task is destructed. If the Query is
    // destructed during that time window, and with it the AqlItemBlockManager,
    // the prefetch task could hold a SharedAqlItemBlockPtr in its result, that
    // would then be released to the already-destroyed AqlItemBlockManager.
    // In maintainer mode, the AqlItemBlockManager's destructor asserts that
    // no AqlItemBlocks are still leased: This test would run into that
    // assertion.
    testAsyncPrefetchRegressionBts2211 : function () {
      // After a prefetch task has written its result, wait a little before
      // destructing the shared_ptr to the task. It's now likely to be
      // destructed after the Query. The AqlItemBlockManager's destructor
      // asserts that no AqlItemBlocks are still in use, particularly for a
      // result in the prefetch task.
      IM.debugSetFailAt("AsyncPrefetch::delayDestructor");
      const query = aql`
        FOR d IN ${col}
          FILTER NOOPT(d.val >= 0)
          LET x = FAIL('failed')
          RETURN d.val
      `;
      query.options = {};
      const expectedNodes = [
        ["SingletonNode", false],
        ["EnumerateCollectionNode", true],
        ["CalculationNode", false],
        ["FilterNode", true],
        ["CalculationNode", false],
        ["ReturnNode", false],
      ];

      const result = db._createStatement(query).explain();

      const actualNodes = result.plan.nodes.map((n) =>
        [ n.type,
          n.isAsyncPrefetchEnabled === true,
        ]);
      assertEqual(expectedNodes, actualNodes, query);

      assertNotEqual(-1, result.plan.rules.indexOf(ruleName));

      for (let i = 0; i < 10000; ++i) {
        assertQueryError(
          errors.ERROR_QUERY_FAIL_CALLED.code,
          query,
          query.bindParams,
          query.options
        );
      }
    },
  };
}


jsunity.run(AsyncPrefetchFailure);

return jsunity.done();
