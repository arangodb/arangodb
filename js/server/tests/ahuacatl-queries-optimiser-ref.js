////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, reference optimiser
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
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimiserRefTestSuite () {
  var users = null;
  var cn = "UnitTestsAhuacatlOptimiserRef";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      users = internal.db._create(cn);
      users.save({ "id" : 100, "name" : "John", "age" : 37, "active" : true, "gender" : "m" });
      users.save({ "id" : 101, "name" : "Fred", "age" : 36, "active" : true, "gender" : "m" });
      users.save({ "id" : 102, "name" : "Jacob", "age" : 35, "active" : false, "gender" : "m" });
      users.save({ "id" : 103, "name" : "Ethan", "age" : 34, "active" : false, "gender" : "m" });
      users.save({ "id" : 104, "name" : "Michael", "age" : 33, "active" : true, "gender" : "m" });
      users.save({ "id" : 105, "name" : "Alexander", "age" : 32, "active" : true, "gender" : "m" });
      users.save({ "id" : 106, "name" : "Daniel", "age" : 31, "active" : true, "gender" : "m" });
      users.save({ "id" : 107, "name" : "Anthony", "age" : 30, "active" : true, "gender" : "m" });
      users.save({ "id" : 108, "name" : "Jim", "age" : 29, "active" : true, "gender" : "m" });
      users.save({ "id" : 109, "name" : "Diego", "age" : 28, "active" : true, "gender" : "m" });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
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
    }
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimiserRefTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
