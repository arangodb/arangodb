/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, fail */

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
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'query.max-runtime': "5.0"
  };
}
let jsunity = require('jsunity');
const errors = require('@arangodb').errors;
let db = require('internal').db;

function testSuite() {
  return {
    testDefaultOk : function() {
      let res = db._query("FOR i IN 1..3 RETURN SLEEP(0.1)").toArray();
      assertEqual(3, res.length);
    },

    testDefaultExceeding : function() {
      try {
        // should be killed
        db._query("FOR i IN 1..10 RETURN SLEEP(1)");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_KILLED.code, err.errorNum);
      }
    },

    testOverwrittenOk : function() {
      let res = db._query("FOR i IN 1..5 RETURN SLEEP(1)", {}, { maxRuntime: 30 }).toArray();
      assertEqual(5, res.length);
      
      res = db._query("FOR i IN 1..10 RETURN SLEEP(1)", {}, { maxRuntime: 30 }).toArray();
      assertEqual(10, res.length);
    },
    
    testOverwrittenExceeding : function() {
      try {
        // should be killed
        db._query("FOR i IN 1..10 RETURN SLEEP(1)", {}, { maxRuntime: 1 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_KILLED.code, err.errorNum);
      }
      
      try {
        // should be killed
        db._query("FOR i IN 1..5 RETURN SLEEP(0.1)", {}, { maxRuntime: 0.2 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_KILLED.code, err.errorNum);
      }
    },

    
  };
}

jsunity.run(testSuite);
return jsunity.done();
