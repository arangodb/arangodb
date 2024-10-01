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
const users = require("@arangodb/users");
const internal = require('internal');
const errors = internal.errors;

const jwtSecret = 'haxxmann';
const cn = "UnitTestsQueries";
// fetch everything at once
const batchSize = 10 * 1000;

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.jwt-secret': jwtSecret,
    'query.collection-logger-enabled': 'true',
    'query.collection-logger-include-system-database': 'true',
    'query.collection-logger-probability': '100',
    'query.collection-logger-push-interval': '250',
  };
}

const jwt = crypto.jwtEncode(jwtSecret, {
  "server_id": "ABCD",
  "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
}, 'HS256');
  
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
      auth: {bearer: jwt},
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


function QueryPermissionsSuite() { 
  return {
    setUpAll: function () {
      waitForCollection();

      db._create(cn);
      // run at least one AQL query so that the _queries collection
      // will be created
      db._query(`FOR i IN 1..100 INSERT {} INTO ${cn}`);
    },
    
    tearDownAll: function () {
      db._drop(cn);
    },

    tearDown: function () {
      try {
        users.remove("test_user");
      } catch (err) {
      }
    },

    testNoAccessWithoutDatabasePermissions: function () {
      users.save("test_user", "testi");
      users.grantDatabase("test_user", "_system", "none");

      let result = request.post({
        url: baseUrl() + "/_api/cursor",
        body: {query:"FOR doc IN _queries RETURN doc", batchSize, options: {batchSize}},
        json: true,
        auth: {username: "test_user", password: "testi"},
      });
      assertEqual(401, result.statusCode);
      let body = JSON.parse(result.body);
      assertTrue(body.error, body);
    },
    
    testAccessWithDatabaseReadOnlyPermissions: function () {
      users.save("test_user", "testi");
      users.grantDatabase("test_user", "_system", "ro");
      
      let result = request.post({
        url: baseUrl() + "/_api/cursor",
        body: {query:"FOR doc IN _queries RETURN doc", batchSize, options: {batchSize}},
        json: true,
        auth: {username: "test_user", password: "testi"},
      });
      assertEqual(201, result.statusCode);
      let body = JSON.parse(result.body);
      assertFalse(body.error, body);
    },
    
    testAccessWithDatabaseReadWritePermissions: function () {
      users.save("test_user", "testi");
      users.grantDatabase("test_user", "_system", "rw");

      let result = request.post({
          url: baseUrl() + "/_api/cursor",
          body: {query:"FOR doc IN _queries RETURN doc", batchSize, options: {batchSize}},
          json: true,
          auth: {username: "test_user", password: "testi"},
        });
      assertEqual(201, result.statusCode);
      let body = JSON.parse(result.body);
      assertFalse(body.error, body);
    },

    testDifferentDatabaseNoAccessWithoutDatabasePermissions: function () {
      db._createDatabase(cn);
      try {
        db._useDatabase(cn);
        users.save("test_user", "testi");
        users.grantDatabase("test_user", cn, "none");

        let result = request.post({
          url: baseUrl(cn) + "/_api/cursor",
          body: {query:"FOR doc IN _queries RETURN doc", batchSize, options: {batchSize}},
          json: true,
          auth: {username: "test_user", password: "testi"},
        });
        assertEqual(401, result.statusCode);
        let body = JSON.parse(result.body);
        assertTrue(body.error, body);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },
    
    testDifferentDatabaseAccessWithDatabaseReadOnlyPermissions: function () {
      db._createDatabase(cn);
      try {
        db._useDatabase(cn);
        users.save("test_user", "testi");
        users.grantDatabase("test_user", cn, "ro");
      
        let result = request.post({
          url: baseUrl(cn) + "/_api/cursor",
          body: {query:"FOR doc IN _queries RETURN doc", batchSize, options: {batchSize}},
          json: true,
          auth: {username: "test_user", password: "testi"},
        });
        // no _queries collection in non-system database
        assertEqual(404, result.statusCode);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },
    
    testDifferentDatabasesAccessWithDatabaseReadWritePermissions: function () {
      db._createDatabase(cn);
      try {
        db._useDatabase(cn);
        users.save("test_user", "testi");
        users.grantDatabase("test_user", cn, "rw");

        let result = request.post({
            url: baseUrl(cn) + "/_api/cursor",
            body: {query:"FOR doc IN _queries RETURN doc", batchSize, options: {batchSize}},
            json: true,
            auth: {username: "test_user", password: "testi"},
          });
        // no _queries collection in non-system database
        assertEqual(404, result.statusCode);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },
  };
}

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
      auth: {bearer: jwt},
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
    let length = 0;
    while (++tries < 40) {
      queries = getQueries();
      let filtered = queries.filter((q) => {
        return Object.keys(values).every((key) => {
          if (Array.isArray(values[key])) {
            return values[key].find((elm) => elm === q[key]) !== undefined;
          } else {
            return q[key] === values[key];
          }
        });
      });
      length = filtered.length;
      if (length === n) {
        return filtered;
      }
      internal.sleep(0.25);
    }
    assertEqual(length, n, {n, values, queries});
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
      assertEqual("_system", db._name());
      db._flushCache();
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: true, 
        stream: false,
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: true, 
        stream: false, 
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: true, 
        stream: false, 
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: false, 
        stream: false, 
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: false, 
        stream: false, 
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: false, 
        stream: false, 
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: false, 
        stream: false, 
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: false, 
        stream: false, 
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: false, 
        stream: false, 
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: false, 
        stream: false, 
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: false, 
        stream: false, 
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
        user: ["", "root"],
        state: "finished",
        modificationQuery: false, 
        stream: true, 
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testPeakMemoryUsage: function () {
      const query = `${uniqid()} FOR i IN 1..10000 RETURN i`;
      db._query(query).toArray();

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: ["", "root"],
        state: "finished",
        modificationQuery: false, 
        stream: false, 
        peakMemoryUsage: 32768,
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testHigherPeakMemoryUsage: function () {
      const query = `${uniqid()} FOR i IN 1..100000 RETURN i`;
      db._query(query).toArray();

      checkForQuery(1, {
        query, 
        database: "_system", 
        user: ["", "root"],
        state: "finished",
        modificationQuery: false, 
        stream: false, 
        peakMemoryUsage: 360448,
        warnings: 0, 
        exitCode: 0
      });
    },
    
    testDifferentDatabase: function () {
      db._createDatabase(cn);
      try {
        db._useDatabase(cn);
        const query = `${uniqid()} FOR i IN 1..10000 RETURN i`;
        db._query(query).toArray();

        checkForQuery(1, {
          query, 
          database: cn,
          user: ["", "root"],
          state: "finished",
          modificationQuery: false, 
          stream: false, 
          warnings: 0, 
          exitCode: 0
        });
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },

    testDifferentUser: function () {
      let endpoint = arango.getEndpoint();
      users.save("test_user", "testi");
      users.grantDatabase("test_user", "_system", "rw");
      try {
        arango.reconnect(endpoint, db._name(), "test_user", "testi");
        const query = `${uniqid()} FOR i IN 1..10000 RETURN i`;
        db._query(query).toArray();

        checkForQuery(1, {
          query, 
          database: "_system",
          user: "test_user", 
          state: "finished",
          modificationQuery: false, 
          stream: false, 
          warnings: 0, 
          exitCode: 0
        });
      } finally {
        arango.reconnect(endpoint, db._name(), "root", "");
        try {
          users.remove("test_user");
        } catch (err) {
        }
      }
    },
    
    testDifferentUserInDifferentDatabase: function () {
      let endpoint = arango.getEndpoint();
      users.save("test_user", "testi");
      users.grantDatabase("test_user", cn, "ro");
      users.grantDatabase("test_user", "_system", "ro");
      db._createDatabase(cn);
      try {
        arango.reconnect(endpoint, cn, "test_user", "testi");
        const query = `${uniqid()} FOR i IN 1..10000 RETURN i`;
        db._query(query).toArray();

        checkForQuery(1, {
          query, 
          database: cn,
          user: "test_user", 
          state: "finished",
          modificationQuery: false, 
          stream: false, 
          warnings: 0, 
          exitCode: 0
        });
      } finally {
        arango.reconnect(endpoint, "_system", "root", "");
        try {
          users.remove("test_user");
        } catch (err) {
        }
        db._dropDatabase(cn);
      }
    },

  };
}

jsunity.run(QueryPermissionsSuite);
jsunity.run(QueryLoggerSuite);
return jsunity.done();
