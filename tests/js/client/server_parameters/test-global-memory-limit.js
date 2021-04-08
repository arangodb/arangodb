/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertMatch, fail, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for memory limit
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'query.global-memory-limit': "6000000",
    'query.memory-limit': "5000000"
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const cn = "UnitTestsCollection";
const db = require('internal').db;

function testSuite() {
  let getMetric = function(name) {
    let res = arango.GET_RAW("/_admin/metrics/v2").body.toString();
    let re = new RegExp("^" + name + "\\{");
    let matches = res.split('\n').filter((line) => !line.match(/^#/)).filter((line) => line.match(re));
    if (!matches.length) {
      throw "Metric " + name + " not found";
    }
    return Number(matches[0].replace(/^.*?\} (\d+)$/, '$1'));
  };

  return {
    testQueryBelowGlobalLimit: function() {
      let result = db._query("FOR i IN 1..1000 RETURN i").toArray();
      assertEqual(1000, result.length);
    },
    
    testQueryAboveGlobalLimit: function() {
      let tasks = require("@arangodb/tasks");
      // start a background query that will allocate some memory and then goes to sleep
      let id = tasks.register({ 
        id: "background-query",
        command: function() { require("@arangodb").db._query("LET testi = (FOR i IN 1..100000 RETURN CONCAT('testmann-der-fuxx', i)) LET s = SLEEP(9000) RETURN { s, testi }"); }
      });
      // wait until this query has started
      let queries = require("@arangodb/aql/queries");
      let current = [];
      let tries = 0;
      while (tries++ < 60) {
        current = queries.current().filter((q) => q.query.match(/testmann-der-fuxx/));
        if (current.length > 0) {
          break;
        }
        require("internal").sleep(0.5);
      }
      assertEqual(1, current.length);
      // other query has started
      // now give it a second to make sure it has allocated _some_ memory
      require("internal").sleep(1.0);

      const previousValue = getMetric("arangodb_aql_global_query_memory_limit_reached");
      try {
        // we expect this query here to violate the global memory limit, because some memory is already
        // allocated by the other, sleeping query
        db._query("LET testi = (FOR i IN 1..10000 FOR j IN 1..100 RETURN CONCAT('testmann-der-fuxxx', i, j)) RETURN testi");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
        assertMatch(/global/, err.errorMessage);
      } finally {
        // kill the other long-running query
        queries.kill(current[0].id);
      }
      
      const currentValue = getMetric("arangodb_aql_global_query_memory_limit_reached");
      assertTrue(currentValue > previousValue);
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
