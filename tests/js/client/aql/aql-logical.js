/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertException */

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
const db = require("@arangodb").db;
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;

function ahuacatlLogicalTestSuite () {
  const cn = "UnitTestsLogical";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot1 : function () {
      let expected = [ false ];
      let actual = getQueryResults("RETURN !true");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot2 : function () {
      let expected = [ true ];
      let actual = getQueryResults("RETURN !false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot3 : function () {
      let expected = [ false ];
      let actual = getQueryResults("RETURN !!false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot4 : function () {
      let expected = [ true ];
      let actual = getQueryResults("RETURN !!!false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot5 : function () {
      let expected = [ false ];
      let actual = getQueryResults("RETURN !(1 == 1)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot6 : function () {
      let expected = [ true ];
      let actual = getQueryResults("RETURN !(!(1 == 1))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNot7 : function () {
      let expected = [ true ];
      let actual = getQueryResults("RETURN !true == !!false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNotNonBoolean : function () {
      let c = db._create(cn);
      let doc = c.insert({});
      try {
        assertEqual([ true ], getQueryResults("RETURN !null")); 
        assertEqual([ true ], getQueryResults("RETURN !0")); 
        assertEqual([ true ], getQueryResults("RETURN !\"\"")); 
        assertEqual([ false ], getQueryResults("RETURN !\" \"")); 
        assertEqual([ false ], getQueryResults("RETURN !\"0\"")); 
        assertEqual([ false ], getQueryResults("RETURN !\"1\"")); 
        assertEqual([ false ], getQueryResults("RETURN !\"value\"")); 
        assertEqual([ false ], getQueryResults("RETURN ![]")); 
        assertEqual([ false ], getQueryResults("RETURN !{}")); 
        assertEqual([ false ], getQueryResults("RETURN !DOCUMENT(" + cn + ", " + JSON.stringify(doc._id) + ")")); 
      } finally {
        db._drop(cn);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unary not
////////////////////////////////////////////////////////////////////////////////
    
    testUnaryNotPrecedence : function () {
      // not has higher precedence than ==
      assertEqual([ false ], getQueryResults("RETURN !1 == 0")); 
      assertEqual([ true ], getQueryResults("RETURN !1 == !1")); 
      assertEqual([ false ], getQueryResults("RETURN !1 != !1")); 
      assertEqual([ false ], getQueryResults("RETURN !1 > 7")); 
      assertEqual([ true ], getQueryResults("RETURN !1 < 7")); 
      assertEqual([ true ], getQueryResults("RETURN !1 IN [!1]")); 
      assertEqual([ false ], getQueryResults("RETURN !1 NOT IN [!1]")); 
      assertEqual([ false ], getQueryResults("RETURN !1 IN [1]")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAnd1 : function () {
      let expected = [ true ];
      let actual = getQueryResults("RETURN true && true");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAnd2 : function () {
      let expected = [ false ];
      let actual = getQueryResults("RETURN true && false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAnd3 : function () {
      let expected = [ false ];
      let actual = getQueryResults("RETURN false && true");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAnd4 : function () {
      let expected = [ false ];
      let actual = getQueryResults("RETURN false && false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAnd5 : function () {
      let expected = [ true ];
      let actual = getQueryResults("RETURN true && !false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and 
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAndNonBoolean : function () {
      assertEqual([ null ], getQueryResults("RETURN null && true"));
      assertEqual([ true ], getQueryResults("RETURN 1 && true"));
      assertEqual([ 0 ], getQueryResults("RETURN 1 && 0"));
      assertEqual([ false ], getQueryResults("RETURN 1 && false"));
      assertEqual([ 1 ], getQueryResults("RETURN 1 && 1"));
      assertEqual([ 0 ], getQueryResults("RETURN 0 && true"));
      assertEqual([ 0 ], getQueryResults("RETURN 0 && 0"));
      assertEqual([ 0 ], getQueryResults("RETURN 0 && false"));
      assertEqual([ 0 ], getQueryResults("RETURN 0 && 1"));
      assertEqual([ "" ], getQueryResults("RETURN \"\" && true"));
      assertEqual([ true ], getQueryResults("RETURN \" \" && true"));
      assertEqual([ true ], getQueryResults("RETURN \"false\" && true"));
      assertEqual([ true ], getQueryResults("RETURN [ ] && true"));
      assertEqual([ true ], getQueryResults("RETURN { } && true"));
      assertEqual([ [ ] ], getQueryResults("RETURN 23 && [ ]"));
      assertEqual([ { } ], getQueryResults("RETURN 23 && { }"));
      assertEqual([ null ], getQueryResults("RETURN true && null"));
      assertEqual([ 1 ], getQueryResults("RETURN true && 1"));
      assertEqual([ "" ], getQueryResults("RETURN true && \"\""));
      assertEqual([ "false" ], getQueryResults("RETURN true && \"false\""));
      assertEqual([ false ], getQueryResults("RETURN true && false"));
      assertEqual([ [ ] ], getQueryResults("RETURN true && [ ]"));
      assertEqual([ { } ], getQueryResults("RETURN true && { }"));
      assertEqual([ null ], getQueryResults("RETURN null && false"));
      assertEqual([ false ], getQueryResults("RETURN 1 && false"));
      assertEqual([ "" ], getQueryResults("RETURN \"\" && false"));
      assertEqual([ false ], getQueryResults("RETURN \"false\" && false"));
      assertEqual([ false ], getQueryResults("RETURN [ ] && false"));
      assertEqual([ false ], getQueryResults("RETURN { } && false"));
      assertEqual([ false ], getQueryResults("RETURN false && null"));
      assertEqual([ false ], getQueryResults("RETURN false && 1"));
      assertEqual([ false ], getQueryResults("RETURN false && \"\""));
      assertEqual([ false ], getQueryResults("RETURN false && \"false\""));
      assertEqual([ false ], getQueryResults("RETURN false && [ ]"));
      assertEqual([ false ], getQueryResults("RETURN false && { }"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary and, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAndShortCircuit1 : function () {
      let expected = [ false ];
      let actual = getQueryResults("RETURN false && FAIL('this will fail')");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryAndShortCircuit2 : function () {
      assertException(function() { getQueryResults("RETURN true && FAIL('this will fail')"); });
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr1 : function () {
      let expected = [ true ];
      let actual = getQueryResults("RETURN true || true");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr2 : function () {
      let expected = [ true ];
      let actual = getQueryResults("RETURN true || false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr3 : function () {
      let expected = [ true ];
      let actual = getQueryResults("RETURN false || true");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr4 : function () {
      let expected = [ false ];
      let actual = getQueryResults("RETURN false || false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr5 : function () {
      let expected = [ true ];
      let actual = getQueryResults("RETURN true || !false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOr6 : function () {
      let expected = [ true ];
      let actual = getQueryResults("RETURN false || !false");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOrNonBoolean : function () {
      assertEqual([ true ], getQueryResults("RETURN null || true"));
      assertEqual([ true ], getQueryResults("RETURN 0 || true"));
      assertEqual([ true ], getQueryResults("RETURN false || true"));
      assertEqual([ 1 ], getQueryResults("RETURN 1 || true"));
      assertEqual([ true ], getQueryResults("RETURN \"\" || true"));
      assertEqual([ " " ], getQueryResults("RETURN \" \" || true"));
      assertEqual([ "0" ], getQueryResults("RETURN \"0\" || true"));
      assertEqual([ "1" ], getQueryResults("RETURN \"1\" || true"));
      assertEqual([ "false" ], getQueryResults("RETURN \"false\" || true"));
      assertEqual([ [ ] ], getQueryResults("RETURN [ ] || true"));
      assertEqual([ { } ], getQueryResults("RETURN { } || true"));
      assertEqual([ true ], getQueryResults("RETURN true || null"));
      assertEqual([ true ], getQueryResults("RETURN true || 1"));
      assertEqual([ true ], getQueryResults("RETURN true || \"\""));
      assertEqual([ true ], getQueryResults("RETURN true || \"false\""));
      assertEqual([ true ], getQueryResults("RETURN true || [ ]"));
      assertEqual([ true ], getQueryResults("RETURN true || { }"));
      assertEqual([ false ], getQueryResults("RETURN null || false"));
      assertEqual([ false ], getQueryResults("RETURN 0 || false"));
      assertEqual([ 1 ], getQueryResults("RETURN 1 || false"));
      assertEqual([ false ], getQueryResults("RETURN false || false"));
      assertEqual([ true ], getQueryResults("RETURN true || false"));
      assertEqual([ false ], getQueryResults("RETURN \"\" || false"));
      assertEqual([ " " ], getQueryResults("RETURN \" \" || false"));
      assertEqual([ "0" ], getQueryResults("RETURN \"0\" || false"));
      assertEqual([ "1" ], getQueryResults("RETURN \"1\" || false"));
      assertEqual([ "false" ], getQueryResults("RETURN \"false\" || false"));
      assertEqual([ [ ] ], getQueryResults("RETURN [ ] || false"));
      assertEqual([ { } ], getQueryResults("RETURN { } || false"));
      assertEqual([ null ], getQueryResults("RETURN false || null"));
      assertEqual([ 1 ], getQueryResults("RETURN false || 1"));
      assertEqual([ "" ], getQueryResults("RETURN false || \"\""));
      assertEqual([ " " ], getQueryResults("RETURN false || \" \""));
      assertEqual([ "0" ], getQueryResults("RETURN false || \"0\""));
      assertEqual([ "1" ], getQueryResults("RETURN false || \"1\""));
      assertEqual([ "false" ], getQueryResults("RETURN false || \"false\""));
      assertEqual([ [ ] ], getQueryResults("RETURN false || [ ]"));
      assertEqual([ { } ], getQueryResults("RETURN false || { }"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOrShortCircuit1 : function () {
      assertEqual([ true ], getQueryResults("RETURN true || FAIL('this will fail')")); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOrShortCircuit2 : function () {
      assertException(function() { getQueryResults("RETURN false || FAIL('this will fail')"); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test binary or, short circuit evaluation
////////////////////////////////////////////////////////////////////////////////
    
    testBinaryOrShortCircuit3 : function () {
      assertException(function() { getQueryResults("RETURN FAIL('this will fail') || true"); });
    },

    testBinaryAndShortConditionOnlyExecutedOnce : function () {
      let c = db._create(cn);
      try {
        let result = db._query(`RETURN LENGTH(FOR i IN 1..10 INSERT {} INTO ${cn} RETURN NEW) > 5 && [1, 2, 3]`).toArray();
        assertEqual(1, result.length);
        assertEqual([1, 2, 3], result[0]);
        assertEqual(10, c.count());
      } finally {
        db._drop(cn);
      }
    },
    
    testBinaryOrShortConditionOnlyExecutedOnce : function () {
      let c = db._create(cn);
      try {
        let result = db._query(`RETURN LENGTH(FOR i IN 1..10 INSERT {} INTO ${cn} RETURN NEW) < 5 || [1, 2, 3]`).toArray();
        assertEqual(1, result.length);
        assertEqual([1, 2, 3], result[0]);
        assertEqual(10, c.count());
      } finally {
        db._drop(cn);
      }
    },
    
    testBinaryAndRightOperandExecutedConditionally : function () {
      let c = db._create(cn);
      try {
        let result = db._query(`RETURN LENGTH(FOR i IN 1..10 INSERT {} INTO ${cn} RETURN NEW) < 5 && ASSERT(false, 'ass')`).toArray();
        assertEqual(1, result.length);
        assertEqual(false, result[0]);
        assertEqual(10, c.count());
      } finally {
        db._drop(cn);
      }
    },
    
    testBinaryOrRightOperandExecutedConditionally : function () {
      let c = db._create(cn);
      try {
        let result = db._query(`RETURN LENGTH(FOR i IN 1..10 INSERT {} INTO ${cn} RETURN NEW) > 5 || ASSERT(false, 'ass')`).toArray();
        assertEqual(1, result.length);
        assertEqual(true, result[0]);
        assertEqual(10, c.count());
      } finally {
        db._drop(cn);
      }
    },
    
    testNestedConditions : function () {
      const queries = [
        [ `FOR i IN 1..3 FOR j IN 1..4 FILTER (i <= 3 || ASSERT(false, 'ass1')) && (j <= 4 || ASSERT(false, 'ass2')) RETURN 1`, 12 ],
        [ `FOR i IN 1..3 FOR j IN 1..4 FILTER (i >= 4 && ASSERT(false, 'ass1')) || (j >= 5 && ASSERT(false, 'ass2')) RETURN 1`, 0 ],
        [ `FOR i IN 1..3 FOR j IN 1..4 FILTER (i <= 3 && j <= 4) || ASSERT(false, 'ass') RETURN 1`, 12 ],
        [ `FOR i IN 1..3 FOR j IN 1..4 FILTER (i >= 4 || j >= 5) && ASSERT(false, 'ass') RETURN 1`, 0 ],
        // the (FOR v IN i / FOR v IN j) parts will fail when executed, as i / j are non-arrays
        [ `FOR i IN 1..3 FOR j IN 1..4 FILTER (i <= 3 || (FOR v IN i RETURN v)) && (j <= 4 || (FOR v IN j RETURN v)) RETURN 1`, 12 ],
        [ `FOR i IN 1..3 FOR j IN 1..4 FILTER ((i == 1 || i >= 3) && (j == 2 || j >= 4)) || (i == 2 || j IN [1, 3]) || ASSERT(false, 'ass') RETURN 1`, 12 ],
      ];

      queries.forEach((query) => {
        let result = db._query(query[0]).toArray();
        assertEqual(query[1], result.length, query);
      });
    },

  };
}

jsunity.run(ahuacatlLogicalTestSuite);

return jsunity.done();
