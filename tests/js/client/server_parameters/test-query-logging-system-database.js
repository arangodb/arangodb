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
    'query.collection-logger-include-system-database': 'false',
    'query.collection-logger-probability': '100',
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
      
  const checkForQuery = (n, values) => {
    let tries = 0;
    let queries;
    while (++tries < 40) {
      queries = getQueries();
      let filtered = queries.filter((q) => {
        return Object.keys(values).every((key) => {
          return q[key] === values[key];
        });
      });
      if (filtered.length === n) {
        return filtered;
      }
      internal.sleep(0.25);
    }
    assertFalse(true, {n, values, queries});
  };

  return {
    setUpAll: function () {
      db._create(cn, {numberOfShards: 3});

      db._createDatabase(cn);
      db._useDatabase(cn);
      db._create(cn, {numberOfShards: 3});
     
      // run a dummy query so that the _queries collection will
      // be created
      db._query("FOR i IN 1..10 RETURN i");
      waitForCollection();
    },
    
    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(cn);
      db._drop(cn);
    },

    setUp: function () {
      clearQueries();
    },

    testQueriesInSystemDatabase: function () {
      let old = db._name();
      db._useDatabase("_system");
      const query = `${uniqid()} INSERT {} INTO ${cn}`;
      try {
        const n = 100;
        for (let i = 0; i < n; ++i) {
          db._query(query);
        }
      } finally {
        db._useDatabase(old);
      }

      checkForQuery(0, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: true, 
        stream: false,
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testQueriesInOtherDatabase: function () {
      assertEqual(cn, db._name());
      const query = `${uniqid()} INSERT {} INTO ${cn}`;
      const n = 100;
      for (let i = 0; i < n; ++i) {
        db._query(query);
      }

      checkForQuery(n, {
        query, 
        database: cn,
        user: "root", 
        state: "finished",
        modificationQuery: true, 
        stream: false,
        warnings: 0, 
        exitCode: 0
      });
    },
    
  };
}

jsunity.run(QueryLoggerSuite);
return jsunity.done();
