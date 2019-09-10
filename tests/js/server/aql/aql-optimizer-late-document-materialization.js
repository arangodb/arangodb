/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for late document materialization rule
///
/// @file
///
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
/// @author Andrei Lobov
/// @author Copyright 2019, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");
let db = require("@arangodb").db;
function lateDocumentMaterializationRuleTestSuite () {
  const ruleName = "late-document-materialization";
  const cn = "UnitTestsCollection";
  const cn1 = "UnitTestsCollection1";
  const vn = "UnitTestsView";
  const svn = "SortedTestsView";
  return {
	setUpAll : function () {
      db._dropView(vn);
	  db._dropView(svn);
      db._drop(cn);
      db._drop(cn1);
      
      let c = db._create(cn, { numberOfShards: 3 });
      let c2 = db._create(cn1, { numberOfShards: 3 });
	  db._createView(vn, "arangosearch", { 
	    links: { 
		  [cn] : { includeAllFields: true }, 
		  [cn1] : { includeAllFields: true } 
		}});
	  db._createView(svn, "arangosearch", {
	    consolidationIntervalMsec: 5000, 
		primarySort: [{"field": "str", "direction": "asc"}], 
		links: { 
		  [cn] : { includeAllFields: true }, 
		  [cn1] : { includeAllFields: true } 
		}});

	  for (var i = 0; i < 10; ++i) {
        c.save({ _key: 'c' + i,  col: 'c',  str: 'str' + i , value: i });
		c2.save({_key: 'c_' + i, col: 'c2',  str: 'str' + i , value: i });
      }
	  // trigger view sync
	  db._query("FOR d IN " + vn + " OPTIONS { waitForSync: true } RETURN d");
    },

    tearDownAll : function () {
      db._dropView(vn);
	  db._dropView(svn);
      db._drop(cn);
      db._drop(cn1);
    }, 
	testNotAppliedDueToFilter() {
	  let query = "FOR d IN " + vn  + " FILTER d.value IN [1,2] SORT BM25(d) LIMIT 10 RETURN d ";
	  let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
	},
	testNotAppliedDueToSort() {
	  let query = "FOR d IN " + vn  + " SEARCH d.value IN [1,2] SORT d.value LIMIT 10 RETURN d ";
	  let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
	},
	testNotAppliedDueToCalculation() {
	  let query = "FOR d IN " + vn  + " LET c = d.value + RAND()  SORT BM25(d) LIMIT 10 RETURN c ";
	  let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
	},
	testNotAppliedDueToNoSort() {
	  let query = "FOR d IN " + vn  + " LIMIT 10 RETURN d ";
	  let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
	},
	testNotAppliedDueToNoLimit() {
	  let query = "FOR d IN " + vn  + " SORT BM25(d)  RETURN d ";
	  let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
	},
	testQueryResultsWithMultipleCollections() {
	  let query = "FOR d IN " + vn  + " SEARCH d.value IN [1,2] SORT BM25(d) LIMIT 10 RETURN d ";
	  let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
	  let result = AQL_EXECUTE(query);
      assertEqual(4, result.json.length);
	  let expectedKeys = new Set(['c1', 'c2', 'c_1', 'c_2']);
	  result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc._key));
		expectedKeys.delete(doc._key);
      });
	  assertEqual(0, expectedKeys.size);
	},
	testQueryResultsWithMultipleCollectionsAfterCalc() {
	  let query = "FOR d IN " + vn  + " SEARCH d.value IN [1,2] SORT BM25(d) LIMIT 10 LET c = CONCAT(d._key, '-C') RETURN c ";
	  let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
	  let result = AQL_EXECUTE(query);
      assertEqual(4, result.json.length);
	  let expected = new Set(['c1-C', 'c2-C', 'c_1-C', 'c_2-C']);
	  result.json.forEach(function(doc) {
        assertTrue(expected.has(doc));
		expected.delete(doc);
      });
	  assertEqual(0, expected.size);
	}
  };
}

jsunity.run(lateDocumentMaterializationRuleTestSuite);

return jsunity.done();