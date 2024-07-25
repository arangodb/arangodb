/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global getOptions, fail, arango, assertEqual, assertFalse, assertTrue, assertMatch, assertInstanceOf */

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
// fetch everything at once
const batchSize = 10 * 1000;

if (getOptions === true) {
  return {
    'query.collection-logger-enabled': 'true',
    'query.collection-logger-include-system-database': 'true',
    'query.collection-logger-probability': '5',
    'query.collection-logger-push-interval': '100',
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
  
  const clearQueries = () => {
    let result = request.put({
      url: baseUrl() + "/_api/collection/_queries/truncate",
      body: {},
      json: true,
    });
    
    assertInstanceOf(request.Response, result);
    if (result.statusCode !== 404) {
      assertEqual(200, result.statusCode);
    }
  };
  
  const getQueries = () => {
    let result = request.post({
      url: baseUrl() + "/_api/cursor",
      body: {query:"FOR doc IN _queries RETURN doc", batchSize, options: {batchSize}},
      json: true,
    });

    assertInstanceOf(request.Response, result);
    let body = JSON.parse(result.body);
    assertEqual(201, result.statusCode);
    assertTrue(Array.isArray(body.result));
    return body.result;
  };
      
  const checkForQuery = (values) => {
    let tries = 0;
    let filtered = [];
    while (++tries < 20) {
      let queries = getQueries();
      filtered = queries.filter((q) => {
        return Object.keys(values).every((key) => {
          return q[key] === values[key];
        });
      });
      internal.sleep(0.25);
    }
    return filtered;
  };

  return {
    setUpAll: function () {
      // run a few queries so that the system collection gets created
      for (let i = 0; i < 50; ++i) {
        db._query("RETURN 1");
      }
      waitForCollection();
    },
    
    setUp: function () {
      clearQueries();
    },

    testSampling: function () {
      const query = `${uniqid()} FOR i IN 1..10 RETURN i`;
      const n = 1000;
      for (let i = 0; i < n; ++i) {
        db._query(query);
      }

      let expected = n * 0.05;
      let min = expected * 0.5;
      let max = expected * 1.5;

      let results = checkForQuery({
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: false, 
        stream: false,
        warnings: 0, 
        exitCode: 0
      });

      assertTrue(results.length > min, { actual: results.length, min });
      assertTrue(results.length < max, { actual: results.length, max });
    },
    
  };
}

jsunity.run(QueryLoggerSuite);
return jsunity.done();
