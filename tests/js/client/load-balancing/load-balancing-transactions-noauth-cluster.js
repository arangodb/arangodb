/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, assertNotUndefined, require*/

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
/// @author Jan Steemann
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");

const db = require("internal").db;
const request = require("@arangodb/request");
const url = require('url');
const _ = require("lodash");

function getCoordinators() {
  const isCoordinator = (d) => (_.toLower(d.role) === 'coordinator');
  const toEndpoint = (d) => (d.endpoint);
  const endpointToURL = (endpoint) => {
    if (endpoint.substr(0, 6) === 'ssl://') {
      return 'https://' + endpoint.substr(6);
    }
    var pos = endpoint.indexOf('://');
    if (pos === -1) {
      return 'http://' + endpoint;
    }
    return 'http' + endpoint.substr(pos);
  };

  const instanceInfo = JSON.parse(require('internal').env.INSTANCEINFO);
  return instanceInfo.arangods.filter(isCoordinator)
                              .map(toEndpoint)
                              .map(endpointToURL);
}

const servers = getCoordinators();

function TransactionsSuite () {
  'use strict';
  const cn = 'UnitTestsCollection';
  let coordinators = [];

  function sendRequest(method, endpoint, body, headers, usePrimary) {
    let res;
    const i = usePrimary ? 0 : 1;
    try {
      const envelope = {
        json: true,
        method,
        url: `${coordinators[i]}${endpoint}`,
        headers,
      };
      if (method !== 'GET') {
        envelope.body = body;
      }
      res = request(envelope);
    } catch(err) {
      console.error(`Exception processing ${method} ${endpoint}`, err.stack);
      return {};
    }

    if (typeof res.body === "string") {
      if (res.body === "") {
        res.body = {};
      } else {
        res.body = JSON.parse(res.body);
      }
    }
    return res;
  }

  let assertInList = function (list, trx) {
    assertTrue(list.filter(function(data) { return data.id === trx.id; }).length > 0,
               "transaction " + trx.id + " not found in list of transactions " + JSON.stringify(trx));
  };
  
  let assertNotInList = function (list, trx) {
    assertTrue(list.filter(function(data) { return data.id === trx.id; }).length === 0,
               "transaction " + trx.id + " not found in list of transactions " + JSON.stringify(trx));
  };

  return {
    setUp: function() {
      coordinators = getCoordinators();
      if (coordinators.length < 2) {
        throw new Error('Expecting at least two coordinators');
      }

      db._drop(cn);
      db._create(cn);

      for (let i = 0; i < 10; ++i) {
        db[cn].save({i});
      }

      require("internal").wait(2);
    },

    tearDown: function() {
      coordinators = [];
      db._drop(cn);
    },

    testListTransactions: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let result = sendRequest('POST', url + "/begin", obj, {}, true);

      assertEqual(result.status, 201);
      assertNotUndefined(result.body.result.id);

      let trx1 = result.body.result;

      try {
        result = sendRequest('GET', url, {}, {}, true);

        assertEqual(result.status, 200);
        assertInList(result.body.transactions, trx1);

        result = sendRequest('GET', url, {}, {}, false);
        
        assertEqual(result.status, 200);
        assertInList(result.body.transactions, trx1);
      } finally {
        sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, {}, true);
      }
    },
    
    testListTransactions2: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let trx1, trx2;
      
      try {
        let result = sendRequest('POST', url + "/begin", obj, {}, true);
        assertEqual(result.status, 201);
        assertNotUndefined(result.body.result.id);
        trx1 = result.body.result;
        
        result = sendRequest('POST', url + "/begin", obj, {}, false);
        assertEqual(result.status, 201);
        assertNotUndefined(result.body.result.id);
        trx2 = result.body.result;
      
        result = sendRequest('GET', url, {}, {}, true);

        assertEqual(result.status, 200);
        assertInList(result.body.transactions, trx1);
        assertInList(result.body.transactions, trx2);

        result = sendRequest('GET', url, {}, {}, false);
        
        assertEqual(result.status, 200);
        assertInList(result.body.transactions, trx1);
        assertInList(result.body.transactions, trx2);

        // commit trx1 on different coord
        result = sendRequest('PUT', url + "/" + encodeURIComponent(trx1.id), {}, {}, false);
        assertEqual(trx1.id, result.body.result.id);
        assertEqual("committed", result.body.result.status);
        
        result = sendRequest('GET', url, {}, {}, false);
        
        assertEqual(result.status, 200);
        assertNotInList(result.body.transactions, trx1);
        assertInList(result.body.transactions, trx2);

        // abort trx2 on different coord
        result = sendRequest('DELETE', url + "/" + encodeURIComponent(trx2.id), {}, {}, true);
        assertEqual(trx2.id, result.body.result.id);
        assertEqual("aborted", result.body.result.status);
        
        result = sendRequest('GET', url, {}, {}, false);
        
        assertEqual(result.status, 200);
        assertNotInList(result.body.transactions, trx1);
        assertNotInList(result.body.transactions, trx2);

      } finally {
        sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, {}, true);
      }
    },
    
    testCreateAndCommitElsewhere: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let result = sendRequest('POST', url + "/begin", obj, {}, true);

      assertEqual(result.status, 201);
      assertNotUndefined(result.body.result.id);

      let trx1 = result.body.result;

      try {
        // commit on different coord
        result = sendRequest('PUT', url + "/" + encodeURIComponent(trx1.id), {}, {}, false);

        assertEqual(result.status, 200);
        assertEqual(trx1.id, result.body.result.id);
        assertEqual("committed", result.body.result.status);
        
        result = sendRequest('GET', url, {}, {}, true);
        assertNotInList(result.body.transactions, trx1);
      } finally {
        sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, {}, true);
      }
    },
    
    testCreateAndAbortElsewhere: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let result = sendRequest('POST', url + "/begin", obj, {}, true);

      assertEqual(result.status, 201);
      assertNotUndefined(result.body.result.id);

      let trx1 = result.body.result;

      try {
        // abort on different coord
        result = sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, {}, true);

        assertEqual(result.status, 200);
        assertEqual(trx1.id, result.body.result.id);
        assertEqual("aborted", result.body.result.status);
        
        result = sendRequest('GET', url, {}, {}, true);
        assertNotInList(result.body.transactions, trx1);
      } finally {
        sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, {}, true);
      }
    },

    testUseTransactionCursorOther: function () {
      let trx;
      try {
        const obj = { collections: { read: cn } };
        let result = sendRequest('POST', "/_api/transaction/begin", obj, {}, true);
        assertEqual(result.status, 201);
        assertNotUndefined(result.body.result.id);
        trx = result.body.result.id;
        
        // use trx on different coord to run a query
        const query = `FOR doc IN ${cn} RETURN doc`;
        result = sendRequest('POST', "/_api/cursor", { query }, { "x-arango-trx-id": trx }, false);
        // request must have been forwarded
        assertTrue(result.headers.hasOwnProperty("x-arango-request-forwarded-to"));
        assertFalse(result.body.error);
        assertEqual(10, result.body.result.length);
      } finally {
        sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx), {}, {}, true);
      }
    },
    
    testUseTransactionCursorSame: function () {
      let trx;
      try {
        const obj = { collections: { read: cn } };
        let result = sendRequest('POST', "/_api/transaction/begin", obj, {}, true);
        assertEqual(result.status, 201);
        assertNotUndefined(result.body.result.id);
        trx = result.body.result.id;
        
        // use trx on same coord to run a query
        const query = `FOR doc IN ${cn} RETURN doc`;
        result = sendRequest('POST', "/_api/cursor", { query }, { "x-arango-trx-id": trx }, true);
        // request must not have been forwarded
        assertFalse(result.headers.hasOwnProperty("x-arango-request-forwarded-to"));
        assertFalse(result.body.error);
        assertEqual(10, result.body.result.length);
      } finally {
        sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx), {}, {}, true);
      }
    },
    
    testUseTransactionCursorIncremental: function () {
      let trx;
      try {
        const obj = { collections: { read: cn } };
        let result = sendRequest('POST', "/_api/transaction/begin", obj, {}, true);
        assertEqual(result.status, 201);
        assertNotUndefined(result.body.result.id);
        trx = result.body.result.id;
        
        // use trx on different coord to run a query
        const query = `FOR doc IN ${cn} RETURN doc`;
        let cursor;
        result = sendRequest('POST', "/_api/cursor", { query, batchSize: 1 }, { "x-arango-trx-id": trx }, false);
        try {
          cursor = result.body.id;
          assertTrue(cursor);
          // request must have been forwarded
          assertTrue(result.headers.hasOwnProperty("x-arango-request-forwarded-to"));
          assertFalse(result.body.error);
          assertEqual(1, result.body.result.length);
          assertTrue(result.body.hasMore);

          for (let i = 0; i < 9; ++i) {
            let same = i % 2 === 0;
            result = sendRequest('PUT', "/_api/cursor/" + cursor, {}, { "x-arango-trx-id": trx }, same);
            assertEqual(same, !result.headers.hasOwnProperty("x-arango-request-forwarded-to"));
            assertFalse(result.body.error);
            assertEqual(1, result.body.result.length);
            assertEqual(i < 8, result.body.hasMore);
          }
        } finally {
          sendRequest('DELETE', '/_api/cursor/' + encodeURIComponent(cursor), {}, { "x-arango-trx-id": trx }, true);
        }
      } finally {
        sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx), {}, {}, true);
      }
    },
    
    testUseTransactionWithGharial: function () {
      let trx;
      try {
        const graphDef = {
          "name" : "myGraph",
          "edgeDefinitions" : [
            {
              "collection": "edges",
              "from": [
                "startVertices"
              ],
              "to": [
                "endVertices"
              ]
            }
          ]
        };
        let result = sendRequest('POST', "/_api/gharial", graphDef, {}, true);
        assertEqual(result.status, 202);

        const obj = { collections: { write: "startVertices" } };
        result = sendRequest('POST', "/_api/transaction/begin", obj, {}, true);
        assertEqual(result.status, 201);
        assertNotUndefined(result.body.result.id);
        trx = result.body.result.id;

        const doc = { value: true };
        result = sendRequest('POST', "/_api/gharial/myGraph/vertex/startVertices", doc, { "x-arango-trx-id": trx }, true);
        assertFalse(result.headers.hasOwnProperty("x-arango-request-forwarded-to"));
        assertEqual(result.status, 202);
        assertNotUndefined(result.body.vertex._key);
        const key = result.body.vertex._key;

        // get it via a different coordinator
        result = sendRequest('GET', `/_api/gharial/myGraph/vertex/startVertices/${key}`, {}, { "x-arango-trx-id": trx }, false);
        // request must have been forwarded
        assertTrue(result.headers.hasOwnProperty("x-arango-request-forwarded-to"));
        assertEqual(result.status, 200);
        assertEqual(result.body.vertex._key, key);
        assertTrue(result.body.vertex.value);
      } finally {
        sendRequest('DELETE', '/_api/transaction/' + encodeURIComponent(trx), {}, {}, true);
        sendRequest('DELETE', '/_api/gharial/myGraph?dropCollections=true', {}, {}, true);
      }
    }

  };
}

jsunity.run(TransactionsSuite);

return jsunity.done();
