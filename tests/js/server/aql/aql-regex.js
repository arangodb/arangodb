/*jshint globalstrict:false, strict:false, maxlen:5000 */
/*global assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, functions
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
var db = internal.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlRegexTestSuite () {
  var c;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      db._drop("UnitTestsAhuacatlRegex");
      c = db._create("UnitTestsAhuacatlRegex");
      
      let docs = [];
      for (var i = 0; i < 1000; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop("UnitTestsAhuacatlRegex");
      c = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test regex matching
////////////////////////////////////////////////////////////////////////////////
    
    testRegexMatch : function () {
      var values = [
        [ '^test$', 0 ],
        [ '^test\\d+$', 1000 ],
        [ '^test1$', 1 ],
        [ '^test1', 111 ],
        [ '^test1*', 1000 ],
        [ '^test1+', 111 ],
        [ '^test1..$', 100 ],
        [ '^test1.$', 10 ],
        [ '^test11.', 10 ],
        [ 'test', 1000 ],
        [ 'test123', 1 ],
        [ 'test12', 11 ],
        [ '111', 1 ],
        [ '111$', 1 ],
        [ '11$', 10 ],
        [ '1$', 100 ]
      ];

      values.forEach(function(v) {
        // test match
        var query = "FOR doc IN @@collection FILTER doc._key =~ @re RETURN doc._key";
        var result = AQL_EXECUTE(query, { "@collection": c.name(), re: v[0] }).json;
        assertEqual(v[1], result.length);
        
        // test non-match
        query = "FOR doc IN @@collection FILTER doc._key !~ @re RETURN doc._key";
        result = AQL_EXECUTE(query, { "@collection": c.name(), re: v[0] }).json;
        assertEqual(1000 - v[1], result.length);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlRegexTestSuite);

return jsunity.done();

