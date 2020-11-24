/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertFalse, assertEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief queries for all databases tests
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
let db = arangodb.db;
let queries = require("@arangodb/aql/queries");

function queriesAllDatabasesSuite () {
  'use strict';
  const prefix = 'UnitTestsTransaction';
  const asyncHeaders = { "x-arango-async": "store" };
      
  return {

    setUpAll: function () {
      for (let i = 0; i < 5; ++i) {
        db._createDatabase(prefix + i);
      }
    },

    setUp: function () {
      queries.clearSlow({ all: true });
    },

    tearDownAll: function () {
      for (let i = 0; i < 5; ++i) {
        db._dropDatabase(prefix + i);
      }
    },

    tearDown: function () {
      queries.clearSlow({ all: true });
    },

    testCurrentQueriesAllDatabases: function () {
      let ids = [];
      for (let i = 0; i < 5; ++i) {
        let res = arango.POST_RAW("/_db/" + prefix + i + "/_api/cursor", {
          query: "RETURN SLEEP(300)"
        }, asyncHeaders);

        assertFalse(res.error);
        assertEqual(202, res.code);
        ids.push(res.headers['x-arango-async-id']);
      }

      try {
        let found;
        let tries = 0;
        while (++tries < 60) {
          // we are in the system database here, so we don't expect
          // any results to come out here
          assertEqual(0, queries.current({ all: false }).filter((q) => {
            q.query.match(/^RETURN SLEEP/);
          }).length);
          
          // now retrieve the global list of queries
          found = {};
          queries.current({ all: true }).filter((q) => q.query.match(/^RETURN SLEEP/) ).forEach((q) => {
            found[q.database] = true;
          });

          if (Object.keys(found).length === 5) {
            break;
          }
          internal.sleep(0.5);
        }
        assertEqual(5, Object.keys(found).length, found);
      } finally {
        // finally kill all pending queries
        ids.forEach((id) => {
          let res = arango.PUT("/_api/job/" + encodeURIComponent(id) + "/cancel", {});
          assertTrue(res.result);
        });
      }
        
      // wait for cancelation to kick in
      let tries = 0;
      let found;
      while (++tries < 60) {
        found = queries.current({ all: true }).filter((q) => q.query.match(/^RETURN SLEEP/) ).length;
        if (found === 0) {
          break;
        }
        internal.sleep(0.5);
      }
      assertEqual(0, found);
    },
    
    testSlowQueriesAllDatabases: function () {
      for (let i = 0; i < 5; ++i) {
        // clear list of slow queries
        let res = arango.DELETE("/_db/" + prefix + i + "/_api/query/slow");
        assertFalse(res.error);
        assertEqual(200, res.code);

        // set slow query thresholds to 3 seconds and start queries
        res = arango.PUT("/_db/" + prefix + i + "/_api/query/properties", {
          slowQueryThreshold: 3 
        });
        assertEqual(3, res.slowQueryThreshold);
        res = arango.POST_RAW("/_db/" + prefix + i + "/_api/cursor", {
          query: "RETURN SLEEP(4)"
        }, asyncHeaders);

        assertFalse(res.error);
        assertEqual(202, res.code);
      }

      let found;
      let tries = 0;
      while (++tries < 60) {
        // we are in the system database here, so we don't expect
        // any results to come out here
        assertEqual(0, queries.slow({ all: false }).filter((q) => {
          q.query.match(/^RETURN SLEEP/);
        }).length);
        
        // now retrieve the global list of queries
        found = {};
        queries.slow(true).filter((q) => q.query.match(/^RETURN SLEEP/) ).forEach((q) => {
          found[q.database] = true;
        });

        if (Object.keys(found).length === 5) {
          break;
        }
        internal.sleep(0.5);
      }
      assertEqual(5, Object.keys(found).length, found);
    },
    
    testClearSlowQueriesAllDatabases: function () {
      for (let i = 0; i < 5; ++i) {
        // clear list of slow queries
        let res = arango.DELETE("/_db/" + prefix + i + "/_api/query/slow");
        assertFalse(res.error);
        assertEqual(200, res.code);

        // set slow query thresholds to 3 seconds and start queries
        res = arango.PUT("/_db/" + prefix + i + "/_api/query/properties", {
          slowQueryThreshold: 3 
        });
        assertEqual(3, res.slowQueryThreshold);
        res = arango.POST_RAW("/_db/" + prefix + i + "/_api/cursor", {
          query: "RETURN SLEEP(4)"
        }, asyncHeaders);

        assertFalse(res.error);
        assertEqual(202, res.code);
      }

      let found;
      let tries = 0;
      while (++tries < 60) {
        // we are in the system database here, so we don't expect
        // any results to come out here
        assertEqual(0, queries.slow({ all: false }).filter((q) => {
          q.query.match(/^RETURN SLEEP/);
        }).length);
        
        // now retrieve the global list of queries
        found = {};
        queries.slow({ all: true }).filter((q) => q.query.match(/^RETURN SLEEP/) ).forEach((q) => {
          found[q.database] = true;
        });

        if (Object.keys(found).length === 5) {
          break;
        }
        internal.sleep(0.5);
      }
      assertEqual(5, Object.keys(found).length, found);
      
      for (let i = 0; i < 5; ++i) {
        // set slow query thresholds back to 10 seconds
        let res = arango.PUT("/_db/" + prefix + i + "/_api/query/properties", {
          slowQueryThreshold: 10 
        });
        assertEqual(10, res.slowQueryThreshold);
      }

      // call clearSlow in the system database. this should have no effect
      let res = queries.clearSlow({ all: false });
      assertFalse(res.error);
      assertEqual(200, res.code);
          
      assertEqual(5, queries.slow({ all: true }).filter((q) => q.query.match(/^RETURN SLEEP/) ).length);
     
      // call it globally. this should work
      res = queries.clearSlow({ all: true });
      assertEqual(0, queries.slow({ all: true }).filter((q) => q.query.match(/^RETURN SLEEP/) ).length);
    },
    
    testKillQueriesAllDatabases: function () {
      for (let i = 0; i < 5; ++i) {
        let res = arango.POST_RAW("/_db/" + prefix + i + "/_api/cursor", {
          query: "RETURN SLEEP(300)"
        }, asyncHeaders);

        assertFalse(res.error);
        assertEqual(202, res.code);
      }

      let found;
      let tries = 0;
      while (++tries < 60) {
        // now retrieve the global list of queries
        found = queries.current({ all: true }).filter((q) => q.query.match(/^RETURN SLEEP/) );
        if (found.length === 5) {
          break;
        }
        internal.sleep(0.5);
      }
      assertEqual(5, found.length);
      
      found.forEach((q) => {
        let res = queries.kill({ id: q.id, all: true });
        assertFalse(res.error);
        assertEqual(200, res.code);
      });
      
      // wait for killing to have finished
      found = 0;
      while (++tries < 60) {
        found = queries.current({ all: true }).filter((q) => q.query.match(/^RETURN SLEEP/) ).length;
        if (found === 0) {
          break;
        }
        internal.sleep(0.5);
      }
      assertEqual(0, found);
    },
  };
}

jsunity.run(queriesAllDatabasesSuite);

return jsunity.done();
