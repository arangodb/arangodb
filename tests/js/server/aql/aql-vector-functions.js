/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertNull, assertNotNull, fail */
////////////////////////////////////////////////////////////////////////////////
/// @brief tests for vector functions
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
/// @author Copyright 2022, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const db = require("@arangodb").db;
const errors = require("internal").errors;
const jsunity = require("jsunity");
const { deriveTestSuite } = require('@arangodb/test-helper');
    
const buildArray = (n) => {
  let result = [];
  for (let i = 0; i < n; ++i) {
    result.push(i);
  }
  return result;
};

function BaseTestConfig (f) {
  'use strict';

  const fn = f;

  return {
    testInvalidNumberOfArguments : function() {
      for (let i = 0; i < 10; ++i) { 
        if (i === 2) { 
          continue;
        }
        let values = []; 
        for (let j = 0; j < i; ++j) { 
          values.push('[]'); 
        } 
        const q = `RETURN ${fn}(${values.join(', ')})`; 
        try { 
          db._query(q); 
          fail();
        } catch (err) { 
          assertEqual(err.errorNum, errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code);
        } 
      }
    },

    testInvalidInputTypes : function() {
      const vals = [null, false, true, -1, 1, 9999, '', '1', '0', '-1', 'foo', {}];
      const q = `RETURN ${fn}(@val1, @val2)`;
      vals.forEach((val1) => {
        vals.forEach((val2) => {
          let res = db._query(q, { val1, val2 });
          assertNull(res.toArray()[0]);
          assertEqual(1, res.getExtra().warnings.length);
        });
      });
    },

    testInvalidNumberOfArrayMembers : function() {
      const vals = [0, 1, 2, 3];
      const q = `RETURN ${fn}(@val1, @val2)`; 
      vals.forEach((val1) => { 
        vals.forEach((val2) => { 
          if (val1 === val2 && val1 !== 0) {
            return;
          }
          let res = db._query(q, { val1: buildArray(val1), val2: buildArray(val2) }); 
          assertNull(res.toArray()[0]);
          assertEqual(1, res.getExtra().warnings.length);
        }); 
      }); 
    },

    testNonNumericArrayMembers : function() {
      const vals = [null, false, true, '', '1', '0', '-1', 'foo', {}]; 
      const q = `RETURN ${fn}(@val1, @val2)`; 
      vals.forEach((val1) => { 
        vals.forEach((val2) => { 
          let res = db._query(q, { val1: [val1], val2: [val2] }); 
          assertNull(res.toArray()[0]);
          assertEqual(1, res.getExtra().warnings.length);
        }); 
      }); 
    },

    testWithLargeArrays : function() {
      const vals = [ 100, 1000, 10000, 100000, 1000000, 10000000 ];
      const q = `RETURN ${fn}(@val, @val)`; 
      vals.forEach((n) => { 
        let res = db._query(q, { val: buildArray(n) }); 
        assertNotNull(res.toArray()[0]);
        assertEqual(0, res.getExtra().warnings.length);
      });
    },

    testInvalidMemberInArray : function() {
      const vals = [
        [["foo"]],
        [["foo", "bar"]],
        [[true]],
        [[true, false]],
        [[1, 2], [1, 2], [1, 2, 3]],  
        [[1, 2], [1, 2], [1, "piff"]],
        [[1, 2], [1, 2], {}],      
        [[1, 2], [1, 2], {trust: "me", better: "not"}],        
      ];
      vals.forEach((val) => {
        const q = `RETURN ${fn}(@val, [1, 2])`;
        let res = db._query(q, { val }); 
        assertNull(res.toArray()[0]);
        assertEqual(1, res.getExtra().warnings.length);
      });

    },

  };
}

function cosineSimilaritySuite () {
  'use strict';

  let suite = {};
  deriveTestSuite(BaseTestConfig("COSINE_SIMILARITY"), suite, '_cosineSimilarity');
  return suite;
}

function l1DistanceSuite () {
  'use strict';

  let suite = {};
  deriveTestSuite(BaseTestConfig("L1_DISTANCE"), suite, '_l1Distance');
  return suite;
}

function l2DistanceSuite () {
  'use strict';

  let suite = {};
  deriveTestSuite(BaseTestConfig("L2_DISTANCE"), suite, '_l2Distance');
  return suite;
}

jsunity.run(cosineSimilaritySuite);
jsunity.run(l1DistanceSuite);
jsunity.run(l2DistanceSuite);

return jsunity.done();
