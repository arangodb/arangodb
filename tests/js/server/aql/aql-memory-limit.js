/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, fail, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for memory limits
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlMemoryLimitTestSuite () {
  var errors = internal.errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test unlimited memory
////////////////////////////////////////////////////////////////////////////////
    
    testUnlimited : function () {
      var actual = AQL_EXECUTE("FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)").json;
      assertEqual(100000, actual.length);
      
      actual = AQL_EXECUTE("FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", null, { memoryLimit: 0 }).json;
      assertEqual(100000, actual.length);
    },

    testLimitedButValid : function () {
      var actual = AQL_EXECUTE("FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", null, { memoryLimit: 100 * 1000 * 1000 }).json;
      assertEqual(100000, actual.length);
      
      // should still be ok
      actual = AQL_EXECUTE("FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", null, { memoryLimit: 10 * 1000 * 1000 }).json;
      assertEqual(100000, actual.length);
      
      // should still be ok
      actual = AQL_EXECUTE("FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", null, { memoryLimit: 5 * 1000 * 1000 }).json;
      assertEqual(100000, actual.length);
      
      // should still be ok
      actual = AQL_EXECUTE("FOR i IN 1..10000 RETURN i", null, { memoryLimit: 1000 * 1000 }).json;
      assertEqual(10000, actual.length);
      
      // should still be ok
      actual = AQL_EXECUTE("FOR i IN 1..10000 RETURN i", null, { memoryLimit: 100 * 1000 + 4096 }).json;
      assertEqual(10000, actual.length);
    },

    testLimitedAndInvalid : function () {
      var queries = [
        [ "FOR i IN 1..100000 SORT CONCAT('foobarbaz', i) RETURN CONCAT('foobarbaz', i)", 200000 ],
        [ "FOR i IN 1..100000 SORT CONCAT('foobarbaz', i) RETURN CONCAT('foobarbaz', i)", 100000 ],
        [ "FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", 20000 ],
        [ "FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", 10000 ],
        [ "FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", 1000 ],
        [ "FOR i IN 1..100000 RETURN CONCAT('foobarbaz', i)", 100 ],
        [ "FOR i IN 1..10000 RETURN CONCAT('foobarbaz', i)", 10000 ],
        [ "FOR i IN 1..10000 RETURN CONCAT('foobarbaz', i)", 1000 ],
        [ "FOR i IN 1..10000 RETURN CONCAT('foobarbaz', i)", 100 ],
        [ "FOR i IN 1..10000 RETURN CONCAT('foobarbaz', i)", 10 ]
      ];

      queries.forEach(function(q) {
        try {
          AQL_EXECUTE(q[0], null, { memoryLimit: q[1] });
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
        }
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlMemoryLimitTestSuite);

return jsunity.done();

