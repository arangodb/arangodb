////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, PARSE function
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
var PARSE = internal.AQL_PARSE;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlParseTestSuite () {
  var errors = internal.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the error code from a result
////////////////////////////////////////////////////////////////////////////////

  function getErrorCode (fn) {
    try {
      fn();
    }
    catch (e) {
      return e.errorNum;
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection names from the result
////////////////////////////////////////////////////////////////////////////////

  function getCollections (result) {
    var collections = result.collections;

    assertTrue(result.parsed);
   
    collections.sort();
    return collections;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the bind parameter names from the result
////////////////////////////////////////////////////////////////////////////////

  function getParameters (result) {
    var parameters = result.parameters;

    assertTrue(result.parsed);
   
    parameters.sort();
    return parameters;
  }

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test empty query
////////////////////////////////////////////////////////////////////////////////

    testEmptyQuery : function () {
      assertEqual(errors.ERROR_QUERY_EMPTY.code, getErrorCode(function() { PARSE(""); } ));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test broken queries
////////////////////////////////////////////////////////////////////////////////

    testBrokenQueries : function () {
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE(" "); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("  "); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for "); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for u"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for u "); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for u in"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for u in "); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for u in ["); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for u in [1"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for u in [1]"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for u in [1] return"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for u in [1] return u;"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE(";"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("1"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for @u in users return 1"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("return"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("return "); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("return 1;"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("return 1 +"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("return 1 + 1 +"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("return (1"); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { PARSE("for f1 in x1"); } ));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind parameter names
////////////////////////////////////////////////////////////////////////////////

    testParameterNames : function () {
      assertEqual([ ], getParameters(PARSE("return 1")));
      assertEqual([ ], getParameters(PARSE("for u in [ 1, 2, 3] return 1")));
      assertEqual([ "u" ], getParameters(PARSE("for u in users return @u")));
      assertEqual([ "b" ], getParameters(PARSE("for a in b return @b")));
      assertEqual([ "b", "c" ], getParameters(PARSE("for a in @b return @c")));
      assertEqual([ "friends", "relations", "u", "users" ], getParameters(PARSE("for u in @users for f in @friends for r in @relations return @u")));
      assertEqual([ "friends", "relations", "u", "users" ], getParameters(PARSE("for r in @relations for f in @friends for u in @users return @u")));
      assertEqual([ "1", "hans", "r" ], getParameters(PARSE("for r in (for x in @hans return @1) return @r")));
      assertEqual([ "1", "2", "hans" ], getParameters(PARSE("for r in [ @1, @2 ] return @hans")));
      assertEqual([ "@users", "users" ], getParameters(PARSE("for r in @@users return @users")));
      assertEqual([ "@users" ], getParameters(PARSE("for r in @@users return @@users")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection names
////////////////////////////////////////////////////////////////////////////////

    testCollectionNames : function () {
      assertEqual([ ], getCollections(PARSE("return 1")));
      assertEqual([ ], getCollections(PARSE("for u in [ 1, 2, 3] return 1")));
      assertEqual([ "users" ], getCollections(PARSE("for u in users return u")));
      assertEqual([ "b" ], getCollections(PARSE("for a in b return b")));
      assertEqual([ "friends", "relations", "users" ], getCollections(PARSE("for u in users for f in friends for r in relations return u")));
      assertEqual([ "friends", "relations", "users" ], getCollections(PARSE("for r in relations for f in friends for u in users return u")));
      assertEqual([ "hans" ], getCollections(PARSE("for r in (for x in hans return 1) return r")));
      assertEqual([ "hans" ], getCollections(PARSE("for r in [ 1, 2 ] return hans")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind parameter names in comments
////////////////////////////////////////////////////////////////////////////////

    testComments : function () {
      assertEqual([ ], getParameters(PARSE("return /* @nada */ 1")));
      assertEqual([ ], getParameters(PARSE("return /* @@nada */ 1")));
      assertEqual([ ], getParameters(PARSE("/*   @nada   */ return /* @@nada */ /*@@nada*/ 1 /*@nada*/")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test too many collections
////////////////////////////////////////////////////////////////////////////////

    testTooManyCollections : function () {
      assertEqual(errors.ERROR_QUERY_TOO_MANY_COLLECTIONS.code, getErrorCode(function() { PARSE("for x1 in y1 for x2 in y2 for x3 in y3 for x4 in y4 for x5 in y5 for x6 in y6 for x7 in y7 for x8 in y8 for x9 in y9 for x10 in y10 for x11 in y11 for x12 in y12 for x13 in y13 for x14 in y14 for x15 in y15 for x16 in y16 for x17 in y17 for x18 in y18 for x19 in y19 for x20 in y20 for x21 in y21 for x22 in y22 for x23 in y23 for x24 in y24 for x25 in y25 for x26 in y26 for x27 in y27 for x28 in y28 for x29 in y29 for x30 in y30 for x31 in y31 for x32 in y32 for x33 in y33 return x1"); } ));
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlParseTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
