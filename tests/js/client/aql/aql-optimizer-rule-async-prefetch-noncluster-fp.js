/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual */

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
const internal = require('internal');
const db = internal.db;
const helper = require("@arangodb/aql-helper");
const errors = internal.errors;
const assertQueryError = helper.assertQueryError;
let IM = global.instanceManager;
const {aql} = require('@arangodb');

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

    // This failure was observed when the exectution block were not destroyed in topological order
    // and the async task had outlived its execution block
    testAsyncPrefetchFailure : function () {
      IM.debugSetFailAt("AsyncPrefetch::blocksDestroyedOutOfOrder");
      // This query is forced to have async prefetch optimization
      const query = aql`FOR d IN ${col} FILTER d.val > 0 LET x = FAIL('failed') RETURN d`;
      query.options = {optimizer: {rules: ["-move-filters-into-enumerate", "-move-filters-into-enumerate2", "-fuse-filters"]}};
      const expectedNodes = [["SingletonNode", false], ["EnumerateCollectionNode", true], ["CalculationNode", true], ["FilterNode", true], ["CalculationNode", false], ["ReturnNode", false]];

      const result = db._createStatement(query).explain();

      const actualNodes = result.plan.nodes.map((n) => [n.type, n.isAsyncPrefetchEnabled === true ? true : false]);
      assertEqual(expectedNodes, actualNodes, query);
      
      let expectPrefetchRule = expectedNodes.filter((n) => n[1]).length > 0;
      if (expectPrefetchRule) {
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName));
      } else {
        assertEqual(-1, result.plan.rules.indexOf(ruleName));
      }
      assertQueryError(
          errors.ERROR_QUERY_FAIL_CALLED.code,
          query,
          query.bindParams,
          query.options
      );

      IM.debugRemoveFailAt("AsyncPrefetch::blocksDestroyedOutOfOrder");
    },
  };
}


jsunity.run(AsyncPrefetchFailure);

return jsunity.done();
