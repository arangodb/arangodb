/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author 
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var db = require("@arangodb").db;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function gatherBlockTestSuite () {
  var cn1 = "UnitTestsGatherBlock1";
  var cn2 = "UnitTestsGatherBlock2";
  var cn3 = "UnitTestsGatherBlock3";
  var c1, c2, c3;    
 
  var explain = function (result) {
    return helper.getCompactPlan(result).map(function(node) 
        { return node.type; });
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var j, k;
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      c1 = db._create(cn1, {numberOfShards:3});
      c2 = db._create(cn2);
      for (j = 0; j < 400; j++) {
        for (k = 0; k < 10; k++){
          c1.insert({Hallo:k});
          c2.insert({Hallo:k});
        }
      }
      c3 = db._create(cn3, {numberOfShards:3});
      for (k = 0; k < 10; k++){
        c3.insert({Hallo:k});
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
      db._drop(cn3);
      c1 = null;
      c2 = null;
      c3 = null;
    },
    
    testSubqueryValuePropagation : function () {
      c3.truncate();
      c3.insert({Hallo:1});
      var query = "FOR i IN 1..1 LET s = (FOR j IN 1..i FOR k IN " + cn3 + " RETURN j) RETURN s";
      // check the return value
      var expected = [ [ 1 ] ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testCalculationNotMovedOverBoundary : function () {
      c3.truncate();
      c3.insert({Hallo:1});
      var query = "FOR i IN " + cn3 + " FOR j IN 1..1 FOR k IN " + cn3 + " RETURN j";
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 1 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },

    testSimple1 : function () {
      var query = "FOR d IN " + cn1 + " LIMIT 10 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 10;
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },

    testSimple2 : function () {
      var query = 
          "FOR d IN " + cn1 + " FILTER d.Hallo in [5,63] RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ ];
      for (var i = 0; i < 400; i++) {
        expected.push(5);
      }
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testSimple3 : function () {
      var query = 
          "FOR d IN " + cn1 
            + " FILTER d.Hallo in [0,0,5,6,6,63] LIMIT 399,2 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 2;
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },
    
    testSimple4 : function () {
      var query = 
          "FOR d IN " + cn1 
            + " FILTER d.Hallo in [0,5,6] LIMIT 995,10 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 10;
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple1 : function () {
      var query = "FOR d IN " + cn3 + " SORT d.Hallo RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },

    testNonSimple2 : function () {
      var query = 
          "FOR d IN " + cn3 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0, 5, 6 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple3 : function () {
      var query = 
          "FOR d IN " + cn1 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo LIMIT 399,2 RETURN d.Hallo";
      
      // check 2the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0, 5 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple4 : function () {
      var query = 
          "FOR d IN " + cn1 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo LIMIT 1 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple5 : function () {
      var query = 
          "FOR d IN " + cn1 
            + " FILTER d.Hallo in [0,5,6] SORT d.Hallo LIMIT 995,10 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },

    testSimple5 : function () {
      var query = "FOR d IN " + cn2 + " LIMIT 10 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 10;
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },

    testSimple6 : function () {
      var query = 
          "FOR d IN " + cn3 + " FILTER d.Hallo in [5,63] RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 5 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testSimple7 : function () {
      var query = 
          "FOR d IN " + cn2 
            + " FILTER d.Hallo in [0,0,5,6,6,63] LIMIT 399,2 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 2;
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },
    
    testSimple8 : function () {
      var query = 
          "FOR d IN " + cn2 
            + " FILTER d.Hallo in [0,5,6] LIMIT 990,21 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 21; 
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple7 : function () {
      var query = 
          "FOR d IN " + cn3 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0, 5, 6 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple8 : function () {
      var query = 
          "FOR d IN " + cn2 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo DESC LIMIT 399,2 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = 2;
      var actual = AQL_EXECUTE(query).json.length;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple9 : function () {
      var query = 
          "FOR d IN " + cn2 
            + " FILTER d.Hallo in [0,5,6,62] SORT d.Hallo DESC LIMIT 1 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 6 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    },
    
    testNonSimple10 : function () {
      var query = 
          "FOR d IN " + cn2 
            + " FILTER d.Hallo in [0,5,6] SORT d.Hallo DESC LIMIT 995,10 RETURN d.Hallo";
      
      // check the GatherNode is in the plan!
      assertTrue(explain(AQL_EXPLAIN(query)).indexOf("GatherNode") !== -1, query);
      
      // check the return value
      var expected = [ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ];
      var actual = AQL_EXECUTE(query).json;

      assertEqual(expected, actual, query);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(gatherBlockTestSuite);

return jsunity.done();

