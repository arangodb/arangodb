/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse */

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
        docs.push({
          value: i,
          name: "test" + i
        });
      }
      c.insert(docs);
      c.ensureIndex({
        type: "persistent",
        fields: ["value"]
      });
    },

    tearDownAll: function () {
      db._drop(cn);
    },

    // --- x IN [a] → x == a --------------------------------------------------

    // Single-element IN is rewritten to ==; check both result and plan.
    testInSingleElementRewrittenToEqual: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value IN [5] RETURN doc.value`;
      assertEqual([5], query(q));
      const plan = db._createStatement(q).explain().plan;
      const planStr = JSON.stringify(plan);
      assertFalse(planStr.includes('"compare in"'), 'IN should be rewritten away');
      assertTrue(planStr.includes('"compare =="'), 'should use == after rewrite');
      assertTrue(plan.rules.includes('use-indexes'), 'use-indexes must have fired');
    },

    // [a, a] deduplicates to [a], then x IN [a] → x == a.
    testInDuplicateElementsDeduplicateThenRewrite: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value IN [5, 5] RETURN doc.value`;
      assertEqual([5], query(q));
      const plan = db._createStatement(q).explain().plan;
      assertTrue(plan.rules.includes('use-indexes'), 'use-indexes must have fired');
      assertEqual(0, plan.nodes.filter(n => n.type === 'FilterNode').length, 'condition should be absorbed into index scan');
    },

    // Two-element IN must NOT be rewritten.
    testInTwoElementsNotRewritten: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value IN [5, 6] RETURN doc.value`;
      assertEqual([5, 6], sorted(query(q)));
      const planStr = JSON.stringify(db._createStatement(q).explain().plan);
      assertTrue(planStr.includes('"compare in"'), 'two-element IN must not be rewritten');
    },

    testSingleElementInRewrittenForLetBoundVar: function () {
      // Use an unindexed field so use-indexes cannot absorb the condition —
      // the FilterNode survives and we can verify the IN→EQ rewrite directly.
      const q = `LET v = "test5" FOR doc IN ${cn} FILTER doc.name IN [v] RETURN doc.name`;
      assertEqual(["test5"], query(q));
      const plan = db._createStatement(q).explain().plan;
      const planStr = JSON.stringify(plan);
      assertFalse(planStr.includes('"compare in"'), 'single-element IN should be rewritten to ==');
      assertTrue(planStr.includes('"compare =="'), 'should use == after rewrite');
    },

    // TODO: x IN [a] must not rewrite to == for multivalued ArangoSearch fields.
    // Requires view setup; covered in the ArangoSearch-specific test suite.

    // IN with a non-constant array must NOT be rewritten to ==, even if it has
    // one element at runtime. isConstant() guards the rewrite.
    testInNonConstantArrayNotRewritten: function () {
      const q = `LET arr = NOOPT([5]) FOR doc IN ${cn} FILTER doc.value IN arr RETURN doc.value`;
      assertEqual([5], query(q));
      const planStr = JSON.stringify(db._createStatement(q).explain().plan);
      assertTrue(planStr.includes('"compare in"'), 'non-constant array IN must not be rewritten');
      assertFalse(planStr.includes('"compare =="'), 'non-constant array IN must not be rewritten to ==');
    },

    // IN [] is always false; no rewrite to ==, branch is dropped as false.
    testInEmptyArrayProducesNoResults: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value IN [] RETURN doc.value`;
      assertEqual([], query(q));
      const nodeTypes = db._createStatement(q).explain().plan.nodes.map(n => n.type);
      assertNotEqual(-1, nodeTypes.indexOf('NoResultsNode'), 'IN [] must produce NoResultsNode');
    },

    // --- false AND branch removal -------------------------------------------

    // doc.value IN [] is always false, so the whole AND branch is dropped.
    testFalseAndBranchDropped: function () {
      const q = `
        FOR doc IN ${cn}
          FILTER (doc.value > 8 AND doc.value IN []) OR doc.value == 0
          RETURN doc.value`;
      assertEqual([0], query(q));
      const plan = db._createStatement(q).explain().plan;
      const planStr = JSON.stringify(plan);
      assertFalse(planStr.includes('"compare in"'), 'false AND branch must be removed from plan');
      assertTrue(plan.rules.includes('use-indexes'), 'use-indexes must have fired');
    },

    // All OR branches are false → no results.
    testAllBranchesFalse: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value IN [] OR doc.value IN [] RETURN doc.value`;
      assertEqual([], query(q));
      const nodeTypes = db._createStatement(q).explain().plan.nodes.map(n => n.type);
      assertNotEqual(-1, nodeTypes.indexOf('NoResultsNode'), 'all-false OR must produce NoResultsNode');
    },

    // --- true condition removal ---------------------------------------------

    testTrueConditionStripped: function () {
      const q = `
        FOR doc IN ${cn}
          FILTER doc.value > 7 AND doc.value NOT IN []
          RETURN doc.value`;
      assertEqual([8, 9], sorted(query(q)));
      const plan = db._createStatement(q).explain().plan;
      const planStr = JSON.stringify(plan);
      assertFalse(planStr.includes('"compare not in"'), 'always-true NOT IN [] must be removed from plan');
      assertTrue(plan.rules.includes('use-indexes'), 'use-indexes must have fired');
    },

    // When all conditions are true, at least one is kept (not dropped entirely).
    testAllTrueConditionsKeepOne: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value NOT IN [] AND doc.value NOT IN [] RETURN doc.value`;
      assertEqual(10, query(q).length);
    },

    // --- duplicate AND condition removal ------------------------------------

    // Exact duplicate is removed; result is unchanged.
    testDuplicateAndConditionRemoved: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value > 7 AND doc.value > 7 RETURN doc.value`;
      assertEqual([8, 9], sorted(query(q)));
      const plan = db._createStatement(q).explain().plan;
      assertTrue(plan.rules.includes('use-indexes'), 'use-indexes must have fired');
      assertEqual(0, plan.nodes.filter(n => n.type === 'FilterNode').length, 'condition should be absorbed into index scan');
    },

    // Multiple copies all collapse to one.
    testManyDuplicatesAndConditionRemoved: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value > 7 AND doc.value > 7 AND doc.value > 7 RETURN doc.value`;
      assertEqual([8, 9], sorted(query(q)));
      const plan = db._createStatement(q).explain().plan;
      assertTrue(plan.rules.includes('use-indexes'), 'use-indexes must have fired');
      assertEqual(0, plan.nodes.filter(n => n.type === 'FilterNode').length, 'condition should be absorbed into index scan');
    },

    // doc.value == 5 and 5 == doc.value are treated as the same condition.
    testCommutativeEqualityDuplicate: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value == 5 AND 5 == doc.value RETURN doc.value`;
      assertEqual([5], query(q));
      const plan = db._createStatement(q).explain().plan;
      assertTrue(plan.rules.includes('use-indexes'), 'use-indexes must have fired');
      assertEqual(0, plan.nodes.filter(n => n.type === 'FilterNode').length, 'condition should be absorbed into index scan');
    },

    testNonDeterministicNotDeduplicated: function () {
      const q = `FOR doc IN ${cn} FILTER RAND() >= 0 AND RAND() >= 0 RETURN doc.value`;
      assertEqual(10, query(q).length);
      const planStr = JSON.stringify(db._createStatement(q).explain().plan);
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
      const q = `FOR doc IN ${cn} FILTER doc.value > 7 OR doc.value > 7 RETURN doc.value`;
      assertEqual([8, 9], sorted(query(q)));
      const plan = db._createStatement(q).explain().plan;
      assertTrue(plan.rules.includes('use-indexes'), 'use-indexes must have fired');
      assertEqual(0, plan.nodes.filter(n => n.type === 'FilterNode').length, 'condition should be absorbed into index scan');
    },

    // Mixed: one duplicate OR branch removed, distinct branch kept.
    testOrWithOneDuplicateAndOneDistinct: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value > 7 OR doc.value == 0 OR doc.value > 7 RETURN doc.value`;
      assertEqual([0, 8, 9], sorted(query(q)));
      const plan = db._createStatement(q).explain().plan;
      assertTrue(plan.rules.includes('use-indexes'), 'use-indexes must have fired');
    },

    // RAND() is non-deterministic, dedup skips it.
    testDuplicateOrWithNonDeterministicMiddle: function () {
      const q = `FOR doc IN ${cn} FILTER doc.value > 7 OR RAND() >= 0 OR doc.value > 7 RETURN doc.value`;
      assertEqual(10, sorted(query(q)).length);
      const planStr = JSON.stringify(db._createStatement(q).explain().plan);
      const randCount = (planStr.match(/"rand"/gi) || []).length;
      assertTrue(randCount >= 1, 'RAND() branch must survive dedup');
    },

    // --- subquery guard ------------------------------------------------------

    // LET-bound ref produces a REFERENCE node, not SUBQUERY, so dedup fires normally.
    testLetBoundSubqueryRefIsDeduplicated: function () {
      const q = `
        FOR doc IN ${cn}
          LET sub = (FOR x IN ${cn} FILTER x.value > 5 RETURN x)
          FILTER LENGTH(sub) > 0 AND LENGTH(sub) > 0
          RETURN doc.value`;
      assertEqual(10, query(q).length);
      const filterNodes = db._createStatement(q).explain().plan.nodes.filter(n => n.type === 'FilterNode');
      assertEqual(1, filterNodes.length, 'dedup must reduce two identical conditions to one FilterNode');
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
      const tagCn = cn + '_tags';
      db._drop(tagCn);
      const c = db._create(tagCn);
      try {
        c.insert({
          value: 1,
          tags: [1, 2, 3]
        });
        c.insert({
          value: 2,
          tags: [4, 5]
        });
        const res = db._query(`
          FOR doc IN ${tagCn}
            FILTER doc.tags[* RETURN CURRENT] ANY == 1
               AND doc.tags[* RETURN CURRENT] ANY == 1
            RETURN doc.value`).toArray();
        assertEqual(1, res.length, 'dedup must not drop the valid match');
        assertEqual(1, res[0]);
      } finally {
        db._drop(tagCn);
      }
    },

    // --- compareUtf8 fix: == and != use byte comparison, not ICU ---------------

    // == compares bytes; NFC U+00E9 and NFD e+U+0301 are byte-distinct, dedup must not fire
    testEqualConditionsNfcNfdNotDeduplicated: function () {
      // if dedup fires, the AND collapses to NFC-only and returns 1 result instead of 0
      const testCn = cn + '_nfctest';
      db._drop(testCn);
      const c = db._create(testCn);
      try {
        c.insert({ name: "café" }); // NFC U+00E9
        // NFD version: same visual but different bytes (e + U+0301)
        const res = db._query(
          `FOR doc IN ${testCn} FILTER doc.name == "café" AND doc.name == "café" RETURN doc`
        ).toArray();
        assertEqual([], res, 'NFC and NFD must not deduplicate');
      } finally {
        db._drop(testCn);
      }
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
    }
  };
}

jsunity.run(DuplicateConditionOptimizationSuite);
return jsunity.done();
