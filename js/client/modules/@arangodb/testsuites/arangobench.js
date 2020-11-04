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
  'arangobench': 'arangobench tests'
};
const optionsDocumentation = [
  '   - `skipArangoBenchNonConnKeepAlive`: if set to true benchmark do not use keep-alive',
  '   - `skipArangoBench`: if set to true benchmark tests are skipped',
  '',
  '   - `benchargs`: additional commandline arguments to arangobench'
];

const _ = require('lodash');
const pu = require('@arangodb/testutils/process-utils');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'arangobench': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: arangobench
// //////////////////////////////////////////////////////////////////////////////

const benchTodos = [{
  'requests': '10000',
  'concurrency': '2',
  'test-case': 'version',
  'keep-alive': 'false'
}, {
  'requests': '10000',
  'concurrency': '2',
  'test-case': 'version',
  'async': 'true'
}, {
  'requests': '20000',
  'concurrency': '1',
  'test-case': 'version',
  'async': 'true'
}, {
  'requests': '10000',
  'concurrency': '3',
  'test-case': 'stream-cursor',
  'complexity': '4'
}, {
  'requests': '100000',
  'concurrency': '2',
  'test-case': 'shapes',
  'batch-size': '16',
  'complexity': '2'
}, {
  'requests': '100000',
  'concurrency': '2',
  'test-case': 'shapes-append',
  'batch-size': '16',
  'complexity': '4'
}, {
  'requests': '100000',
  'concurrency': '2',
  'test-case': 'random-shapes',
  'batch-size': '16',
  'complexity': '2'
}, {
  'requests': '1000',
  'concurrency': '2',
  'test-case': 'version',
  'batch-size': '16'
}, {
  'requests': '100',
  'concurrency': '1',
  'test-case': 'version',
  'batch-size': '0'
}, {
  'requests': '100',
  'concurrency': '2',
  'test-case': 'document',
  'batch-size': '10',
  'complexity': '1'
}, {
  'requests': '2000',
  'concurrency': '2',
  'test-case': 'crud',
  'complexity': '1'
}, {
  'requests': '4000',
  'concurrency': '2',
  'test-case': 'crud-append',
  'complexity': '4'
}, {
  'requests': '4000',
  'concurrency': '2',
  'test-case': 'edge',
  'complexity': '4'
}, {
  'requests': '5000',
  'concurrency': '2',
  'test-case': 'hash',
  'complexity': '1'
}, {
  'requests': '5000',
  'concurrency': '2',
  'test-case': 'skiplist',
  'complexity': '1'
}, {
  'requests': '500',
  'concurrency': '3',
  'test-case': 'aqltrx',
  'complexity': '1',
  'transaction': true
}, {
  'requests': '1000',
  'concurrency': '4',
  'test-case': 'aqltrx',
  'complexity': '1',
  'transaction': true
}, {
  'requests': '100',
  'concurrency': '3',
  'test-case': 'counttrx',
  'transaction': true
}, {
  'requests': '500',
  'concurrency': '3',
  'test-case': 'multitrx',
  'transaction': true
}];

function arangobench (options) {
  if (options.skipArangoBench === true) {
    print('skipping Benchmark tests!');
    return {
      arangobench: {
        status: true,
        skipped: true
      }
    };
  }

  print(CYAN + 'arangobench tests...' + RESET);

  let instanceInfo = pu.startInstance('tcp', options, {}, 'arangobench');

  if (instanceInfo === false) {
    return {
      arangobench: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  let results = { failed: 0 };
  let continueTesting = true;

  for (let i = 0; i < benchTodos.length; i++) {
    const benchTodo = benchTodos[i];
    const name = 'case' + i;

    if ((options.skipArangoBenchNonConnKeepAlive) &&
        benchTodo.hasOwnProperty('keep-alive') &&
        (benchTodo['keep-alive'] === 'false')) {
      benchTodo['keep-alive'] = true;
    }

    // On the cluster we do not yet have working transaction functionality:
    if (!options.cluster || !benchTodo.transaction) {
      if (!continueTesting) {
        print(RED + 'Skipping ' + benchTodo + ', server is gone.' + RESET);

        results[name] = {
          status: false,
          message: instanceInfo.exitStatus
        };

        instanceInfo.exitStatus = 'server is gone.';

        break;
      }

      let args = _.clone(benchTodo);
      delete args.transaction;

      if (options.hasOwnProperty('benchargs')) {
        args = Object.assign(args, options.benchargs);
      }

      let oneResult = pu.run.arangoBenchmark(options, instanceInfo, args, instanceInfo.rootDir, options.coreCheck);
      print();

      results[name] = oneResult;
      results[name].total++;
      results[name].failed = 0;

      if (!results[name].status) {
        results[name].failed = 1;
        results.status = false;
        results.failed += 1;
      }

      continueTesting = pu.arangod.check.instanceAlive(instanceInfo, options);

      if (oneResult.status !== true && !options.force) {
        break;
      }
    }
  }

  print(CYAN + 'Shutting down...' + RESET);
  results['shutdown'] = pu.shutdownInstance(instanceInfo, options);
  print(CYAN + 'done.' + RESET);

  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['arangobench'] = arangobench;

  defaultFns.push('arangobench');

  opts['skipArangoBench'] = false;
  opts['skipArangoBenchNonConnKeepAlive'] = true;

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
