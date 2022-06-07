/*jshint strict: true */
/*global assertTrue, assertEqual*/
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const request = require("@arangodb/request");
const _ = require('lodash');
const db = arangodb.db;
const lh = require("@arangodb/testutils/replicated-logs-helper");
const sh = require("@arangodb/testutils/replicated-state-helper");
const spreds = require("@arangodb/testutils/replicated-state-predicates");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");

const database = "replication2_prototype_state_test_db";

const insertEntries = function (url, stateId, payload) {
  return request.post({
    url: `${url}/_db/${database}/_api/prototype-state/${stateId}/insert`,
    body: payload,
    json: true
  });
};

const compareExchange = function (url, stateId, payload) {
  return request.put({
    url: `${url}/_db/${database}/_api/prototype-state/${stateId}/cmp-ex`,
    body: payload,
    json: true
  });
};

const getEntry = function (url, stateId, entry, waitForApplied) {
  if (waitForApplied === undefined) {
    return request.get({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/entry/${entry}`});
  }
  return request.get({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/entry/${entry}?waitForApplied=${waitForApplied}`});
};

const getEntries = function (url, stateId, entries, waitForApplied) {
  let params = (waitForApplied === undefined ? '' : `?waitForApplied=${waitForApplied}`);
  return request.post({
    url: `${url}/_db/${database}/_api/prototype-state/${stateId}/multi-get${params}`,
    body: entries,
    json: true
  });
};

const removeEntry = function (url, stateId, entry) {
  return request.delete({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/entry/${entry}`});
};

const dropState = function (url, stateId) {
  return request.delete({url: `${url}/_db/${database}/_api/replicated-state/${stateId}`});
};

const removeEntries = function (url, stateId, entries) {
  return request.delete({
    url: `${url}/_db/${database}/_api/prototype-state/${stateId}/multi-remove`,
    body: entries,
    json: true
  });
};

const getSnapshot = function (url, stateId, waitForIndex) {
  return request.get({url: `${url}/_db/${database}/_api/prototype-state/${stateId}/snapshot?waitForIndex=${waitForIndex}`});
};

const replicatedStateSuite = function () {
  const {setUpAll, tearDownAll} = (function () {
    let previousDatabase, databaseExisted = true;
    return {
      setUpAll: function () {
        previousDatabase = db._name();
        if (!_.includes(db._databases(), database)) {
          db._createDatabase(database);
          databaseExisted = false;
        }
        db._useDatabase(database);
      },

      tearDownAll: function () {
        db._useDatabase(previousDatabase);
        if (!databaseExisted) {
          db._dropDatabase(database);
        }
      },
    };
  }());

  return {
    setUpAll, tearDownAll,
    setUp: lh.registerAgencyTestBegin,
    tearDown: lh.registerAgencyTestEnd,

    testPrototypeReplicatedStateMethods: function() {
      const stateId = lh.nextUniqueLogId();

      const servers = _.sampleSize(lh.dbservers, 3);
      let participants = {};
      for (const server of servers) {
        participants[server] = {};
      }

      sh.updateReplicatedStateTarget(database, stateId,
                                  function(target) {
                                    return {
                                      id: stateId,
                                      participants: participants,
                                      config: {
                                        waitForSync: true,
                                        writeConcern: 2,
                                        softWriteConcern: 3,
                                      },
                                      properties: {
                                        implementation: {
                                          type: "prototype"
                                        }
                                      }
                                    };
      });

      lh.waitFor(spreds.replicatedStateIsReady(database, stateId, servers));
      lh.waitForReplicatedLogAvailable(stateId);

      let {leader} = lh.getReplicatedLogLeaderPlan(database, stateId);
      let leaderUrl = lh.getServerUrl(leader);

      let coord = lh.coordinators[0];
      let coordUrl = lh.getServerUrl(coord);

      let follower = null;
      for (const server of servers) {
        if (server !== leader) {
          follower = server;
          break;
        }
      }
      let followerUrl = lh.getServerUrl(follower);

      let result = insertEntries(leaderUrl, stateId, {foo0 : "bar0", foo1: "bar1", foo2: "bar2"});
      lh.checkRequestResult(result);
      let index = result.json.result.index;
      assertEqual(index, 2);

      result = getEntry(leaderUrl, stateId, "foo0", index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result.foo0,  "bar0");

      result = getEntry(followerUrl, stateId, "foo0", index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result.foo0,  "bar0");

      result = getEntries(leaderUrl, stateId, ["foo1", "foo2"], index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result, {foo1: "bar1", foo2: "bar2"});

      result = getEntries(followerUrl, stateId, ["foo1", "foo2"], index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result, {foo1: "bar1", foo2: "bar2"});

      result = removeEntry(leaderUrl, stateId, "foo0");
      lh.checkRequestResult(result);
      index = result.json.result.index;
      assertEqual(index, 3);

      result = getEntry(leaderUrl, stateId, "foo0", index);
      assertEqual(result.json.code, 404);

      result = removeEntries(leaderUrl, stateId, ["foo1", "foo2"]);
      lh.checkRequestResult(result);
      index = result.json.result.index;
      assertEqual(index, 4);

      result = getEntry(leaderUrl, stateId, "foo2", index);
      assertEqual(result.json.code, 404);

      result = insertEntries(coordUrl, stateId, {foo100: "bar100", foo200: "bar200", foo300: "bar300", foo400: "bar400"});
      lh.checkRequestResult(result);
      index = result.json.result.index;
      assertEqual(index, 5);

      result = getEntry(coordUrl, stateId, "foo100", index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result.foo100,  "bar100");

      result = getEntries(coordUrl, stateId, ["foo200", "foo300"], index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result.foo200,  "bar200");
      assertEqual(result.json.result.foo300,  "bar300");

      result = removeEntry(coordUrl, stateId, "foo300");
      lh.checkRequestResult(result);
      index = result.json.result.index;
      assertEqual(index, 6);

      result = removeEntries(coordUrl, stateId, ["foo200", "foo300"]);
      lh.checkRequestResult(result);
      index = result.json.result.index;
      assertEqual(index, 7);

      result = getEntry(leaderUrl, stateId, "foo400", index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result.foo400,  "bar400");

      result = getSnapshot(leaderUrl, stateId, index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result, {foo100: "bar100", foo400: "bar400"});

      result = removeEntry(coordUrl, stateId, "foo100");
      lh.checkRequestResult(result);
      index = result.json.result.index;
      assertEqual(index, 8);

      result = getSnapshot(leaderUrl, stateId, index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result, {foo400: "bar400"});

      result = compareExchange(leaderUrl, stateId, {"foo400": {"oldValue": "bar400", "newValue": "foobar"}});
      lh.checkRequestResult(result);
      index = result.json.result.index;
      assertEqual(index, 9);
      result = getEntry(coordUrl, stateId, "foo400", index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result.foo400,  "foobar");

      result = compareExchange(leaderUrl, stateId, {"foo400": {"oldValue": "bar400", "newValue": "foobar"}});
      assertEqual(result.json.code, 409);
      result = getEntry(coordUrl, stateId, "foo400", index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result.foo400,  "foobar");

      result = compareExchange(coordUrl, stateId, {"foo400": {"oldValue": "foobar", "newValue": "bar400"}});
      lh.checkRequestResult(result);
      index = result.json.result.index;
      assertEqual(index, 10);
      result = getEntry(coordUrl, stateId, "foo400", index);
      lh.checkRequestResult(result);
      assertEqual(result.json.result.foo400,  "bar400");

      result = compareExchange(coordUrl, stateId, {"foo400": {"oldValue": "foobar", "newValue": "bar400"}});
      assertEqual(result.json.code, 409);

      dropState(coordUrl, stateId);
      lh.waitFor(spreds.replicatedStateIsGone(database, stateId));
      lh.waitFor(lpreds.replicatedLogIsGone(database, stateId));
    },
  };
};

jsunity.run(replicatedStateSuite);
return jsunity.done();
