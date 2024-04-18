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
const crypto = require('@arangodb/crypto');
const request = require("@arangodb/request");
const internal = require('internal');
const errors = internal.errors;

const jwtSecret = 'haxxmann';

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.jwt-secret': jwtSecret,
    'query.collection-logger-enabled': 'true',
    'query.collection-logger-include-system-database': 'true',
    'query.collection-logger-probability': '100',
  };
}

const jwt = crypto.jwtEncode(jwtSecret, {
  "server_id": "ABCD",
  "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
}, 'HS256');

function QueryLoggerSuite() { 
  const cn = "UnitTests";
  
  const baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:') + '/_db/' + db._name();
  };

  let qid = 0;

  const uniqid = () => {
    return `/* test query ${++qid} */`;
  };
  
  const clearQueries = () => {
    let result = request.put({
      url: baseUrl() + "/_api/collection/_queries/truncate",
      body: {},
      json: true,
      auth: {bearer: jwt},
    });
    
    assertInstanceOf(request.Response, result);
    if (result.statusCode !== 404) {
      assertEqual(200, result.statusCode);
    }
  };
  
  const waitForCollection = () => {
    let tries = 0;
    while (++tries < 200) {
      let result = request.post({
        url: baseUrl() + "/_api/cursor",
        body: {query:"FOR doc IN _queries LIMIT 1 RETURN doc"},
        json: true,
        auth: {bearer: jwt},
      });

      assertInstanceOf(request.Response, result);
      let body = JSON.parse(result.body);
      if (!body.error) {
        return;
      }
      if (body.code !== 404 || body.errorNum !== errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
        throw body;
      }
      internal.sleep(0.25);
    }
    assertFalse(true, "_queries collection did not appear in time");
  };

  const getQueries = () => {
    // fetch everything at once
    const batchSize = 10 * 1000;

    let result = request.post({
      url: baseUrl() + "/_api/cursor",
      body: {query:"FOR doc IN _queries RETURN doc", batchSize, options: {batchSize}},
      json: true,
      auth: {bearer: jwt},
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
        return;
      }
      internal.sleep(0.25);
    }
    assertFalse(true, {n, values, queries});
  };

  return {
    setUpAll: function () {
      waitForCollection();
    },

    setUp: function () {
      db._create(cn, {numberOfShards: 3});
      clearQueries();
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testModificationQuery: function () {
      const query = `${uniqid()} INSERT {} INTO ${cn}`;
      const n = 100;
      for (let i = 0; i < n; ++i) {
        db._query(query, null, {optimizer: {rules: ["-all"] } }).toArray();
      }

      checkForQuery(n, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: true, 
        stream: false,
        /*
        writesExecuted: 1, 
        writesIgnored: 0, 
        documentLookups: 0, 
        scannedFull: 0, 
        scannedIndex: 0, 
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 0, 
        intermediateCommits: 0, 
        count: 0, 
        */
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testInsertManyQuery: function () {
      const query = `FOR i IN 1..2000 ${uniqid()} INSERT {} INTO ${cn}`;
      db._query(query, null, {optimizer: {rules: ["-all"] } }).toArray();

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: true, 
        stream: false, 
        /*
        writesExecuted: 2000, 
        writesIgnored: 0, 
        documentLookups: 0, 
        scannedFull: 0, 
        scannedIndex: 0, 
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 0, 
        intermediateCommits: 0, 
        count: 0, 
        */
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testInsertManyQueryIntermediateCommit: function () {
      const query = `FOR i IN 1..2000 ${uniqid()} INSERT {} INTO ${cn}`;
      db._query(query, null, {intermediateCommitCount: 500, optimizer: {rules: ["-all"] }}).toArray();

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: "root",
        state: "finished",
        modificationQuery: true, 
        stream: false, 
        /*
        writesExecuted: 2000, 
        writesIgnored: 0, 
        documentLookups: 0, 
        scannedFull: 0, 
        scannedIndex: 0, 
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 0, 
        intermediateCommits: 2, 
        count: 0, 
        */
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testReadQueryEmpty: function () {
      const query = `${uniqid()} FOR doc IN ${cn} RETURN doc`;
      const n = 13;
      for (let i = 0; i < n; ++i) {
        db._query(query).toArray();
      }

      checkForQuery(n, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: false, 
        stream: false, 
        /*
        writesExecuted: 0, 
        writesIgnored: 0, 
        documentLookups: 0, 
        scannedFull: 0, 
        scannedIndex: 0, 
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 0, 
        intermediateCommits: 0, 
        count: 0, 
        */
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testReadQueryNonEmpty: function () {
      let docs = [];
      for (let i = 0; i < 25000; ++i) {
        docs.push({ value1: i, value2: i });
        if (docs.length === 5000) {
          db[cn].insert(docs);
          docs = [];
        }
      }

      const query = `${uniqid()} FOR doc IN ${cn} RETURN doc`;
      db._query(query).toArray();

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: false, 
        stream: false, 
        /*
        writesExecuted: 0, 
        writesIgnored: 0, 
        documentLookups: 0, 
        scannedFull: 25000, 
        scannedIndex: 0, 
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 0, 
        intermediateCommits: 0, 
        count: 25000, 
        */
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testReadQueryScanIndex: function () {
      let docs = [];
      for (let i = 0; i < 25000; ++i) {
        docs.push({ value1: i, value2: i });
        if (docs.length === 5000) {
          db[cn].insert(docs);
          docs = [];
        }
      }

      const query = `${uniqid()} FOR doc IN ${cn} RETURN doc._key`;
      db._query(query).toArray();

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: false, 
        stream: false, 
        /*
        writesExecuted: 0, 
        writesIgnored: 0, 
        documentLookups: 0, 
        scannedFull: 0,
        scannedIndex: 25000, 
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 0, 
        intermediateCommits: 0, 
        count: 25000, 
        */
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testReadQueryScanWithFilter: function () {
      let docs = [];
      for (let i = 0; i < 25000; ++i) {
        docs.push({ value: i });
        if (docs.length === 5000) {
          db[cn].insert(docs);
          docs = [];
        }
      }

      const query = `${uniqid()} FOR doc IN ${cn} FILTER doc.value >= 12500 RETURN doc._key`;
      db._query(query).toArray();

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: false, 
        stream: false, 
        /*
        writesExecuted: 0, 
        writesIgnored: 0, 
        documentLookups: 0, 
        scannedFull: 25000,
        scannedIndex: 0,
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 12500, 
        intermediateCommits: 0, 
        count: 12500, 
        */
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testReadQueryScanIndexWithDocumentLookup: function () {
      let docs = [];
      for (let i = 0; i < 25000; ++i) {
        docs.push({ value1: i, value2: i });
        if (docs.length === 5000) {
          db[cn].insert(docs);
          docs = [];
        }
      }
      db[cn].ensureIndex({ type: "persistent", fields: ["value1"] });

      const query = `${uniqid()} FOR doc IN ${cn} FILTER doc.value1 >= 12500 FILTER doc.value2 < 20000 RETURN doc`;
      db._query(query).toArray();

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: false, 
        stream: false, 
        /*
        writesExecuted: 0, 
        writesIgnored: 0, 
        documentLookups: 12500, 
        scannedFull: 0,
        scannedIndex: 12500,
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 5000, 
        intermediateCommits: 0, 
        count: 7500, 
        */
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testQueryWithWarnings: function () {
      const query = `${uniqid()} FOR i IN 1..3 RETURN i / 0`;
      db._query(query).toArray();

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: false, 
        stream: false, 
        /*
        writesExecuted: 0, 
        writesIgnored: 0, 
        documentLookups: 0, 
        scannedFull: 0, 
        scannedIndex: 0, 
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 0, 
        intermediateCommits: 0, 
        count: 3, 
        */
        warnings: 3, 
        exitCode: 0
      });
    },
    
    testQueryWithErrors: function () {
      const query = `${uniqid()} FOR i IN 1..10 RETURN ASSERT(i < 10, 'piff')`;
      try {
        db._query(query).toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_USER_ASSERT.code, err.errorNum);
      }

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: false, 
        stream: false, 
        /*
        writesExecuted: 0, 
        writesIgnored: 0, 
        documentLookups: 0, 
        scannedFull: 0, 
        scannedIndex: 0, 
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 0, 
        intermediateCommits: 0, 
        count: 0, 
        */
        warnings: 0, 
        exitCode: errors.ERROR_QUERY_USER_ASSERT.code,
      });
    },
    
    testReadCount: function () {
      const query = `${uniqid()} FOR i IN 1..10000 RETURN i`;
      db._query(query).toArray();

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: false, 
        stream: false, 
        /*
        writesExecuted: 0, 
        writesIgnored: 0, 
        documentLookups: 0, 
        scannedFull: 0, 
        scannedIndex: 0, 
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 0, 
        intermediateCommits: 0, 
        count: 10000, 
        */
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testStream: function () {
      const query = `${uniqid()} FOR i IN 1..10000 RETURN i`;
      db._query(query, null, {stream: true}).toArray();

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: "root", 
        state: "finished",
        modificationQuery: false, 
        stream: true, 
        /*
        writesExecuted: 0, 
        writesIgnored: 0, 
        documentLookups: 0, 
        scannedFull: 0, 
        scannedIndex: 0, 
        cacheHits: 0, 
        cacheMisses: 0, 
        filtered: 0, 
        intermediateCommits: 0, 
        count: 10000, 
        */
        warnings: 0, 
        exitCode: 0
      });
    },

  };
}

jsunity.run(QueryLoggerSuite);
return jsunity.done();
