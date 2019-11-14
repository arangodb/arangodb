/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, require*/

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

const base64Encode = require('internal').base64Encode;
const db = require("internal").db;
const request = require("@arangodb/request");
const url = require('url');
const userModule = require("@arangodb/users");
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
  const users = [
    { username: 'alice', password: 'pass1' },
    { username: 'bob', password: 'pass2' },
  ];

  function sendRequest(auth, method, endpoint, body, usePrimary) {
    let res;
    const i = usePrimary ? 0 : 1;
    try {
      const envelope = {
        headers: {
          authorization:
            `Basic ${base64Encode(auth.username + ':' + auth.password)}`
        },
        json: true,
        method,
        url: `${coordinators[i]}${endpoint}`
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
      
      try {
        userModule.remove(users[0].username);
        userModule.remove(users[1].username);
      } catch (err) {}
      userModule.save(users[0].username, users[0].password);
      userModule.save(users[1].username, users[1].password);

      userModule.grantDatabase(users[0].username, '_system', 'rw');
      userModule.grantDatabase(users[1].username, '_system', 'rw');
      userModule.grantCollection(users[0].username, '_system', cn, 'rw');
      userModule.grantCollection(users[1].username, '_system', cn, 'rw');
      
      require("internal").wait(2);
    },

    tearDown: function() {
      coordinators = [];
      db._drop(cn);
      userModule.remove(users[0].username);
      userModule.remove(users[1].username);
    },

    testListTransactions: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let result = sendRequest(users[0], 'POST', url + "/begin", obj, true);

      assertEqual(result.status, 201);
      assertFalse(result.body.result.id === undefined);

      let trx1 = result.body.result;

      try {
        result = sendRequest(users[0], 'GET', url, {}, true);

        assertEqual(result.status, 200);
        assertInList(result.body.transactions, trx1);

        result = sendRequest(users[0], 'GET', url, {}, false);
        
        assertEqual(result.status, 200);
        assertInList(result.body.transactions, trx1);
      } finally {
        sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);
      }
    },
    
    testListTransactions2: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let trx1, trx2;
      
      try {
        let result = sendRequest(users[0], 'POST', url + "/begin", obj, true);
        assertEqual(result.status, 201);
        assertFalse(result.body.result.id === undefined);
        trx1 = result.body.result;
        
        result = sendRequest(users[0], 'POST', url + "/begin", obj, false);
        assertEqual(result.status, 201);
        assertFalse(result.body.result.id === undefined);
        trx2 = result.body.result;
      
        result = sendRequest(users[0], 'GET', url, {}, true);

        assertEqual(result.status, 200);
        assertInList(result.body.transactions, trx1);
        assertInList(result.body.transactions, trx2);

        result = sendRequest(users[0], 'GET', url, {}, false);
        
        assertEqual(result.status, 200);
        assertInList(result.body.transactions, trx1);
        assertInList(result.body.transactions, trx2);

        // commit trx1 on different coord
        result = sendRequest(users[0], 'PUT', url + "/" + encodeURIComponent(trx1.id), {}, false);
        assertEqual(trx1.id, result.body.result.id);
        assertEqual("committed", result.body.result.status);
        
        result = sendRequest(users[0], 'GET', url, {}, false);
        
        assertEqual(result.status, 200);
        assertNotInList(result.body.transactions, trx1);
        assertInList(result.body.transactions, trx2);

        // abort trx2 on different coord
        result = sendRequest(users[0], 'DELETE', url + "/" + encodeURIComponent(trx2.id), {}, true);
        assertEqual(trx2.id, result.body.result.id);
        assertEqual("aborted", result.body.result.status);
        
        result = sendRequest(users[0], 'GET', url, {}, false);
        
        assertEqual(result.status, 200);
        assertNotInList(result.body.transactions, trx1);
        assertNotInList(result.body.transactions, trx2);

      } finally {
        sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);
      }
    },
    
    testCreateAndCommitElsewhere: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let result = sendRequest(users[0], 'POST', url + "/begin", obj, true);

      assertEqual(result.status, 201);
      assertFalse(result.body.result.id === undefined);

      let trx1 = result.body.result;

      try {
        // commit on different coord
        result = sendRequest(users[0], 'PUT', url + "/" + encodeURIComponent(trx1.id), {}, false);

        assertEqual(result.status, 200);
        assertEqual(trx1.id, result.body.result.id);
        assertEqual("committed", result.body.result.status);
        
        result = sendRequest(users[0], 'GET', url, {}, true);
        assertNotInList(result.body.transactions, trx1);
      } finally {
        sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);
      }
    },
    
    testCreateAndAbortElsewhere: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let result = sendRequest(users[0], 'POST', url + "/begin", obj, true);

      assertEqual(result.status, 201);
      assertFalse(result.body.result.id === undefined);

      let trx1 = result.body.result;

      try {
        // abort on different coord
        result = sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);

        assertEqual(result.status, 200);
        assertEqual(trx1.id, result.body.result.id);
        assertEqual("aborted", result.body.result.status);
        
        result = sendRequest(users[0], 'GET', url, {}, true);
        assertNotInList(result.body.transactions, trx1);
      } finally {
        sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);
      }
    },

    testCreateAndCommitDifferentUser: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let result = sendRequest(users[0], 'POST', url + "/begin", obj, true);

      assertEqual(result.status, 201);
      assertFalse(result.body.result.id === undefined);

      let trx1 = result.body.result;

      try {
        // commit with different user
        result = sendRequest(users[1], 'PUT', url + "/" + encodeURIComponent(trx1.id), {}, false);
        // should fail with not found
        assertEqual(result.status, 404);

        result = sendRequest(users[0], 'PUT', url + "/" + encodeURIComponent(trx1.id), {}, false);
        assertEqual(trx1.id, result.body.result.id);
        assertEqual("committed", result.body.result.status);
        
        result = sendRequest(users[0], 'GET', url, {}, true);
        assertNotInList(result.body.transactions, trx1);
      } finally {
        sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);
      }
    },
    
    testCreateAndAbortDifferentUser: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let result = sendRequest(users[0], 'POST', url + "/begin", obj, true);

      assertEqual(result.status, 201);
      assertFalse(result.body.result.id === undefined);

      let trx1 = result.body.result;

      try {
        // abort with different user
        result = sendRequest(users[1], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);
        // should fail with not found
        assertEqual(result.status, 404);

        result = sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);

        assertEqual(result.status, 200);
        assertEqual(trx1.id, result.body.result.id);
        assertEqual("aborted", result.body.result.status);
        
        result = sendRequest(users[0], 'GET', url, {}, true);
        assertNotInList(result.body.transactions, trx1);
      } finally {
        sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);
      }
    },

    testCreateAndGetDifferentUser: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let result = sendRequest(users[0], 'POST', url + "/begin", obj, true);

      assertEqual(result.status, 201);
      assertFalse(result.body.result.id === undefined);

      let trx1 = result.body.result;

      try {
        // abort with different user
        result = sendRequest(users[1], 'GET', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);
        // should fail with not found
        assertEqual(result.status, 404);

        result = sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);

        assertEqual(result.status, 200);
        assertEqual(trx1.id, result.body.result.id);
        assertEqual("aborted", result.body.result.status);
        
        result = sendRequest(users[0], 'GET', url, {}, true);
        assertNotInList(result.body.transactions, trx1);
      } finally {
        sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);
      }
    },

    testCreateAndGetListDifferentUser: function() {
      const obj = { collections: { read: cn } };

      let url = "/_api/transaction";
      let result = sendRequest(users[0], 'POST', url + "/begin", obj, true);

      assertEqual(result.status, 201);
      assertFalse(result.body.result.id === undefined);

      let trx1 = result.body.result;

      try {
        // get list with different user
        result = sendRequest(users[1], 'GET', url, {}, true);
        // should not find it in list
        assertNotInList(result.body.transactions, trx1);

        result = sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);

        assertEqual(result.status, 200);
        assertEqual(trx1.id, result.body.result.id);
        assertEqual("aborted", result.body.result.status);
        
        result = sendRequest(users[0], 'GET', url, {}, true);
        assertNotInList(result.body.transactions, trx1);
      } finally {
        sendRequest(users[0], 'DELETE', '/_api/transaction/' + encodeURIComponent(trx1.id), {}, true);
      }
    },

  };
}

jsunity.run(TransactionsSuite);

return jsunity.done();
