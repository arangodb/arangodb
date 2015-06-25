/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, AQL_EXECUTE, 
  AQL_QUERY_CACHE_PROPERTIES, AQL_QUERY_CACHE_INVALIDATE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, bind parameters
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
var db = require("org/arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryCacheTestSuite () {
  var cacheProperties;
  var c1, c2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      cacheProperties = AQL_QUERY_CACHE_PROPERTIES();
      AQL_QUERY_CACHE_INVALIDATE();

      db._drop("UnitTestsAhuacatlQueryCache1");
      db._drop("UnitTestsAhuacatlQueryCache2");

      c1 = db._create("UnitTestsAhuacatlQueryCache1");
      c2 = db._create("UnitTestsAhuacatlQueryCache2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsAhuacatlQueryCache1");
      db._drop("UnitTestsAhuacatlQueryCache2");

      c1 = null;
      c2 = null;

      AQL_QUERY_CACHE_PROPERTIES(cacheProperties);
      AQL_QUERY_CACHE_INVALIDATE();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test setting modes
////////////////////////////////////////////////////////////////////////////////

    testModes : function () {
      var result;

      result = AQL_QUERY_CACHE_PROPERTIES({ mode: "off" });
      assertEqual("off", result.mode);
      result = AQL_QUERY_CACHE_PROPERTIES();
      assertEqual("off", result.mode);

      result = AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      assertEqual("on", result.mode);
      result = AQL_QUERY_CACHE_PROPERTIES();
      assertEqual("on", result.mode);

      result = AQL_QUERY_CACHE_PROPERTIES({ mode: "demand" });
      assertEqual("demand", result.mode);
      result = AQL_QUERY_CACHE_PROPERTIES();
      assertEqual("demand", result.mode);
    },

    testInvalidationAfterInsertSingle : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      c1.save({ value: 1 });

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1 ], result.json);

      c1.save({ value: 2 }); // this will invalidate cache

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2 ], result.json);
    }


  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryCacheTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
