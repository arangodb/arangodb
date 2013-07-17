////////////////////////////////////////////////////////////////////////////////
/// @brief tests for Ahuacatl, hash index queries
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

var internal = require("internal");
var jsunity = require("jsunity");
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var getQueryExplanation = helper.getQueryExplanation;
var errors = internal.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlHashTestSuite () {
  var hash;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop("UnitTestsAhuacatlHash");

      hash = internal.db._create("UnitTestsAhuacatlHash");

      for (var i = 1; i <= 5; ++i) {
        for (var j = 1; j <= 5; ++j) {
          hash.save({ "a" : i, "b": j, "c" : i });
        }
      }

      hash.ensureHashIndex("a", "b");
      hash.ensureHashIndex("c");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop("UnitTestsAhuacatlHash");
      hash = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the single field hash index with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle1 : function () {
      var query = "FOR v IN " + hash.name() + " FILTER v.c == 1 SORT v.b RETURN [ v.b ]";
      var expected = [ [ 1 ], [ 2 ], [ 3 ], [ 4 ], [ 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);
      
      var explain = getQueryExplanation(query);
      assertEqual("for", explain[0].type);
      assertEqual("index", explain[0].expression.extra.accessType);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the single field hash index with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle2 : function () {
      var query = "FOR v IN " + hash.name() + " FILTER 1 == v.c SORT v.b RETURN [ v.b ]";
      var expected = [ [ 1 ], [ 2 ], [ 3 ], [ 4 ], [ 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);
      
      var explain = getQueryExplanation(query);
      assertEqual("for", explain[0].type);
      assertEqual("index", explain[0].expression.extra.accessType);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first hash index field with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle3 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ], [ 3, 4 ], [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + hash.name() + " FILTER v.c == 4 SORT v.b RETURN [ v.b, v.c ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first hash index field with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle4 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ], [ 3, 4 ], [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + hash.name() + " FILTER 4 == v.c SORT v.b RETURN [ v.b, v.c ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple hash fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiAll1 : function () {
      for (var i = 1; i <= 5; ++i) {
        for (var j = 1; j <=5; ++j) {
          var query = "FOR v IN " + hash.name() + " FILTER v.a == @a && v.b == @b RETURN [ v.a, v.b ]";
          var expected = [ [ i, j ] ];
          var actual = getQueryResults(query, { "a": i, "b": j });

          assertEqual(expected, actual);
      
          var explain = getQueryExplanation(query, { "a": i, "b": j });
          assertEqual("for", explain[0].type);
          assertEqual("index", explain[0].expression.extra.accessType);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple hash fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiAll2 : function () {
      for (var i = 1; i <= 5; ++i) {
        for (var j = 1; j <=5; ++j) {
          var query = "FOR v IN " + hash.name() + " FILTER @a == v.a && @b == v.b RETURN [ v.a, v.b ]";
          var expected = [ [ i, j ] ];
          var actual = getQueryResults(query, { "a": i, "b": j });

          assertEqual(expected, actual);
          
          var explain = getQueryExplanation(query, { "a": i, "b": j });
          assertEqual("for", explain[0].type);
          assertEqual("index", explain[0].expression.extra.accessType);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test references with constants
////////////////////////////////////////////////////////////////////////////////

    testRefConst1 : function () {
      var query = "LET x = 4 FOR v IN " + hash.name() + " FILTER v.c == x SORT v.b RETURN [ v.b, v.c ]";
      var expected = [ [ 1, 4 ], [ 2, 4 ], [ 3, 4 ], [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);
          
      var explain = getQueryExplanation(query);
      assertEqual("let", explain[0].type);
      assertEqual("for", explain[1].type);
      assertEqual("index", explain[1].expression.extra.accessType);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test references with constants
////////////////////////////////////////////////////////////////////////////////

    testRefConst2 : function () {
      var expected = [ [ 3, 5 ] ];
      var actual = getQueryResults("LET x = 3 LET y = 5 FOR v IN " + hash.name() + " FILTER v.a == x && v.b == y RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefSingle1 : function () {
      for (var i = 1; i <= 5; ++i) {
        var expected = [ [ i, i ] ];
        var actual = getQueryResults("FOR v1 IN " + hash.name() + " FOR v2 IN " + hash.name() + " FILTER v1.c == @c && v1.b == 1 && v2.c == v1.c && v2.b == 1 RETURN [ v1.c, v2.c ]", { "c" : i });

        assertEqual(expected, actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefSingle2 : function () {
      for (var i = 1; i <= 5; ++i) {
        var expected = [ [ i, i ] ];
        var actual = getQueryResults("FOR v1 IN " + hash.name() + " FOR v2 IN " + hash.name() + " FILTER v1.c == @c && v1.b == 1 && v2.c == v1.c && v2.b == v1.b RETURN [ v1.c, v2.c ]", { "c" : i });

        assertEqual(expected, actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefSingle3 : function () {
      for (var i = 1; i <= 5; ++i) {
        var expected = [ [ i, i ] ];
        var actual = getQueryResults("FOR v1 IN " + hash.name() + " FOR v2 IN " + hash.name() + " FILTER @c == v1.c && 1 == v1.b && v1.c == v2.c && v1.b == v2.b RETURN [ v1.c, v2.c ]", { "c" : i });

        assertEqual(expected, actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMulti1 : function () {
      for (var i = 1; i <= 5; ++i) {
        var expected = [ [ i, 1 ] ];
        var actual = getQueryResults("FOR v1 IN " + hash.name() + " FOR v2 IN " + hash.name() + " FILTER v1.c == @a && v1.b == 1 && v2.c == v1.c && v2.b == v1.b RETURN [ v1.a, v1.b ]", { "a" : i });

        assertEqual(expected, actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMulti2 : function () {
      for (var i = 1; i <= 5; ++i) {
        var expected = [ [ i, 1 ] ];
        var actual = getQueryResults("FOR v1 IN " + hash.name() + " FOR v2 IN " + hash.name() + " FILTER @a == v1.c && 1 == v1.b && v1.c == v2.c && v1.b == v2.b RETURN [ v1.a, v1.b ]", { "a" : i });

        assertEqual(expected, actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefMulti3 : function () {
      var query = "FOR v1 IN " + hash.name() + " FILTER @a == v1.a && @b == v1.b RETURN [ v1.a, v1.b ]";
      var expected = [ [ 2, 3 ] ];
      var actual = getQueryResults(query, { "a": 2, "b": 3 });

      assertEqual(expected, actual);

      var explain = getQueryExplanation(query, { "a": 2, "b": 3 });
      assertEqual("for", explain[0].type);
      assertEqual("index", explain[0].expression.extra.accessType);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testRefFilterSame1 : function () {
      var query = "FOR a IN " + hash.name() + " FILTER a.a == a.a SORT a.a RETURN a.a";
      var expected = [ 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5 ];
      var actual = getQueryResults(query);
      
      assertEqual(expected, actual);

      var explain = getQueryExplanation(query);
      assertEqual("for", explain[0].type);
      assertEqual("all", explain[0].expression.extra.accessType);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testRefFilterSame2 : function () {
      var query = "FOR a IN " + hash.name() + " FILTER a.a == a.c SORT a.a RETURN a.a";
      var expected = [ 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5 ];
      var actual = getQueryResults(query);
      
      assertEqual(expected, actual);

      var explain = getQueryExplanation(query);
      assertEqual("for", explain[0].type);
      assertEqual("all", explain[0].expression.extra.accessType);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testRefNon1 : function () {
      var query = "FOR a IN " + hash.name() + " FILTER a.a == 1 RETURN a.a";
      var expected = [ 1, 1, 1, 1, 1 ]; 
      var actual = getQueryResults(query);
      
      assertEqual(expected, actual);

      var explain = getQueryExplanation(query);
      assertEqual("for", explain[0].type);
      assertEqual("all", explain[0].expression.extra.accessType);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testRefNon2 : function () {
      var query = "FOR a IN " + hash.name() + " FILTER a.d == a.a SORT a.a RETURN a.a";
      var expected = [ ];
      var actual = getQueryResults(query);
      
      assertEqual(expected, actual);

      var explain = getQueryExplanation(query);
      assertEqual("for", explain[0].type);
      assertEqual("all", explain[0].expression.extra.accessType);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testRefNon3 : function () {
      var query = "FOR a IN " + hash.name() + " FILTER a.d == 1 SORT a.a RETURN a.a";
      var expected = [ ];
      var actual = getQueryResults(query);
      
      assertEqual(expected, actual);

      var explain = getQueryExplanation(query);
      assertEqual("for", explain[0].type);
      assertEqual("all", explain[0].expression.extra.accessType);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlHashTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
