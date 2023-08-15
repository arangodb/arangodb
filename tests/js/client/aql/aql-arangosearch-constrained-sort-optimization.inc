/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for constrained sort arangosearch rule
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
(function() {
  var db = require("@arangodb").db;
  const isCluster = require("internal").isCluster();
  const {AQL_EXPLAIN, AQL_EXECUTE} = require('@arangodb/test-helper.js');
  return function (isSearchAlias, isCluster, newDbName, dbArgs) {
    const ruleName = "arangosearch-constrained-sort";
    const cn = "UnitTestsCollection";
    const cn1 = "UnitTestsCollection1";
    const vn = "UnitTestsView";
    const longValue = "VeryLongValueToNotFitIntoSSOAndTriggerBufferCopyingIssuesIfAnyBlaBlaBla";
    return {
      setUpAll: function () {
        db._useDatabase("_system");
        try {
          db._dropDatabase(newDbName);
        } catch (err) {
        }
        db._createDatabase(newDbName, dbArgs);
        db._useDatabase(newDbName);
        let numberOfShards = 3;
        if (dbArgs.sharding === "single") {
          numberOfShards = 1;
        }
        let c = db._create(cn, {numberOfShards});
        let c1 = db._create(cn1, {numberOfShards});
        if (isSearchAlias) {
          let props = {
            type: "inverted", includeAllFields: true,
            consolidationIntervalMsec: 5000,
            primarySort: {
              fields: [{"field": "obj.a.a1", "direction": "asc"}, {"field": "obj.ps", "direction": "desc"}],
              compression: "none"
            },
            storedValues: [["obj.a.a1"], ["compound"],
              {fields: ["obj.c"], compression: "none"},
              ["storedVal"],
              {fields: ["obj.d.d1", "obj.e.e1"], compression: "lz4"},
              ["obj.f", "obj.g", "obj.h"]],
            fields: [{name: "strval", analyzer: "text_en"}]
          };
          let i = c.ensureIndex(props);
          let i1 = c1.ensureIndex(props);
          db._createView(vn, "search-alias", {
            indexes: [
              {collection: cn, index: i.name},
              {collection: cn1, index: i1.name},
            ]
          });
        } else {
          db._createView(vn, "arangosearch", {
            consolidationIntervalMsec: 5000,
            primarySort: [{"field": "obj.a.a1", "direction": "asc"}, {"field": "obj.ps", "direction": "desc"}],
            primarySortCompression: "none",
            storedValues: [["obj.a.a1"], ["compound"],
              {fields: ["obj.c"], compression: "none"},
              ["storedVal"],
              {fields: ["obj.d.d1", "obj.e.e1"], compression: "lz4"},
              ["obj.f", "obj.g", "obj.h"]],
            links: {
              [cn]: {includeAllFields: true, fields: {strval: {analyzers: ['text_en']}}},
              [cn1]: {includeAllFields: true, fields: {strval: {analyzers: ['text_en']}}}
            }
          });
        }

        c.save({_key: 'c0', obj: {a: {a1: 0}, b: {b1: 1}, c: 2, d: {d1: 3}, e: {e1: 4}, f: 5, g: 6, h: 7, j: 8}});
        c1.save({
          _key: 'c_0',
          obj: {a: {a1: 10}, b: {b1: 11}, c: 12, d: {d1: 13}, e: {e1: 14}, f: 15, g: 16, h: 17, j: 18}
        });
        c.save({
          _key: 'c1',
          obj: {a: {a1: 20}, b: {b1: 21}, c: 22, d: {d1: 23}, e: {e1: 24}, f: 25, g: 26, h: 27, j: 28}
        });
        c1.save({
          _key: 'c_1',
          obj: {a: {a1: 30}, b: {b1: 31}, c: 32, d: {d1: 33}, e: {e1: 34}, f: 35, g: 36, h: 37, j: 38}
        });

        // some data for multi-batch tests
        let data = [];
        let data2 = [];
        let cats = "cat";
        let j = 0;
        let sort = 0;
        for (let i = 0; i < 5000; ++i) {
          data.push({field1:i, field2: "A" + i % 1000, field3: "B" + i % 100, strval: cats, idx:i, storedVal: longValue + i,
          compound: {val:sort},
          obj: {ps:sort, a: {a1: 30}, b: {b1: 31}, c: i, d: {d1: 33}, e: {e1: 34}, f: j, g: 36, h: 37, j: 38}});
          data2.push({field1:i/2, field2: "A" + i % 2000, field3: "B" + i % 300, strval: cats, idx:i, storedVal: longValue + i,
          compound: {val:sort + 1},
          obj: {ps:sort + 1, a: {a1: 30}, b: {b1: 31}, c: i, d: {d1: 33}, e: {e1: 34}, f: j + 1, g: 36, h: 37, j: 38}});
          cats = cats + " cat";
          sort = sort + 2;
        }
        c.save(data);
        c1.save(data2);
        // trigger view sync
        db._query("FOR d IN " + vn + " OPTIONS { waitForSync: true } RETURN d");
      },
      tearDownAll : function () {
        db._useDatabase("_system");
        db._dropDatabase(newDbName);
      },
      testNotAppliedDueToNotSorted() {
        let query = "FOR d IN " + vn + " LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToSortWrapped() {
        let query = "FOR d IN " + vn + " SORT ROUND(BM25(d)) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToNonDetermCalc() {
        let query = "FOR d IN " + vn + " SEARCH d.a.a1 == 0  LET a = NOOPT(d.b.b1) SORT BM25(d) LIMIT 10 RETURN {dD:d, aA:a}";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToNonDetermCalc2() {
        let query = "FOR d IN " + vn + " SEARCH d.a.a1 == 0  SORT BM25(d) LET a = NOOPT(d.b.b1) LIMIT 10 RETURN {dD:d, aA:a}";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToNoLimit() {
        let query = "FOR d IN " + vn + " SEARCH d.a.a1 == 0 SORT BM25(d) RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToLimitBefore() {
        let query = "FOR d IN " + vn + " SEARCH d.a.a1 == 0 LIMIT 10 SORT BM25(d) RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToInLoop() {
        let query = "FOR a IN 1..10 FOR d IN " + vn + " SEARCH d.a.a1 == a SORT BM25(d) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToNonScoresSort() {
        let query = "FOR d IN " + vn + " SEARCH d.a.a1 == 1 SORT BM25(d), d.a.a1 ASC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToNonScoresSort2() {
        let query = "FOR d IN " + vn + " SEARCH d.a.a1 == 1 SORT d.a.a1 ASC, BM25(d) LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToUpdateDoc() {
        let query = "FOR d IN " + vn + " SEARCH d.field1 IN [1, 2, 11, 12] UPDATE d IN " + cn + " SORT BM25(d) DESC LIMIT 10 RETURN d";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToSubqueryWithDocumentAccess() {
        let query = "FOR d IN " + vn + " SEARCH d.field1 IN [1, 2, 11, 12] " +
                    "LET a = NOOPT(d.foo) " +
                    "LET e = SUM(FOR c IN " + vn + " LET p = CONCAT(d.field1, c.foo) RETURN p) " +
                    "SORT BM25(d) LIMIT 10 RETURN {dd:d, ee:e, aa:a}";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testNotAppliedDueToSubqueryWithReturnDocument() {
        let query = "FOR d IN " + vn + " SEARCH d.field1 IN [1, 2, 11, 12] " +
                    "LET a = NOOPT(d.foo) " +
                    "LET e = SUM(FOR c IN " + vn + " LET p = NOOPT(CONCAT(d.foo, c.foo)) RETURN d) " +
                    "SORT BM25(d) LIMIT 10 RETURN {dd:d, ee:e, aa:a}";
        let plan = AQL_EXPLAIN(query).plan;
        assertEqual(-1, plan.rules.indexOf(ruleName));
      },
      testApplyWithFullCount() {
          let query = "FOR d IN " + vn + " SEARCH d.field2 >= 'A3' SORT BM25(d) " +
                      " DESC LIMIT 1000, 300 COLLECT WITH COUNT INTO c  RETURN c ";
          let plan = AQL_EXPLAIN(query).plan;
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query, {}, {fullCount:true});
          assertEqual(1, result.json.length);
          assertEqual(300, result.json[0]);
          assertEqual(6216, result.stats.fullCount);
          assertEqual(6216, result.stats.scannedIndex);
      },
      testApplyWithNoFullCount() {
          let query = "FOR d IN " + vn + " SEARCH d.field2 >= 'A3' SORT BM25(d) " +
                      " DESC LIMIT 1000, 300 COLLECT WITH COUNT INTO c  RETURN c ";
          let plan = AQL_EXPLAIN(query).plan;
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query, {}, {fullCount:false});
          assertEqual(1, result.json.length);
          assertEqual(300, result.json[0]);
          assertEqual(6216, result.stats.scannedIndex);
      },
      testApplyWithFullCountDocs() {
          let query = "FOR d IN " + vn + " SEARCH d.field2 >= 'A3' SORT BM25(d) " +
                      " DESC LIMIT 1000, 300 RETURN d ";
          let plan = AQL_EXPLAIN(query).plan;
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query, {}, {fullCount:true});
          assertEqual(300, result.json.length);
          assertEqual(6216, result.stats.fullCount);
          assertEqual(6216, result.stats.scannedIndex);
      },
      testApplyWithNoFullCountDocs() {
          let query = "FOR d IN " + vn + " SEARCH d.field2 >= 'A3' SORT BM25(d) " +
                      " DESC LIMIT 1000, 300 RETURN d";
          let plan = AQL_EXPLAIN(query).plan;
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query, {}, {fullCount:false});
          assertEqual(300, result.json.length);
          assertEqual(6216, result.stats.scannedIndex);
      },
      testApplyWithFullCountDocsNoOffset() {
          let query = "FOR d IN " + vn + " SEARCH d.field2 >= 'A3' SORT BM25(d) " +
                      " DESC LIMIT 1500 RETURN d ";
          let plan = AQL_EXPLAIN(query).plan;
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query, {}, {fullCount:true});
          assertEqual(1500, result.json.length);
          assertEqual(6216, result.stats.fullCount);
          assertEqual(6216, result.stats.scannedIndex);
      },
      testApplyWithNoFullCountDocsNoOffset() {
          let query = "FOR d IN " + vn + " SEARCH d.field2 >= 'A3' SORT BM25(d) " +
                      " DESC LIMIT 1500 RETURN d";
          let plan = AQL_EXPLAIN(query).plan;
          assertNotEqual(-1, plan.rules.indexOf(ruleName));
          let result = AQL_EXECUTE(query, {}, {fullCount:false});
          assertEqual(1500, result.json.length);
          assertEqual(6216, result.stats.scannedIndex);
      },
      testAppliedWithNonDetermAfter() {
        let query = "FOR d IN " + vn + " SEARCH d.field2 >= 'A3'  SORT BM25(d) LIMIT 10 LET a = NOOPT(d.b.b1) RETURN {dD:d, aA:a}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query, {}, {fullCount:true});
        assertEqual(10, result.json.length);
        assertEqual(6216, result.stats.scannedIndex);
        assertEqual(6216, result.stats.fullCount);
      },
      testAppliedDeterministicUsedBetweenSort() {
        let query = "FOR d IN " + vn + " SEARCH d.field2 < 'A3'  SORT BM25(d) LET a = UPPER(d.b.b1) LIMIT 10 RETURN {dD:d, aA:a}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName), {}, {optimizer:{rules:['-move-calculations-down']}});
        let result = AQL_EXECUTE(query, {}, {fullCount:true, optimizer:{rules:['-move-calculations-down']}});
        assertEqual(10, result.json.length);
        assertEqual(3784, result.stats.scannedIndex);
        assertEqual(3784, result.stats.fullCount);
      },
      testAppliedDeterministicUsedBeforeSort() {
        let query = "FOR d IN " + vn + " SEARCH d.field2 < 'A3' LET a = UPPER(d.b.b1) SORT BM25(d)  LIMIT 10 RETURN {dD:d, aA:a}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName), {}, {optimizer:{rules:['-move-calculations-down']}});
        let result = AQL_EXECUTE(query, {}, {fullCount:true, optimizer:{rules:['-move-calculations-down']}});
        assertEqual(10, result.json.length);
        assertEqual(3784, result.stats.scannedIndex);
        assertEqual(3784, result.stats.fullCount);
      },
      testAppliedDeterministicUsedBeforeSortTwoSorts() {
        let query = "FOR d IN " + vn + " SEARCH d.field2 < 'A3' LET a = UPPER(d.b.b1) SORT BM25(d) ASC "
                    + "LIMIT 10 SORT TFIDF(d) DESC RETURN {dD:d, aA:a}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName), {}, {optimizer:{rules:['-move-calculations-down']}});
        let result = AQL_EXECUTE(query, {}, {fullCount:true, optimizer:{rules:['-move-calculations-down']}});
        assertEqual(10, result.json.length);
        assertEqual(3784, result.stats.scannedIndex);
        assertEqual(3784, result.stats.fullCount);
      },
      testAppliedMultiSorts() {
        let query = "FOR d IN " + vn + " SEARCH d.field2 < 'A3' LET a = TFIDF(d) LET b = BM25(d) LET t = a + b SORT b ASC, a DESC "
                    + "LIMIT 10 RETURN {dD:d, aA:t}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName), {}, {optimizer:{rules:['-move-calculations-down']}});
        let result = AQL_EXECUTE(query, {}, {fullCount:true, optimizer:{rules:['-move-calculations-down']}});
        assertEqual(10, result.json.length);
        assertEqual(3784, result.stats.scannedIndex);
        assertEqual(3784, result.stats.fullCount);
      },
      testAppliedSort() {
        let query = "FOR d IN " + vn + " SEARCH ANALYZER(d.strval == 'cat', 'text_en') SORT TFIDF(d) DESC LIMIT 10, 10 "
                    + " RETURN {idx:d.idx, score:TFIDF(d)}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(10, result.json.length);
        let expectedIdx = 4994;
        for (let i = 0; i < 10; ++i) {
          assertEqual(result.json[i].idx, expectedIdx);
          if (i % 2 === 1) {
            expectedIdx--;
          }
        }
      },
      testAppliedSortAsc() {
        let query = "FOR d IN " + vn + " SEARCH ANALYZER(d.strval == 'cat', 'text_en') SORT TFIDF(d) ASC LIMIT 1002, 10 "
                    + " RETURN {idx:d.idx, score:TFIDF(d)}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(10, result.json.length);
        let expectedIdx = 501;
        for (let i = 0; i < 10; ++i) {
          assertEqual(result.json[i].idx, expectedIdx);
          if (i % 2 === 1) {
            expectedIdx++;
          }
        }
      },
      testAppliedSortAscWithStoredValues() {
        let query = "FOR d IN " + vn + " SEARCH ANALYZER(d.strval == 'cat', 'text_en') SORT TFIDF(d) ASC LIMIT 1002, 10 "
                    + " RETURN {dv:d.storedVal, score:TFIDF(d)}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(10, result.json.length);
        let expectedIdx = 501;
        for (let i = 0; i < 10; ++i) {
          assertEqual(result.json[i].dv, longValue + expectedIdx);
          if (i % 2 === 1) {
            expectedIdx++;
          }
        }
      },
      testAppliedStoredValuesOnly() {
        let query = "FOR d IN " + vn + " SEARCH ANALYZER(d.strval == 'cat', 'text_en') SORT d.storedVal ASC LIMIT 10 "
                    + " RETURN {dv:d.storedVal, score:TFIDF(d)}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(10, result.json.length);
        let expectedIdx = 0;
        for (let i = 0; i < 10; ++i) {
          assertEqual(result.json[i].dv, longValue + expectedIdx);
          if (i % 2 === 1) {
            if (expectedIdx === 0) {
                expectedIdx = 1;
            } else {
                expectedIdx = expectedIdx * 10;
            }
          }
        }
      },
      testAppliedStoredValuesSplitted() {
        let query = "FOR d IN " + vn + " SEARCH ANALYZER(d.strval == 'cat', 'text_en') SORT d.storedVal ASC LIMIT 10 "
                    + " RETURN {dv:d.obj.c, score:TFIDF(d)}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(10, result.json.length);
        let expectedIdx = 0;
        for (let i = 0; i < 10; ++i) {
          assertEqual(result.json[i].dv, expectedIdx);
          if (i % 2 === 1) {
            if (expectedIdx === 0) {
                expectedIdx = 1;
            } else {
                expectedIdx = expectedIdx * 10;
            }
          }
        }
      },
      testAppliedStoredValuesScoreAndStoredDesc() {
        let query = "FOR d IN " + vn + " SEARCH ANALYZER(d.strval == 'cat', 'text_en') SORT TFIDF(d) ASC, d.obj.f DESC LIMIT 10 "
                    + " RETURN {dv:d.obj.f, score:TFIDF(d)}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(10, result.json.length);
        let expectedIdx = 1;
        let score = 0;
        for (let i = 0; i < 10; ++i) {
          assertTrue(score <= result.json[i].score);
          score = result.json[i].score;
          assertEqual(result.json[i].dv, expectedIdx);
          if (expectedIdx === 0) {
            expectedIdx = 1;
          } else {
            expectedIdx = 0;
          }
        }
      },
      testAppliedStoredValuesScoreAndStoredAsc() {
        let query = "FOR d IN " + vn + " SEARCH ANALYZER(d.strval == 'cat', 'text_en') SORT TFIDF(d) ASC, d.obj.f ASC LIMIT 10 "
                    + " RETURN {dv:d.obj.f, score:TFIDF(d)}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(10, result.json.length);
        let expectedIdx = 0;
        let score = 0;
        for (let i = 0; i < 10; ++i) {
          assertTrue(score <= result.json[i].score);
          score = result.json[i].score;
          assertEqual(result.json[i].dv, expectedIdx);
          if (expectedIdx === 0) {
            expectedIdx = 1;
          } else {
            expectedIdx = 0;
          }
        }
      },
      testAppliedUsePrimarySort() {
        let query = "FOR d IN " + vn + " SEARCH ANALYZER(d.strval == 'cat', 'text_en') SORT d.obj.ps ASC LIMIT 10, 10 "
                    + " RETURN {dv:d.obj.ps}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(10, result.json.length);
        let expectedIdx = 10;
        for (let i = 0; i < 10; ++i) {
          assertEqual(result.json[i].dv, expectedIdx);
          expectedIdx = expectedIdx + 1;
        }
      },
      testAppliedWithPostfix() {
        let query = "FOR d IN " + vn + " SEARCH ANALYZER(d.strval == 'cat', 'text_en') SORT d.compound.val DESC LIMIT 10, 10 "
                    + " RETURN {dv:d.compound.val, stored: d.storedVal}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(10, result.json.length);
        let expectedIdx = 9989;
        for (let i = 0; i < 10; ++i) {
          assertEqual(result.json[i].dv, expectedIdx);
          expectedIdx = expectedIdx - 1;
        }
      },
      testAppliedWithPostfixNoReturnValue() {
        let query = "FOR d IN " + vn + " SEARCH ANALYZER(d.strval == 'cat', 'text_en') SORT d.compound.val DESC LIMIT 10, 10 "
                    + " RETURN {stored: d.obj.ps}";
        let plan = AQL_EXPLAIN(query).plan;
        assertNotEqual(-1, plan.rules.indexOf(ruleName));
        let result = AQL_EXECUTE(query);
        assertEqual(10, result.json.length);
        let expectedIdx = 9989;
        for (let i = 0; i < 10; ++i) {
          assertEqual(result.json[i].stored, expectedIdx);
          expectedIdx = expectedIdx - 1;
        }
      }
    };
  };
}());
