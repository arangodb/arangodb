/* jshint strict: false, sub: true */
/* global print */
'use strict';

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
// / @author Manuel Baesler
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'readOnly': 'read only tests'
};

const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const ct = require('@arangodb/testutils/client-tools');
const im = require('@arangodb/testutils/instance-manager');
const request = require('@arangodb/request');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'readOnly': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: readOnly
// //////////////////////////////////////////////////////////////////////////////

function readOnly (options) {
  const results = { failed: 0 };
  if (options.cluster) {
    print('skipping readOnly tests on cluster!');
    return {
      failed: 0,
      readOnly: {
        failed: 0,
        status: true,
        skipped: true
      }
    };
  }

  const conf = Object.assign(
    {},
    tu.testServerAuthInfo, {
      'server.authentication-system-only': false
    });

  print(CYAN + 'readOnly tests...' + RESET);
  
  let instanceManager = new im.instanceManager('tcp', options, conf, 'readOnly');
  instanceManager.prepareInstance();
  instanceManager.launchTcpDump("");
  if (!instanceManager.launchInstance()) {
    return {
      readOnly: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }
  instanceManager.reconnect();

  const requests = [
    [200, 'post', '/_api/collection', 'root', {name: 'testcol'}],
    [202, 'post', '/_api/document/testcol', 'root', {_key: 'abcd'}],
    [201, 'post', '/_api/index?collection=testcol', 'root', {fields: ['abc'], type: 'hash'}],
    [200, 'get', '/_api/index?collection=testcol', 'root', {}],

    // create and delete index
    [403, 'delete', '/_api/index/', 'test', {}],
    [403, 'post', '/_api/index?collection=testcol', 'test', {fields: ['xyz'], type: 'hash'}],

    // create, delete, truncate collection
    [403, 'post', '/_api/collection', 'test', {name: 'testcol2'}],
    [403, 'delete', '/_api/collection/testcol', 'test', {}],
    [403, 'put', '/_api/collection/testcol/truncate', 'test', {}],

    // get, delete, update, replace document
    [200, 'get', '/_api/document/testcol/abcd', 'test', {}],
    [403, 'post', '/_api/document/testcol', 'test', {_key: 'wxyz'}],
    [403, 'delete', '/_api/document/testcol/abcd', 'test', {}],
    [403, 'patch', '/_api/document/testcol/abcd', 'test', {foo: 'bar'}],
    [403, 'put', '/_api/document/testcol/abcd', 'test', {foo: 'bar'}],

    // create, delete database
    [201, 'post', '/_db/_system/_api/database', 'root', {name: 'wxyzdb'}],
    [403, 'post', '/_db/_system/_api/database', 'test', {name: 'abcddb'}],
    [403, 'delete', '/_db/_system/_api/database/wxyzdb', 'test', {}],

    // database with only collection access
    [200, 'get', '/_db/testdb2/_api/document/testcol2/one', 'test', {}],
    [200, 'get', '/_db/testdb2/_api/document/testcol3/one', 'test', {}],
    [403, 'post', '/_db/testdb2/_api/document/testcol2', 'test', {_key: 'wxyz'}],
    [403, 'post', '/_db/testdb2/_api/document/testcol3', 'test', {_key: 'wxyz'}],

    [200, 'get', '/_db/testdb2/_api/document/testcol2/one', 'test2', {}],
    [200, 'get', '/_db/testdb2/_api/document/testcol3/one', 'test2', {}],
    [202, 'post', '/_db/testdb2/_api/document/testcol2', 'test2', {_key: 'wxyz'}],
    [403, 'post', '/_db/testdb2/_api/document/testcol3', 'test2', {_key: 'wxyz'}],

    [200, 'get', '/_db/testdb2/_api/document/testcol2/wxyz', 'test', {}],
    [403, 'delete', '/_db/testdb2/_api/document/testcol2/wxyz', 'test', {}]
  ];

  const run = (tests) => {
    const bodies = [];
    for (const r of tests) {
      let req = {
        url: `${instanceManager.url}${r[2]}`,
        body: Object.keys(r[4]).length ? JSON.stringify(r[4]) : '',
        auth: { username: r[3], password: '' },
        timeout: 60.0
      };
      const res = request[r[1]](req);
      try {
        bodies.push(JSON.parse(res.body));
      } catch (e) {
        bodies.push({});
      }
      r.splice(1, 0, res.statusCode);
      if (r[0] === r[1]) {
        results[r.slice(0, 5).join('_')] = {
          failed: 0,
          status: true
        };
      } else {
        results.failed += 1;
        results[r.slice(0, 5).join('_')] = {
          failed: 1,
          status: false,
          message: "Result: expected " + r[0] + " got " + r[1] + " - req: " + JSON.stringify(req) + " - reply: " + JSON.stringify(res)
        };
      }
    }
    return bodies;
  };

  let res = ct.run.arangoshCmd(options, instanceManager, {}, [
    '--javascript.execute-string',
    `const users = require('@arangodb/users');
    users.save('test', '', true);
    users.save('test2', '', true);
    users.grantDatabase('test', '_system', 'ro');

    db._createDatabase('testdb2');
    db._useDatabase('testdb2');
    db._createDocumentCollection('testcol2');
    db._createDocumentCollection('testcol3');
    db.testcol2.save({_key: 'one'});
    db.testcol3.save({_key: 'one'});
    users.grantDatabase('test', 'testdb2', 'ro');
    users.grantCollection('test', 'testdb2', 'testcol2', 'ro');
    users.grantDatabase('test2', 'testdb2', 'ro');
    users.grantCollection('test2', 'testdb2', 'testcol2', 'rw');
    db._useDatabase('_system');
    /* let res = db._query("for u in _users filter u.user == 'test' return u").toArray();
       print(res); */`
  ],
  options.coreCheck);

  if (res.status !== true) {
    let shutdownStatus = instanceManager.shutdownInstance();
    instanceManager.destructor(false);
    return {
      readOnly : {
        status: false,
        total: 1,
        message: 'the readonly suite failed to setup the environment.',
        duration: 2,
        failed: 1,
        shutdown: shutdownStatus,
        failTest: {
          status: false,
          total: 1,
          duration: 1,
          message: 'the readonly suite failed to setup the environment.'
        }
      }
    };
  }
  let bodies = run(requests.splice(0, 4));
  requests[0][2] += bodies.pop().indexes.filter(idx => idx.type === 'hash')[0].id;
  run(requests);

  results['shutdown'] = instanceManager.shutdownInstance();
  instanceManager.destructor(results.failed === 0);

  return results;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['readOnly'] = readOnly;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
