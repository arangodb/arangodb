/*jshint strict: true */
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const console = require('console');
const request = require('@arangodb/request');
const _ = require('lodash');
const db = require('@arangodb').db;
const {checkRequestResult} = require('@arangodb/arangosh');
const { defaultMaxListeners } = require('events');
const {
  assertEqual,
  assertFalse,
  assertNotNull,
  assertNotUndefined,
  assertTypeOf,
  assertIdentical,
} = jsunity.jsUnity.assertions;

const replication2Enabled = require('internal').db._version(true).details['replication2-enabled'] === 'true';

const getUrl = endpoint => endpoint.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');

const replicationApi = {
  createLog: (id, server) => {
    const res = request.post({
      url: getUrl(server.endpoint) + `/_api/log`,
      // Note that `targetConfig` is deserialized by the RestHandler, but ignored (on a DBServer, as is the case here).
      // Just the `id` is needed. We might want to change this if the API is kept, i.e. make passing `targetConfig`
      // obsolete.
      body: JSON.stringify({id, targetConfig: {waitForSync: false, writeConcern: 2}}),
    });
    checkRequestResult(res);
    assertTypeOf('object', res.json);
    assertIdentical(false, res.json.error, JSON.stringify(res.json));
  },

  becomeLeader: (id, server, {term, writeConcern, follower}) => {
    const res = request.post({
      url: getUrl(server.endpoint) + `/_api/log/${id}/becomeLeader`,
      body: JSON.stringify({term, writeConcern, follower: follower.map(server => server.id)}),
    });
    checkRequestResult(res);
    assertTypeOf('object', res.json);
    assertIdentical(false, res.json.error, JSON.stringify(res.json));
  },

  becomeFollower: (id, server, {term, leader}) => {
    const res = request.post({
      url: getUrl(server.endpoint) + `/_api/log/${id}/becomeFollower`,
      body: JSON.stringify({term, leader: leader.id}),
    });
    checkRequestResult(res);
    assertTypeOf('object', res.json);
    assertIdentical(false, res.json.error, JSON.stringify(res.json));
  },

  insert: (id, server, data) => {
    const res = request.post({
      url: getUrl(server.endpoint) + `/_api/log/${id}/insert`,
      body: JSON.stringify(data),
    });
    checkRequestResult(res);
    assertTypeOf('object', res.json);
    assertIdentical(false, res.json.error, JSON.stringify(res.json));
    const result = res.json.result;
    assertTypeOf('object', result, JSON.stringify(res.json));
    assertEqual(["index", "term", "quorum"].sort(), Object.keys(result).sort());
    return result;
  },

  readEntry: (id, server, entry) => {
    const res = request.get({
      url: getUrl(server.endpoint) + `/_api/log/${id}/readEntry/${entry}`,
    });
    checkRequestResult(res);
    assertTypeOf('object', res.json);
    assertIdentical(false, res.json.error, JSON.stringify(res.json));
    const result = res.json.result;
    assertEqual(["logIndex", "payload", "logTerm"].sort(), Object.keys(result).sort());
    return result;
  },

};

const runInDb = (name, callback) => {
  const oldDbName = db._name();
  try {
    db._useDatabase(name);
    callback();
  } catch (e) {
    db._useDatabase(oldDbName);
    throw e;
  }
  db._useDatabase(oldDbName);
};

function ddlSuite() {
  return {
    testCreateDatabaseReplicationVersion: function () {
      const dbName = 'replicationVersionDb';

      const tests = [
        // default to v1
        { params: undefined,
          expected: {properties: {replicationVersion: "1"}},
        },
        { params: null,
          expected: {properties: {replicationVersion: "1"}},
        },
        { params: {replicationVersion: undefined},
          expected: {properties: {replicationVersion: "1"}},
        },
        { params: {},
          expected: {properties: {replicationVersion: "1"}},
        },
        // set v1 explicitly
        { params: {replicationVersion: "1"},
          expected: {properties: {replicationVersion: "1"}},
        },
        // set v2 explicitly
        { params: {replicationVersion: "2"},
          expected: {properties: {replicationVersion: "2"}},
        },
        // erroneous inputs
        { params: {replicationVersion: null},
          expected: {error: true},
        },
        { params: {replicationVersion: "3"},
          expected: {error: true},
        },
        { params: {replicationVersion: "11"},
          expected: {error: true},
        },
        { params: {replicationVersion: "22"},
          expected: {error: true},
        },
        { params: {replicationVersion: -1},
          expected: {error: true},
        },
        { params: {replicationVersion: 0},
          expected: {error: true},
        },
        { params: {replicationVersion: 1},
          expected: {error: true},
        },
        { params: {replicationVersion: 2},
          expected: {error: true},
        },
        { params: {replicationVersion: '1 hund'},
          expected: {error: true},
        },
        { params: {replicationVersion: '2 hunde'},
          expected: {error: true},
        },
        { params: {replicationVersion: {some: 'object'}},
          expected: {error: true},
        },
        { params: {replicationVersion: ['some',  'array']},
          expected: {error: true},
        },
      ];

      const runCreateDbTest = (test) => {
        const {params, expected} = Object.assign({}, test);

        if (expected.hasOwnProperty('properties')) {
          // Creating the database should work, and we want to check the properties are as expected
          db._createDatabase(dbName, params);
          try {
            runInDb(dbName, () => {
              const props = db._properties();
              for (const prop of Object.keys(expected.properties)) {
                assertIdentical(props[prop], expected.properties[prop]);
              }
            });
          } catch(e) {
            // rather throw the first exception here, it'll be more interesting
            try { db._dropDatabase(dbName); } catch (e) {}
            throw e;
          }
          db._dropDatabase(dbName);
        } else if (expected.hasOwnProperty('error')) {
          // Creating the database should fail
          let success = false;
          try {
            db._createDatabase(dbName, params);
            success = true;
            // make testing.js happy
            db._dropDatabase(dbName);
          } catch (e) {}
          assertFalse(success);
        } else {
          throw Error('Unexpected test object, has neither "properties" nor "error": ' + JSON.stringify(test));
        }
      };

      for (const test of tests) {
        try {
          console.debug(`Running test ${JSON.stringify(test)}...`);
          runCreateDbTest(test);
        } catch (e) {
          if (e instanceof Error) {
            e.message = `When executing test ${JSON.stringify(test)}: ${e.message}`;
          } else if (typeof e === 'string') {
            throw `When executing test ${JSON.stringify(test)}: ${e}`;
          }
          throw e;
        }
      }
    },

    testCreateDatabaseWithOldReplication: function () {
      const dbName = 'replv1db';
      db._createDatabase(dbName, {replicationVersion: "1"});
      try {
        runInDb(dbName, () => {
          const props = db._properties();
          assertNotUndefined(props.replicationVersion);
          assertIdentical("1", props.replicationVersion);
        });
      } catch(e) {
        db._dropDatabase(dbName);
        throw e;
      }
      db._dropDatabase(dbName);
    },

    testCreateDatabaseWithNewReplication: function () {
      const dbName = 'replv2db';
      db._createDatabase(dbName, {replicationVersion: "2"});
      try {
        runInDb(dbName, () => {
          const props = db._properties();
          assertNotUndefined(props.replicationVersion);
          assertIdentical("2", props.replicationVersion);
        });
      } catch(e) {
        db._dropDatabase(dbName);
        throw e;
      }
      db._dropDatabase(dbName);
    },
  };
}

if (replication2Enabled) {
  jsunity.run(ddlSuite);
}
return jsunity.done();
