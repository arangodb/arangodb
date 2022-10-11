/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_PARSE, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for with collections
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
var db = require("org/arangodb").db;
var helper = require("org/arangodb/aql-helper");
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function queryWithCollectionsTestSuite () {
  var c1, c2;
  var errors = internal.errors;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection1");
      db._drop("UnitTestsCollection2");
      c1 = db._create("UnitTestsCollection1");
      c2 = db._create("UnitTestsCollection2");
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection2");
      db._drop("UnitTestsCollection1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with at invalid position
////////////////////////////////////////////////////////////////////////////////
    
    testWithInvalidPos1 : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "LET a = 1 WITH UnitTestsCollection1 RETURN 1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with at invalid position
////////////////////////////////////////////////////////////////////////////////

    testWithInvalidPos2 : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN 1..10 RETURN i WITH UnitTestsCollection1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with at invalid position
////////////////////////////////////////////////////////////////////////////////

    testWithInvalidPos3 : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN 1..10 WITH UnitTestsCollection1 RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with only
////////////////////////////////////////////////////////////////////////////////

    testWithOnlySingle : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "WITH UnitTestsCollection1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with only
////////////////////////////////////////////////////////////////////////////////

    testWithOnlyMulti : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "WITH UnitTestsCollection1, UnitTestsCollection2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with only
////////////////////////////////////////////////////////////////////////////////

    testWithOnlyMultiSpace : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "WITH UnitTestsCollection1 UnitTestsCollection2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with invalid value
////////////////////////////////////////////////////////////////////////////////

    testWithString : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "WITH 'UnitTestsCollection1' RETURN 1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with invalid value
////////////////////////////////////////////////////////////////////////////////

    testWithNumber : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "WITH 1 RETURN 1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with invalid value
////////////////////////////////////////////////////////////////////////////////

    testWithArray : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "WITH [ 'UnitTestsCollection1' ] RETURN 1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with invalid value
////////////////////////////////////////////////////////////////////////////////

    testWithObject : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "WITH { name : 'UnitTestsCollection1' } RETURN 1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with invalid value
////////////////////////////////////////////////////////////////////////////////

    testWithKeyword : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "WITH OUTBOUND RETURN 1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with one collection
////////////////////////////////////////////////////////////////////////////////

    testWithSingle : function () {
      var query = "WITH UnitTestsCollection1 FOR i IN 1..10 RETURN i";

      var result = AQL_EXECUTE(query).json; 
      assertEqual(10, result.length);
      
      var collections = AQL_PARSE(query).collections; 
      assertEqual([ "UnitTestsCollection1" ], collections);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with one collection
////////////////////////////////////////////////////////////////////////////////

    testWithSingleBind : function () {
      // data source bind parameter
      let query = "WITH @@col FOR i IN 1..10 RETURN i";

      let result = AQL_EXECUTE(query, { "@col" : "UnitTestsCollection1" }).json; 
      assertEqual(10, result.length);
      
      let collections = AQL_PARSE(query).collections; 
      assertEqual([ ], collections);

      // simple bind parameter
      query = "WITH @col FOR i IN 1..10 RETURN i";
      
      result = AQL_EXECUTE(query, { "col" : "UnitTestsCollection1" }).json; 
      assertEqual(10, result.length);
      
      collections = AQL_PARSE(query).collections; 
      assertEqual([ ], collections);
      
      // simple bind parameter - invalid types
      [ null, false, true, -1, 0, 1, 123, 94584.43, [], ["foo"], {} ].forEach(function(value) {
        assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_TYPE.code, query, { col: value });
      });
        
      assertQueryError(errors.ERROR_ARANGO_ILLEGAL_NAME.code, query, { col: "" });
      
      // simple bind parameter - valid type, invalid values
      [ " ", "NonExisting", "der fuxx" ].forEach(function(value) {
        assertQueryError(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, query, { col: value });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with one collection
////////////////////////////////////////////////////////////////////////////////

    testWithMissing : function () {
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, "WITH @@col FOR i IN 1..10 RETURN i");
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, "WITH @col FOR i IN 1..10 RETURN i");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithMulti : function () {
      var query = "WITH UnitTestsCollection1, UnitTestsCollection2 FOR i IN 1..10 RETURN i";

      var result = AQL_EXECUTE(query).json; 
      assertEqual(10, result.length);
      
      var collections = AQL_PARSE(query).collections; 
      assertEqual([ "UnitTestsCollection1", "UnitTestsCollection2" ], collections.sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithMultiSpace : function () {
      var query = "WITH UnitTestsCollection1 UnitTestsCollection2 FOR i IN 1..10 RETURN i";

      var result = AQL_EXECUTE(query).json; 
      assertEqual(10, result.length);
      
      var collections = AQL_PARSE(query).collections; 
      assertEqual([ "UnitTestsCollection1", "UnitTestsCollection2" ], collections.sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithReturn : function () {
      var query = "WITH `return` RETURN 1";

      var collections = AQL_PARSE(query).collections; 
      assertEqual([ "return" ], collections.sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithMultiReserved : function () {
      var query = "WITH `for` `with` `replace` `insert` `outbound` RETURN 1";

      var collections = AQL_PARSE(query).collections; 
      assertEqual([ "for", "insert", "outbound", "replace", "with" ], collections.sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithMultiBind : function () {
      var query = "WITH @@col1, @@col2 FOR i IN 1..10 RETURN i";

      var result = AQL_EXECUTE(query, { "@col1" : "UnitTestsCollection1", "@col2" : "UnitTestsCollection2" }).json; 
      assertEqual(10, result.length);
      
      var collections = AQL_PARSE(query).collections; 
      assertEqual([ ], collections.sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithMultiBindSpace : function () {
      var query = "WITH @@col1 @@col2 FOR i IN 1..10 RETURN i";

      var result = AQL_EXECUTE(query, { "@col1" : "UnitTestsCollection1", "@col2" : "UnitTestsCollection2" }).json; 
      assertEqual(10, result.length);
      
      var collections = AQL_PARSE(query).collections; 
      assertEqual([ ], collections.sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithMultiMixed : function () {
      var query = "WITH UnitTestsCollection1, UnitTestsCollection2 FOR i IN _users RETURN i";

      var collections = AQL_PARSE(query).collections; 
      assertEqual([ "UnitTestsCollection1", "UnitTestsCollection2", "_users" ], collections.sort());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithMultiRedundant : function () {
      var query = "WITH UnitTestsCollection1, UnitTestsCollection1 FOR i IN 1..10 RETURN i";
      
      var result = AQL_EXECUTE(query).json; 
      assertEqual(10, result.length);

      var collections = AQL_PARSE(query).collections; 
      assertEqual([ "UnitTestsCollection1" ], collections);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithMultiRedundantBind : function () {
      var query = "WITH @@col1, @@col1 FOR i IN 1..10 RETURN i";
      
      var result = AQL_EXECUTE(query, { "@col1" : "UnitTestsCollection1" }).json; 
      assertEqual(10, result.length);

      var collections = AQL_PARSE(query).collections; 
      assertEqual([ ], collections);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithMultiRedundantMixed : function () {
      var query = "WITH UnitTestsCollection1, UnitTestsCollection1 FOR i IN _users RETURN i";

      var collections = AQL_PARSE(query).collections; 
      assertEqual([ "UnitTestsCollection1", "_users" ], collections);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithMultiRedundantMixedBind : function () {
      var query = "WITH @@col1, @@col2 FOR i IN @@col2 RETURN i";

      var collections = AQL_PARSE(query).collections; 
      assertEqual([ ], collections);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testWithMultiRedundantMixedBind2 : function () {
      var query = "WITH @@col1, _users FOR i IN @@col2 RETURN i";

      var collections = AQL_PARSE(query).collections; 
      assertEqual([ "_users" ], collections);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(queryWithCollectionsTestSuite);

return jsunity.done();
