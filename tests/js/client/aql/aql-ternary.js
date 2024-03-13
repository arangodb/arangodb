/*jshint globalstrict:false, strict:false, strict: false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;
const db = require("@arangodb").db;
  
const cn = "UnittestsTernary";

function TernaryTestSuite () {

  return {
    testTernarySimple : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("RETURN 1 > 0 ? 2 : -1");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN NOOPT(1 > 0 ? 2 : -1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested1 : function () {
      var expected = [ -1 ];

      var actual = getQueryResults("RETURN 15 > 15 ? 1 : -1");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN NOOPT(15 > 15 ? 1 : -1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested2 : function () {
      var expected = [ -1 ];

      var actual = getQueryResults("RETURN 10 + 5 > 15 ? 1 : -1");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN NOOPT(10 + 5 > 15 ? 1 : -1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested3 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("RETURN true ? true ? true ? 1 : -1 : -2 : 3");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN NOOPT(true ? true ? true ? 1 : -1 : -2 : 3)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested4 : function () {
      var expected = [ 3 ];

      var actual = getQueryResults("RETURN false ? true ? true ? 1 : -1 : -2 : 3");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN NOOPT(false ? true ? true ? 1 : -1 : -2 : 3)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested5 : function () {
      var expected = [ -2 ];

      var actual = getQueryResults("RETURN true ? false ? true ? 1 : -1 : -2 : 3");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN NOOPT(true ? false ? true ? 1 : -1 : -2 : 3)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested6 : function () {
      var expected = [ -1 ];

      var actual = getQueryResults("RETURN true ? true ? false ? 1 : -1 : -2 : 3");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN NOOPT(true ? true ? false ? 1 : -1 : -2 : 3)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryLet1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("LET a = 1, b = 2, c = 3 RETURN true ? a : b");
      assertEqual(expected, actual);
      
      actual = getQueryResults("LET a = 1, b = 2, c = 3 RETURN NOOPT(true ? a : b)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryLet2 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("LET a = 1, b = 2, c = 3 RETURN false ? a : b");
      assertEqual(expected, actual);
      
      actual = getQueryResults("LET a = 1, b = 2, c = 3 RETURN NOOPT(false ? a : b)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryLet3 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("LET a = 1, b = 2, c = 3 RETURN true ? false ? a : b : c");
      assertEqual(expected, actual);
      
      actual = getQueryResults("LET a = 1, b = 2, c = 3 RETURN NOOPT(true ? false ? a : b : c)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary with non-boolean condition
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNonBoolean : function () {
      assertEqual([ 2 ], getQueryResults("RETURN 1 ? 2 : 3"));
      assertEqual([ 3 ], getQueryResults("RETURN 0 ? 2 : 3"));
      assertEqual([ 3 ], getQueryResults("RETURN null ? 2 : 3"));
      assertEqual([ 3 ], getQueryResults("RETURN false ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN true ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN (4) ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN (4 - 3) ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN \"true\" ? 2 : 3"));
      assertEqual([ 3 ], getQueryResults("RETURN \"\" ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN [ ] ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN [ 0 ] ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN { } ? 2 : 3"));
      
      assertEqual([ 2 ], getQueryResults("RETURN NOOPT(1 ? 2 : 3)"));
      assertEqual([ 3 ], getQueryResults("RETURN NOOPT(0 ? 2 : 3)"));
      assertEqual([ 3 ], getQueryResults("RETURN NOOPT(null ? 2 : 3)"));
      assertEqual([ 3 ], getQueryResults("RETURN NOOPT(false ? 2 : 3)"));
      assertEqual([ 2 ], getQueryResults("RETURN NOOPT(true ? 2 : 3)"));
      assertEqual([ 2 ], getQueryResults("RETURN NOOPT((4) ? 2 : 3)"));
      assertEqual([ 2 ], getQueryResults("RETURN NOOPT((4 - 3) ? 2 : 3)"));
      assertEqual([ 2 ], getQueryResults("RETURN NOOPT(\"true\" ? 2 : 3)"));
      assertEqual([ 3 ], getQueryResults("RETURN NOOPT(\"\" ? 2 : 3)"));
      assertEqual([ 2 ], getQueryResults("RETURN NOOPT([ ] ? 2 : 3)"));
      assertEqual([ 2 ], getQueryResults("RETURN NOOPT([ 0 ] ? 2 : 3)"));
      assertEqual([ 2 ], getQueryResults("RETURN NOOPT({ } ? 2 : 3)"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator shortcut
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryShortcut : function () {
      var expected = [ 'foo', 'foo', 1, 2 ];

      var actual = getQueryResults("FOR i IN [ null, 0, 1, 2 ] RETURN i ? : 'foo'");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR i IN [ null, 0, 1, 2 ] RETURN i ?: 'foo'");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR i IN [ null, 0, 1, 2 ] RETURN NOOPT(i ? : 'foo')");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR i IN [ null, 0, 1, 2 ] RETURN NOOPT(i ?: 'foo')");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN 1 ?: ASSERT(false, 'peng')");
      assertEqual([ 1 ], actual);
      
      actual = getQueryResults("RETURN NOOPT(1) ?: ASSERT(false, 'peng')");
      assertEqual([ 1 ], actual);
      
      actual = getQueryResults("FOR i IN [ 1, 2, 3, 4 ] RETURN i ?: ASSERT(false, 'peng')");
      assertEqual([ 1, 2, 3, 4 ], actual);
    },

  };
}

function TernaryCollectionTestSuite () {
  let c = null;
  
  return {
    setUp : function () {
      db._drop(cn);
      c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({_key: "test" + i, value: i});
      }
      c.insert(docs);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testCollTernarySimple : function () {
      // this mainly tests if stringification of the ternary works...
      var query = "FOR i IN 1..10 LET v = i == 333 ? 2 : 3 FOR j IN " + cn + " FILTER j._id == v RETURN 1"; 

      var actual = getQueryResults(query);
      assertEqual([], actual);
    },

    testCollTernaryShortcut : function () {
      var query = "RETURN DOCUMENT('" + cn + "/test10').value ?: 'foo'";
      var actual = getQueryResults(query);
      assertEqual([10], actual);
      
      query = "RETURN DOCUMENT('" + cn + "/test100000000').value ?: 'foo'";
      actual = getQueryResults(query);
      assertEqual(['foo'], actual);
    }
  };
}

function TernarySideEffectsTestSuite () {
  let c = null;
  
  return {
    setUp : function () {
      c = db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },
    
    testTernaryConditionOnlyExecutedOnce : function () {
      const query = `
RETURN LENGTH(FOR i IN 1..10 INSERT {} INTO ${cn} RETURN i) == 10 ? true : false
`;
      
      let result = db._query(query).toArray();
      assertEqual([true], result);
      let numDocs = c.count();
      assertEqual(10, numDocs);
    },
    
    testTernaryConditionOnlyExecutedOnceWithSubqueries : function () {
      const query = `
RETURN LENGTH(FOR i IN 1..10 INSERT {} INTO ${cn} RETURN i) == 10 ? (FOR j IN 1..10 RETURN j) : (FOR j IN 1..5 RETURN j)
`;
      
      let result = db._query(query).toArray();
      assertEqual(1, result.length);
      assertEqual([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], result[0]);
      let numDocs = c.count();
      assertEqual(10, numDocs);
    },
    
    testTernaryWithNonDeterministicCondition : function () {
      const query = `
FOR i IN 1..1000
  RETURN RANDOM() >= 0.5 ? (INSERT {value: true} INTO ${cn}) : (null)
`;
      
      db._query(query);
      let numDocs = c.count();
      assertTrue(numDocs > 0, {numDocs});
      assertTrue(numDocs < 1000, {numDocs});

      let result = db._query(`FOR doc IN ${cn} FILTER doc.value == true RETURN doc.value`).toArray();
      assertEqual(numDocs, result.length);
    },

    testQueryThatEnumeratesPotentialNonArray : function () {
      const query = `
LET doc = DOCUMENT(${cn}, "foo") 
LET data = IS_ARRAY(doc.array) ? (
  FOR sub IN doc.array
    RETURN sub * 2
) : 'n/a'
RETURN data
`;
      
      // execute query while search document is not present
      let result = db._query(query).toArray();
      assertEqual(1, result.length);
      let doc = result[0];
      assertEqual("n/a", doc);

      // insert search document and execute query again
      c.insert({ _key: "foo", array: [1, 2, 3] });
      result = db._query(query).toArray();
      assertEqual(1, result.length);
      doc = result[0];
      assertEqual([2, 4, 6], doc);
    },

    testQueryWithAssertionFailure : function () {
      const query = `
FOR i IN 1..10
  LET x = (
    i >= 5 ? (
      FOR x IN 1..5
        RETURN ASSERT(i >= 5, 'ass 1')
    ) : (
      FOR y IN 1..2
        RETURN ASSERT(i < 5, 'ass 2')
    )
  )
  RETURN x
`;
      let result = db._query(query).toArray();
      assertEqual(10, result.length);
      for (let i = 1; i <= 10; ++i) {
        let doc = result[i - 1];
        if (i >= 5) {
          assertEqual([true, true, true, true, true], doc);
        } else {
          assertEqual([true, true], doc);
        }
      }
    },
    
    testNestedQueryWithAssertionFailure : function () {
      const query = `
FOR i IN 1..10
  LET x = (
    i >= 5 ? (
      i >= 7 ? 
        (RETURN ASSERT(i >= 7, 'ass 1'))
      : 
        (RETURN ASSERT(i >= 5 && i < 7, 'ass 2'))
    ) : (
      i >= 3 ? 
        (RETURN ASSERT(i >= 3 && i < 5, 'ass 3'))
      :
        (RETURN ASSERT(i < 3, 'ass 4'))
    )
  )
  RETURN x
`;
      let result = db._query(query).toArray();
      assertEqual(10, result.length);
      for (let i = 1; i <= 10; ++i) {
        let doc = result[i - 1];
        assertEqual([true], doc);
      }
    },
    
    testNestedQueryWithAssertionFailureIndependentVars : function () {
      const query = `
FOR i IN 1..10
  FOR j IN 1..10
    LET x = (
      i >= 5 ? (
        j >= 5 ? 
          (RETURN ASSERT(i >= 5 && j >= 5, 'ass 1'))
        : 
          (RETURN ASSERT(i >= 5 && j < 5, 'ass 2'))
      ) : (
        j >= 5 ? 
          (RETURN ASSERT(i < 5 && j >= 5, 'ass 3'))
        :
          (RETURN ASSERT(i < 5 && j < 5, 'ass 4'))
      )
    )
    RETURN x
`;
      let result = db._query(query, null, {optimizer: {rules: ["-interchange-adjacent-enumerations"] } }).toArray();
      assertEqual(100, result.length);
      result.forEach((doc) => {
        assertEqual([true], doc);
      });
    },
    
    testConditionalInsert : function () {
      const query = `
LET maybeExists = DOCUMENT(${cn}, "foo")
LET upsertDoc = !IS_NULL(maybeExists) ? maybeExists : FIRST(INSERT { bar: "baz" } INTO ${cn} RETURN NEW)
RETURN upsertDoc
`;
     
      let result = db._query(query).toArray();
      assertEqual(1, result.length);
      let doc = result[0];
      assertTrue(doc.hasOwnProperty('bar'), doc);
      assertEqual("baz", doc.bar);
      assertEqual(1, c.count());
      
      // execute query once more
      result = db._query(query).toArray();
      assertEqual(1, result.length);
      doc = result[0];
      assertTrue(doc.hasOwnProperty('bar'), doc);
      assertEqual("baz", doc.bar);
      assertEqual(2, c.count());
      
      // insert search document and execute query once more
      c.insert({ _key: "foo" });
      assertEqual(3, c.count());

      result = db._query(query).toArray();
      assertEqual(1, result.length);
      doc = result[0];
      assertFalse(doc.hasOwnProperty('bar'), doc);
      assertEqual("foo", doc._key);
      assertEqual(3, c.count());
      
      // and one final time...
      result = db._query(query).toArray();
      assertEqual(1, result.length);
      doc = result[0];
      assertFalse(doc.hasOwnProperty('bar'), doc);
      assertEqual("foo", doc._key);
      assertEqual(3, c.count());
    },

    testMustNotExecuteUnreachableConditions : function () {
      const query = `
RETURN NOOPT(true) ? NOOPT(ASSERT(true, 'ass 1')) : NOOPT(ASSERT(false, 'ass 2'))
`;
      let result = db._query(query).toArray();
      assertEqual(1, result.length);
      assertEqual(true, result[0]);
    },
    
    testMustNotExecuteUnreachableConditionsNested : function () {
      // nested ternary operators, with conditions that can only be evaluated
      // at runtime.
      // we must only evaluate the chain NOOPT(true) -> NOOPT(true) here.
      // evaluating any other chain will lead to an ASSERT(false) error.
      const query = `
RETURN NOOPT(true) ? 
  (NOOPT(true) ? NOOPT(ASSERT(true, 'ass 1')) : NOOPT(ASSERT(false, 'ass 2'))) : 
  (NOOPT(true) ? NOOPT(ASSERT(false, 'ass 3')) : NOOPT(ASSERT(false, 'ass 4')))
`;
      let result = db._query(query).toArray();
      assertEqual(1, result.length);
      assertEqual(true, result[0]);
    },

    testShortcutTernary : function () {
      const query = `
RETURN NOOPT(true) ? : ASSERT(false, 'fail')
`;
      let result = db._query(query).toArray();
      assertEqual(1, result.length);
      assertEqual(true, result[0]);
    },
    
    testShortcutTernaryFalseCondition : function () {
      const query = `
RETURN NOOPT(false) ? : ASSERT(true, 'fail')
`;
      let result = db._query(query).toArray();
      assertEqual(1, result.length);
      assertEqual(true, result[0]);
    },
    
    testShortcutTernaryWithSubqueryFalse : function () {
      const query = `
LET values = NOOPT([1, 2, 3])
RETURN !IS_ARRAY(values) ? : (FOR i IN values RETURN i)
`;
      let result = db._query(query).toArray();
      assertEqual(1, result.length);
      assertEqual([1, 2, 3], result[0]);
    },

    testShortcutTernaryWithSubqueryTrue : function () {
      const query = `
LET values = NOOPT([1, 2, 3])
RETURN IS_ARRAY(values) ? : (FOR i IN values RETURN i)
`;
      let result = db._query(query).toArray();
      assertEqual(1, result.length);
      assertEqual(true, result[0]);
    },

  };
}

jsunity.run(TernaryTestSuite);
jsunity.run(TernaryCollectionTestSuite);
jsunity.run(TernarySideEffectsTestSuite);

return jsunity.done();
