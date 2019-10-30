/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

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
/// @author Yuriy Popov
/// @author Copyright 2019, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");
let db = require("@arangodb").db;
let isCluster = require("internal").isCluster();

function lateDocumentMaterializationRuleTestSuite () {
  const ruleName = "late-document-materialization";
  const cn = "UnitTestsCollection";
  const cn1 = "UnitTestsCollection1";
  return {
    setUpAll : function () {
      db._drop(cn);
      db._drop(cn1);

      let c = db._create(cn, { numberOfShards: 3 });
      let c1 = db._create(cn1, { numberOfShards: 3 });

      c.ensureIndex({type: "hash", fields: ["obj.a", "obj.b", "obj.c"]});
      c1.ensureIndex({type: "hash", fields: ["tags.hop[*].foo.fo", "tags.hop[*].bar.br", "tags.hop[*].baz.bz"]});

      c.save({ _key: 'c0',  "obj": {"a": "a_val", "b": "b_val", "c": "c_val", "d": "d_val"}});
      c.save({ _key: 'c1',  "obj": {"a": "a_val_1", "b": "b_val_1", "c": "c_val_1", "d": "d_val_1"}});
      c.save({ _key: 'c2',  "obj": {"a": "a_val", "b": "b_val_2", "c": "c_val_2", "d": "d_val_2"}});
      c.save({ _key: 'c3',  "obj": {"a": "a_val_3", "b": "b_val_3", "c": "c_val_3", "d": "d_val_3"}});

      c1.save({ _key: 'c1_0',  "tags": {"hop": [{"foo": {"fo": "a_val_1"}}, {"bar": {"br": "bar_val"}}, {"baz": {"bz": "baz_val"}}]}});
      c1.save({ _key: 'c1_1',  "tags": {"hop": [{"foo": {"fo": "foo_val_1"}}, {"bar": {"br": "bar_val_1"}}, {"baz": {"bz": "baz_val_1"}}]}});
      c1.save({ _key: 'c1_2',  "tags": {"hop": [{"foo": {"fo": "a_val_1"}}, {"bar": {"br": "bar_val_2"}}, {"baz": {"bz": "baz_val_2"}}]}});
      c1.save({ _key: 'c1_3',  "tags": {"hop": [{"foo": {"fo": "foo_val_3"}}, {"bar": {"br": "bar_val_3"}}, {"baz": {"bz": "baz_val_3"}}]}});
    },

    tearDownAll : function () {
      try { db._drop(cn); } catch(e) {}
      try { db._drop(cn1); } catch(e) {}
    },
    testNotAppliedDueToNoFilter() {
      let query = "FOR d IN " + cn  + " SORT d.obj.c LIMIT 10 RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
    },
    testNotAppliedDueToSort() {
      let query = "FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' SORT d.obj.b LIMIT 10 RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
    },
    testNotAppliedDueToNoSort() {
      let query = "FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' LIMIT 10 RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
    },
    testNotAppliedDueToUsedInInnerSort() {
      let query = "FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' SORT d.obj.c ASC SORT d.obj.b LIMIT 10 RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
    },
    testNotAppliedDueToNoLimit() {
      let query = "FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' SORT d.obj.c RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
    },
    testNotAppliedDueToLimitOnWrongNode() {
      let query = "FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' " +
                  " SORT d.obj.c LET c = CHAR_LENGTH(d.obj.d) * 2 SORT CONCAT(d.obj.c, c) LIMIT 10 RETURN { doc: d, sc: c} ";
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
    },
    testNotAppliedDueToNoReferences() {
      let query = "FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' SORT RAND() LIMIT 10 RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
    },
    testQueryResultsWithCalculation() {
      let query = "FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' LET c = CONCAT(d.obj.b, RAND()) SORT c LIMIT 10 RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expectedKeys = new Set(['c0', 'c2']);
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc._key));
        expectedKeys.delete(doc._key);
      });
      assertEqual(0, expectedKeys.size);
    },
    testQueryResultsWithAfterSort() {
      let query = "FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' SORT d.obj.c LIMIT 10 SORT NOOPT(d.obj.a) ASC RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expectedKeys = new Set(['c0', 'c2']);
      let currentValue  = 0;
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc._key));
        expectedKeys.delete(doc._key);
      });
      assertEqual(0, expectedKeys.size);
    },
    testQueryResultsWithMultiSort() {
      let query = "FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' " +
                  "SORT d.obj.c LIMIT 2 SORT d.obj.b DESC LIMIT 1 RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let materializeNodeFound = false;
      let nodeDependency  = null;
      plan.nodes.forEach(function(node) {
        if (node.type === "MaterializeNode") {
          // there should be no materializer before (e.g. double materialization)
          assertFalse(materializeNodeFound);
          materializeNodeFound = true;
          // the other sort node should be limited but not have a materializer
          // d.obj.c node on single and d.obj.b on cluster as for cluster
          // only first sort will be on DBServers
          assertEqual(nodeDependency.limit, isCluster ? 2 : 1);
        }
        nodeDependency = node; // as we walk the plan this will be next node dependency
      });
      // materilizer should be there
      assertTrue(materializeNodeFound);
      let result = AQL_EXECUTE(query);
      assertEqual(1, result.json.length);
      let expectedKeys = new Set(['c2']);
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc._key));
        expectedKeys.delete(doc._key);
      });
      assertEqual(0, expectedKeys.size);
    },
    testQueryResultsWithAfterCalc() {
      let query = "FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' SORT d.obj.c LIMIT 10 LET c = CONCAT(NOOPT(d._key), '-C') RETURN c ";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expected = new Set(['c0-C', 'c2-C']);
      result.json.forEach(function(doc) {
        assertTrue(expected.has(doc));
        expected.delete(doc);
      });
      assertEqual(0, expected.size);
    },
    testQueryResultsSkipSome() {
      let query = "FOR d IN " + cn + " FILTER d.obj.a == 'a_val' SORT d.obj.c DESC LIMIT 1, 1 RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let result = AQL_EXECUTE(query);
      assertEqual(1, result.json.length);
      assertEqual(result.json[0]._key, 'c0');
    },
    testQueryResultsSkipAll() {
      let query = "FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' SORT d.obj.c LIMIT 5, 10 RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let result = AQL_EXECUTE(query);
      assertEqual(0, result.json.length);
    },
    testQueryResultsInSubquery() {
      let query = "FOR c IN " + cn + " FILTER c.obj.a == 'a_val_1' " +
                    " FOR d IN " + cn1  + " FILTER c.obj.a IN d.tags.hop[*].foo.fo SORT d.tags.hop[*].baz.bz LIMIT 10 RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expected = new Set(['c1_0', 'c1_2']);
      result.json.forEach(function(doc) {
        assertTrue(expected.has(doc._key));
        expected.delete(doc._key);
      });
      assertEqual(0, expected.size);
    },
    testQueryResultsInOuterSubquery() {
      let query = "FOR c IN " + cn + " FILTER c.obj.a == 'a_val_1' SORT c.obj.c LIMIT 10 " +
                    " FOR d IN " + cn1  + " FILTER c.obj.a IN d.tags.hop[*].foo.fo RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expected = new Set(['c1_0', 'c1_2']);
      result.json.forEach(function(doc) {
        assertTrue(expected.has(doc._key));
        expected.delete(doc._key);
      });
      assertEqual(0, expected.size);
    },
    testQueryResultsMultipleLimits() {
      let query = " FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' SORT d.obj.c " +
                  " LIMIT 1, 5 SORT d.obj.b LIMIT 1, 3 SORT NOOPT(d.obj.d) DESC  " +
                  " LIMIT 1, 1 RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let materializeNodeFound = false;
      let nodeDependency = null;
      // sort by d.obj.b node`s limit must be appended with materializer (identified by limit value = 3)
      // as last SORT needs materialized document
      // and SORT by d.obj.d is not lowest possible variant
      // However in cluster only first sort suitable, as later sorts depend 
      // on all db servers results and performed on coordinator
      plan.nodes.forEach(function(node) {
        if (node.type === "MaterializeNode") {
          assertFalse(materializeNodeFound); // no double materialization
          assertEqual(nodeDependency.limit, isCluster ? 6 : 3);
          materializeNodeFound = true;
        }
        nodeDependency = node;
      });
      assertTrue(materializeNodeFound);
    },
    testQueryResultsMultipleLimits2() {
      // almost the same as testQueryResultsMultipleLimits but without last sort - this 
      // will not create addition variable for sort 
      // value but it should not affect results especially on cluster!
      let query = " FOR d IN " + cn  + " FILTER d.obj.a == 'a_val' SORT d.obj.c " +
                  " LIMIT 1, 5 SORT d.obj.b LIMIT 1, 3 " +
                  " RETURN d ";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let materializeNodeFound = false;
      // sort by d.obj.b node`s limit must be appended with materializer (identified by limit value = 3)
      // as SORT by d.obj.c is not lowest possible variant
      // However in cluster only first sort suitable, as later sorts depend 
      // on all db servers results and performed on coordinator
      let nodeDependency = null;
      plan.nodes.forEach(function(node) {
        if (node.type === "MaterializeNode") {
          assertFalse(materializeNodeFound);
          assertEqual(nodeDependency.limit, isCluster ? 6 : 3);
          materializeNodeFound = true;
        }
        nodeDependency = node;
      });
      assertTrue(materializeNodeFound);
    },
  };
}

jsunity.run(lateDocumentMaterializationRuleTestSuite);

return jsunity.done();
