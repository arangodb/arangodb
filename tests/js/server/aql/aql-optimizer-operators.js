/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for static operator optimizations
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

function optimizerOperatorsTestSuite () {
  var testCombinations = function(queries, bind) {
    queries.forEach(function(query) {
      bind.forEach(function(bind) {
        var result = AQL_EXECUTE(query, { data: bind[0] }).json[0];
        assertEqual(bind[1], result, { query, bind });
      });
    });
  };

  return {

    testOrLhsDynamicRhsFalse : function() {
      var queries = [ 
        "RETURN @data.value || 0",
        "RETURN NOOPT(@data.value || 0)"
      ];

      var bind = [
        [ {}, 0 ],
        [ { val: 1 }, 0 ],
        [ { value: null }, 0 ],
        [ { value: false }, 0 ],
        [ { value: true }, true ],
        [ { value: 23 }, 23 ],
        [ { value: true, something: "else" }, true ],
        [ { value: 2, something: "else" }, 2 ]
      ];

      testCombinations(queries, bind);
    },
    
    testOrLhsDynamicRhsTrue : function() {
      var queries = [ 
        "RETURN @data.value || 1",
        "RETURN NOOPT(@data.value || 1)"
      ];

      var bind = [
        [ {}, 1 ],
        [ { val: 1 }, 1 ],
        [ { value: null }, 1 ],
        [ { value: false }, 1 ],
        [ { value: true }, true ],
        [ { value: 23 }, 23 ],
        [ { value: true, something: "else" }, true ],
        [ { value: 2, something: "else" }, 2 ]
      ];

      testCombinations(queries, bind);
    },
    
    testOrRhsDynamicLhsFalse : function() {
      var queries = [ 
        "RETURN 0 || @data.value",
        "RETURN NOOPT(0 || @data.value)"
      ];

      var bind = [
        [ {}, null ],
        [ { val: 1 }, null ],
        [ { value: null }, null ],
        [ { value: false }, false ],
        [ { value: true }, true ],
        [ { value: 23 }, 23 ],
        [ { value: true, something: "else" }, true ],
        [ { value: 2, something: "else" }, 2 ]
      ];

      testCombinations(queries, bind);
    },
    
    testOrRhsDynamicLhsTrue : function() {
      var queries = [ 
        "RETURN 1 || @data.value",
        "RETURN NOOPT(1 || @data.value)"
      ];

      var bind = [
        [ {}, 1 ],
        [ { val: 1 }, 1 ],
        [ { value: null }, 1 ],
        [ { value: false }, 1 ],
        [ { value: true }, 1 ],
        [ { value: 23 }, 1 ],
        [ { value: true, something: "else" }, 1 ],
        [ { value: 2, something: "else" }, 1 ]
      ];

      testCombinations(queries, bind);
    },

    testAndLhsDynamicRhsFalse : function() {
      var queries = [ 
        "RETURN @data.value && 0",
        "RETURN NOOPT(@data.value && 0)"
      ];

      var bind = [
        [ {}, null ],
        [ { val: 1 }, null ],
        [ { value: false }, false ],
        [ { value: true }, 0 ],
        [ { value: 23 }, 0 ],
        [ { value: true, something: "else" }, 0 ],
        [ { value: 2, something: "else" }, 0 ]
      ];

      testCombinations(queries, bind);
    },

    testAndLhsDynamicRhsTrue : function() {
      var queries = [ 
        "RETURN @data.value && 1",
        "RETURN NOOPT(@data.value && 1)"
      ];

      var bind = [
        [ {}, null ],
        [ { val: 1 }, null ],
        [ { value: false }, false ],
        [ { value: true }, 1 ],
        [ { value: 23 }, 1 ],
        [ { value: true, something: "else" }, 1 ],
        [ { value: 2, something: "else" }, 1 ]
      ];

      testCombinations(queries, bind);
    },

    testNot : function() {
      var queries = [ 
        "RETURN !@data.value",
        "RETURN NOOPT(!@data.value)"
      ];

      var bind = [
        [ {}, true ],
        [ { val: 1 }, true ],
        [ { value: null }, true ],
        [ { value: false }, true ],
        [ { value: true }, false ],
        [ { value: 23 }, false ],
        [ { value: true, something: "else" }, false ],
        [ { value: 0, something: "else" }, true ],
        [ { value: 2, something: "else" }, false ]
      ];

      testCombinations(queries, bind);
    }
  };
}

jsunity.run(optimizerOperatorsTestSuite);

return jsunity.done();

