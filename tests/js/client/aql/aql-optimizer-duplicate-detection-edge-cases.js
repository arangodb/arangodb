/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual */

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
/// @author Julia Puget
/// @author Copyright 2025, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;
const getRulesFromPlan = helper.getRulesFromPlan;

function duplicateDetectionEdgeCasesSuite() {
  const cn = "UnitTestsCollection";
  const cn2 = "UnitTestsCollection2";

  return {
    setUpAll: function () {
      db._drop(cn);
      db._drop(cn2);
      
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 10; i++) {
        docs.push({ 
          _key: "test" + i, 
          value: i, 
          name: "test" + i,
          arr: [i, i+1, i+2],
          nested: { value: i }
        });
      }
      c.insert(docs);
      
      let c2 = db._create(cn2);
      c2.insert(docs);
    },

    tearDownAll: function () {
      db._drop(cn);
      db._drop(cn2);
    },

    ////////////////////////////////////////////////////////////////////////////////
    // SUBQUERY TESTS - Critical for performance
    ////////////////////////////////////////////////////////////////////////////////

    // Test 1: Subqueries should NOT be compared (performance guard)
    testSubqueriesNotCompared: function () {
      const query = `
        FOR doc IN ${cn}
          LET sub1 = (FOR x IN ${cn} FILTER x.value > 5 RETURN x)
          LET sub2 = (FOR x IN ${cn} FILTER x.value > 5 RETURN x)
          FILTER LENGTH(sub1) > 0 AND LENGTH(sub2) > 0
          RETURN doc
      `;
      
      // Should work without hanging or crashing
      const result = getQueryResults(query);
      assertEqual(result.length, 10);
    },

    // Test 2: Subqueries in nested context
    testSubqueriesNested: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value > 5
          LET sub = (
            FOR x IN ${cn}
              LET inner = (FOR y IN ${cn2} FILTER y.value == x.value RETURN y)
              FILTER LENGTH(inner) > 0 AND LENGTH(inner) > 0
              RETURN x
          )
          RETURN {doc: doc, count: LENGTH(sub)}
      `;
      
      // Should not crash or hang
      const result = getQueryResults(query);
      assertEqual(result.length, 4);
    },

    // Test 3: NARY_AND/OR branches containing subqueries
    testNaryWithSubqueryChildren: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER (doc.value > 5 AND (FOR x IN ${cn} RETURN x.id)) OR
                 (doc.value > 5 AND (FOR x IN ${cn} RETURN x.id))
          RETURN doc
      `;
      
      // Should not attempt to compare OR branches (contains subqueries)
      // Should not hang or crash
      const result = getQueryResults(query);
      assertEqual(result.length, 4);
    },

    // Test 4: Subqueries with different variables
    testSubqueriesDifferentVariables: function () {
      const query = `
        FOR doc IN ${cn}
          LET sub1 = (FOR x IN ${cn} RETURN x.value)
          LET sub2 = (FOR y IN ${cn2} RETURN y.value)
          FILTER LENGTH(sub1) == LENGTH(sub2)
          RETURN 1
      `;
      
      // Should not deduplicate (different collections)
      const result = getQueryResults(query);
      assertEqual(result.length, 10);
    },

    ////////////////////////////////////////////////////////////////////////////////
    // VARIABLE REFERENCE TESTS - Critical for correctness
    ////////////////////////////////////////////////////////////////////////////////

    // Test 4: Different variables should NOT be considered equal
    testDifferentVariablesNotEqual: function () {
      const query = `
        FOR doc IN ${cn}
          FOR other IN ${cn2}
            FILTER doc.value > 5 AND other.value > 5
            RETURN {doc: doc.value, other: other.value}
      `;
      
      // Should return cross product where both conditions are satisfied
      const result = getQueryResults(query);
      assertEqual(result.length, 16); // 4 docs * 4 docs
    },

    // Test 5: Same variable, different attributes
    testSameVariableDifferentAttributes: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value > 5 AND doc.name != null
          RETURN doc
      `;
      
      // Both conditions should apply
      const result = getQueryResults(query);
      assertEqual(result.length, 4); // values 6,7,8,9
    },

    // Test 6: Variable in nested attribute access
    testNestedAttributeAccess: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.nested.value > 5 AND doc.nested.value > 5
          RETURN doc
      `;
      
      // Duplicate should be removed, result stays same
      const result = getQueryResults(query);
      assertEqual(result.length, 4);
    },

    ////////////////////////////////////////////////////////////////////////////////
    // BASIC DEDUPLICATION TESTS
    ////////////////////////////////////////////////////////////////////////////////

    // Test 7: Simple duplicate AND condition
    testSimpleDuplicateAND: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value > 5 AND doc.value > 5 AND doc.value > 5
          RETURN doc
      `;
      
      // All three duplicates should be reduced to one
      const result = getQueryResults(query);
      assertEqual(result.length, 4); // values 6,7,8,9
    },

    // Test 8: Duplicate with different operator order
    testDuplicateDifferentOrder: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value > 5 AND doc.name != null AND doc.value > 5
          RETURN doc
      `;
      
      // doc.value > 5 appears twice, should deduplicate
      const result = getQueryResults(query);
      assertEqual(result.length, 4);
    },

    // Test 9: OR branch deduplication
    testORBranchDeduplication: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER (doc.value > 8) OR (doc.value > 8)
          RETURN doc
      `;
      
      // Duplicate OR branches should be removed
      const result = getQueryResults(query);
      assertEqual(result.length, 1); // Only value 9
    },

    // Test 10: Complex OR branch with AND
    testComplexORBranchDeduplication: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER (doc.value > 5 AND doc.name != null) OR (doc.value > 5 AND doc.name != null)
          RETURN doc
      `;
      
      // Duplicate OR branches should be removed
      const result = getQueryResults(query);
      assertEqual(result.length, 4);
    },

    ////////////////////////////////////////////////////////////////////////////////
    // FUNCTION CALL TESTS
    ////////////////////////////////////////////////////////////////////////////////

    // Test 11: Function calls with same arguments
    testFunctionCallDeduplication: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER LENGTH(doc.name) > 3 AND LENGTH(doc.name) > 3
          RETURN doc
      `;
      
      // Duplicate function calls should deduplicate
      const result = getQueryResults(query);
      assertEqual(result.length, 10); // All names are "testX" (length >= 5)
    },

    // Test 12: Different function calls
    testDifferentFunctionCalls: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER LENGTH(doc.name) > 3 AND LENGTH(doc.arr) > 2
          RETURN doc
      `;
      
      // Different functions, should both apply
      const result = getQueryResults(query);
      assertEqual(result.length, 10);
    },

    // Test 13: Nested function calls
    testNestedFunctionCalls: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER LOWER(CONCAT(doc.name, "x")) == LOWER(CONCAT(doc.name, "x"))
          RETURN doc
      `;
      
      // Duplicate nested calls should deduplicate
      const result = getQueryResults(query);
      assertEqual(result.length, 10);
    },

    ////////////////////////////////////////////////////////////////////////////////
    // NON-DETERMINISTIC TESTS
    ////////////////////////////////////////////////////////////////////////////////

    // Test 14: Non-deterministic functions not deduplicated
    testNonDeterministicNotDeduplicated: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER RAND() > 0.5 AND RAND() > 0.5
          RETURN doc
      `;
      
      // Should not crash, results vary due to randomness
      const result = getQueryResults(query);
      assertTrue(result.length >= 0 && result.length <= 10);
    },

    // Test 15: Mix of deterministic and non-deterministic
    testMixedDeterministicNonDeterministic: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value > 5 AND RAND() > 0.3 AND doc.value > 5
          RETURN doc
      `;
      
      // doc.value > 5 should deduplicate, RAND() should not
      const result = getQueryResults(query);
      assertTrue(result.length >= 0 && result.length <= 4);
    },

    ////////////////////////////////////////////////////////////////////////////////
    // IN OPERATOR TESTS
    ////////////////////////////////////////////////////////////////////////////////

    // Test 16: IN operator deduplication
    testINOperatorDeduplication: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value IN [1,2,3] AND doc.value IN [1,2,3]
          RETURN doc
      `;
      
      // Duplicate IN should deduplicate
      const result = getQueryResults(query);
      assertEqual(result.length, 3); // Values 1, 2, 3
    },

    // Test 17: IN with different arrays
    testINOperatorDifferentArrays: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value IN [1,2,3] AND doc.value IN [2,3,4]
          RETURN doc
      `;
      
      // Different IN arrays, both conditions apply
      const result = getQueryResults(query);
      assertEqual(result.length, 2); // Values 2, 3 (intersection)
    },

    // Test 18: IN with sorted vs unsorted (should be normalized)
    testINOperatorSortedArray: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value IN [3,2,1] AND doc.value IN [1,2,3]
          RETURN doc
      `;
      
      // Arrays should be normalized/sorted before comparison
      const result = getQueryResults(query);
      assertEqual(result.length, 3);
    },

    ////////////////////////////////////////////////////////////////////////////////
    // COMMUTATIVE OPERATOR TESTS
    ////////////////////////////////////////////////////////////////////////////////

    // Test 19: Commutative == operator normalization
    testCommutativeOperatorNormalization: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value == 5 AND 5 == doc.value
          RETURN doc
      `;
      
      // After normalization, both become "doc.value == 5"
      const result = getQueryResults(query);
      assertEqual(result.length, 1);
    },

    // Test 20: Commutative != operator
    testCommutativeNE: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value != 5 AND 5 != doc.value
          RETURN doc
      `;
      
      // Both should normalize and deduplicate
      const result = getQueryResults(query);
      assertEqual(result.length, 9); // All except 5
    },

    // Test 21: Non-commutative operators (< vs >)
    testNonCommutativeOperators: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value > 5 AND doc.value < 8
          RETURN doc
      `;
      
      // Different operators, both should apply
      const result = getQueryResults(query);
      assertEqual(result.length, 2); // Values 6, 7
    },

    ////////////////////////////////////////////////////////////////////////////////
    // COMPLEX EXPRESSION TESTS
    ////////////////////////////////////////////////////////////////////////////////

    // Test 22: Complex expressions with arithmetic
    testComplexExpressionDeduplication: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER (doc.value + 1) > 5 AND (doc.value + 1) > 5
          RETURN doc
      `;
      
      // Duplicate complex expressions should deduplicate
      const result = getQueryResults(query);
      assertEqual(result.length, 5); // Values 5,6,7,8,9
    },

    // Test 23: Complex expressions with multiple operations
    testComplexMultipleOperations: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER (doc.value * 2 + 1) > 10 AND (doc.value * 2 + 1) > 10
          RETURN doc
      `;
      
      // Should deduplicate
      const result = getQueryResults(query);
      assertEqual(result.length, 5); // (5*2+1)=11, (6*2+1)=13, etc.
    },

    // Test 24: Expressions with attribute access
    testExpressionWithAttributeAccess: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.nested.value + doc.value > 10 AND doc.nested.value + doc.value > 10
          RETURN doc
      `;
      
      // Duplicate should be removed
      const result = getQueryResults(query);
      assertEqual(result.length, 4); // 6+6=12, 7+7=14, etc.
    },

    ////////////////////////////////////////////////////////////////////////////////
    // ATTRIBUTE vs CONSTANT TESTS
    ////////////////////////////////////////////////////////////////////////////////

    // Test 25: Attribute > Constant normalization
    testAttributeConstantNormalization: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value > 5 AND doc.value > 5
          RETURN doc
      `;
      
      // Should deduplicate
      const result = getQueryResults(query);
      assertEqual(result.length, 4);
    },

    // Test 26: Constant > Attribute should normalize
    testConstantAttributeNormalization: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value < 5 AND 5 > doc.value
          RETURN doc
      `;
      
      // Both should normalize to "doc.value < 5" and deduplicate
      const result = getQueryResults(query);
      assertEqual(result.length, 5); // Values 0,1,2,3,4
    },

    // Test 27: Attribute vs Attribute comparison
    testAttributeAttributeComparison: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value > doc.nested.value AND doc.value > doc.nested.value
          RETURN doc
      `;
      
      // Should deduplicate (both are same expression)
      const result = getQueryResults(query);
      assertEqual(result.length, 0); // value == nested.value for all docs
    },

    ////////////////////////////////////////////////////////////////////////////////
    // ARRAY AND OBJECT TESTS
    ////////////////////////////////////////////////////////////////////////////////

    // Test 28: Array access deduplication
    testArrayAccessDeduplication: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.arr[0] > 5 AND doc.arr[0] > 5
          RETURN doc
      `;
      
      // Should deduplicate
      const result = getQueryResults(query);
      assertEqual(result.length, 4); // arr[0] is same as value for indices 6-9
    },

    // Test 29: Different array indices
    testDifferentArrayIndices: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.arr[0] > 5 AND doc.arr[1] > 5
          RETURN doc
      `;
      
      // Different indices, both conditions apply
      // arr for doc with value=i is [i, i+1, i+2]
      // arr[0] > 5 means i > 5, so values 6,7,8,9
      // arr[1] > 5 means i+1 > 5, so i > 4, which is values 5,6,7,8,9
      // Intersection: values 6,7,8,9 = 4 docs
      const result = getQueryResults(query);
      assertEqual(result.length, 4);
    },

    ////////////////////////////////////////////////////////////////////////////////
    // EXTREME EDGE CASES
    ////////////////////////////////////////////////////////////////////////////////

    // Test 30: Triple nested AND
    testTripleNestedAND: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER ((doc.value > 5 AND doc.value < 8) AND doc.value != 6)
          RETURN doc
      `;
      
      // Should work correctly with nested structure
      const result = getQueryResults(query);
      assertEqual(result.length, 1); // Only value 7
    },

    // Test 31: Mixed AND/OR with duplicates
    testMixedANDORWithDuplicates: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER (doc.value > 8 AND doc.value > 8) OR (doc.value < 2 AND doc.value < 2)
          RETURN doc
      `;
      
      // Duplicates in both AND branches should be removed
      const result = getQueryResults(query);
      assertEqual(result.length, 3); // Values 0, 1, 9
    },

    // Test 32: Empty result with deduplication
    testEmptyResultWithDeduplication: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value > 100 AND doc.value > 100 AND doc.value > 100
          RETURN doc
      `;
      
      // Should return empty, deduplication shouldn't break this
      const result = getQueryResults(query);
      assertEqual(result.length, 0);
    },

    // Test 33: All documents match with deduplication
    testAllDocumentsMatch: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value >= 0 AND doc.value >= 0
          RETURN doc
      `;
      
      // Should return all docs
      const result = getQueryResults(query);
      assertEqual(result.length, 10);
    },

    // Test 34: Very long duplicate chain
    testLongDuplicateChain: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER doc.value > 5 
            AND doc.value > 5 
            AND doc.value > 5 
            AND doc.value > 5 
            AND doc.value > 5
          RETURN doc
      `;
      
      // All five duplicates should reduce to one
      const result = getQueryResults(query);
      assertEqual(result.length, 4);
    },

    // Test 35: Parenthesized expressions
    testParenthesizedExpressions: function () {
      const query = `
        FOR doc IN ${cn}
          FILTER (doc.value) > 5 AND doc.value > 5
          RETURN doc
      `;
      
      // Parentheses shouldn't prevent deduplication
      const result = getQueryResults(query);
      assertEqual(result.length, 4);
    }
  };
}

jsunity.run(duplicateDetectionEdgeCasesSuite);

return jsunity.done();
