/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, reference optimizer
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerRefTestSuite () {
  var users = null;
  var cn = "UnitTestsAhuacatlOptimizerRef";
  
  var explain = function (query, params) {
    return helper.getCompactPlan(AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-indexes" ] } })).map(function(node) { return node.type; });
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      internal.db._drop(cn);
      users = internal.db._create(cn);
      users.insert([
        { "id" : 100, "name" : "John", "age" : 37, "active" : true, "gender" : "m" },
        { "id" : 101, "name" : "Fred", "age" : 36, "active" : true, "gender" : "m" },
        { "id" : 102, "name" : "Jacob", "age" : 35, "active" : false, "gender" : "m" },
        { "id" : 103, "name" : "Ethan", "age" : 34, "active" : false, "gender" : "m" },
        { "id" : 104, "name" : "Michael", "age" : 33, "active" : true, "gender" : "m" },
        { "id" : 105, "name" : "Alexander", "age" : 32, "active" : true, "gender" : "m" },
        { "id" : 106, "name" : "Daniel", "age" : 31, "active" : true, "gender" : "m" },
        { "id" : 107, "name" : "Anthony", "age" : 30, "active" : true, "gender" : "m" },
        { "id" : 108, "name" : "Jim", "age" : 29, "active" : true, "gender" : "m" },
        { "id" : 109, "name" : "Diego", "age" : 28, "active" : true, "gender" : "m" },
      ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testRefAccess1 : function () {
      var expected = [ { "name" : "John" }, { "name" : "Fred" }, { "name" : "Jacob" }, { "name" : "Ethan" }, { "name" : "Michael" }, { "name" : "Alexander" }, { "name" : "Daniel" }, { "name" : "Anthony" }, { "name" : "Jim"} , { "name" : "Diego" } ];
      var actual = getQueryResults("FOR u1 IN " + cn + " FOR u2 IN " + cn + " FILTER u1.name == u2.name SORT u1.id RETURN { \"name\" : u1.name }");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes (reverted expression)
////////////////////////////////////////////////////////////////////////////////

    testRefAccess2 : function () {
      var expected = [ { "name" : "John" }, { "name" : "Fred" }, { "name" : "Jacob" }, { "name" : "Ethan" }, { "name" : "Michael" }, { "name" : "Alexander" }, { "name" : "Daniel" }, { "name" : "Anthony" }, { "name" : "Jim"} , { "name" : "Diego" } ];
      var actual = getQueryResults("FOR u1 IN " + cn + " FOR u2 IN " + cn + " FILTER u2.name == u1.name SORT u1.id RETURN { \"name\" : u1.name }");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access with _id filter
////////////////////////////////////////////////////////////////////////////////

    testRefAccessId1 : function () {
      var expected = [ { "name" : "John" }, { "name" : "Fred" }, { "name" : "Jacob" }, { "name" : "Ethan" }, { "name" : "Michael" }, { "name" : "Alexander" }, { "name" : "Daniel" }, { "name" : "Anthony" }, { "name" : "Jim"} , { "name" : "Diego" } ];
      var actual = getQueryResults("FOR u1 IN " + cn + " FOR u2 IN " + cn + " FILTER u1._id == u2._id SORT u1.id RETURN { \"name\" : u1.name }");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access with _id filter (reverted expression)
////////////////////////////////////////////////////////////////////////////////

    testRefAccessId2 : function () {
      var expected = [ { "name" : "John" }, { "name" : "Fred" }, { "name" : "Jacob" }, { "name" : "Ethan" }, { "name" : "Michael" }, { "name" : "Alexander" }, { "name" : "Daniel" }, { "name" : "Anthony" }, { "name" : "Jim"} , { "name" : "Diego" } ];
      var actual = getQueryResults("FOR u1 IN " + cn + " FOR u2 IN " + cn + " FILTER u2._id == u1._id SORT u1.id RETURN { \"name\" : u1.name }");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access with index
////////////////////////////////////////////////////////////////////////////////

    testRefAccessIndex1 : function () {
      users.ensureHashIndex("name");

      var expected = [ { "name" : "John" }, { "name" : "Fred" }, { "name" : "Jacob" }, { "name" : "Ethan" }, { "name" : "Michael" }, { "name" : "Alexander" }, { "name" : "Daniel" }, { "name" : "Anthony" }, { "name" : "Jim"} , { "name" : "Diego" } ];
      var actual = getQueryResults("FOR u1 IN " + cn + " FOR u2 IN " + cn + " FILTER u1.name == u2.name SORT u1.id RETURN { \"name\" : u1.name }");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access with index (reverted expression)
////////////////////////////////////////////////////////////////////////////////

    testRefAccessIndex2 : function () {
      users.ensureHashIndex("name");

      var expected = [ { "name" : "John" }, { "name" : "Fred" }, { "name" : "Jacob" }, { "name" : "Ethan" }, { "name" : "Michael" }, { "name" : "Alexander" }, { "name" : "Daniel" }, { "name" : "Anthony" }, { "name" : "Jim"} , { "name" : "Diego" } ];
      var actual = getQueryResults("FOR u1 IN " + cn + " FOR u2 IN " + cn + " FILTER u2.name == u1.name SORT u1.id RETURN { \"name\" : u1.name }");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access with index (multiple filters)
////////////////////////////////////////////////////////////////////////////////

    testRefAccessIndex3 : function () {
      users.ensureHashIndex("name");

      var expected = [ { "name" : "John" }, { "name" : "Fred" }, { "name" : "Jacob" }, { "name" : "Ethan" }, { "name" : "Michael" }, { "name" : "Alexander" }, { "name" : "Daniel" }, { "name" : "Anthony" }, { "name" : "Jim"} , { "name" : "Diego" } ];
      var actual = getQueryResults("FOR u1 IN " + cn + " FOR u2 IN " + cn + " FILTER u2.name == u1.name && u1.name == u2.name SORT u1.id RETURN { \"name\" : u1.name }");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access with index (multiple filters)
////////////////////////////////////////////////////////////////////////////////

    testRefAccessIndex4 : function () {
      users.ensureHashIndex("name");

      var expected = [ { "name" : "John" }, { "name" : "Fred" }, { "name" : "Jacob" }, { "name" : "Ethan" }, { "name" : "Michael" }, { "name" : "Alexander" }, { "name" : "Daniel" }, { "name" : "Anthony" }, { "name" : "Jim"} , { "name" : "Diego" } ];
      var actual = getQueryResults("FOR u1 IN " + cn + " FOR u2 IN " + cn + " FILTER u2.name == u1.name && u1.name == u2.name && u2.name == u1.name && u1._id == u2._id SORT u1.id RETURN { \"name\" : u1.name }");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access with index and bind parameters
////////////////////////////////////////////////////////////////////////////////

    testRefAccessIndexBind1 : function () {
      users.ensureHashIndex("name");

      var expected = [ 28 ];
      var query = "FOR u IN " + cn + " FILTER u.@att == 'Diego' RETURN u.@what";
      var actual = getQueryResults(query, { "att" : "name", "what" : "age" });

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "ScatterNode", "RemoteNode", "IndexNode", "RemoteNode", "GatherNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query, { att: "name", what: "age" }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access with index and bind parameters
////////////////////////////////////////////////////////////////////////////////

    testRefAccessIndexBind2 : function () {
      var expected = [ "Diego" ];
      var query = "FOR u IN " + cn + " FILTER u.@att == 28 RETURN u.@what";
      var actual = getQueryResults(query, { "att" : "age", "what" : "name" });

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query, { att: "age", what: "name" }));
    }

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimizerRefTestSuite);

return jsunity.done();

