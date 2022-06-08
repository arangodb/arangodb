/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertEqual, assertNotEqual, require*/

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
const _ = require("lodash");
const isEnterprise = require("internal").isEnterprise();
const ERRORS = require("@arangodb").errors;
let isCluster = require("internal").isCluster();

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

function KeyGeneratorSuite() {
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
    } catch (err) {
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

  function generateCollectionAndTest(name) {
    let lastKey = null;
    let url = "/_db/" + cn + "/_api/document/" + name;
    let keyOptions = {};
    let increment = 1;
    if (Number(name[name.length - 1]) === 1) {
      keyOptions = {keyOptions: {type: "autoincrement"}};
    } else if (Number(name[name.length - 1]) === 2) {
      increment = 55;
      keyOptions = {keyOptions: {type: "autoincrement", offset: 10, increment: increment}};
    } else {
      increment = 10;
      keyOptions = {keyOptions: {type: "autoincrement", offset: 4, increment: increment}};
    }
    db._create(name, keyOptions);
    assertNotEqual("", db[name].properties().distributeShardsLike);

    for (let i = 0; i < 10000; ++i) {
      let result = sendRequest('POST', url, /*payload*/ {}, {}, i % 2 === 0);
      assertEqual(result.status, 202);
      let key = result.body._key;
      assertTrue(Number(key) === Number(lastKey) + increment || lastKey === null, {key, lastKey});
      lastKey = key;
    }
  }

  return {
    setUpAll: function() {
      coordinators = getCoordinators();
      if (coordinators.length < 2) {
        throw new Error('Expecting at least two coordinators');
      }
    },

    testPadded: function() {
      // check that the generated keys are sequential when we send the requests
      // via multiple coordinators.
      db._create(cn, {numberOfShards: 1, keyOptions: {type: "padded"}});

      try {
        let lastKey = null;
        let url = "/_api/document/" + cn;
        // send documents to both coordinators
        for (let i = 0; i < 10000; ++i) {
          let result = sendRequest('POST', url, /*payload*/ {}, {}, i % 2 === 0);
          assertEqual(result.status, 202);
          let key = result.body._key;
          assertTrue(key > lastKey || lastKey === null, {key, lastKey});
          lastKey = key;
        }
      } finally {
        db._drop(cn);
      }
    },

    testPaddedOnOneShard: function() {
      if (!isEnterprise) {
        return;
      }

      db._createDatabase(cn, {sharding: "single"});
      try {
        db._useDatabase(cn);

        // check that the generated keys are sequential when we send the requests
        // via multiple coordinators.
        db._create(cn, {keyOptions: {type: "padded"}});
        assertNotEqual("", db[cn].properties().distributeShardsLike);

        let lastKey = null;
        let url = "/_db/" + cn + "/_api/document/" + cn;
        // send documents to both coordinators
        for (let i = 0; i < 10000; ++i) {
          let result = sendRequest('POST', url, /*payload*/ {}, {}, i % 2 === 0);
          assertEqual(result.status, 202);
          let key = result.body._key;
          assertTrue(key > lastKey || lastKey === null, {key, lastKey});
          lastKey = key;
        }
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },

    testAutoincrementOnMultipleShards: function() {
      try {
        db._create(cn, {numberOfShards: 5, keyOptions: {type: "autoincrement"}});
      } catch (error) {
        assertEqual(ERRORS.ERROR_CLUSTER_UNSUPPORTED.code, error.errorNum);
      }
    },

    testAutoincrementOnOneShard: function() {
      if (!isEnterprise || !isCluster) {
        return;
      }
      db._createDatabase(cn, {sharding: "single"});
      try {
        db._useDatabase(cn);
        for (let i = 1; i < 4; ++i) {
          generateCollectionAndTest(cn + i);
        }
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },

  };
}

jsunity.run(KeyGeneratorSuite);

return jsunity.done();
