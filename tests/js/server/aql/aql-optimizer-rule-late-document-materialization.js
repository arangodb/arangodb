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
  const projectionRuleName = "reduce-extraction-to-projection";
  const earlyPruningRuleName = "move-filters-into-enumerate";
  const numOfCollectionIndexes = 2;
  const numOfExpCollections = 2;
  const primaryIndexCollectionName = "UnitTestsPrimCollection";
  const edgeIndexCollectionName = "UnitTestsEdgeCollection";
  const severalIndexesCollectionName = "UnitTestsSeveralIndexesCollection";
  const projectionsCoveredByIndexCollectionName = "ProjectionsCoveredByIndexCollection";
  const prefixIndexCollectionName = "PrefixIndexCollection";
  let collectionNames = [];
  let expCollectionNames = [];
  var i;
  var j;
  for (i = 0; i < numOfCollectionIndexes; ++i) {
    collectionNames.push("UnitTestsCollection" + i);
  }
  for (i = 0; i < numOfExpCollections * numOfCollectionIndexes; ++i) {
    expCollectionNames.push("UnitTestsExpCollection" + i);
  }
  return {
    setUpAll : function () {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        db._drop(collectionNames[i]);
        for (j = 0; j < numOfExpCollections; ++j) {
          db._drop(expCollectionNames[i * numOfCollectionIndexes + j]);
        }
      }
      db._drop(primaryIndexCollectionName);
      db._drop(edgeIndexCollectionName);
      db._drop(severalIndexesCollectionName);
      db._drop(projectionsCoveredByIndexCollectionName);
      db._drop(prefixIndexCollectionName);

      var collections = [];
      var expCollections = [];
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        collections.push(db._create(collectionNames[i], { numberOfShards: 3 }));
        for (j = 0; j < numOfExpCollections; ++j) {
          expCollections.push(db._create(expCollectionNames[i * numOfCollectionIndexes + j], { numberOfShards: 3 }));
        }
      }
      var primCollection = db._create(primaryIndexCollectionName, { numberOfShards: 3 });
      var edgeCollection = db._createEdgeCollection(edgeIndexCollectionName, { numberOfShards: 3 });
      var severalIndexesCollection = db._create(severalIndexesCollectionName, { numberOfShards: 3 });
      var projectionsCoveredByIndexCollection = db._create(projectionsCoveredByIndexCollectionName, { numberOfShards: 3 });
      var prefixIndexCollection = db._create(prefixIndexCollectionName, { numberOfShards: 3 });

      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let type;
        if (i < numOfCollectionIndexes / 2) {
          type = "hash";
        } else {
          type = "skiplist";
        }

        collections[i].ensureIndex({type: type, fields: ["obj.a", "obj.b", "obj.c"]});
        for (j = 0; j < numOfExpCollections; ++j) {
          let fields;
          if (i === 0) {
            fields = ["tags.hop[*].foo.fo", "tags.hop[*].bar.br", "tags.hop[*].baz.bz"];
          } else {
            fields = ["tags.hop[*]"]; // last expansion
          }
          expCollections[i * numOfCollectionIndexes + j].ensureIndex({type: type, fields: fields});
        }
      }
      severalIndexesCollection.ensureIndex({type: "hash", fields: ["a"]});
      severalIndexesCollection.ensureIndex({type: "hash", fields: ["b"]});

      projectionsCoveredByIndexCollection.ensureIndex({type: "hash", fields: ["a", "b", "c"]});

      prefixIndexCollection.ensureIndex({type: "hash", fields: ["obj.a", "obj.b"]});

      for (i = 0; i < numOfCollectionIndexes; ++i) {
        collections[i].save({_key: 'c0',  "obj": {"a": "a_val", "b": "b_val", "c": "c_val", "d": "d_val"}});
        collections[i].save({_key: 'c1',  "obj": {"a": "a_val_1", "b": "b_val_1", "c": "c_val_1", "d": "d_val_1"}});
        collections[i].save({_key: 'c2',  "obj": {"a": "a_val", "b": "b_val_2", "c": "c_val_2", "d": "d_val_2"}});
        collections[i].save({_key: 'c3',  "obj": {"a": "a_val_3", "b": "b_val_3", "c": "c_val_3", "d": "d_val_3"}});

        for (j = 0; j < numOfExpCollections; ++j) {
          let doc;
          if (i === 0) {
            doc = {_key: 'c1',  "tags": {"hop": [{"foo": {"fo": "a_val_1"}}, {"bar": {"br": "bar_val"}}, {"baz": {"bz": "baz_val"}}]}};
          } else {
            doc = {"tags": {"hop": ["hop_array_val"]}}; // for last expansion
          }
          expCollections[i * numOfCollectionIndexes + j].save(doc);
        }
      }
      primCollection.save({_key: "c0", foo: "a_val"});
      primCollection.save({_key: "c1", foo: "b_val"});
      primCollection.save({_key: "c2", foo: "c_val"});

      edgeCollection.save({_key: "c0", _from: "testVertices/c0", _to: "testVertices/c1"});
      edgeCollection.save({_key: "c1", _from: "testVertices/c0", _to: "testVertices/c0"});

      severalIndexesCollection.save({_key: "c0", a: "a_val", b: "b_val"});

      projectionsCoveredByIndexCollection.save({_key: "c0", a: "a_val", b: "b_val", c: "c_val"});

      prefixIndexCollection.save({_key: "c0", obj: {a: {sa: "a_val_0"}, b: {sb: "b_val_0"}}});
      prefixIndexCollection.save({_key: "c1", obj: {a: {sa: "a_val_1"}, b: {sb: "b_val_1"}}});
    },

    tearDownAll : function () {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        try { db._drop(collectionNames[i]); } catch(e) {}
        for (j = 0; j < numOfExpCollections; ++j) {
          try { db._drop(expCollectionNames[i * numOfCollectionIndexes + j]); } catch(e) {}
        }
      }
      try { db._drop(primaryIndexCollectionName); } catch(e) {}
      try { db._drop(edgeIndexCollectionName); } catch(e) {}
      try { db._drop(severalIndexesCollectionName); } catch(e) {}
      try { db._drop(projectionsCoveredByIndexCollectionName); } catch(e) {}
      try { db._drop(prefixIndexCollectionName); } catch(e) {}
    },
    testIssue10845() {
      // this tests a regression described in https://github.com/arangodb/arangodb/issues/10845#issuecomment-575723029:
      // when there is a collection with an index in a query, all LIMITs in the query may be inspected.
      // however, each LIMIT was supposed to be present underneath a FOR loop, otherwise
      // the query could crash. This test is here to just make sure there are no crashes.
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "LET results = (FOR d IN " + collectionNames[i]  + " FILTER d.obj.a == 'a_val' SORT d.obj.c DESC RETURN d) FOR p IN results " +
          "LET validFrom = (RETURN IS_NULL(p.valid_from) ? 0 : p.valid_from) " +
          "LET validTo = (RETURN IS_NULL(p.valid_to) ? 253370764800000 : p.valid_to) " +
          "LET inactive = (RETURN (validFrom[0] > 1579279781654 || validTo[0] < 1579279781654)) " +
          "RETURN (inactive[0]) ? MERGE(p, {inactive: true}) : p";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      }
    },
    testNotAppliedDueToNoFilter() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " SORT d.obj.c LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      }
    },
    testNotAppliedDueToSort() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' SORT d.obj.b LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      }
    },
    testNotAppliedDueToNoSort() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      }
    },
    testNotAppliedDueToUsedInInnerSort() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' SORT d.obj.c ASC SORT d.obj.b LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      }
    },
    testNotAppliedDueToNoLimit() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' SORT d.obj.c RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      }
    },
    testNotAppliedDueToLimitOnWrongNode() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' " +
                    "SORT d.obj.c LET c = CHAR_LENGTH(d.obj.d) * 2 SORT CONCAT(d.obj.c, c) LIMIT 10 RETURN { doc: d, sc: c}";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      }
    },
    testNotAppliedDueToNoReferences() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' SORT RAND() LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      }
    },
    testNotAppliedDueToUpdateDoc() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' UPDATE d IN " + collectionNames[i] + " SORT d.obj.c LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      }
    },
    testNotAppliedDueToExpansion() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        for (j = 0; j < numOfExpCollections; ++j) {
          let filterSort;
          if (i === 0) {
            filterSort = "'a_val' IN d.tags.hop[*].foo.fo SORT d.tags.hop[*].baz.bz";
          } else {
            filterSort = "'hop_array_val' IN d.tags.hop[*] SORT NOOPT(d.tags.hop[*])";
          }
          let query = "FOR d IN " + expCollectionNames[i * numOfCollectionIndexes + j] + " FILTER " + filterSort + " LIMIT 10 RETURN d";
          let plan = AQL_EXPLAIN(query).plan;
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      }
    },
    testNotAppliedDueToEarlyPruningAndNotInIndex() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' AND d.obj.d SORT d.obj.c LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
        assertNotEqual(-1, plan.rules.indexOf(earlyPruningRuleName));
      }
    },
    testNotAppliedDueToPrefixEarlyPruningAndNotInIndex() {
      let query = "FOR d IN " + prefixIndexCollectionName + " FILTER d.obj.b == {sb: 'b_val_0'} AND d.obj.c SORT d.obj.b LIMIT 10 RETURN d";
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
      assertNotEqual(-1, plan.rules.indexOf(earlyPruningRuleName));
    },
    testNotAppliedDueToSeveralIndexes() {
      let query = "FOR d IN " + severalIndexesCollectionName + " FILTER d.a == 'a_val' OR d.b == 'b_val' SORT NOOPT(d.b) DESC LIMIT 10 RETURN d";
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
    },
    testNotAppliedDueToSeveralIndexesPrimary() {
      let query = "FOR d IN " + severalIndexesCollectionName + " FILTER d._key == 'c0' OR d.a == 'a_val' SORT d._key DESC LIMIT 10 RETURN d";
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
    },
    testNotAppliedDueToProjectionsCoveredByIndex() {
      let query = "FOR d IN " + projectionsCoveredByIndexCollectionName + " FILTER d.a == 'a_val' SORT d.c LIMIT 10 RETURN d.b";
      let plan = AQL_EXPLAIN(query).plan;
      assertEqual(-1, plan.rules.indexOf(ruleName));
      assertNotEqual(-1, plan.rules.indexOf(projectionRuleName));
    },
    testNotAppliedDueToSubqueryWithDocumentAccess() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' " +
                    "LET a = NOOPT(d.obj.b) " +
                    "LET e = SUM(FOR c IN " + collectionNames[(i + 1) % numOfCollectionIndexes] + " LET p = CONCAT(d, c.obj.a) RETURN p) " +
                    "SORT CONCAT(a, e) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      }
    },
    testNotAppliedDueToSubqueryWithReturnDocument() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' " +
                    "LET a = NOOPT(d.obj.b) " +
                    "LET e = SUM(FOR c IN " + collectionNames[(i + 1) % numOfCollectionIndexes] + " LET p = NOOPT(CONCAT(d.obj.a, c.obj.a)) RETURN d) " +
                    "SORT CONCAT(a, e) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      }
    },
    testQueryResultsWithSubqueryWithDocumentAccessByAttribute() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' " +
                    "LET a = NOOPT(d.obj.b) " +
                    "LET e = SUM(FOR c IN " + collectionNames[(i + 1) % numOfCollectionIndexes] + " LET p = CONCAT(d.obj.a, c.obj.a) RETURN p) " +
                    "SORT CONCAT(a, e) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        if (!isCluster) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query);
          assertEqual(2, result.json.length);
          let expectedKeys = new Set(['c0', 'c2']);
          result.json.forEach(function(doc) {
            assertTrue(expectedKeys.has(doc._key));
            expectedKeys.delete(doc._key);
          });
          assertEqual(0, expectedKeys.size);
        } else {
          // on cluster this will not be applied as remote node placed before sort node
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      }
    },
    testQueryResultsWithInnerSubqueriesWithDocumentAccessByAttribute() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' " +
                    "LET a = NOOPT(d.obj.b) " +
                    "LET e = SUM(FOR c IN " + collectionNames[(i + 1) % numOfCollectionIndexes] + " LET p = CONCAT(d.obj.a, c.obj.a) " +
                      "LET f = SUM(FOR k IN " + collectionNames[i] + " LET r = CONCAT(c.obj.a, k.obj.a) RETURN CONCAT(c.obj.c, r)) RETURN CONCAT(d.obj.c, p, f)) " +
                    "SORT CONCAT(a, e) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        if (!isCluster) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query);
          assertEqual(2, result.json.length);
          let expectedKeys = new Set(['c0', 'c2']);
          result.json.forEach(function(doc) {
            assertTrue(expectedKeys.has(doc._key));
            expectedKeys.delete(doc._key);
          });
          assertEqual(0, expectedKeys.size);
        } else {
          // on cluster this will not be applied as remote node placed before sort node
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      }
    },
    testQueryResultsWithSubqueryWithoutDocumentAccess() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' " +
                    "LET a = NOOPT(d.obj.b) " +
                    "LET e = SUM(FOR c IN " + collectionNames[(i + 1) % numOfCollectionIndexes] + " LET p = CONCAT(c.obj.b, c.obj.a) RETURN p) " +
                    "SORT CONCAT(a, e) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        if (!isCluster) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query);
          assertEqual(2, result.json.length);
          let expectedKeys = new Set(['c0', 'c2']);
          result.json.forEach(function(doc) {
            assertTrue(expectedKeys.has(doc._key));
            expectedKeys.delete(doc._key);
          });
          assertEqual(0, expectedKeys.size);
        } else {
          // on cluster this will not be applied as remote node placed before sort node
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      }
    },
    testQueryResultsWithCalculation() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' LET c = CONCAT(d.obj.b, RAND()) SORT c LIMIT 10 RETURN d";
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
      }
    },
    testQueryResultsWithAfterSort() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' SORT d.obj.c LIMIT 10 SORT NOOPT(d.obj.a) ASC RETURN d";
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
      }
    },
    testQueryResultsWithMultipleSort() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' " +
                    "SORT d.obj.c LIMIT 2 SORT d.obj.b DESC LIMIT 1 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        let nodeDependency = null;
        plan.nodes.forEach(function(node) {
          if (node.type === "MaterializeNode") {
            // there should be no materializer before (e.g. double materialization)
            assertFalse(materializeNodeFound);
            materializeNodeFound = true;
            // the other sort node should be sorted but not have a materializer
            // d.obj.c node on single and d.obj.b on cluster as for cluster
            // only first sort will be on DBServers (identified by sort ASC)
            isCluster ? assertTrue(nodeDependency.elements[0].ascending) : assertEqual(nodeDependency.limit, 1);
          }
          nodeDependency = node; // as we walk the plan this will be next node dependency
        });
        // materilizer should be there
        assertTrue(materializeNodeFound);
        let result = AQL_EXECUTE(query);
        assertEqual(1, result.json.length);
        assertEqual(result.json[0]._key, 'c2');
      }
    },
    testQueryResultsWithAfterCalc() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' SORT d.obj.c LIMIT 10 LET c = CONCAT(NOOPT(d._key), '-C') RETURN c";
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
      }
    },
    testQueryResultsWithBetweenCalc() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' SORT d.obj.c LET c = CONCAT(NOOPT(d.obj.d), '-C') LIMIT 10 RETURN c";
        let plan = AQL_EXPLAIN(query).plan;
        if (!isCluster) {
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query);
          assertEqual(2, result.json.length);
          let expected = new Set(['d_val-C', 'd_val_2-C']);
          result.json.forEach(function(doc) {
            assertTrue(expected.has(doc));
            expected.delete(doc);
          });
          assertEqual(0, expected.size);
        } else {
          // on cluster this will not be applied as calculation node will be moved up
          assertEqual(-1, plan.rules.indexOf(ruleName));
        }
      }
    },
    testQueryResultsSkipSome() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' SORT d.obj.c DESC LIMIT 1, 1 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(1, result.json.length);
        assertEqual(result.json[0]._key, 'c0');
      }
    },
    testQueryResultsSkipAll() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' SORT d.obj.c LIMIT 5, 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(0, result.json.length);
      }
    },
    testQueryResultsInSubquery() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR c IN " + collectionNames[i] + " FILTER c.obj.a == 'a_val_1' " +
                    "FOR d IN " + collectionNames[(i + 1) % numOfCollectionIndexes] + " FILTER c.obj.a == d.obj.a SORT d.obj.c LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(1, result.json.length);
        assertEqual(result.json[0]._key, 'c1');
      }
    },
    testQueryResultsInOuterSubquery() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR c IN " + collectionNames[i] + " FILTER c.obj.a == 'a_val_1' SORT c.obj.c LIMIT 10 " +
                    "FOR d IN " + collectionNames[(i + 1) % numOfCollectionIndexes] + " FILTER c.obj.a == d.obj.a RETURN d";
        let plan = AQL_EXPLAIN(query, null, {optimizer:{rules:["-reduce-extraction-to-projection"]}}).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(1, result.json.length);
        assertEqual(result.json[0]._key, 'c1');
      }
    },
    testQueryResultsMultipleLimits() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' SORT d.obj.c " +
                    "LIMIT 1, 5 SORT d.obj.b LIMIT 1, 3 SORT NOOPT(d.obj.d) DESC " +
                    "LIMIT 1, 1 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        let nodeDependency = null;
        // sort by d.obj.b node`s limit must be appended with materializer (identified by sort ASC)
        // as last SORT needs materialized document
        // and SORT by d.obj.d is not lowest possible variant
        // However in cluster only first sort suitable, as later sorts depend
        // on all db servers results and performed on coordinator
        plan.nodes.forEach(function(node) {
          if (node.type === "MaterializeNode") {
            assertFalse(materializeNodeFound); // no double materialization
            isCluster ? assertTrue(nodeDependency.elements[0].ascending) : assertEqual(nodeDependency.limit, 3);
            materializeNodeFound = true;
          }
          nodeDependency = node;
        });
        assertTrue(materializeNodeFound);
      }
    },
    testQueryResultsMultipleLimits2() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        // almost the same as testQueryResultsMultipleLimits but without last sort - this
        // will not create addition variable for sort
        // value but it should not affect results especially on cluster!
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' SORT d.obj.c " +
                    "LIMIT 1, 5 SORT d.obj.b DESC LIMIT 1, 3 " +
                    "RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let materializeNodeFound = false;
        // sort by d.obj.b node`s limit must be appended with materializer (identified by SORT ASC)
        // as SORT by d.obj.c is not lowest possible variant
        // However in cluster only first sort suitable, as later sorts depend
        // on all db servers results and performed on coordinator
        let nodeDependency = null;
        plan.nodes.forEach(function(node) {
          if (node.type === "MaterializeNode") {
            assertFalse(materializeNodeFound);
            isCluster ? assertTrue(nodeDependency.elements[0].ascending) : assertEqual(nodeDependency.limit, 3);
            materializeNodeFound = true;
          }
          nodeDependency = node;
        });
        assertTrue(materializeNodeFound);
      }
    },
    testQueryResultsPrimaryIndex() {
      let query = "FOR d IN " + primaryIndexCollectionName + " FILTER d._key IN ['c0', 'c1'] SORT NOOPT(d._key) DESC LIMIT 1 RETURN d";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let result = AQL_EXECUTE(query);
      assertEqual(1, result.json.length);
      assertEqual(result.json[0]._key, 'c1');
    },
    testQueryResultsEdgeIndex() {
      let query = "FOR d IN " + edgeIndexCollectionName + " FILTER d._from == 'testVertices/c0' SORT NOOPT(d._from) DESC LIMIT 10 RETURN d";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let result = AQL_EXECUTE(query);
      assertEqual(2, result.json.length);
      let expectedKeys = new Set(['c0', 'c1']);
      result.json.forEach(function(doc) {
        assertTrue(expectedKeys.has(doc._key));
        expectedKeys.delete(doc._key);
      });
      assertEqual(0, expectedKeys.size);
    },
    testQueryResultsPrefixIndex() {
      let query = "FOR d IN " + prefixIndexCollectionName + " FILTER d.obj.a == {sa: 'a_val_0'} SORT d.obj.b.sb LIMIT 10 RETURN d";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let result = AQL_EXECUTE(query);
      assertEqual(1, result.json.length);
      assertEqual(1, result.json.length);
      assertEqual(result.json[0]._key, 'c0');
    },
    testQueryResultsPrefixIndexWithVariable() {
      let query = "FOR d IN " + prefixIndexCollectionName + " FILTER d.obj.a == {sa: 'a_val_0'} " +
                  "LET c = CONCAT(d.obj.a.sa, d.obj.b.sb) " +
                  "SORT c LIMIT 10 RETURN d";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      let result = AQL_EXECUTE(query);
      assertEqual(1, result.json.length);
      assertEqual(result.json[0]._key, 'c0');
    },
    testQueryResultsEarlyPruning() {
      for (i = 0; i < numOfCollectionIndexes; ++i) {
        let query = "FOR d IN " + collectionNames[i] + " FILTER d.obj.a == 'a_val' AND d.obj.c IN ['c_val', 'c_val_2'] SORT d.obj.c LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        assertNotEqual(-1, plan.rules.indexOf(earlyPruningRuleName));
        let result = AQL_EXECUTE(query);
        assertEqual(2, result.json.length);
        let expected = new Set(['c0', 'c2']);
        result.json.forEach(function(doc) {
          assertTrue(expected.has(doc._key));
          expected.delete(doc._key);
        });
        assertEqual(0, expected.size);
      }
    },
    testQueryResultsPrefixEarlyPruning() {
      let query = "FOR d IN " + prefixIndexCollectionName + " FILTER d.obj.b == {sb: 'b_val_0'} SORT d.obj.b LIMIT 10 RETURN d";
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));
      assertNotEqual(-1, plan.rules.indexOf(earlyPruningRuleName));
      let result = AQL_EXECUTE(query);
      assertEqual(1, result.json.length);
      assertEqual(result.json[0]._key, 'c0');
    }
  };
}

jsunity.run(lateDocumentMaterializationRuleTestSuite);

return jsunity.done();
