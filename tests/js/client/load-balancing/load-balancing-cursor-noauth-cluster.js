/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, assertMatch, require*/

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dan Larkin-York
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");

const db = require("internal").db;
const request = require("@arangodb/request");
const url = require('url');
const _ = require("lodash");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const errors = require('internal').errors;
const dbs = ["testDatabase", "abc123", "maçã", "mötör", "😀", "ﻚﻠﺑ ﻞﻄﻴﻓ", "かわいい犬"];
const getCoordinatorEndpoints = require('@arangodb/test-helper').getCoordinatorEndpoints;

const servers = getCoordinatorEndpoints();

function CursorSyncSuite (databaseName) {
  'use strict';
  const cns = ["animals", "fruits"];
  const keys = [
    ["ant", "bird", "cat", "dog"],
    ["apple", "banana", "coconut", "date"]
  ];
  let cs = [];
  let coordinators = [];
  const baseCursorUrl = `/_db/${encodeURIComponent(databaseName)}/_api/cursor`; 
 
  function sendRequest(method, endpoint, body, usePrimary) {
    let res;
    const i = usePrimary ? 0 : 1;

    try {
      const envelope = {
        body,
        json: true,
        method,
        url: `${coordinators[i]}${endpoint}`
      };
      res = request(envelope);
    } catch(err) {
      console.error(`Exception processing ${method} ${endpoint}`, err.stack);
      return {};
    }
    var resultBody = res.body;
    if (typeof resultBody === "string") {
      resultBody = JSON.parse(resultBody);
    }
    return resultBody;
  }

  return {
    
    setUpAll: function() {
      coordinators = getCoordinatorEndpoints();
      if (coordinators.length < 2) {
        throw new Error('Expecting at least two coordinators');
      }
    },

    setUp: function() {
      db._createDatabase(databaseName);
      db._useDatabase(databaseName);
      cs = [];
      for (let i = 0; i < cns.length; i++) {
        db._drop(cns[i]);
        cs.push(db._create(cns[i]));
        assertTrue(cs[i].name() === cns[i]);
        for (let key in keys[i]) {
          cs[i].save({ _key: key });
        }
      }

      require("internal").wait(2);
    },

    tearDown: function() {
      db._drop(cns[0]);
      db._drop(cns[1]);
      cs = [];
      db._useDatabase("_system");
      if(databaseName !== "_system") {
        db._dropDatabase(databaseName);
      }
    },

    testCursorForwardingBasicPut: function() {
      let url = baseCursorUrl;
      const query = {
        query: `FOR doc IN @@coll LIMIT 4 RETURN doc`,
        count: true,
        batchSize: 2,
        bindVars: {
          "@coll": cns[0]
        }
      };
      let result = sendRequest('POST', url, query, true);

      assertFalse(result === undefined || result === {});
      assertFalse(result.error);
      assertEqual(result.code, 201);
      assertTrue(result.hasMore);
      assertEqual(result.count, 4);
      assertEqual(result.result.length, 2);

      const cursorId = result.id;
      url = `${baseCursorUrl}/${cursorId}`;
      result = sendRequest('PUT', url, {}, false);

      assertFalse(result === undefined || result === {});
      assertFalse(result.error);
      assertEqual(result.code, 200);
      assertFalse(result.hasMore);
      assertEqual(result.count, 4);
      assertEqual(result.result.length, 2);

      url = `${baseCursorUrl}/${cursorId}`;
      result = sendRequest('PUT', url, {}, false);

      assertFalse(result === undefined || result === {});
      assertTrue(result.error);
      assertEqual(result.code, 404);
    },
    
    testCursorForwardingBasicPost: function() {
      let url = baseCursorUrl;
      const query = {
        query: `FOR doc IN @@coll LIMIT 4 RETURN doc`,
        count: true,
        batchSize: 2,
        bindVars: {
          "@coll": cns[0]
        }
      };
      let result = sendRequest('POST', url, query, true);
      assertFalse(result === undefined || result === {});
      assertFalse(result.error);
      assertEqual(result.code, 201);
      assertTrue(result.hasMore);
      assertEqual(result.count, 4);
      assertEqual(result.result.length, 2);

      const cursorId = result.id;
      url = `${baseCursorUrl}/${cursorId}`;
      result = sendRequest('POST', url, {}, false);

      assertFalse(result === undefined || result === {});
      assertFalse(result.error);
      assertEqual(result.code, 200);
      assertFalse(result.hasMore);
      assertEqual(result.count, 4);
      assertEqual(result.result.length, 2);

      url = `${baseCursorUrl}/${cursorId}`;
      result = sendRequest('POST', url, {}, false);

      assertFalse(result === undefined || result === {});
      assertTrue(result.error);
      assertEqual(result.code, 404);
    },

    testCursorForwardingDeletionPut: function() {
      let url = baseCursorUrl;
      const query = {
        query: `FOR doc IN @@coll LIMIT 4 RETURN doc`,
        count: true,
        batchSize: 2,
        bindVars: {
          "@coll": cns[0]
        }
      };
      let result = sendRequest('POST', url, query, true);

      assertFalse(result === undefined || result === {});
      assertFalse(result.error);
      assertEqual(result.code, 201);
      assertTrue(result.hasMore);
      assertEqual(result.count, 4);
      assertEqual(result.result.length, 2);

      const cursorId = result.id;
      url = `${baseCursorUrl}/${cursorId}`;
      result = sendRequest('DELETE', url, {}, false);

      assertFalse(result === undefined || result === {});
      assertFalse(result.error);
      assertEqual(result.code, 202);

      url = `${baseCursorUrl}/${cursorId}`;
      result = sendRequest('PUT', url, {}, false);

      assertFalse(result === undefined || result === {});
      assertTrue(result.error);
      assertEqual(result.code, 404);
    },
    
    testCursorForwardingDeletionPost: function() {
      let url = baseCursorUrl;
      const query = {
        query: `FOR doc IN @@coll LIMIT 4 RETURN doc`,
        count: true,
        batchSize: 2,
        bindVars: {
          "@coll": cns[0]
        }
      };
      let result = sendRequest('POST', url, query, true);

      assertFalse(result === undefined || result === {});
      assertFalse(result.error);
      assertEqual(result.code, 201);
      assertTrue(result.hasMore);
      assertEqual(result.count, 4);
      assertEqual(result.result.length, 2);

      const cursorId = result.id;
      url = `${baseCursorUrl}/${cursorId}`;
      result = sendRequest('DELETE', url, {}, false);

      assertFalse(result === undefined || result === {});
      assertFalse(result.error);
      assertEqual(result.code, 202);

      url = `${baseCursorUrl}/${cursorId}`;
      result = sendRequest('POST', url, {}, false);

      assertFalse(result === undefined || result === {});
      assertTrue(result.error);
      assertEqual(result.code, 404);
    },
    
    testCursorForwardingDeletionWrongId: function() {
      let url = baseCursorUrl;
      const query = {
        query: `FOR doc IN @@coll LIMIT 4 RETURN doc`,
        count: true,
        batchSize: 2,
        bindVars: {
          "@coll": cns[0]
        }
      };
      let result = sendRequest('POST', url, query, true);

      assertFalse(result === undefined || result === {});
      assertFalse(result.error);
      assertEqual(result.code, 201);
      assertTrue(result.hasMore);
      assertEqual(result.count, 4);
      assertEqual(result.result.length, 2);

      const cursorId = result.id;
      // send using wrong id
      result = sendRequest('DELETE', `${baseCursorUrl}/12345${cursorId}`, {}, true);
      assertEqual(errors.ERROR_CURSOR_NOT_FOUND.code, result.errorNum);
      assertMatch(/cannot find target server/, result.errorMessage);
      
      result = sendRequest('DELETE', `${baseCursorUrl}/12345${cursorId}`, {}, false);
      assertEqual(errors.ERROR_CURSOR_NOT_FOUND.code, result.errorNum);
      assertMatch(/cannot find target server/, result.errorMessage);
      
      result = sendRequest('DELETE', `${baseCursorUrl}/${cursorId}`, {}, true);

      assertFalse(result === undefined || result === {});
      assertFalse(result.error);
      assertEqual(result.code, 202);

      url = `${baseCursorUrl}/${cursorId}`;
      result = sendRequest('POST', url, {}, false);

      assertFalse(result === undefined || result === {});
      assertTrue(result.error);
      assertEqual(result.code, 404);
    },

  };
}

dbs.forEach((databaseName) => {
  let func = function() {
    let suite = {};
    deriveTestSuite(CursorSyncSuite(databaseName), suite, databaseName);
    return suite;
  };
  Object.defineProperty(func, 'name', {value: "CursorSyncSuite" + databaseName, writable: false});
  jsunity.run(func);
});

return jsunity.done();
