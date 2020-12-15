/* jshint strict: false, sub: true */
/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2014 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
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
  'queryCacheAuthorization': 'authorization check for query cache'
};
const optionsDocumentation = [
  '   - `skipQueryCacheAuthorization` : if set to true the read only tests are skipped'
];

const pu = require('@arangodb/process-utils');
const request = require('@arangodb/request');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'queryCacheAuthorization': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: queryCacheAuthorization
// //////////////////////////////////////////////////////////////////////////////

function queryCacheAuthorization (options) {
  const results = { failed: 0 };

  if (options.skipQueryCacheAuthorization === true) {
    print('skipping queryCacheAuthorization tests!');
    return {
      failed: 0,
      queryCacheAuthorization: {
        failed: 0,
        status: true,
        skipped: true
      }
    };
  } // if

  if (options.cluster) {
    print('skipping queryCacheAuthorization tests on cluster!');
    return {
      failed: 0,
      queryCacheAuthorization: {
        failed: 0,
        status: true,
        skipped: true
      }
    };
  }

  const conf = {
    'server.authentication': true,
    'server.authentication-system-only': false
  };

  print(CYAN + 'queryCacheAuthorization tests...' + RESET);

  const adbInstance = pu.startInstance('tcp', options, conf, 'queryCacheAuthorization');
  if (adbInstance === false) {
    results.failed += 1;
    results['test'] = {
      failed: 1,
      status: false,
      message: 'failed to start server!'
    };
  }

  const requests = [
    [200, 'put', '/_api/query-cache/properties', 'root', {mode: 'on', maxResults: 128}],
    [201, 'post', '/_api/cursor', 'root', {query: 'for d in testcol filter d.a == 2 return d'}],
    [201, 'post', '/_api/cursor', 'root', {query: 'for d in testcol filter d.a == 2 return d'}],
    [403, 'post', '/_api/cursor', 'test', {query: 'for d in testcol filter d.a == 2 return d'}]
  ];

  const run = (tests) => {
    const bodies = [];
    for (const r of tests) {
      const res = request[r[1]]({
        url: `${adbInstance.arangods[0].url}${r[2]}`,
        body: Object.keys(r[4]).length ? JSON.stringify(r[4]) : '',
        auth: {username: r[3], password: ''}
      });
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
          status: false
        };
      }
    }
    return bodies;
  };

  pu.run.arangoshCmd(
    options, adbInstance, {}, [
      '--javascript.execute-string',
      `const users = require('@arangodb/users');
      users.save('test', '', true);
      users.grantDatabase('test', '_system', 'ro');

      db._createDocumentCollection('testcol');
      db.testcol.save({_key:'one', a:1});
      db.testcol.save({_key:'two', a:2});

      users.grantCollection('test', '_system', 'testcol', 'none');

      users.reload();`
    ],
  options.coreCheck);

  run(requests);
  results['shutdown'] = pu.shutdownInstance(adbInstance, options);

  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['queryCacheAuthorization'] = queryCacheAuthorization;

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
