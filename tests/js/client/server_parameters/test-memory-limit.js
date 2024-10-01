/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertMatch, assertNotMatch, fail, arango */

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
    'query.global-memory-limit': "0",
    'query.memory-limit': "5000000",
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const cn = "UnitTestsCollection";
const db = require('internal').db;
const getMetric = require('@arangodb/test-helper').getMetricSingle;

function testSuite() {
  return {
    testQueryBelowLimit: function() {
      let result = db._query("FOR i IN 1..1000 RETURN i").toArray();
      assertEqual(1000, result.length);
    },
    
    testQueryAboveLimit: function() {
      const previousValue = getMetric("arangodb_aql_local_query_memory_limit_reached_total");
      try {
        // we expect this query here to violate the memory limit
        db._query("LET testi = (FOR i IN 1..10000 FOR j IN 1..100 RETURN CONCAT('testmann-der-fuxxx', i, j)) RETURN testi");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
        assertNotMatch(/global/, err.errorMessage);
      }
      
      const currentValue = getMetric("arangodb_aql_local_query_memory_limit_reached_total");
      assertTrue(currentValue > previousValue);
    },
    
    testQueryAboveLimitButAllowed: function() {
      let crsr = db._query("LET testi = (FOR i IN 1..10000 FOR j IN 1..100 RETURN CONCAT('testmann-der-fuxxx', i, j)) RETURN testi", {}, {memoryLimit: 0});
      let result = crsr.toArray();
      assertEqual(1, result.length);
      assertEqual(10000 * 100, result[0].length);
      assertTrue(crsr.getExtra().stats.peakMemoryUsage > 5000000);
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
