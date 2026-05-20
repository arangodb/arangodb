/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;

function DuplicateConditionOptimizationSuite() {
  const cn = "UnitTestsDuplicateDetection";

  function query(q) {
    return db._query(q).toArray();
  }

  function sorted(arr) {
    return arr.slice().sort((a, b) => a - b);
  }

  return {
    setUpAll: function () {
      db._drop(cn);
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 10; i++) {
        docs.push({ value: i, name: "test" + i });
      }
      c.insert(docs);
    },

    tearDownAll: function () {
      db._drop(cn);
    },

    // --- x IN [a] → x == a --------------------------------------------------

    // Single-element IN is rewritten to ==; check both result and plan.
    testInSingleElementRewrittenToEqual: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value IN [5] RETURN doc.value`;
      assertEqual([5], query(q));
      const planStr = JSON.stringify(db._createStatement(q).explain().plan);
      assertFalse(planStr.includes('"compare in"'), 'IN should be rewritten away');
      assertTrue(planStr.includes('"compare =="'), 'should use == after rewrite');
    },

    // [a, a] deduplicates to [a], then x IN [a] → x == a.
    testInDuplicateElementsDeduplicateThenRewrite: function () {
      const res = query(`FOR doc IN ${cn} FILTER doc.value IN [5, 5] RETURN doc.value`);
      assertEqual([5], res);
    },

    // Two-element IN must NOT be rewritten.
    testInTwoElementsNotRewritten: function () {
      const res = sorted(query(`FOR doc IN ${cn} FILTER doc.value IN [5, 6] RETURN doc.value`));
      assertEqual([5, 6], res);
    },

    testSingleElementInRewrittenForLetBoundVar: function () {
      const q = `LET v = NOOPT(5) FOR doc IN ${cn} FILTER doc.value IN [v] RETURN doc.value`;
      assertEqual([5], query(q));
      const planStr = JSON.stringify(db._createStatement(q).explain().plan);
      assertFalse(planStr.includes('"compare in"'), 'single-element IN should be rewritten');
      assertTrue(planStr.includes('"compare =="'), 'should use == after rewrite');
    },

    // IN with a non-constant array must NOT be rewritten to ==, even if it has
    // one element at runtime. isConstant() guards the rewrite.
    testInNonConstantArrayNotRewritten: function () {
      const q = `LET arr = NOOPT([5]) FOR doc IN ${cn} FILTER doc.value IN arr RETURN doc.value`;
      assertEqual([5], query(q));
      const planStr = JSON.stringify(db._createStatement(q).explain().plan);
      assertTrue(planStr.includes('"compare in"'), 'non-constant array IN must not be rewritten');
    },

    // IN [] is always false; no rewrite to ==, branch is dropped as false.
    testInEmptyArrayProducesNoResults: function () {
      const res = query(`FOR doc IN ${cn} FILTER doc.value IN [] RETURN doc.value`);
      assertEqual([], res);
    },

    // --- false AND branch removal -------------------------------------------

    // doc.value IN [] is always false, so the whole AND branch is dropped.
    testFalseAndBranchDropped: function () {
      const res = query(`
        FOR doc IN ${cn}
          FILTER (doc.value > 8 AND doc.value IN []) OR doc.value == 0
          RETURN doc.value`);
      assertEqual([0], res);
    },

    // All OR branches are false → no results.
    testAllBranchesFalse: function () {
      const res = query(`FOR doc IN ${cn} FILTER doc.value IN [] OR doc.value IN [] RETURN doc.value`);
      assertEqual([], res);
    },

    // --- true condition removal ---------------------------------------------

    // doc.value NOT IN [] is always true and is stripped from the AND,
    // leaving only doc.value > 7.
    testTrueConditionStripped: function () {
      const res = sorted(query(`
        FOR doc IN ${cn}
          FILTER doc.value > 7 AND doc.value NOT IN []
          RETURN doc.value`));
      assertEqual([8, 9], res);
    },

    // When all conditions are true, at least one is kept (not dropped entirely).
    testAllTrueConditionsKeepOne: function () {
      const res = query(`FOR doc IN ${cn} FILTER doc.value NOT IN [] AND doc.value NOT IN [] RETURN doc.value`);
      assertEqual(10, res.length);
    },

    // --- duplicate AND condition removal ------------------------------------

    // Exact duplicate is removed; result is unchanged.
    testDuplicateAndConditionRemoved: function () {
      const res = sorted(query(`
        FOR doc IN ${cn}
          FILTER doc.value > 7 AND doc.value > 7
          RETURN doc.value`));
      assertEqual([8, 9], res);
    },

    // Multiple copies all collapse to one.
    testManyDuplicatesAndConditionRemoved: function () {
      const res = sorted(query(`
        FOR doc IN ${cn}
          FILTER doc.value > 7 AND doc.value > 7 AND doc.value > 7
          RETURN doc.value`));
      assertEqual([8, 9], res);
    },

    // doc.value == 5 and 5 == doc.value are treated as the same condition.
    testCommutativeEqualityDuplicate: function () {
      const res = query(`FOR doc IN ${cn} FILTER doc.value == 5 AND 5 == doc.value RETURN doc.value`);
      assertEqual([5], res);
    },

    // Non-deterministic conditions are skipped by deduplication.
    testNonDeterministicNotDeduplicated: function () {
      // RAND() >= 0 is non-deterministic; dedup must not fire so both calls
      // remain in the plan.
      const q = `FOR doc IN ${cn} FILTER RAND() >= 0 AND RAND() >= 0 RETURN doc.value`;
      assertEqual(10, query(q).length);
      const planStr = JSON.stringify(db._createStatement(q).explain().plan);
      // both RAND() calls must survive
      const randCount = (planStr.match(/"rand"/gi) || []).length;
      assertTrue(randCount >= 2, 'both RAND() calls must remain in the plan after dedup is skipped');
    },

    // Duplicate with both sides non-constant (two attribute accesses).
    testDuplicateAndConditionRemovedNonConstantOperands: function () {
      const res = sorted(query(`
        FOR doc IN ${cn}
          FILTER doc.value > doc.name AND doc.value > doc.name
          RETURN doc.value`));
      const expected = sorted(query(`FOR doc IN ${cn} FILTER doc.value > doc.name RETURN doc.value`));
      assertEqual(expected, res);
    },

    // --- duplicate OR branch removal ----------------------------------------

    testDuplicateOrBranchRemoved: function () {
      const res = sorted(query(`
        FOR doc IN ${cn}
          FILTER doc.value > 7 OR doc.value > 7
          RETURN doc.value`));
      assertEqual([8, 9], res);
    },

    // Mixed: one duplicate OR branch removed, distinct branch kept.
    testOrWithOneDuplicateAndOneDistinct: function () {
      const res = sorted(query(`
        FOR doc IN ${cn}
          FILTER doc.value > 7 OR doc.value == 0 OR doc.value > 7
          RETURN doc.value`));
      assertEqual([0, 8, 9], res);
    },

    // Non-deterministic OR branch must not be deduplicated; the deterministic
    // duplicate on either side of it must still be removed correctly.
    testDuplicateOrWithNonDeterministicMiddle: function () {
      const res = sorted(query(`
        FOR doc IN ${cn}
          FILTER doc.value > 7 OR RAND() >= 0 OR doc.value > 7
          RETURN doc.value`));
      // RAND() >= 0 always true → all docs match regardless.
      assertEqual(10, res.length);
    },

    // --- subquery guard ------------------------------------------------------

    // sub is a LET-bound variable; LENGTH(sub) contains a REFERENCE node, not
    // a SUBQUERY node, so deduplication fires normally and gives the right result.
    testLetBoundSubqueryRefIsDeduplicated: function () {
      const res = query(`
        FOR doc IN ${cn}
          LET sub = (FOR x IN ${cn} FILTER x.value > 5 RETURN x)
          FILTER LENGTH(sub) > 0 AND LENGTH(sub) > 0
          RETURN doc.value`);
      assertEqual(10, res.length);
    },

    // Inline subqueries produce NODE_TYPE_SUBQUERY nodes in the AST.
    // The containsSubquery guard fires and deduplication is skipped; verify no crash.
    testInlineSubqueryConditionNotDeduplicated: function () {
      const res = query(`
        FOR doc IN ${cn}
          FILTER LENGTH((FOR x IN ${cn} FILTER x.value > 5 RETURN x)) > 0
             AND LENGTH((FOR x IN ${cn} FILTER x.value > 5 RETURN x)) > 0
          RETURN doc.value`);
      assertEqual(10, res.length);
    },

    // --- array comparison operators -----------------------------------------

    testDuplicateArrayEqConditionRemoved: function () {
      // ARRAY_EQ (ANY ==) duplicate should be deduplicated just like plain ==
      const res = query(`
        FOR doc IN ${cn}
          FILTER doc.tags[* RETURN CURRENT] ANY == 1
             AND doc.tags[* RETURN CURRENT] ANY == 1
          RETURN doc.value`);
      assertEqual([], res);  // no docs have a tags array with value 1
    },

    // --- empty / corner cases -----------------------------------------------

    testEmptyResultPreservedAfterDedup: function () {
      const res = query(`
        FOR doc IN ${cn}
          FILTER doc.value > 100 AND doc.value > 100
          RETURN doc.value`);
      assertEqual([], res);
    },

    testAllDocumentsMatchAfterDedup: function () {
      const res = query(`FOR doc IN ${cn} FILTER doc.value >= 0 AND doc.value >= 0 RETURN doc.value`);
      assertEqual(10, res.length);
    },
  };
}

jsunity.run(DuplicateConditionOptimizationSuite);
return jsunity.done();
