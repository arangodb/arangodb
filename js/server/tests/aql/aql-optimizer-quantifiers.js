/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for ANY|ALL|NONE
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
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerQuantifiersTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 10; ++i) {
        c.insert({ value: i % 5 });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },
    
    testAllEmpty : function () {
      var query = "[] ALL == '1'", result;
      
      result = AQL_EXECUTE("RETURN (" + query + ")").json[0];
      assertEqual(true, result);

      result = AQL_EXECUTE("RETURN NOOPT(" + query + ")").json[0];
      assertEqual(true, result);
      
      result = AQL_EXECUTE("RETURN V8(" + query + ")").json[0];
      assertEqual(true, result);
    },
    
    testAnyEmpty : function () {
      var query = "[] ANY == '1'", result;
      
      result = AQL_EXECUTE("RETURN (" + query + ")").json[0];
      assertEqual(false, result);

      result = AQL_EXECUTE("RETURN NOOPT(" + query + ")").json[0];
      assertEqual(false, result);
      
      result = AQL_EXECUTE("RETURN V8(" + query + ")").json[0];
      assertEqual(false, result);
    },

    testNoneEmpty : function () {
      var query = "[] NONE == '1'", result;
      
      result = AQL_EXECUTE("RETURN (" + query + ")").json[0];
      assertEqual(true, result);

      result = AQL_EXECUTE("RETURN NOOPT(" + query + ")").json[0];
      assertEqual(true, result);
      
      result = AQL_EXECUTE("RETURN V8(" + query + ")").json[0];
      assertEqual(true, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ALL IN
////////////////////////////////////////////////////////////////////////////////

    testAllIn : function () {
      var queries = [
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ALL IN [ doc.value ] SORT doc.value RETURN doc.value", [ ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ALL NOT IN [ doc.value ] SORT doc.value RETURN doc.value", [ 0, 0, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ALL == doc.value SORT doc.value RETURN doc.value", [ ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ALL != doc.value SORT doc.value RETURN doc.value", [ 0, 0, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ALL > doc.value SORT doc.value RETURN doc.value", [ 0, 0 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ALL >= doc.value SORT doc.value RETURN doc.value", [ 0, 0, 1, 1 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ALL < doc.value SORT doc.value RETURN doc.value", [ 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ALL <= doc.value SORT doc.value RETURN doc.value", [ 3, 3, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ALL IN [ doc.value ]) SORT doc.value RETURN doc.value", [ ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ALL NOT IN [ doc.value ]) SORT doc.value RETURN doc.value", [ 0, 0, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ALL == doc.value) SORT doc.value RETURN doc.value", [ ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ALL != doc.value) SORT doc.value RETURN doc.value", [ 0, 0, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ALL > doc.value) SORT doc.value RETURN doc.value", [ 0, 0 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ALL >= doc.value) SORT doc.value RETURN doc.value", [ 0, 0, 1, 1 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ALL < doc.value) SORT doc.value RETURN doc.value", [ 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ALL <= doc.value) SORT doc.value RETURN doc.value", [ 3, 3, 4, 4 ] ]
      ];

      queries.forEach(function(query) {
        var result = AQL_EXECUTE(query[0]).json;
        assertEqual(query[1], result);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ANY IN
////////////////////////////////////////////////////////////////////////////////

    testAnyIn : function () {
      var queries = [
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ANY IN [ doc.value ] SORT doc.value RETURN doc.value", [ 1, 1, 2, 2, 3, 3 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ANY NOT IN [ doc.value ] SORT doc.value RETURN doc.value", [ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ANY == doc.value SORT doc.value RETURN doc.value", [ 1, 1, 2, 2, 3, 3 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ANY != doc.value SORT doc.value RETURN doc.value", [ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ANY > doc.value SORT doc.value RETURN doc.value", [ 0, 0, 1, 1, 2, 2 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ANY >= doc.value SORT doc.value RETURN doc.value", [ 0, 0, 1, 1, 2, 2, 3, 3 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ANY < doc.value SORT doc.value RETURN doc.value", [ 2, 2, 3, 3, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] ANY <= doc.value SORT doc.value RETURN doc.value", [ 1, 1, 2, 2, 3, 3, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ANY IN [ doc.value ]) SORT doc.value RETURN doc.value", [ 1, 1, 2, 2, 3, 3 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ANY NOT IN [ doc.value ]) SORT doc.value RETURN doc.value", [ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ANY == doc.value) SORT doc.value RETURN doc.value", [ 1, 1, 2, 2, 3, 3 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ANY != doc.value) SORT doc.value RETURN doc.value", [ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ANY > doc.value) SORT doc.value RETURN doc.value", [ 0, 0, 1, 1, 2, 2 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ANY >= doc.value) SORT doc.value RETURN doc.value", [ 0, 0, 1, 1, 2, 2, 3, 3 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ANY < doc.value) SORT doc.value RETURN doc.value", [ 2, 2, 3, 3, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] ANY <= doc.value) SORT doc.value RETURN doc.value", [ 1, 1, 2, 2, 3, 3, 4, 4 ] ]
      ];

      queries.forEach(function(query) {
        var result = AQL_EXECUTE(query[0]).json;
        assertEqual(query[1], result);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test NONE IN
////////////////////////////////////////////////////////////////////////////////

    testNoneIn : function () {
      var queries = [
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] NONE IN [ doc.value ] SORT doc.value RETURN doc.value", [ 0, 0, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] NONE NOT IN [ doc.value ] SORT doc.value RETURN doc.value", [ ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] NONE == doc.value SORT doc.value RETURN doc.value", [ 0, 0, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] NONE != doc.value SORT doc.value RETURN doc.value", [ ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] NONE > doc.value SORT doc.value RETURN doc.value", [ 3, 3, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] NONE >= doc.value SORT doc.value RETURN doc.value", [ 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] NONE < doc.value SORT doc.value RETURN doc.value", [ 0, 0, 1, 1 ] ],
        [ "FOR doc IN " + c.name() + " FILTER [ 1, 2, 3 ] NONE <= doc.value SORT doc.value RETURN doc.value", [ 0, 0 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] NONE IN [ doc.value ]) SORT doc.value RETURN doc.value", [ 0, 0, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] NONE NOT IN [ doc.value ]) SORT doc.value RETURN doc.value", [ ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] NONE == doc.value) SORT doc.value RETURN doc.value", [ 0, 0, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] NONE != doc.value) SORT doc.value RETURN doc.value", [ ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] NONE > doc.value) SORT doc.value RETURN doc.value", [ 3, 3, 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] NONE >= doc.value) SORT doc.value RETURN doc.value", [ 4, 4 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] NONE < doc.value) SORT doc.value RETURN doc.value", [ 0, 0, 1, 1 ] ],
        [ "FOR doc IN " + c.name() + " FILTER V8([ 1, 2, 3 ] NONE <= doc.value) SORT doc.value RETURN doc.value", [ 0, 0 ] ]
      ];

      queries.forEach(function(query) {
        var result = AQL_EXECUTE(query[0]).json;
        assertEqual(query[1], result);
      });
    },

  };
}
jsunity.run(optimizerQuantifiersTestSuite);

return jsunity.done();

