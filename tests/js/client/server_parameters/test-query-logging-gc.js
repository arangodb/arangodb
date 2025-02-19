/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global getOptions, fail, arango, assertEqual, assertNotEqual, assertFalse, assertTrue, assertInstanceOf */

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

'use strict';
const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const db = arangodb.db;
const request = require("@arangodb/request");
const internal = require('internal');
const errors = internal.errors;

const cn = "UnitTestsQueries";

if (getOptions === true) {
  return {
    'query.collection-logger-enabled': 'true',
    'query.collection-logger-include-system-database': 'true',
    'query.collection-logger-probability': '100',
    'query.collection-logger-push-interval': '100', // milliseconds
    'query.collection-logger-retention-time': '5', // seconds
    'query.collection-logger-cleanup-interval': '1000', // milliseconds
  };
}

const baseUrl = function (dbName = '_system') {
  return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + `/_db/${dbName}`;
};
  
const waitForCollection = () => {
  let tries = 0;
  while (++tries < 200) {
    let result = request.post({
      url: baseUrl() + "/_api/cursor",
      body: {query:"FOR doc IN _queries LIMIT 1 RETURN doc"},
      json: true,
    });

    assertInstanceOf(request.Response, result);
    let body = JSON.parse(result.body);
    if (!body.error) {
      return;
    }
    if (body.code !== 404 || body.errorNum !== errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code) {
      throw body;
    }
    internal.sleep(0.25);
  }
  assertFalse(true, "_queries collection did not appear in time");
};

function QueryLoggerSuite() { 
  let qid = 0;

  const uniqid = () => {
    return `/* test query ${++qid} */`;
  };
  
  return {
    setUpAll: function () {
      waitForCollection();
    },

    testGarbageCollection: function () {
      const n = 10;
      const query = `${uniqid()} RETURN 1`;
      for (let i = 0; i < n; ++i) {
        db._query(query);
      }

      // wait for everything to be pushed
      let tries = 0;
      let results;
      while (++tries < 100) {
        results = db._query(`FOR doc IN _queries FILTER doc.query == @query RETURN doc`, {query}).toArray();
        if (results.length === n) {
          break;
        }
        internal.sleep(0.25);
      }
      assertEqual(n, results.length, results);

      // wait for gc
      tries = 0;
      while (++tries < 100) {
        results = db._query(`FOR doc IN _queries FILTER doc.query == @query RETURN doc`, {query}).toArray();
        if (results.length === 0) {
          break;
        }
        internal.sleep(0.25);
      }
      assertEqual(0, results.length, results);
    },
    
    testWontBeGarbageCollected: function () {
      const query = `${uniqid()} RETURN 1`;
      db._query(query);

      // update expire timestamp of existing entry, so that it does not expire
      // wait for everything to be pushed
      let tries = 0;
      let results;
      while (++tries < 100) {
        results = db._query(`FOR doc IN _queries FILTER doc.query == @query UPDATE doc WITH {started: DATE_ADD(doc.started, 2, 'year')} IN _queries RETURN NEW`, {query}).toArray();
        if (results.length > 0) {
          break;
        }
        internal.sleep(0.25);
      }
      assertEqual(1, results.length, results);

      // wait for gc
      tries = 0;
      while (++tries < 40) {
        results = db._query(`FOR doc IN _queries FILTER doc.query == @query RETURN doc`, {query}).toArray();
        if (results.length === 0) {
          break;
        }
        internal.sleep(0.25);
      }
      assertNotEqual(0, results.length, results);
    } 
  };
}

jsunity.run(QueryLoggerSuite);
return jsunity.done();
