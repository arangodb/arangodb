/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require('internal').db;

function optimizerOperatorsTestSuite () {
  const testCombinations = function(queries, bind) {
    queries.forEach(function(query) {
      bind.forEach(function(bind) {
        let result = db._query(query, { data: bind[0] }).toArray()[0];
        assertEqual(bind[1], result, { query, bind });
      });
    });
  };

  return {

    testOrLhsDynamicRhsFalse : function() {
      const queries = [ 
        "RETURN @data.value || 0",
        "RETURN NOOPT(@data.value || 0)"
      ];

      const bind = [
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
      const queries = [ 
        "RETURN @data.value || 1",
        "RETURN NOOPT(@data.value || 1)"
      ];

      const bind = [
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
      const queries = [ 
        "RETURN 0 || @data.value",
        "RETURN NOOPT(0 || @data.value)"
      ];

      const bind = [
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
      const queries = [ 
        "RETURN 1 || @data.value",
        "RETURN NOOPT(1 || @data.value)"
      ];

      const bind = [
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
      const queries = [ 
        "RETURN @data.value && 0",
        "RETURN NOOPT(@data.value && 0)"
      ];

      const bind = [
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
      const queries = [ 
        "RETURN @data.value && 1",
        "RETURN NOOPT(@data.value && 1)"
      ];

      const bind = [
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
      const queries = [ 
        "RETURN !@data.value",
        "RETURN NOOPT(!@data.value)"
      ];

      const bind = [
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
