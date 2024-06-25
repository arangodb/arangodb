/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, arango */

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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {assertTrue, assertEqual} = jsunity.jsUnity.assertions;
const internal = require('internal');
const arangodb = require('@arangodb');
const db = arangodb.db;
const _ = require('lodash');

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function aqlKillSuite () {
  'use strict';
  const cn = "UnitTestsCollection";

  function findQueryId(query) {
    for (let tries = 0; tries < 30; ++tries) {
      const queries = require("@arangodb/aql/queries").current();
      const queryId = _.first(queries
      .filter(q => q.query === query)
      .map(q => q.id));
      if (queryId !== undefined) {
        return queryId;
      }

      internal.wait(1, false);
    }
    return undefined;
  }

  function runAbortQueryTest(query) {
    const cursorResult = arango.POST_RAW("/_api/cursor",
      {query},
      {"x-arango-async" : "store"});

    const jobId = cursorResult.headers["x-arango-async-id"];

    const queryId = findQueryId(query);

    assertTrue(queryId > 0);

    const killResult = arango.DELETE("/_api/query/" + queryId);
    assertEqual(killResult.code, 200);

    let putResult;
    for (let tries = 0; tries < 30; ++tries) {
      putResult = arango.PUT_RAW("/_api/job/" + jobId, {});
      if (putResult.code === 410) {
        break;
      }
      internal.wait(1, false);
    }
    assertEqual(410, putResult.code);
  }
  function runCancelQueryTest(query) {
    const cursorResult = arango.POST_RAW("/_api/cursor",
      {query},
      {"x-arango-async" : "store"});

    const jobId = cursorResult.headers["x-arango-async-id"];

    const queryId = findQueryId(query);

    assertTrue(queryId > 0);

    // cancel the async job

    const result = arango.PUT_RAW("/_api/job/" + jobId + "/cancel", {});
    assertEqual(result.code, 200);

    // make sure the query is no longer in the list of running queries
    let tries;
    for (tries = 0; tries < 30; ++tries) {
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
      internal.wait(1, false);
    }
    assertTrue(tries < 30);
  }
  
  return {

    setUpAll: function () {
      db._drop(cn);
      db._create(cn, { numberOfShards: 3 });
    },

    tearDownAll: function () {
      db._drop(cn);
    },
    
    testAbortReadQuery: function () {
      runAbortQueryTest("FOR i IN 1..10000000 RETURN SLEEP(1)");
    },

    testAbortWriteQuery: function () {
      runAbortQueryTest(`FOR i IN 1..10000000 INSERT {} INTO ${cn}`);
    },

    // test killing the query via the Job API
    testCancelWriteQuery: function () {
      runCancelQueryTest(`FOR i IN 1..10000000 INSERT {} INTO ${cn}`);
    },
  };
}

jsunity.run(aqlKillSuite);

return jsunity.done();
