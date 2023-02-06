/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
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

var jsunity = require('jsunity');
var internal = require('internal');
var arangodb = require('@arangodb');
var db = arangodb.db;

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function aqlKillSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  
  return {

    setUpAll: function () {
      db._drop(cn);
      db._create(cn, { numberOfShards: 3 });
    },

    tearDownAll: function () {
      db._drop(cn);
    },
    
    testAbortReadQuery: function () {
      let result = arango.POST_RAW("/_api/cursor", {
        query: "FOR i IN 1..10000000 RETURN SLEEP(1)"
      }, { 
        "x-arango-async" : "store"
      });

      let jobId = result.headers["x-arango-async-id"];

      let queryId = 0;
      let tries = 0;
      while (++tries < 30) {
        let queries = require("@arangodb/aql/queries").current();
        queries.filter(function(data) {
          if (data.query.indexOf("SLEEP(1)") !== -1) {
            queryId = data.id;
          }
        });
        if (queryId > 0) {
          break;
        }
        
        require("internal").wait(1, false);
      }

      assertTrue(queryId > 0);

      result = arango.DELETE("/_api/query/" + queryId);
      assertEqual(result.code, 200);

      tries = 0;
      while (++tries < 30) {
        result = arango.PUT_RAW("/_api/job/" + jobId, {});
        if (result.code === 410) {
          break;
        }
        require("internal").wait(1, false);
      }
      assertEqual(410, result.code);
    },

    testAbortWriteQuery: function () {
      let result = arango.POST_RAW("/_api/cursor", {
        query: "FOR i IN 1..10000000 INSERT {} INTO " + cn
      }, { 
        "x-arango-async" : "store"
      });

      let jobId = result.headers["x-arango-async-id"];

      let queryId = 0;
      let tries = 0;
      while (++tries < 30) {
        let queries = require("@arangodb/aql/queries").current();
        queries.filter(function(data) {
          if (data.query.indexOf(cn) !== -1) {
            queryId = data.id;
          }
        });
        if (queryId > 0) {
          break;
        }
        
        require("internal").wait(1, false);
      }

      assertTrue(queryId > 0);

      result = arango.DELETE("/_api/query/" + queryId);
      assertEqual(result.code, 200);

      tries = 0;
      while (++tries < 30) {
        result = arango.PUT_RAW("/_api/job/" + jobId, {});
        if (result.code === 410) {
          break;
        }
        require("internal").wait(1, false);
      }
      assertEqual(410, result.code);
    },

    // test killing the query via the Job API
    testCancelWriteQuery: function () {
      let result = arango.POST_RAW("/_api/cursor", {
        query: "FOR i IN 1..10000000 INSERT {} INTO " + cn
      }, { 
        "x-arango-async" : "store"
      });

      let jobId = result.headers["x-arango-async-id"];

      let queryId = 0;
      let tries = 0;
      while (++tries < 30) {
        let queries = require("@arangodb/aql/queries").current();
        queries.filter(function(data) {
          if (data.query.indexOf(cn) !== -1) {
            queryId = data.id;
          }
        });
        if (queryId > 0) {
          break;
        }
        
        require("internal").wait(1, false);
      }

      assertTrue(queryId > 0);

      // cancel the async job

      result = arango.PUT_RAW("/_api/job/" + jobId + "/cancel", {});
      assertEqual(result.code, 200);

      // make sure the query is no longer in the list of running queries
      tries = 0;
      while (++tries < 30) {
        let queries = require("@arangodb/aql/queries").current();
        let stillThere = false;
        queries.filter(function(data) {
          if (data.id === queryId) {
            stillThere = true;
          }
        });
        if (!stillThere) {
          break;
        }
        require("internal").wait(1, false);
      }
      assertTrue(tries < 30);
    },
  };
}

jsunity.run(aqlKillSuite);

return jsunity.done();
