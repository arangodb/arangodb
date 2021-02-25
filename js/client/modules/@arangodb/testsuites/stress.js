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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'stress_crud': 'stress_crud - todo',
  'stress_killing': 'stress_killing todo',
  'stress_locks': 'stress_locks todo'
};
const optionsDocumentation = [
];

const _ = require('lodash');
const pu = require('@arangodb/testutils/process-utils');
const yaml = require('js-yaml');

const download = require('internal').download;
const wait = require('internal').wait;

const testPaths = {
  'stress_crud': [],
  'stress_killing': [],
  'stress_locks': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a stress test on arangod
// //////////////////////////////////////////////////////////////////////////////

function runStressTest (options, command, testname) {
  const concurrency = options.concurrency;

  let extra = {
    'javascript.v8-contexts': concurrency + 2,
    'server.threads': concurrency + 2
  };

  let instanceInfo = pu.startInstance('tcp', options, extra, testname);

  if (instanceInfo === false) {
    return {
      status: false,
      message: 'failed to start server!'
    };
  }

  const requestOptions = pu.makeAuthorizationHeaders(options);
  requestOptions.method = 'POST';
  requestOptions.returnBodyOnError = true;
  requestOptions.headers = {
    'x-arango-async': 'store'
  };

  const reply = download(
    instanceInfo.url + '/_admin/execute?returnAsJSON=true',
    command,
    requestOptions);

  if (reply.error || reply.code !== 202) {
    print('cannot execute command: (' +
          reply.code + ') ' + reply.message);

    let shutdownStatus = pu.shutdownInstance(instanceInfo, options);

    return {
      status: false,
      message: reply.hasOwnProperty('body') ? reply.body : yaml.safeDump(reply),
      shutdown: shutdownStatus
    };
  }

  const id = reply.headers['x-arango-async-id'];

  const checkOpts = pu.makeAuthorizationHeaders(options);
  checkOpts.method = 'PUT';
  checkOpts.returnBodyOnError = true;

  while (true) {
    const check = download(
      instanceInfo.url + '/_api/job/' + id,
      '', checkOpts);

    if (!check.error) {
      if (check.code === 204) {
        print('still running (' + (new Date()) + ')');
        wait(60);
        continue;
      }

      if (check.code === 200) {
        print('stress test finished');
        break;
      }
    }

    print(yaml.safeDump(check));

    let shutdownStatus = pu.shutdownInstance(instanceInfo, options);

    return {
      status: false,
      message: check.hasOwnProperty('body') ? check.body : yaml.safeDump(check),
      shutdown: shutdownStatus
    };
  }

  print('Shutting down...');
  let shutdownStatus = pu.shutdownInstance(instanceInfo, options);
  print('done.');

  return {
    status: true,
    message: "",
    shutdown: shutdownStatus
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief STRESS TEST: stress_crud
// //////////////////////////////////////////////////////////////////////////////

function stressCrud (options) {
  const duration = options.duration;
  const concurrency = options.concurrency;

  const command = `
  const stressCrud = require('./' + tu.pathForTesting('server/stress/crud'));

  stressCrud.createDeleteUpdateParallel({
    concurrency: ${concurrency},
    duration: ${duration},
    gnuplot: true,
    pauseFor: 60
  })
  `;

  return runStressTest(options, command, 'stress_crud');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief STRESS TEST: stress_killing
// //////////////////////////////////////////////////////////////////////////////

function stressKilling (options) {
  const duration = options.duration;

  let opts = {
    concurrency: 4
  };

  _.defaults(opts, options);

  const command = `
  const stressCrud = require('./' + tu.pathForTesting('server/stress/killingQueries'));

  stressCrud.killingParallel({
    duration: ${duration},
    gnuplot: true
  })
  `;

  return runStressTest(opts, command, 'stress_killing');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief STRESS TEST: stress_locks
// //////////////////////////////////////////////////////////////////////////////

function stressLocks (options) {
  const duration = options.duration;
  const concurrency = options.concurrency;

  const command = `
  const deadlock = require('./' + tu.pathForTesting('server/stress/deadlock'));

  deadlock.lockCycleParallel({
    concurrency: ${concurrency},
    duration: ${duration},
    gnuplot: true
  })
  `;

  return runStressTest(options, command, 'stress_lock');
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['stress_crud'] = stressCrud;
  testFns['stress_killing'] = stressKilling;
  testFns['stress_locks'] = stressLocks;

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
