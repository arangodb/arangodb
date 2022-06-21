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

const fs = require('fs');
const _ = require('lodash');
const pu = require('@arangodb/testutils/process-utils');
const internal = require('internal');

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
  'histogram.generate': true,
  'requests': '10000',
  'threads': '2',
  'test-case': 'version',
  'keep-alive': 'false'
}, {
  'histogram.generate': true,
  'requests': '10000',
  'threads': '2',
  'test-case': 'version',
  'async': 'true'
}, {
  'histogram.generate': true,
  'requests': '20000',
  'threads': '1',
  'test-case': 'version',
  'async': 'true'
}, {
  'histogram.generate': true,
  'requests': '1000',
  'threads': '2',
  'test-case': 'version',
  'batch-size': '16'
}, {
  'histogram.generate': true,
  'requests': '100',
  'threads': '1',
  'test-case': 'version',
  'batch-size': '0'
}, {
  'histogram.generate': true,
  'requests': '100',
  'threads': '2',
  'test-case': 'document',
  'batch-size': '10',
  'complexity': '1'
}, {
  'histogram.generate': true,
  'requests': '2000',
  'threads': '2',
  'test-case': 'crud',
  'complexity': '1'
}, {
  'histogram.generate': true,
  'requests': '4000',
  'threads': '2',
  'test-case': 'crud-append',
  'complexity': '4'
}, {
  'histogram.generate': true,
  'requests': '4000',
  'threads': '2',
  'test-case': 'edge',
  'complexity': '4'
}, {
  'histogram.generate': true,
  'requests': '5000',
  'threads': '2',
  'test-case': 'persistent-index',
  'complexity': '1'
},{
  'histogram.generate': true,
  'requests': '1',
  'threads': '1',
  'test-case': 'version',
  'keep-alive': 'true',
  'server.database': 'arangobench_testdb',
  'create-database': true
},{
  'histogram.generate': true,
  'requests': '100',
  'threads': '1',
  'test-case': 'custom-query',
  'custom-query': 'RETURN 1',
  'keep-alive': 'true',
  // test with Unicode database name
  'server.database': 'c\\1234 @!§$ имя базы данных юникода!\'',
  'create-database': true
},{
  'histogram.generate': true,
  'requests': '100',
  'threads': '1',
  'test-case': 'version',
  'keep-alive': 'true',
  // test with Unicode database name
  'server.database': '이것은 테스트입니까 ! @abc " mötör',
  'create-database': true
}, {
  'histogram.generate': true,
  'requests': '100',
  'threads': '1',
  'test-case': 'version',
  'keep-alive': 'true',
  // test with Unicode database name
  'server.database': '이것은 테스트입니까 ! @abc " mötör',
  'create-database': true,
  'collection': 'testCollection',
  //these flags have double @s because of the feature that trims the @ for escaping it in configuration files in /etc
  //the double @s will be removed when this feature is deprecated
  'custom-query': 'FOR doc IN @@@@collectionName FILTER doc.name == @@name RETURN doc',
  'custom-query-bindvars': '{"@@collectionName": "testCollection", "name": "test"}'
}, {
  'histogram.generate': true,
  'requests': '100',
  'threads': '1',
  'test-case': 'version',
  'keep-alive': 'true',
  // test with Unicode database name
  'server.database': '이것은 테스트입니까 ! @abc " mötör',
  'create-database': true,
  'collection': 'testCollection',
  //these flags have double @s because of the feature that trims the @ for escaping it in configuration files in /etc
  //the double @s will be removed when this feature is deprecated
  'custom-query': 'FOR doc IN @@@@collectionName FILTER doc.name == @@name RETURN doc',
  'custom-query-bindvars': '{"@@collectionName": "testCollection", "value": "test"}',
  'expected-failure': true
}, {
  'histogram.generate': true,
  'requests': '100',
  'threads': '1',
  'test-case': 'version',
  'keep-alive': 'true',
  // test with Unicode database name
  'server.database': '이것은 테스트입니까 ! @abc " mötör',
  'create-database': true,
  'collection': 'testCollection',
  //these flags have double @s because of the feature that trims the @ for escaping it in configuration files in /etc
  //the double @s will be removed when this feature is deprecated
  'custom-query': 'FOR doc IN @@@@testCollection FILTER doc.name == @@name RETURN doc',
  'custom-query-bindvars': '{"@@collectionName": "testCollection", "name": "test"}',
  'expected-failure': true
}
];

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
    const reportfn = fs.join(instanceInfo.rootDir, 'report_' + name + '.json');

    if ((options.skipArangoBenchNonConnKeepAlive) &&
        benchTodo.hasOwnProperty('keep-alive') &&
        (benchTodo['keep-alive'] === 'false')) {
      benchTodo['keep-alive'] = true;
    }
    benchTodo['json-report-file'] = reportfn;

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
      delete args['expected-failure'];
      if (options.hasOwnProperty('benchargs')) {
        args = Object.assign(args, options.benchargs);
      }
      let oneResult = pu.run.arangoBenchmark(options, instanceInfo, args, instanceInfo.rootDir, options.coreCheck);
      if (benchTodo.hasOwnProperty('expected-failure') && benchTodo['expected-failure']) {
        continueTesting = pu.arangod.check.instanceAlive(instanceInfo, options);
        if (benchTodo.hasOwnProperty('create-database') && benchTodo['create-database']) {
          if (internal.db._databases().find(
              dbName => dbName === benchTodo['server.database']) !== undefined) {
            internal.db._dropDatabase(benchTodo['server.database']);
          }
        }
        results[name] = oneResult;
        results[name].total++;
        results[name].failed = 0;

        if (results[name].status) {
          results[name].failed = 1;
          results.status = false;
          results.failed += 1;
        }
        if (oneResult.status && !options.force) {
          break;
        }
      } else {
        if (benchTodo.hasOwnProperty('duration')) {
          oneResult.status = oneResult.status && oneResult.duration >= benchTodo['duration'];
          if (!oneResult.status) {
            oneResult.message += ` didn't run for the expected time ${benchTodo.duration} but only ${oneResult.duration}`;
          }
          if (!oneResult.status && options.extremeVerbosity) {
            print("Duration test failed: " + JSON.stringify(oneResult));
          }
        }

        if (benchTodo.hasOwnProperty('create-database') && benchTodo['create-database']) {
          if (internal.db._databases().find(
              dbName => dbName === benchTodo['server.database']) === undefined) {
            oneResult.message += " no database was created!";
            oneResult.status = false;
          } else {
            internal.db._dropDatabase(benchTodo['server.database']);
          }
        }
        let content;
        try {
          content = fs.read(reportfn);
          const jsonResult = JSON.parse(content);
          const haveResultFields = jsonResult.hasOwnProperty('histogram') &&
              jsonResult.hasOwnProperty('results') &&
              jsonResult.hasOwnProperty('avg');
          if (!haveResultFields) {
            oneResult.status = false;
            oneResult.message += "critical fields have been missing in the json result: '" +
                content + "'";
          }
        } catch (x) {
          oneResult.message += "failed to parse json report for '" +
              reportfn + "' - '" + x.message + "' - content: '" + content;
          oneResult.status = false;
        }

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
