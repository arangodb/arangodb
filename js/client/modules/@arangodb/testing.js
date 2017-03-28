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
  'all': 'run all tests (marked with [x])',
  'agency': 'run agency tests',
  'arangobench': 'arangobench tests',
  'arangosh': 'arangosh exit codes tests',
  'authentication': 'authentication tests',
  'authentication_parameters': 'authentication parameters tests',
  'catch': 'catch test suites',
  'config': 'checks the config file parsing',
  'client_resilience': 'client resilience tests',
  'cluster_sync': 'cluster sync tests',
  'dump': 'dump tests',
  'dump_authentication': 'dump tests with authentication',
  'export': 'export formats tests',
  'dfdb': 'start test',
  'endpoints': 'endpoints tests',
  'foxx_manager': 'foxx manager tests',
  'http_replication': 'http replication tests',
  'http_server': 'http server tests in Ruby',
  'server_http': 'http server tests in Mocha',
  'importing': 'import tests',
  'recovery': 'run recovery tests',
  'replication_ongoing': 'replication ongoing tests',
  'replication_static': 'replication static tests',
  'replication_sync': 'replication sync tests',
  'resilience': 'resilience tests',
  'shell_client': 'shell client tests',
  'shell_replication': 'shell replication tests',
  'shell_server': 'shell server tests',
  'shell_server_aql': 'AQL tests in the server',
  'shell_server_only': 'server specific tests',
  'shell_server_perf': 'bulk tests intended to get an overview of executiontime needed',
  'single_client': 'run one test suite isolated via the arangosh; options required\n' +
    '            run without arguments to get more detail',
  'single_server': 'run one test suite on the server; options required\n' +
    '            run without arguments to get more detail',
  'ssl_server': 'https server tests',
  'upgrade': 'upgrade tests',
  'fail': 'this job will always produce a failed result'
};

const optionsDocumentation = [
  '',
  ' The following properties of `options` are defined:',
  '',
  '   - `jsonReply`: if set a json is returned which the caller has to ',
  '        present the user',
  '   - `force`: if set to true the tests are continued even if one fails',
  '',
  '   - `skipAql`: if set to true the AQL tests are skipped',
  '   - `skipArangoBenchNonConnKeepAlive`: if set to true benchmark do not use keep-alive',
  '   - `skipArangoBench`: if set to true benchmark tests are skipped',
  '   - `skipAuthentication : testing authentication and authentication_paramaters will be skipped.',
  '   - `skipCatch`: if set to true the catch unittests are skipped',
  '   - `skipCache`: if set to true, the hash cache unittests are skipped',
  '   - `skipConfig`: omit the noisy configuration tests',
  '   - `skipFoxxQueues`: omit the test for the foxx queues',
  '   - `skipEndpoints`: if set to true endpoints tests are skipped',
  '   - `skipGeo`: if set to true the geo index tests are skipped',
  '   - `skipGraph`: if set to true the graph tests are skipped',
  "   - `skipLogAnalysis`: don't try to crawl the server logs",
  '   - `skipMemoryIntense`: tests using lots of resources will be skipped.',
  '   - `skipNightly`: omit the nightly tests',
  '   - `skipRanges`: if set to true the ranges tests are skipped',
  '   - `skipSsl`: omit the ssl_server rspec tests.',
  '   - `skipTimeCritical`: if set to true, time critical tests will be skipped.',
  '   - `skipNondeterministic`: if set, nondeterministic tests are skipped.',
  '   - `skipShebang`: if set, the shebang tests are skipped.',
  '   - `testBuckets`: split tests in to buckets and execute on, for example',
  '       10/2 will split into 10 buckets and execute the third bucket.',
  '',
  '   - `onlyNightly`: execute only the nightly tests',
  '   - `loopEternal`: to loop one test over and over.',
  '   - `loopSleepWhen`: sleep every nth iteration',
  '   - `loopSleepSec`: sleep seconds between iterations',
  '',
  '   - `storageEngine`: set to `rocksdb` or `mmfiles` - defaults to `mmfiles`',
  '',
  '   - `server`: server_url (e.g. tcp://127.0.0.1:8529) for external server',
  '   - `cluster`: if set to true the tests are run with the coordinator',
  '     of a small local cluster',
  '   - `dbServers`: number of DB-Servers to use',
  '   - `coordinators`: number coordinators to use',
  '   - `agency`: if set to true agency tests are done',
  '   - `agencySize`: number of agents in agency',
  '   - `agencySupervision`: run supervision in agency',
  '   - `test`: path to single test to execute for "single" test target',
  '   - `cleanup`: if set to true (the default), the cluster data files',
  '     and logs are removed after termination of the test.',
  '',
  '   - `benchargs`: additional commandline arguments to arangobench',
  '',
  '   - `build`: the directory containing the binaries',
  '   - `buildType`: Windows build type (Debug, Release), leave empty on linux',
  '   - `configDir`: the directory containing the config files, defaults to',
  '                  etc/testing',
  '   - `rspec`: the location of rspec program',
  '   - `ruby`: the location of ruby program; if empty start rspec directly',
  '',
  '   - `rr`: if set to true arangod instances are run with rr',
  '',
  '   - `sanitizer`: if set the programs are run with enabled sanitizer',
  '     and need longer timeouts',
  '',
  '   - `valgrind`: if set the programs are run with the valgrind',
  '     memory checker; should point to the valgrind executable',
  '   - `valgrindFileBase`: string to prepend to the report filename',
  '   - `valgrindArgs`: commandline parameters to add to valgrind',
  '   - valgrindHosts  - configure which clustercomponents to run using valgrind',
  '        Coordinator - flag to run Coordinator with valgrind',
  '        DBServer    - flag to run DBServers with valgrind',
  '',
  '   - `extraArgs`: list of extra commandline arguments to add to arangod',
  '',
  '   - `verbose`: if set to true, be more verbose',
  '   - `extremeVerbosity`: if set to true, then there will be more test run',
  '     output, especially for cluster tests.',
  ''
];

const optionsDefaults = {
  'agencySize': 3,
  'agencyWaitForSync': false,
  'agencySupervision': true,
  'build': '',
  'buildType': '',
  'cleanup': true,
  'cluster': false,
  'concurrency': 3,
  'configDir': 'etc/testing',
  'coordinators': 1,
  'coreDirectory': '/var/tmp',
  'dbServers': 2,
  'duration': 10,
  'extraArgs': {},
  'extremeVerbosity': false,
  'force': true,
  'jsonReply': false,
  'loopEternal': false,
  'loopSleepSec': 1,
  'loopSleepWhen': 1,
  'minPort': 1024,
  'maxPort': 32768,
  'mochaGrep': undefined,
  'onlyNightly': false,
  'password': '',
  'replication': false,
  'rr': false,
  'rspec': 'rspec',
  'ruby': '',
  'sanitizer': false,
  'skipAql': false,
  'skipArangoBench': false,
  'skipArangoBenchNonConnKeepAlive': true,
  'skipAuthentication': false,
  'skipCatch': false,
  'skipCache': true,
  'skipEndpoints': false,
  'skipGeo': false,
  'skipLogAnalysis': true,
  'skipMemoryIntense': false,
  'skipNightly': true,
  'skipNondeterministic': false,
  'skipRanges': false,
  'skipShebang': false,
  'skipSsl': false,
  'skipTimeCritical': false,
  'storageEngine': 'mmfiles',
  'test': undefined,
  'testBuckets': undefined,
  'username': 'root',
  'valgrind': false,
  'valgrindFileBase': '',
  'valgrindArgs': {},
  'valgrindHosts': false,
  'verbose': false,
  'walFlushTimeout': 30000,
  'writeXmlReport': true
};

const _ = require('lodash');
const fs = require('fs');
const yaml = require('js-yaml');
const xmldom = require('xmldom');

const download = require('internal').download;
const time = require('internal').time;
const toArgv = require('internal').toArgv;
const wait = require('internal').wait;
const platform = require('internal').platform;
const sleep = require('internal').sleep;

const executeExternalAndWait = require('internal').executeExternalAndWait;

const pu = require('@arangodb/process-utils');
const cu = require('@arangodb/crash-utils');

let GDB_OUTPUT = cu.GDB_OUTPUT;

const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const YELLOW = require('internal').COLORS.COLOR_YELLOW;

function doOnePathInner (path) {
  return _.filter(fs.list(makePathUnix(path)),
                  function (p) {
                    return (p.substr(-3) === '.js');
                  })
    .map(function (x) {
      return fs.join(makePathUnix(path), x);
    }).sort();
}

function scanTestPath (path) {
  var community = doOnePathInner(path);
  if (global.ARANGODB_CLIENT_VERSION(true)['enterprise-version']) {
    return community.concat(doOnePathInner('enterprise/' + path));
  } else {
    return community;
  }
}

function makeResults (testname, instanceInfo) {
  const startTime = time();

  return function (status, message) {
    let duration = time() - startTime;
    let results;

    if (status) {
      let result;

      try {
        result = JSON.parse(fs.read(instanceInfo.rootDir + '/testresult.json'));

        if ((typeof result[0] === 'object') &&
          result[0].hasOwnProperty('status')) {
          results = result[0];
        }
      } catch (x) {}
    }

    if (results === undefined) {
      results = {
        status: status,
        duration: duration,
        total: 1,
        failed: status ? 0 : 1,
        'testing.js': {
          status: status,
          duration: duration
        }
      };

      if (message) {
        results['testing.js'].message = message;
      }
    }

    let full = {};
    full[testname] = results;

    return full;
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief build a unix path
// //////////////////////////////////////////////////////////////////////////////

function makePathUnix (path) {
  return fs.join.apply(null, path.split('/'));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief build a generic path
// //////////////////////////////////////////////////////////////////////////////

function makePathGeneric (path) {
  return fs.join.apply(null, path.split(fs.pathSeparator));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a remote unittest file using /_admin/execute
// //////////////////////////////////////////////////////////////////////////////

function runThere (options, instanceInfo, file) {
  try {
    let testCode;
    let mochaGrep = options.mochaGrep ? ', ' + JSON.stringify(options.mochaGrep) : '';
    if (file.indexOf('-spec') === -1) {
      testCode = 'const runTest = require("jsunity").runTest; ' +
        'return runTest(' + JSON.stringify(file) + ', true);';
    } else {
      testCode = 'const runTest = require("@arangodb/mocha-runner"); ' +
        'return runTest(' + JSON.stringify(file) + ', true' + mochaGrep + ');';
    }

    testCode = 'global.instanceInfo = ' + JSON.stringify(instanceInfo) + ';\n' + testCode;

    let httpOptions = pu.makeAuthorizationHeaders(options);
    httpOptions.method = 'POST';
    httpOptions.timeout = 3600;

    if (options.valgrind) {
      httpOptions.timeout *= 2;
    }

    httpOptions.returnBodyOnError = true;

    const reply = download(instanceInfo.url + '/_admin/execute?returnAsJSON=true',
      testCode,
      httpOptions);

    if (!reply.error && reply.code === 200) {
      return JSON.parse(reply.body);
    } else {
      if (reply.hasOwnProperty('body')) {
        return {
          status: false,
          message: reply.body
        };
      } else {
        return {
          status: false,
          message: yaml.safeDump(reply)
        };
      }
    }
  } catch (ex) {
    return {
      status: false,
      message: ex.message || String(ex),
      stack: ex.stack
    };
  }
}
runThere.info = 'runThere';

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a list of tests
// //////////////////////////////////////////////////////////////////////////////

function performTests (options, testList, testname, runFn) {
  let instanceInfo = pu.startInstance('tcp', options, {}, testname);

  if (instanceInfo === false) {
    return {
      setup: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  if (testList.length === 0) {
    print('Testsuite is empty!');

    return {
      'EMPTY TESTSUITE': {
        status: false,
        message: 'no testsuites found!'
      }
    };
  }

  let results = {};
  let continueTesting = true;
  let count = 0;

  for (let i = 0; i < testList.length; i++) {
    let te = testList[i];
    let filtered = {};

    if (filterTestcaseByOptions(te, options, filtered)) {
      let first = true;
      let loopCount = 0;
      count += 1;

      while (first || options.loopEternal) {
        if (!continueTesting) {
          print('oops!');
          print('Skipping, ' + te + ' server is gone.');

          results[te] = {
            status: false,
            message: instanceInfo.exitStatus
          };

          instanceInfo.exitStatus = 'server is gone.';

          break;
        }

        print('\n' + Date() + ' ' + runFn.info + ': Trying', te, '...');
        let reply = runFn(options, instanceInfo, te);

        if (reply.hasOwnProperty('status')) {
          results[te] = reply;

          if (results[te].status === false) {
            options.cleanup = false;
          }

          if (!reply.status && !options.force) {
            break;
          }
        } else {
          results[te] = {
            status: false,
            message: reply
          };

          if (!options.force) {
            break;
          }
        }

        continueTesting = pu.arangod.check.instanceAlive(instanceInfo, options);

        first = false;

        if (options.loopEternal) {
          if (loopCount % options.loopSleepWhen === 0) {
            print('sleeping...');
            sleep(options.loopSleepSec);
            print('continuing.');
          }

          ++loopCount;
        }
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + te + ' because of ' + filtered.filter);
      }
    }
  }

  if (count === 0) {
    results['ALLTESTS'] = {
      status: false,
      skipped: true
    };
    results.status = false;
    print(RED + 'No testcase matched the filter.' + RESET);
  }

  print('Shutting down...');
  pu.shutdownInstance(instanceInfo, options);
  print('done.');

  return results;
}

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

    pu.shutdownInstance(instanceInfo, options);

    return {
      status: false,
      message: reply.hasOwnProperty('body') ? reply.body : yaml.safeDump(reply)
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

    pu.shutdownInstance(instanceInfo, options);

    return {
      status: false,
      message: check.hasOwnProperty('body') ? check.body : yaml.safeDump(check)
    };
  }

  print('Shutting down...');
  pu.shutdownInstance(instanceInfo, options);
  print('done.');

  return {};
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs ruby tests using RSPEC
// //////////////////////////////////////////////////////////////////////////////

function camelize (str) {
  return str.replace(/(?:^\w|[A-Z]|\b\w)/g, function (letter, index) {
    return index === 0 ? letter.toLowerCase() : letter.toUpperCase();
  }).replace(/\s+/g, '');
}

function rubyTests (options, ssl) {
  let instanceInfo;

  if (ssl) {
    instanceInfo = pu.startInstance('ssl', options, {}, 'ssl_server');
  } else {
    instanceInfo = pu.startInstance('tcp', options, {}, 'http_server');
  }

  if (instanceInfo === false) {
    return {
      status: false,
      message: 'failed to start server!'
    };
  }

  const tmpname = fs.getTempFile() + '.rb';

  fs.write(tmpname, 'RSpec.configure do |c|\n' +
    '  c.add_setting :ARANGO_SERVER\n' +
    '  c.ARANGO_SERVER = "' +
    instanceInfo.endpoint.substr(6) + '"\n' +
    '  c.add_setting :ARANGO_SSL\n' +
    '  c.ARANGO_SSL = "' + (ssl ? '1' : '0') + '"\n' +
    '  c.add_setting :ARANGO_USER\n' +
    '  c.ARANGO_USER = "' + options.username + '"\n' +
    '  c.add_setting :ARANGO_PASSWORD\n' +
    '  c.ARANGO_PASSWORD = "' + options.password + '"\n' +
    '  c.add_setting :SKIP_TIMECRITICAL\n' +
    '  c.SKIP_TIMECRITICAL = ' + JSON.stringify(options.skipTimeCritical) + '\n' +
    'end\n');
  try {
    fs.makeDirectory(pu.LOGS_DIR);
  } catch (err) {}

  const files = fs.list(fs.join('UnitTests', 'HttpInterface'));

  let continueTesting = true;
  let filtered = {};
  let result = {};

  let args;
  let command;
  let rspec;

  if (options.ruby === '') {
    command = options.rspec;
    rspec = undefined;
  } else {
    command = options.ruby;
    rspec = options.rspec;
  }

  const parseRspecJson = function (testCase, res, totalDuration) {
    let tName = camelize(testCase.description);
    let status = (testCase.status === 'passed');

    res[tName] = {
      status: status,
      message: testCase.full_description,
      duration: totalDuration // RSpec doesn't offer per testcase time...
    };

    res.total++;

    if (!status) {
      const msg = yaml.safeDump(testCase)
        .replace(/.*rspec\/core.*\n/gm, '')
        .replace(/.*rspec\\core.*\n/gm, '');
      print('RSpec test case falied: \n' + msg);
      res[tName].message += '\n' + msg;
    }
  };

  let count = 0;
  for (let i = 0; i < files.length; i++) {
    const te = files[i];

    if (te.substr(0, 4) === 'api-' && te.substr(-3) === '.rb') {
      if (filterTestcaseByOptions(te, options, filtered)) {
        count += 1;
        if (!continueTesting) {
          print('Skipping ' + te + ' server is gone.');

          result[te] = {
            status: false,
            message: instanceInfo.exitStatus
          };

          instanceInfo.exitStatus = 'server is gone.';
          break;
        }

        args = ['--color',
          '-I', fs.join('UnitTests', 'HttpInterface'),
          '--format', 'd',
          '--format', 'j',
          '--out', fs.join('out', 'UnitTests', te + '.json'),
          '--require', tmpname,
          fs.join('UnitTests', 'HttpInterface', te)
        ];

        if (rspec !== undefined) {
          args = [rspec].concat(args);
        }

        print('\n' + Date() + ' rspec trying', te, '...');
        const res = pu.executeAndWait(command, args, options, 'arangosh', instanceInfo.rootDir);

        result[te] = {
          total: 0,
          status: res.status
        };

        const resultfn = fs.join('out', 'UnitTests', te + '.json');

        try {
          const jsonResult = JSON.parse(fs.read(resultfn));

          if (options.extremeVerbosity) {
            print(yaml.safeDump(jsonResult));
          }

          for (let j = 0; j < jsonResult.examples.length; ++j) {
            parseRspecJson(jsonResult.examples[j], result[te],
              jsonResult.summary.duration);
          }

          result[te].duration = jsonResult.summary.duration;
        } catch (x) {
          print('Failed to parse rspec result: ' + x);
          result[te]['complete_' + te] = res;

          if (res.status === false) {
            options.cleanup = false;

            if (!options.force) {
              break;
            }
          }
        }

        continueTesting = pu.arangod.check.instanceAlive(instanceInfo, options);
      } else {
        if (options.extremeVerbosity) {
          print('Skipped ' + te + ' because of ' + filtered.filter);
        }
      }
    }
  }

  print('Shutting down...');

  if (count === 0) {
    result['ALLTESTS'] = {
      status: false,
      skipped: true
    };
    result.status = false;
    print(RED + 'No testcase matched the filter.' + RESET);
  }

  fs.remove(tmpname);
  pu.shutdownInstance(instanceInfo, options);
  print('done.');

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief sort test-cases according to pathname
// //////////////////////////////////////////////////////////////////////////////

let testsCases = {
  setup: false
};

function findTests () {
  if (testsCases.setup) {
    return;
  }

  testsCases.common = scanTestPath('js/common/tests/shell');

  testsCases.server_only = scanTestPath('js/server/tests/shell');

  testsCases.client_only = scanTestPath('js/client/tests/shell');

  testsCases.server_aql = scanTestPath('js/server/tests/aql');
  testsCases.server_aql = _.filter(testsCases.server_aql,
    function (p) { return p.indexOf('ranges-combined') === -1; });

  testsCases.server_aql_extended = scanTestPath('js/server/tests/aql');
  testsCases.server_aql_extended = _.filter(testsCases.server_aql_extended,
    function (p) { return p.indexOf('ranges-combined') !== -1; });

  testsCases.server_aql_performance = scanTestPath('js/server/perftests');

  testsCases.server_http = scanTestPath('js/common/tests/http');

  testsCases.replication = scanTestPath('js/common/tests/replication');

  testsCases.agency = scanTestPath('js/client/tests/agency');

  testsCases.resilience = scanTestPath('js/server/tests/resilience');

  testsCases.client_resilience = scanTestPath('js/client/tests/resilience');
  testsCases.cluster_sync = scanTestPath('js/server/tests/cluster-sync');

  testsCases.server = testsCases.common.concat(testsCases.server_only);
  testsCases.client = testsCases.common.concat(testsCases.client_only);

  testsCases.setup = true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief filter test-cases according to options
// //////////////////////////////////////////////////////////////////////////////

function filterTestcaseByOptions (testname, options, whichFilter) {
  if (options.hasOwnProperty('test') && (typeof (options.test) !== 'undefined')) {
    whichFilter.filter = 'testcase';
    return testname.search(options.test) >= 0;
  }

  if (options.replication) {
    whichFilter.filter = 'replication';
    return testname.indexOf('replication') !== -1;
  } else if (testname.indexOf('replication') !== -1) {
    whichFilter.filter = 'replication';
    return false;
  }

  if ((testname.indexOf('-cluster') !== -1) && !options.cluster) {
    whichFilter.filter = 'noncluster';
    return false;
  }

  if (testname.indexOf('-noncluster') !== -1 && options.cluster) {
    whichFilter.filter = 'cluster';
    return false;
  }

  if (testname.indexOf('-timecritical') !== -1 && options.skipTimeCritical) {
    whichFilter.filter = 'timecritical';
    return false;
  }

  if (testname.indexOf('-nightly') !== -1 && options.skipNightly && !options.onlyNightly) {
    whichFilter.filter = 'skip nightly';
    return false;
  }

  if (testname.indexOf('-geo') !== -1 && options.skipGeo) {
    whichFilter.filter = 'geo';
    return false;
  }

  if (testname.indexOf('-nondeterministic') !== -1 && options.skipNondeterministic) {
    whichFilter.filter = 'nondeterministic';
    return false;
  }

  if (testname.indexOf('-graph') !== -1 && options.skipGraph) {
    whichFilter.filter = 'graph';
    return false;
  }

  if (testname.indexOf('-disabled') !== -1) {
    whichFilter.filter = 'disabled';
    return false;
  }

  if ((testname.indexOf('-memoryintense') !== -1) && options.skipMemoryIntense) {
    whichFilter.filter = 'memoryintense';
    return false;
  }

  if (testname.indexOf('-nightly') === -1 && options.onlyNightly) {
    whichFilter.filter = 'only nightly';
    return false;
  }

  if ((testname.indexOf('-novalgrind') !== -1) && options.valgrind) {
    whichFilter.filter = 'skip in valgrind';
    return false;
  }

  if ((testname.indexOf('-mmfiles') !== -1) && options.storageEngine === 'rocksdb') {
    whichFilter.filter = 'skip when running as rocksdb';
    return false;
  }
  if ((testname.indexOf('-rocksdb') !== -1) && options.storageEngine === 'mmfiles') {
    whichFilter.filter = 'skip when running as mmfiles';
    return false;
  }
  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief split into buckets
// //////////////////////////////////////////////////////////////////////////////

function splitBuckets (options, cases) {
  if (!options.testBuckets || cases.length === 0) {
    return cases;
  }

  let m = cases.length;
  let n = options.testBuckets.split('/');
  let r = parseInt(n[0]);
  let s = parseInt(n[1]);

  if (r < 1) {
    r = 1;
  }

  if (r === 1) {
    return cases;
  }

  if (s < 0) {
    s = 0;
  }
  if (r <= s) {
    s = r - 1;
  }

  let result = [];

  for (let i = s % m; i < cases.length; i = i + r) {
    result.push(cases[i]);
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test functions for all
// //////////////////////////////////////////////////////////////////////////////

let allTests = [
  'agency',
  'arangosh',
  'authentication',
  'authentication_parameters',
  'catch',
  'config',
  'dump',
  'dump_authentication',
  'export',
  'dfdb',
  'endpoints',
  'http_server',
  'importing',
  'server_http',
  'shell_client',
  'shell_server',
  'shell_server_aql',
  'ssl_server',
  'upgrade',
  'arangobench'
];

// //////////////////////////////////////////////////////////////////////////////
// / @brief internal members of the results
// //////////////////////////////////////////////////////////////////////////////

const internalMembers = [
  'code',
  'error',
  'status',
  'duration',
  'failed',
  'total',
  'crashed',
  'ok',
  'message',
  'suiteName'
];

function skipInternalMember (r, a) {
  return !r.hasOwnProperty(a) || internalMembers.indexOf(a) !== -1;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: all
// //////////////////////////////////////////////////////////////////////////////

let testFuncs = {
  'all': function () {}
};

testFuncs.fail = function (options) {
  return {
    failSuite: {
      status: false,
      total: 1,
      message: 'this suite will always fail.',
      duration: 2,
      failed: 1,
      failTest: {
        status: false,
        total: 1,
        duration: 1,
        message: 'this testcase will always fail.'
      },
      failSuccessTest: {
        status: true,
        duration: 1,
        message: 'this testcase will always succeed, though its in the fail testsuite.'
      }
    },
    successSuite: {
      status: true,
      total: 1,
      message: 'this suite will always be successfull',
      duration: 1,
      failed: 0,
      success: {
        status: true,
        message: 'this testcase will always be successfull',
        duration: 1
      }
    }
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: arangosh
// //////////////////////////////////////////////////////////////////////////////

testFuncs.arangosh = function (options) {
  let ret = {};
  [
    'testArangoshExitCodeNoConnect',
    'testArangoshExitCodeFail',
    'testArangoshExitCodeFailButCaught',
    'testArangoshExitCodeEmpty',
    'testArangoshExitCodeSuccess',
    'testArangoshExitCodeStatements',
    'testArangoshExitCodeStatements2',
    'testArangoshExitCodeNewlines',
    'testArangoshExitCodeEcho',
    'testArangoshShebang'
  ].forEach(function (what) {
    ret[what] = {
      status: true,
      total: 0
    };
  });

  function runTest (section, title, command, expectedReturnCode, opts) {
    print('--------------------------------------------------------------------------------');
    print(title);
    print('--------------------------------------------------------------------------------');

    let args = pu.makeArgs.arangosh(options);
    args['javascript.execute-string'] = command;
    args['log.level'] = 'error';

    for (let op in opts) {
      args[op] = opts[op];
    }

    const startTime = time();
    let rc = executeExternalAndWait(pu.ARANGOSH_BIN, toArgv(args));
    const deltaTime = time() - startTime;
    const failSuccess = (rc.hasOwnProperty('exit') && rc.exit === expectedReturnCode);

    if (!failSuccess) {
      ret[section]['message'] =
        'didn\'t get expected return code (' + expectedReturnCode + '): \n' +
        yaml.safeDump(rc);
    }

    ++ret[section]['total'];
    ret[section]['status'] = failSuccess;
    ret[section]['duration'] = deltaTime;
    print((failSuccess ? GREEN : RED) + 'Status: ' + (failSuccess ? 'SUCCESS' : 'FAIL') + RESET);
    if (options.extremeVerbosity) {
      print(toArgv(args));
      print(ret[section]);
      print(rc);
      print('expect rc: ' + expectedReturnCode);
    }
  }

  runTest('testArangoshExitCodeNoConnect',
          'Starting arangosh with failing connect:',
          'db._databases();',
          1,
          {'server.endpoint': 'tcp://127.0.0.1:0'});

  print();

  runTest('testArangoshExitCodeFail', 'Starting arangosh with exception throwing script:', 'throw(\'foo\')', 1, {});
  print();

  runTest('testArangoshExitCodeFailButCaught', 'Starting arangosh with a caught exception:', 'try { throw(\'foo\'); } catch (err) {}', 0, {});
  print();

  runTest('testArangoshExitCodeEmpty', 'Starting arangosh with empty script:', '', 0, {});
  print();

  runTest('testArangoshExitCodeSuccess', 'Starting arangosh with regular terminating script:', ';', 0, {});
  print();

  runTest('testArangoshExitCodeStatements', 'Starting arangosh with multiple statements:', 'var a = 1; if (a !== 1) throw("boom!");', 0, {});
  print();

  runTest('testArangoshExitCodeStatements2', 'Starting arangosh with multiple statements:', 'var a = 1;\nif (a !== 1) throw("boom!");\nif (a === 1) print("success");', 0, {});
  print();

  runTest('testArangoshExitCodeNewlines', 'Starting arangosh with newlines:', 'q = `FOR i\nIN [1,2,3]\nRETURN i`;\nq += "abc"\n', 0, {});
  print();

  if (platform.substr(0, 3) !== 'win') {
    var echoSuccess = true;
    var deltaTime2 = 0;
    var execFile = fs.getTempFile();

    print('\n--------------------------------------------------------------------------------');
    print('Starting arangosh via echo');
    print('--------------------------------------------------------------------------------');

    fs.write(execFile,
      'echo "db._databases();" | ' + fs.makeAbsolute(pu.ARANGOSH_BIN) + ' --server.endpoint tcp://127.0.0.1:0');

    executeExternalAndWait('sh', ['-c', 'chmod a+x ' + execFile]);

    const startTime2 = time();
    let rc = executeExternalAndWait('sh', ['-c', execFile]);
    deltaTime2 = time() - startTime2;

    echoSuccess = (rc.hasOwnProperty('exit') && rc.exit === 1);

    if (!echoSuccess) {
      ret.testArangoshExitCodeEcho['message'] =
        'didn\'t get expected return code (1): \n' +
        yaml.safeDump(rc);
    }

    fs.remove(execFile);

    ++ret.testArangoshExitCodeEcho['total'];
    ret.testArangoshExitCodeEcho['status'] = echoSuccess;
    ret.testArangoshExitCodeEcho['duration'] = deltaTime2;
    print((echoSuccess ? GREEN : RED) + 'Status: ' + (echoSuccess ? 'SUCCESS' : 'FAIL') + RESET);
  }

  // test shebang execution with arangosh
  if (!options.skipShebang && platform.substr(0, 3) !== 'win') {
    var shebangSuccess = true;
    var deltaTime3 = 0;
    var shebangFile = fs.getTempFile();

    print('\n--------------------------------------------------------------------------------');
    print('Starting arangosh via shebang script');
    print('--------------------------------------------------------------------------------');

    if (options.verbose) {
      print(CYAN + 'shebang script: ' + shebangFile + RESET);
    }

    fs.write(shebangFile,
      '#!' + fs.makeAbsolute(pu.ARANGOSH_BIN) + ' --javascript.execute \n' +
      'print("hello world");\n');

    executeExternalAndWait('sh', ['-c', 'chmod a+x ' + shebangFile]);

    const startTime3 = time();
    let rc = executeExternalAndWait('sh', ['-c', shebangFile]);
    deltaTime3 = time() - startTime3;

    if (options.verbose) {
      print(CYAN + 'execute returned: ' + RESET, rc);
    }

    shebangSuccess = (rc.hasOwnProperty('exit') && rc.exit === 0);

    if (!shebangSuccess) {
      ret.testArangoshShebang['message'] =
        'didn\'t get expected return code (0): \n' +
        yaml.safeDump(rc);
    }

    fs.remove(shebangFile);

    ++ret.testArangoshShebang['total'];
    ret.testArangoshShebang['status'] = shebangSuccess;
    ret.testArangoshShebang['duration'] = deltaTime3;
    print((shebangSuccess ? GREEN : RED) + 'Status: ' + (shebangSuccess ? 'SUCCESS' : 'FAIL') + RESET);
  } else {
    ret.testArangoshShebang['skipped'] = true;
  }

  print();
  return ret;
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

testFuncs.arangobench = function (options) {
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

  let results = {};
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

      let oneResult = pu.run.arangoBenchmark(options, instanceInfo, args);
      print();

      results[name] = oneResult;
      results[name].total++;

      if (!results[name].status) {
        results.status = false;
      }

      continueTesting = pu.arangod.check.instanceAlive(instanceInfo, options);

      if (oneResult.status !== true && !options.force) {
        break;
      }
    }
  }

  print(CYAN + 'Shutting down...' + RESET);
  pu.shutdownInstance(instanceInfo, options);
  print(CYAN + 'done.' + RESET);

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: authentication
// //////////////////////////////////////////////////////////////////////////////

testFuncs.authentication = function (options) {
  if (options.skipAuthentication === true) {
    print('skipping Authentication tests!');

    return {
      authentication: {
        status: true,
        skipped: true
      }
    };
  }

  if (options.cluster) {
    print('skipping Authentication tests on cluster!');
    return {
      authentication: {
        status: true,
        skipped: true
      }
    };
  }

  print(CYAN + 'Authentication tests...' + RESET);

  let instanceInfo = pu.startInstance('tcp', options, {
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann'
  }, 'authentication');

  if (instanceInfo === false) {
    return {
      authentication: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  let results = {};

  results.authentication = pu.runInArangosh(options, instanceInfo,
    fs.join('js', 'client', 'tests', 'auth.js'));

  print(CYAN + 'Shutting down...' + RESET);
  pu.shutdownInstance(instanceInfo, options);
  print(CYAN + 'done.' + RESET);

  print();

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: authentication parameters
// //////////////////////////////////////////////////////////////////////////////

const authTestExpectRC = [
  [401, 401, 401, 401, 401, 401, 401],
  [401, 401, 401, 401, 401, 404, 404],
  [404, 404, 200, 301, 301, 404, 404]
];

const authTestUrls = [
  '/_api/',
  '/_api',
  '/_api/version',
  '/_admin/html',
  '/_admin/html/',
  '/test',
  '/the-big-fat-fox'
];

const authTestNames = [
  'Full',
  'SystemAuth',
  'None'
];

const authTestServerParams = [{
  'server.authentication': 'true',
  'server.authentication-system-only': 'false'
}, {
  'server.authentication': 'true',
  'server.authentication-system-only': 'true'
}, {
  'server.authentication': 'false',
  'server.authentication-system-only': 'true'
}];

function checkBodyForJsonToParse (request) {
  if (request.hasOwnProperty('body')) {
    request.body = JSON.parse(request.hasOwnProperty('body'));
  }
}

testFuncs.authentication_parameters = function (options) {
  if (options.skipAuthentication === true) {
    print(CYAN + 'skipping Authentication with parameters tests!' + RESET);
    return {
      authentication_parameters: {
        status: true,
        skipped: true
      }
    };
  }

  if (options.cluster) {
    print('skipping Authentication with parameters tests on cluster!');
    return {
      authentication: {
        status: true,
        skipped: true
      }
    };
  }

  print(CYAN + 'Authentication with parameters tests...' + RESET);

  let downloadOptions = {
    followRedirects: false,
    returnBodyOnError: true
  };

  if (options.valgrind) {
    downloadOptions.timeout = 300;
  }

  let continueTesting = true;
  let results = {};

  for (let test = 0; test < 3; test++) {
    let instanceInfo = pu.startInstance('tcp', options,
      authTestServerParams[test],
      'authentication_parameters_' + authTestNames[test]);

    if (instanceInfo === false) {
      return {
        authentication_parameters: {
          status: false,
          total: 1,
          failed: 1,
          message: authTestNames[test] + ': failed to start server!'
        }
      };
    }

    print(CYAN + Date() + ' Starting ' + authTestNames[test] + ' test' + RESET);

    const testName = 'auth_' + authTestNames[test];
    results[testName] = {
      failed: 0,
      total: 0
    };

    for (let i = 0; i < authTestUrls.length; i++) {
      const authTestUrl = authTestUrls[i];

      ++results[testName].total;

      print(CYAN + '  URL: ' + instanceInfo.url + authTestUrl + RESET);

      if (!continueTesting) {
        print(RED + 'Skipping ' + authTestUrl + ', server is gone.' + RESET);

        results[testName][authTestUrl] = {
          status: false,
          message: instanceInfo.exitStatus
        };

        results[testName].failed++;
        instanceInfo.exitStatus = 'server is gone.';

        break;
      }

      let reply = download(instanceInfo.url + authTestUrl, '', downloadOptions);

      if (reply.code === authTestExpectRC[test][i]) {
        results[testName][authTestUrl] = {
          status: true
        };
      } else {
        checkBodyForJsonToParse(reply);

        ++results[testName].failed;

        results[testName][authTestUrl] = {
          status: false,
          message: 'we expected ' +
            authTestExpectRC[test][i] +
            ' and we got ' + reply.code +
            ' Full Status: ' + yaml.safeDump(reply)
        };
      }

      continueTesting = pu.arangod.check.instanceAlive(instanceInfo, options);
    }

    results[testName].status = results[testName].failed === 0;

    print(CYAN + 'Shutting down ' + authTestNames[test] + ' test...' + RESET);
    pu.shutdownInstance(instanceInfo, options);
    print(CYAN + 'done with ' + authTestNames[test] + ' test.' + RESET);
  }

  print();

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: Catch
// //////////////////////////////////////////////////////////////////////////////

function locateCatchTest (name) {
  var file = fs.join(pu.UNITTESTS_DIR, name + pu.executableExt);

  if (!fs.exists(file)) {
    return '';
  }
  return file;
}

testFuncs.catch = function (options) {
  let results = {};
  let rootDir = pu.UNITTESTS_DIR;

  const icuDir = pu.UNITTESTS_DIR + '/';
  require('internal').env.ICU_DATA = icuDir;
  const run = locateCatchTest('arangodbtests');
  if (!options.skipCatch) {
    if (run !== '') {
      let argv = [
        '[exclude:longRunning][exclude:cache]',
        '-r',
        'junit',
        '-o',
        fs.join('out', 'catch-standard.xml')];
      results.basics = pu.executeAndWait(run, argv, options, 'all-catch', rootDir);
    } else {
      results.basics = {
        status: false,
        message: 'binary "basics_suite" not found'
      };
    }
  }

  if (!options.skipCache) {
    if (run !== '') {
      let argv = ['[cache]',
                  '-r',
                  'junit',
                  '-o',
                  fs.join('out', 'catch-cache.xml')
                 ];
      results.cache_suite = pu.executeAndWait(run, argv, options,
                                           'cache_suite', rootDir);
    } else {
      results.cache_suite = {
        status: false,
        message: 'binary "cache_suite" not found'
      };
    }
  }

  if (!options.skipGeo) {
    if (run !== '') {
      let argv = ['[geo]',
                  '-r',
                  'junit',
                  '-o',
                  fs.join('out', 'catch-geo.xml')
                 ];
      results.geo_suite = pu.executeAndWait(run, argv, options, 'geo_suite', rootDir);
    } else {
      results.geo_suite = {
        status: false,
        message: 'binary "geo_suite" not found'
      };
    }
  }

  return results;
};

testFuncs.boost = testFuncs.catch;

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: config
// //////////////////////////////////////////////////////////////////////////////

testFuncs.config = function (options) {
  if (options.skipConfig) {
    return {
      config: {
        status: true,
        skipped: true
      }
    };
  }

  let results = {
    absolut: {
      status: true,
      total: 0,
      duration: 0
    },
    relative: {
      status: true,
      total: 0,
      duration: 0
    }
  };

  const ts = [
    'arangod',
    'arangobench',
    'arangodump',
    'arangoimp',
    'arangorestore',
    'arangoexport',
    'arangosh',
    'arango-dfdb',
    'foxx-manager'
  ];

  let rootDir = pu.UNITTESTS_DIR;

  print('--------------------------------------------------------------------------------');
  print('absolute config tests');
  print('--------------------------------------------------------------------------------');

  let startTime = time();

  for (let i = 0; i < ts.length; i++) {
    const test = ts[i];
    print(CYAN + 'checking "' + test + '"' + RESET);

    const args = {
      'configuration': fs.join(pu.CONFIG_ARANGODB_DIR, test + '.conf'),
      'flatCommands': ['--check-configuration']
    };

    const run = fs.join(pu.BIN_DIR, test);

    results.absolut[test] = pu.executeAndWait(run, toArgv(args), options, test, rootDir);

    if (!results.absolut[test].status) {
      results.absolut.status = false;
    }

    results.absolut.total++;

    if (options.verbose) {
      print('Args for [' + test + ']:');
      print(yaml.safeDump(args));
      print('Result: ' + results.absolut[test].status);
    }
  }

  results.absolut.duration = time() - startTime;

  print('\n--------------------------------------------------------------------------------');
  print('relative config tests');
  print('--------------------------------------------------------------------------------');

  startTime = time();

  for (let i = 0; i < ts.length; i++) {
    const test = ts[i];
    print(CYAN + 'checking "' + test + '"' + RESET);

    const args = {
      'configuration': fs.join(pu.CONFIG_RELATIVE_DIR, test + '.conf'),
      'flatCommands': ['--check-configuration']
    };

    const run = fs.join(pu.BIN_DIR, test);

    results.relative[test] = pu.executeAndWait(run, toArgv(args), options, test, rootDir);

    if (!results.relative[test].status) {
      results.relative.status = false;
    }

    results.relative.total++;

    if (options.verbose) {
      print('Args for (relative) [' + test + ']:');
      print(yaml.safeDump(args));
      print('Result: ' + results.relative[test].status);
    }
  }

  results.relative.duration = time() - startTime;

  print();

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: dfdb
// //////////////////////////////////////////////////////////////////////////////

testFuncs.dfdb = function (options) {
  const dataDir = fs.getTempFile();
  const args = ['-c', fs.join(pu.CONFIG_DIR, 'arango-dfdb.conf'), dataDir];

  fs.makeDirectoryRecursive(dataDir);
  let results = {};

  results.dfdb = pu.executeAndWait(pu.ARANGOD_BIN, args, options, 'dfdb', dataDir);

  print();

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: dump
// //////////////////////////////////////////////////////////////////////////////

testFuncs.dump = function (options) {
  let cluster;

  if (options.cluster) {
    cluster = '-cluster';
  } else {
    cluster = '';
  }

  print(CYAN + 'dump tests...' + RESET);

  let instanceInfo = pu.startInstance('tcp', options, {}, 'dump');

  if (instanceInfo === false) {
    return {
      dump: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  print(CYAN + Date() + ': Setting up' + RESET);

  let results = {};
  results.setup = pu.runInArangosh(options, instanceInfo,
    makePathUnix('js/server/tests/dump/dump-setup' + cluster + '.js'));

  if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
    (results.setup.status === true)) {
    print(CYAN + Date() + ': Dump and Restore - dump' + RESET);

    results.dump = pu.run.arangoDumpRestore(options, instanceInfo, 'dump',
      'UnitTestsDumpSrc');

    if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
      (results.dump.status === true)) {
      print(CYAN + Date() + ': Dump and Restore - restore' + RESET);

      results.restore = pu.run.arangoDumpRestore(options, instanceInfo, 'restore',
        'UnitTestsDumpDst');

      if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
        (results.restore.status === true)) {
        print(CYAN + Date() + ': Dump and Restore - dump after restore' + RESET);

        results.test = pu.runInArangosh(options, instanceInfo,
          makePathUnix('js/server/tests/dump/dump' + cluster + '.js'), {
            'server.database': 'UnitTestsDumpDst'
          });

        if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
          (results.test.status === true)) {
          print(CYAN + Date() + ': Dump and Restore - teardown' + RESET);

          results.tearDown = pu.runInArangosh(options, instanceInfo,
            makePathUnix('js/server/tests/dump/dump-teardown' + cluster + '.js'));
        }
      }
    }
  }

  print(CYAN + 'Shutting down...' + RESET);
  pu.shutdownInstance(instanceInfo, options);
  print(CYAN + 'done.' + RESET);

  print();

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: dump
// //////////////////////////////////////////////////////////////////////////////

testFuncs.export = function (options) {
  const cluster = options.cluster ? '-cluster' : '';
  const tmpPath = fs.getTempPath();
  const DOMParser = new xmldom.DOMParser({
    locator: {},
    errorHandler: {
      warning: function (err) {
        xmlErrors = err;
      },
      error: function (err) {
        xmlErrors = err;
      },
      fatalError: function (err) {
        xmlErrors = err;
      }
    }
  }
                                        );
  let xmlErrors = null;

  print(CYAN + 'export tests...' + RESET);

  const instanceInfo = pu.startInstance('tcp', options, {}, 'export');

  if (instanceInfo === false) {
    return {
      export: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  print(CYAN + Date() + ': Setting up' + RESET);

  const args = {
    'configuration': fs.join(pu.CONFIG_DIR, 'arangoexport.conf'),
    'server.username': options.username,
    'server.password': options.password,
    'server.endpoint': instanceInfo.endpoint,
    'server.database': 'UnitTestsExport',
    'collection': 'UnitTestsExport',
    'type': 'json',
    'overwrite': true,
    'output-directory': tmpPath
  };
  const results = {};

  function shutdown () {
    print(CYAN + 'Shutting down...' + RESET);
    pu.shutdownInstance(instanceInfo, options);
    print(CYAN + 'done.' + RESET);
    print();
    return results;
  }

  results.setup = pu.runInArangosh(options, instanceInfo, makePathUnix('js/server/tests/export/export-setup' + cluster + '.js'));
  if (!pu.arangod.check.instanceAlive(instanceInfo, options) || results.setup.status !== true) {
    return shutdown();
  }

  print(CYAN + Date() + ': Export data (json)' + RESET);
  results.exportJson = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath);
  try {
    // const filesContent = JSON.parse(fs.read(fs.join(tmpPath, 'UnitTestsExport.json')));
    results.parseJson = { status: true };
  } catch (e) {
    results.parseJson = {
      status: false,
      message: e
    };
  }

  print(CYAN + Date() + ': Export data (jsonl)' + RESET);
  args['type'] = 'jsonl';
  results.exportJsonl = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath);
  try {
    const filesContent = fs.read(fs.join(tmpPath, 'UnitTestsExport.jsonl')).split('\n');
    for (const line of filesContent) {
      if (line.trim() === '') continue;
      JSON.parse(line);
    }
    results.parseJsonl = {
      status: true
    };
  } catch (e) {
    results.parseJsonl = {
      status: false,
      message: e
    };
  }

  print(CYAN + Date() + ': Export data (xgmml)' + RESET);
  args['type'] = 'xgmml';
  args['graph-name'] = 'UnitTestsExport';
  results.exportXgmml = pu.executeAndWait(pu.ARANGOEXPORT_BIN, toArgv(args), options, 'arangosh', tmpPath);
  try {
    const filesContent = fs.read(fs.join(tmpPath, 'UnitTestsExport.xgmml'));
    DOMParser.parseFromString(filesContent);
    results.parseXgmml = { status: true };

    if (xmlErrors !== null) {
      results.parseXgmml = {
        status: false,
        message: xmlErrors
      };
    }
  } catch (e) {
    results.parseXgmml = {
      status: false,
      message: e
    };
  }

  return shutdown();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: dump_authentication
// //////////////////////////////////////////////////////////////////////////////

testFuncs.dump_authentication = function (options) {
  if (options.cluster) {
    if (options.extremeVerbosity) {
      print(CYAN + 'Skipped because of cluster.' + RESET);
    }

    return {
      'dump_authentication': {
        'status': true,
        'message': 'skipped because of cluster',
        'skipped': true
      }
    };
  }

  print(CYAN + 'dump_authentication tests...' + RESET);

  const auth1 = {
    'server.authentication': 'true'
  };

  const auth2 = {
    'server.authentication': 'true'
  };

  let instanceInfo = pu.startInstance('tcp', options, auth1, 'dump_authentication');

  if (instanceInfo === false) {
    return {
      'dump_authentication': {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  print(CYAN + Date() + ': Setting up' + RESET);

  let results = {};
  results.setup = pu.runInArangosh(options, instanceInfo,
    makePathUnix('js/server/tests/dump/dump-authentication-setup.js'),
    auth2);

  if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
    (results.setup.status === true)) {
    print(CYAN + Date() + ': Dump and Restore - dump' + RESET);

    let authOpts = {
      username: 'foobaruser',
      password: 'foobarpasswd'
    };

    _.defaults(authOpts, options);

    results.dump = pu.run.arangoDumpRestore(authOpts, instanceInfo, 'dump',
      'UnitTestsDumpSrc');

    if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
      (results.dump.status === true)) {
      print(CYAN + Date() + ': Dump and Restore - restore' + RESET);

      results.restore = pu.run.arangoDumpRestore(authOpts, instanceInfo, 'restore',
        'UnitTestsDumpDst');

      if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
        (results.restore.status === true)) {
        print(CYAN + Date() + ': Dump and Restore - dump after restore' + RESET);

        results.test = pu.runInArangosh(authOpts, instanceInfo,
          makePathUnix('js/server/tests/dump/dump-authentication.js'), {
            'server.database': 'UnitTestsDumpDst'
          });

        if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
          (results.test.status === true)) {
          print(CYAN + Date() + ': Dump and Restore - teardown' + RESET);

          results.tearDown = pu.runInArangosh(options, instanceInfo,
            makePathUnix('js/server/tests/dump/dump-teardown.js'), auth2);
        }
      }
    }
  }

  print(CYAN + 'Shutting down...' + RESET);
  pu.shutdownInstance(instanceInfo, options);
  print(CYAN + 'done.' + RESET);

  print();

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: foxx manager
// //////////////////////////////////////////////////////////////////////////////

testFuncs.foxx_manager = function (options) {
  print('foxx_manager tests...');

  let instanceInfo = pu.startInstance('tcp', options, {}, 'foxx_manager');

  if (instanceInfo === false) {
    return {
      foxx_manager: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  let results = {};

  results.update = pu.run.arangoshCmd(options, instanceInfo, {
    'configuration': fs.join(pu.CONFIG_DIR, 'foxx-manager.conf')
  }, ['update']);

  if (results.update.status === true || options.force) {
    results.search = pu.run.arangoshCmd(options, instanceInfo, {
      'configuration': fs.join(pu.CONFIG_DIR, 'foxx-manager.conf')
    }, ['search', 'itzpapalotl']);
  }

  print('Shutting down...');
  pu.shutdownInstance(instanceInfo, options);
  print('done.');

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: http_replication
// //////////////////////////////////////////////////////////////////////////////

testFuncs.http_replication = function (options) {
  var opts = {
    'replication': true
  };
  _.defaults(opts, options);

  return rubyTests(opts, false);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: http_server
// //////////////////////////////////////////////////////////////////////////////

testFuncs.http_server = function (options) {
  var opts = {
    'httpTrustedOrigin': 'http://was-erlauben-strunz.it'
  };
  _.defaults(opts, options);
  return rubyTests(opts, false);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: importing
// //////////////////////////////////////////////////////////////////////////////

const impTodos = [{
  id: 'skip',
  data: makePathUnix('js/common/test-data/import/import-skip.csv'),
  coll: 'UnitTestsImportCsvSkip',
  type: 'csv',
  create: 'true',
  skipLines: 3
}, {
  id: 'json1',
  data: makePathUnix('js/common/test-data/import/import-1.json'),
  coll: 'UnitTestsImportJson1',
  type: 'json',
  create: undefined
}, {
  id: 'json2',
  data: makePathUnix('js/common/test-data/import/import-2.json'),
  coll: 'UnitTestsImportJson2',
  type: 'json',
  create: undefined
}, {
  id: 'json3',
  data: makePathUnix('js/common/test-data/import/import-3.json'),
  coll: 'UnitTestsImportJson3',
  type: 'json',
  create: undefined
}, {
  id: 'json4',
  data: makePathUnix('js/common/test-data/import/import-4.json'),
  coll: 'UnitTestsImportJson4',
  type: 'json',
  create: undefined
}, {
  id: 'json5',
  data: makePathUnix('js/common/test-data/import/import-5.json'),
  coll: 'UnitTestsImportJson5',
  type: 'json',
  create: undefined
}, {
  id: 'csv1',
  data: makePathUnix('js/common/test-data/import/import-1.csv'),
  coll: 'UnitTestsImportCsv1',
  type: 'csv',
  create: 'true'
}, {
  id: 'csv2',
  data: makePathUnix('js/common/test-data/import/import-2.csv'),
  coll: 'UnitTestsImportCsv2',
  type: 'csv',
  create: 'true'
}, {
  id: 'csv3',
  data: makePathUnix('js/common/test-data/import/import-3.csv'),
  coll: 'UnitTestsImportCsv3',
  type: 'csv',
  create: 'true'
}, {
  id: 'csv4',
  data: makePathUnix('js/common/test-data/import/import-4.csv'),
  coll: 'UnitTestsImportCsv4',
  type: 'csv',
  create: 'true',
  separator: ';',
  backslash: true
}, {
  id: 'csv5',
  data: makePathUnix('js/common/test-data/import/import-5.csv'),
  coll: 'UnitTestsImportCsv5',
  type: 'csv',
  create: 'true',
  separator: ';',
  backslash: true
}, {
  id: 'csvnoconvert',
  data: makePathUnix('js/common/test-data/import/import-noconvert.csv'),
  coll: 'UnitTestsImportCsvNoConvert',
  type: 'csv',
  create: 'true',
  separator: ',',
  convert: false,
  backslash: true
}, {
  id: 'csvnoeol',
  data: makePathUnix('js/common/test-data/import/import-noeol.csv'),
  coll: 'UnitTestsImportCsvNoEol',
  type: 'csv',
  create: 'true',
  separator: ',',
  backslash: true
}, {
  id: 'tsv1',
  data: makePathUnix('js/common/test-data/import/import-1.tsv'),
  coll: 'UnitTestsImportTsv1',
  type: 'tsv',
  create: 'true'
}, {
  id: 'tsv2',
  data: makePathUnix('js/common/test-data/import/import-2.tsv'),
  coll: 'UnitTestsImportTsv2',
  type: 'tsv',
  create: 'true'
}, {
  id: 'edge',
  data: makePathUnix('js/common/test-data/import/import-edges.json'),
  coll: 'UnitTestsImportEdge',
  type: 'json',
  create: 'false'
}, {
  id: 'unique',
  data: makePathUnix('js/common/test-data/import/import-ignore.json'),
  coll: 'UnitTestsImportIgnore',
  type: 'json',
  create: 'false',
  onDuplicate: 'ignore'
}, {
  id: 'unique',
  data: makePathUnix('js/common/test-data/import/import-unique-constraints.json'),
  coll: 'UnitTestsImportUniqueConstraints',
  type: 'json',
  create: 'false',
  onDuplicate: 'replace'
}];

testFuncs.importing = function (options) {
  if (options.cluster) {
    if (options.extremeVerbosity) {
      print('Skipped because of cluster.');
    }

    return {
      'importing': {
        'status': true,
        'message': 'skipped because of cluster',
        'skipped': true
      }
    };
  }

  let instanceInfo = pu.startInstance('tcp', options, {}, 'importing');

  if (instanceInfo === false) {
    return {
      'importing': {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  let result = {};

  try {
    result.setup = pu.runInArangosh(options, instanceInfo,
      makePathUnix('js/server/tests/import/import-setup.js'));

    if (result.setup.status !== true) {
      throw new Error('cannot start import setup');
    }

    for (let i = 0; i < impTodos.length; i++) {
      const impTodo = impTodos[i];

      result[impTodo.id] = pu.run.arangoImp(options, instanceInfo, impTodo);

      if (result[impTodo.id].status !== true && !options.force) {
        throw new Error('cannot run import');
      }
    }

    result.check = pu.runInArangosh(options, instanceInfo,
      makePathUnix('js/server/tests/import/import.js'));

    result.teardown = pu.runInArangosh(options, instanceInfo,
      makePathUnix('js/server/tests/import/import-teardown.js'));
  } catch (banana) {
    print('An exceptions of the following form was caught:',
      yaml.safeDump(banana));
  }

  print('Shutting down...');
  pu.shutdownInstance(instanceInfo, options);
  print('done.');

  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: recovery
// //////////////////////////////////////////////////////////////////////////////

function runArangodRecovery (instanceInfo, options, script, setup) {
  if (!instanceInfo.tmpDataDir) {
    let td = fs.join(fs.getTempFile(), 'data');
    fs.makeDirectoryRecursive(td);

    instanceInfo.tmpDataDir = td;
  }

  if (!instanceInfo.recoveryArgs) {
    let args = pu.makeArgs.arangod(options);
    args['server.threads'] = 1;
    args['wal.reserve-logfiles'] = 1;
    args['database.directory'] = instanceInfo.tmpDataDir;

    instanceInfo.recoveryArgv = toArgv(args).concat(['--server.rest-server', 'false']);
  }

  let argv = instanceInfo.recoveryArgv;

  if (setup) {
    argv = argv.concat([
      '--log.level', 'fatal',
      '--javascript.script-parameter', 'setup'
    ]);
  } else {
    argv = argv.concat([
      '--log.level', 'info',
      '--wal.ignore-logfile-errors', 'true',
      '--javascript.script-parameter', 'recovery'
    ]);
  }

  argv = argv.concat([
    '--javascript.script', script
  ]);

  let binary = pu.ARANGOD_BIN;
  if (setup) {
    binary = pu.TOP_DIR + '/scripts/disable-cores.sh';
    argv.unshift(pu.ARANGOD_BIN);
  }

  instanceInfo.pid = pu.executeAndWait(binary, argv, options, 'recovery', instanceInfo.rootDir, setup);
}

testFuncs.recovery = function (options) {
  let results = {};

  if (!global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] ||
      global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] === "false") {
    results.recovery = {
      status: false,
      message: 'failure-tests not enabled. please recompile with -DUSE_FAILURE_TESTS=On'
    };
    return results;
  }

  let status = true;

  let recoveryTests = scanTestPath('js/server/tests/recovery');
  let count = 0;

  for (let i = 0; i < recoveryTests.length; ++i) {
    let test = recoveryTests[i];
    let filtered = {};

    if (filterTestcaseByOptions(test, options, filtered)) {
      let instanceInfo = {};
      count += 1;

      runArangodRecovery(instanceInfo, options, test, true);

      runArangodRecovery(instanceInfo, options, test, false);

      if (instanceInfo.tmpDataDir) {
        fs.removeDirectoryRecursive(instanceInfo.tmpDataDir, true);
      }

      results[test] = instanceInfo.pid;

      if (!results[test].status) {
        status = false;
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + test + ' because of ' + filtered.filter);
      }
    }
  }

  if (count === 0) {
    results['ALLTESTS'] = {
      status: false,
      skipped: true
    };
    status = false;
    print(RED + 'No testcase matched the filter.' + RESET);
  }
  results.status = status;

  return {
    recovery: results
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_ongoing
// //////////////////////////////////////////////////////////////////////////////

testFuncs.replication_ongoing = function (options) {
  let master = pu.startInstance('tcp', options, {}, 'master_ongoing');

  const mr = makeResults('replication', master);

  if (master === false) {
    return mr(false, 'failed to start master!');
  }

  let slave = pu.startInstance('tcp', options, {}, 'slave_ongoing');

  if (slave === false) {
    pu.shutdownInstance(master, options);
    return mr(false, 'failed to start slave!');
  }

  let res = pu.run.arangoshCmd(options, master, {}, [
    '--javascript.unit-tests',
    './js/server/tests/replication/replication-ongoing.js',
    slave.endpoint
  ]);

  let results;

  if (!res.status) {
    results = mr(false, 'replication-ongoing.js failed');
  } else {
    results = mr(true);
  }

  print('Shutting down...');
  pu.shutdownInstance(slave, options);
  pu.shutdownInstance(master, options);
  print('done.');

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_static
// //////////////////////////////////////////////////////////////////////////////

testFuncs.replication_static = function (options) {
  let master = pu.startInstance('tcp', options, {
    'server.authentication': 'true'
  }, 'master_static');

  const mr = makeResults('replication', master);

  if (master === false) {
    return mr(false, 'failed to start master!');
  }

  let slave = pu.startInstance('tcp', options, {}, 'slave_static');

  if (slave === false) {
    pu.shutdownInstance(master, options);
    return mr(false, 'failed to start slave!');
  }

  let res = pu.run.arangoshCmd(options, master, {}, [
    '--javascript.execute-string',
    'var users = require("@arangodb/users"); ' +
    'users.save("replicator-user", "replicator-password", true); ' +
    'users.grantDatabase("replicator-user", "_system"); ' +
    'users.reload();'
  ]);

  let results;

  if (res.status) {
    res = pu.run.arangoshCmd(options, master, {}, [
      '--javascript.unit-tests',
      './js/server/tests/replication/replication-static.js',
      slave.endpoint
    ]);

    if (res.status) {
      results = mr(true);
    } else {
      results = mr(false, 'replication-static.js failed');
    }
  } else {
    results = mr(false, 'cannot create users');
  }

  print('Shutting down...');
  pu.shutdownInstance(slave, options);
  pu.shutdownInstance(master, options);
  print('done.');

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: replication_sync
// //////////////////////////////////////////////////////////////////////////////

testFuncs.replication_sync = function (options) {
  let master = pu.startInstance('tcp', options, {}, 'master_sync');

  const mr = makeResults('replication', master);
  if (master === false) {
    return mr(false, 'failed to start master!');
  }

  let slave = pu.startInstance('tcp', options, {}, 'slave_sync');

  if (slave === false) {
    pu.shutdownInstance(master, options);
    return mr(false, 'failed to start slave!');
  }

  let res = pu.run.arangoshCmd(options, master, {}, [
    '--javascript.execute-string',
    'var users = require("@arangodb/users"); ' +
    'users.save("replicator-user", "replicator-password", true); ' +
    'users.reload();'
  ]);

  let results;

  if (res.status) {
    res = pu.run.arangoshCmd(options, master, {}, [
      '--javascript.unit-tests',
      './js/server/tests/replication/replication-sync.js',
      slave.endpoint
    ]);

    if (res.status) {
      results = mr(true);
    } else {
      results = mr(false, 'replication-sync.js failed');
    }
  } else {
    results = mr(false, 'cannot create users');
  }

  print('Shutting down...');
  pu.shutdownInstance(slave, options);
  pu.shutdownInstance(master, options);
  print('done.');

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: resilience
// //////////////////////////////////////////////////////////////////////////////

testFuncs.resilience = function (options) {
  findTests();
  options.cluster = true;
  options.propagateInstanceInfo = true;
  if (options.dbServers < 5) {
    options.dbServers = 5;
  }
  return performTests(options, testsCases.resilience, 'resilience', runThere);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: client resilience
// //////////////////////////////////////////////////////////////////////////////

testFuncs.client_resilience = function (options) {
  findTests();
  options.cluster = true;
  if (options.coordinators < 2) {
    options.coordinators = 2;
  }

  return performTests(options, testsCases.client_resilience, 'client_resilience', pu.createArangoshRunner());
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_replication
// //////////////////////////////////////////////////////////////////////////////

testFuncs.shell_replication = function (options) {
  findTests();

  var opts = {
    'replication': true
  };
  _.defaults(opts, options);

  return performTests(opts, testsCases.replication, 'shell_replication', runThere);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_client
// //////////////////////////////////////////////////////////////////////////////

testFuncs.shell_client = function (options) {
  findTests();

  return performTests(options, testsCases.client, 'shell_client', pu.createArangoshRunner());
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

testFuncs.server_http = function (options) {
  findTests();

  return performTests(options, testsCases.server_http, 'server_http', runThere);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_server
// //////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server = function (options) {
  findTests();
  options.propagateInstanceInfo = true;

  return performTests(options, testsCases.server, 'shell_server', runThere);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: cluster_sync
// //////////////////////////////////////////////////////////////////////////////

testFuncs.cluster_sync = function (options) {
  if (options.cluster) {
    // may sound strange but these are actually pure logic tests
    // and should not be executed on the cluster
    return {
      'cluster_sync': {
        'status': true,
        'message': 'skipped because of cluster',
        'skipped': true
      }
    };
  }
  findTests();
  options.propagateInstanceInfo = true;

  return performTests(options, testsCases.cluster_sync, 'cluster_sync', runThere);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_server_aql
// //////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server_aql = function (options) {
  findTests();

  let cases;
  let name;

  if (!options.skipAql) {
    if (options.skipRanges) {
      cases = testsCases.server_aql;
      name = 'shell_server_aql_skipranges';
    } else {
      cases = testsCases.server_aql.concat(testsCases.server_aql_extended);
      name = 'shell_server_aql';
    }

    cases = splitBuckets(options, cases);

    return performTests(options, cases, name, runThere);
  }

  return {
    shell_server_aql: {
      status: true,
      skipped: true
    }
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_server_only
// //////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server_only = function (options) {
  findTests();

  return performTests(options, testsCases.server_only, 'shell_server_only', runThere);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_server_perf
// //////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server_perf = function (options) {
  findTests();

  return performTests(options, testsCases.server_aql_performance,
    'shell_server_perf', runThere);
};

testFuncs.endpoints = function (options) {
  print(CYAN + 'Endpoints tests...' + RESET);

  let endpoints = {
    'tcpv4': function () {
      return 'tcp://127.0.0.1:' + pu.findFreePort(options.minPort, options.maxPort);
    },
    'tcpv6': function () {
      return 'tcp://[::1]:' + pu.findFreePort(options.minPort, options.maxPort);
    },
    'unix': function () {
      if (platform.substr(0, 3) === 'win') {
        return undefined;
      }
      return 'unix:///tmp/arangodb-tmp.sock';
    }
  };

  return Object.keys(endpoints).reduce((results, endpointName) => {
    let testName = 'endpoint-' + endpointName;
    results[testName] = (function () {
      let endpoint = endpoints[endpointName]();
      if (endpoint === undefined || options.cluster || options.skipEndpoints) {
        return {
          status: true,
          skipped: true
        };
      } else {
        let instanceInfo = pu.startInstance('tcp', Object.assign(options, {useReconnect: true}), {
          'server.endpoint': endpoint
        }, testName);

        if (instanceInfo === false) {
          return {
            status: false,
            message: 'failed to start server!'
          };
        }

        let result = pu.runInArangosh(options, instanceInfo, 'js/client/tests/endpoint-spec.js');

        print(CYAN + 'Shutting down...' + RESET);
        // mop: mehhh...when launched with a socket we can't use download :S
        pu.shutdownInstance(instanceInfo, Object.assign(options, {useKillExternal: true}));
        print(CYAN + 'done.' + RESET);

        return result;
      }
    }());
    return results;
  }, {});
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: single_client
// //////////////////////////////////////////////////////////////////////////////

function singleUsage (testsuite, list) {
  print('single_' + testsuite + ': No test specified!\n Available tests:');
  let filelist = '';

  for (let fileNo in list) {
    if (/\.js$/.test(list[fileNo])) {
      filelist += ' ' + list[fileNo];
    }
  }

  print(filelist);
  print('usage: single_' + testsuite + ' \'{"test":"<testfilename>"}\'');
  print(' where <testfilename> is one from the list above.');

  return {
    usage: {
      status: false,
      message: 'No test specified!'
    }
  };
}

testFuncs.single_client = function (options) {
  options.writeXmlReport = false;

  if (options.test !== undefined) {
    let instanceInfo = pu.startInstance('tcp', options, {}, 'single_client');

    if (instanceInfo === false) {
      return {
        single_client: {
          status: false,
          message: 'failed to start server!'
        }
      };
    }

    const te = options.test;

    print('\n' + Date() + ' arangosh: Trying ', te, '...');

    let result = {};

    result[te] = pu.runInArangosh(options, instanceInfo, te);

    print('Shutting down...');

    if (result[te].status === false) {
      options.cleanup = false;
    }

    pu.shutdownInstance(instanceInfo, options);

    print('done.');

    return result;
  } else {
    findTests();
    return singleUsage('client', testsCases.client);
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: single_server
// //////////////////////////////////////////////////////////////////////////////

testFuncs.single_server = function (options) {
  options.writeXmlReport = false;

  if (options.test === undefined) {
    findTests();
    return singleUsage('server', testsCases.server);
  }

  let instanceInfo = pu.startInstance('tcp', options, {}, 'single_server');

  if (instanceInfo === false) {
    return {
      single_server: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  const te = options.test;

  print('\n' + Date() + ' arangod: Trying', te, '...');

  let result = {};
  let reply = runThere(options, instanceInfo, makePathGeneric(te));

  if (reply.hasOwnProperty('status')) {
    result[te] = reply;

    if (result[te].status === false) {
      options.cleanup = false;
    }
  }

  print('Shutting down...');

  if (result[te].status === false) {
    options.cleanup = false;
  }

  pu.shutdownInstance(instanceInfo, options);

  print('done.');

  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: ssl_server
// //////////////////////////////////////////////////////////////////////////////

testFuncs.ssl_server = function (options) {
  if (options.skipSsl) {
    return {
      ssl_server: {
        status: true,
        skipped: true
      }
    };
  }
  var opts = {
    'httpTrustedOrigin': 'http://was-erlauben-strunz.it'
  };
  _.defaults(opts, options);
  return rubyTests(opts, true);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: upgrade
// //////////////////////////////////////////////////////////////////////////////

testFuncs.upgrade = function (options) {
  if (options.cluster) {
    return {
      'upgrade': {
        'status': true,
        'message': 'skipped because of cluster',
        'skipped': true
      }
    };
  }

  let result = {
    upgrade: {
      status: true,
      total: 1
    }
  };

  const tmpDataDir = fs.getTempFile();
  fs.makeDirectoryRecursive(tmpDataDir);

  const appDir = fs.join(tmpDataDir, 'app');
  const port = pu.findFreePort(options.minPort, options.maxPort);

  let args = pu.makeArgs.arangod(options, appDir);
  args['server.endpoint'] = 'tcp://127.0.0.1:' + port;
  args['database.directory'] = fs.join(tmpDataDir, 'data');
  args['database.auto-upgrade'] = true;

  fs.makeDirectoryRecursive(fs.join(tmpDataDir, 'data'));

  const argv = toArgv(args);

  result.upgrade.first = pu.executeAndWait(pu.ARANGOD_BIN, argv, options, 'upgrade', tmpDataDir);

  if (result.upgrade.first.status !== true) {
    print('not removing ' + tmpDataDir);
    return result.upgrade;
  }

  ++result.upgrade.total;

  result.upgrade.second = pu.executeAndWait(pu.ARANGOD_BIN, argv, options, 'upgrade', tmpDataDir);

  if (result.upgrade.second.status !== true) {
    print('not removing ' + tmpDataDir);
    return result.upgrade;
  }

  pu.cleanupDBDirectoriesAppend(tmpDataDir);

  result.upgrade.status = true;
  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief STRESS TEST: stress_crud
// //////////////////////////////////////////////////////////////////////////////

testFuncs.stress_crud = function (options) {
  const duration = options.duration;
  const concurrency = options.concurrency;

  const command = `
    const stressCrud = require('./js/server/tests/stress/crud')

    stressCrud.createDeleteUpdateParallel({
      concurrency: ${concurrency},
      duration: ${duration},
      gnuplot: true,
      pauseFor: 60
    })
`;

  return runStressTest(options, command, 'stress_crud');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief STRESS TEST: stress_killing
// //////////////////////////////////////////////////////////////////////////////

testFuncs.stress_killing = function (options) {
  const duration = options.duration;

  let opts = {
    concurrency: 4
  };

  _.defaults(opts, options);

  const command = `
    const stressCrud = require('./js/server/tests/stress/killingQueries')

    stressCrud.killingParallel({
      duration: ${duration},
      gnuplot: true
    })
`;

  return runStressTest(opts, command, 'stress_killing');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief STRESS TEST: stress_locks
// //////////////////////////////////////////////////////////////////////////////

testFuncs.stress_locks = function (options) {
  const duration = options.duration;
  const concurrency = options.concurrency;

  const command = `
    const deadlock = require('./js/server/tests/stress/deadlock')

    deadlock.lockCycleParallel({
      concurrency: ${concurrency},
      duration: ${duration},
      gnuplot: true
    })
`;

  return runStressTest(options, command, 'stress_lock');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief agency tests
// //////////////////////////////////////////////////////////////////////////////

testFuncs.agency = function (options) {
  findTests();

  let saveAgency = options.agency;
  let saveCluster = options.cluster;

  options.agency = true;
  options.cluster = false;

  let results = performTests(options, testsCases.agency, 'agency', pu.createArangoshRunner());

  options.agency = saveAgency;
  options.cluster = saveCluster;

  return results;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief pretty prints the result
// //////////////////////////////////////////////////////////////////////////////

function testCaseMessage (test) {
  if (typeof test.message === 'object' && test.message.hasOwnProperty('body')) {
    return test.message.body;
  } else {
    return test.message;
  }
}

function unitTestPrettyPrintResults (r) {
  print(BLUE + '================================================================================');
  print('TEST RESULTS');
  print('================================================================================\n' + RESET);

  let failedSuite = 0;
  let failedTests = 0;

  let failedMessages = '';
  let SuccessMessages = '';
  try {
    /* jshint forin: false */
    for (let testrunName in r) {
      if (skipInternalMember(r, testrunName)) {
        continue;
      }

      let testrun = r[testrunName];

      let successCases = {};
      let failedCases = {};
      let isSuccess = true;

      for (let testName in testrun) {
        if (skipInternalMember(testrun, testName)) {
          continue;
        }

        let test = testrun[testName];

        if (test.status) {
          successCases[testName] = test;
        } else {
          isSuccess = false;
          ++failedSuite;

          if (test.hasOwnProperty('message')) {
            ++failedTests;
            failedCases[testName] = {
              test: testCaseMessage(test)
            };
          } else {
            let fails = failedCases[testName] = {};

            for (let oneName in test) {
              if (skipInternalMember(test, oneName)) {
                continue;
              }

              let oneTest = test[oneName];

              if (!oneTest.status) {
                ++failedTests;
                fails[oneName] = testCaseMessage(oneTest);
              }
            }
          }
        }
      }

      if (isSuccess) {
        SuccessMessages += '* Test "' + testrunName + '"\n';

        for (let name in successCases) {
          if (!successCases.hasOwnProperty(name)) {
            continue;
          }

          let details = successCases[name];

          if (details.skipped) {
            SuccessMessages += YELLOW + '    [SKIPPED] ' + name + RESET + '\n';
          }
          else {
            SuccessMessages += GREEN + '    [SUCCESS] ' + name + RESET + '\n';
          }
        }
      }
      else {
        failedMessages += '* Test "' + testrunName + '"\n';

        for (let name in successCases) {
          if (!successCases.hasOwnProperty(name)) {
            continue;
          }

          let details = successCases[name];

          if (details.skipped) {
            failedMessages += YELLOW + '    [SKIPPED] ' + name + RESET + '\n';
          } else {
            failedMessages += GREEN + '    [SUCCESS] ' + name + RESET + '\n';
          }
        }

        for (let name in failedCases) {
          if (!failedCases.hasOwnProperty(name)) {
            continue;
          }

          failedMessages += RED + '    [FAILED]  ' + name + RESET + '\n\n';

          let details = failedCases[name];

          let count = 0;
          for (let one in details) {
            if (!details.hasOwnProperty(one)) {
              continue;
            }

            if (count > 0) {
              failedMessages += '\n';
            }
            failedMessages += RED + '      "' + one + '" failed: ' + details[one] + RESET + '\n';
            count++;
          }
        }
      }
    }
    print(SuccessMessages);
    print(failedMessages);
    fs.write('out/testfailures.txt', failedMessages);
    fs.write('out/testfailures.txt', GDB_OUTPUT);
    /* jshint forin: true */

    let color = (!r.crashed && r.status === true) ? GREEN : RED;
    let crashText = '';
    if (r.crashed === true) {
      crashText = RED + ' BUT! - We had at least one unclean shutdown or crash during the testrun.' + RESET;
    }
    print('\n' + color + '* Overall state: ' + ((r.status === true) ? 'Success' : 'Fail') + RESET + crashText);

    if (r.status !== true) {
      print(color + '   Suites failed: ' + failedSuite + ' Tests Failed: ' + failedTests + RESET);
    }
  } catch (x) {
    print('exception caught while pretty printing result: ');
    print(x.message);
    print(JSON.stringify(r));
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief print usage information
// //////////////////////////////////////////////////////////////////////////////

function printUsage () {
  print();
  print('Usage: UnitTest([which, ...], options)');
  print();
  print('       where "which" is one of:\n');

  for (let i in testFuncs) {
    if (testFuncs.hasOwnProperty(i)) {
      let oneFunctionDocumentation;

      if (functionsDocumentation.hasOwnProperty(i)) {
        oneFunctionDocumentation = ' - ' + functionsDocumentation[i];
      } else {
        oneFunctionDocumentation = '';
      }

      let checkAll;

      if (allTests.indexOf(i) !== -1) {
        checkAll = '[x]';
      } else {
        checkAll = '   ';
      }

      print('    ' + checkAll + ' ' + i + ' ' + oneFunctionDocumentation);
    }
  }

  for (let i in optionsDocumentation) {
    if (optionsDocumentation.hasOwnProperty(i)) {
      print(optionsDocumentation[i]);
    }
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief framework to perform unittests
// /
// / This function gets one or two arguments, the first describes which tests
// / to perform and the second is an options object. For `which` the following
// / values are allowed:
// /  Empty will give you a complete list.
// //////////////////////////////////////////////////////////////////////////////

function unitTest (cases, options) {
  if (typeof options !== 'object') {
    options = {};
  }

  _.defaults(options, optionsDefaults);

  if (cases === undefined || cases.length === 0) {
    printUsage();

    print('FATAL: "which" is undefined\n');

    return {
      status: false
    };
  }

  pu.setupBinaries(options.build, options.buildType, options.configDir);
  const jsonReply = options.jsonReply;
  delete options.jsonReply;

  // tests to run
  let caselist = [];

  for (let n = 0; n < cases.length; ++n) {
    let splitted = cases[n].split(/[,;\.|]/);

    for (let m = 0; m < splitted.length; ++m) {
      let which = splitted[m];

      if (which === 'all') {
        caselist = caselist.concat(allTests);
      } else if (testFuncs.hasOwnProperty(which)) {
        caselist.push(which);
      } else {
        let line = 'Unknown test "' + which + '"\nKnown tests are: ';
        let sep = '';

        Object.keys(testFuncs).map(function (key) {
          line += sep + key;
          sep = ', ';
        });

        print(line);

        return {
          status: false
        };
      }
    }
  }

  let globalStatus = true;
  let results = {};

  // running all tests
  for (let n = 0; n < caselist.length; ++n) {
    const currentTest = caselist[n];

    print(BLUE + '================================================================================');
    print('Executing test', currentTest);
    print('================================================================================\n' + RESET);

    if (options.verbose) {
      print(CYAN + 'with options:', options, RESET);
    }

    let result = testFuncs[currentTest](options);
    // grrr...normalize structure
    delete result.status;
    delete result.failed;
    
    let status = Object.values(result).every(testCase => testCase.status === true);
    let failed = Object.values(result).reduce((prev, testCase) => prev + !testCase.status, 0);
    if (!status) {
      globalStatus = false;
    }
    result.failed = failed;
    result.status = status;
    results[currentTest] = result;
  }

  results.status = globalStatus;
  results.crashed = pu.serverCrashed;

  if (globalStatus && !pu.serverCrashed) {
    pu.cleanupDBDirectories(options);
  } else {
    print('not cleaning up as some tests weren\'t successful:\n' +
      pu.getCleanupDBDirectories);
  }

  try {
    yaml.safeDump(JSON.parse(JSON.stringify(results)));
  } catch (err) {
    print(RED + 'cannot dump results: ' + String(err) + RESET);
    print(RED + require('internal').inspect(results) + RESET);
  }

  if (jsonReply === true) {
    return results;
  } else {
    return globalStatus;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief exports
// //////////////////////////////////////////////////////////////////////////////

exports.unitTest = unitTest;

exports.internalMembers = internalMembers;
exports.testFuncs = testFuncs;
exports.unitTestPrettyPrintResults = unitTestPrettyPrintResults;

// TODO write test for 2.6-style queues
// testFuncs.queue_legacy = function (options) {
//   if (options.skipFoxxQueues) {
//     print('skipping test of legacy queue job types')
//     return {}
//   }
//   var startTime
//   var queueAppMountPath = '/test-queue-legacy'
//   print('Testing legacy queue job types')
//   var instanceInfo = pu.startInstance('tcp', options, [], 'queue_legacy')
//   if (instanceInfo === false) {
//     return {status: false, message: 'failed to start server!'}
//   }
//   var data = {
//     naive: {_key: 'potato', hello: 'world'},
//     forced: {_key: 'tomato', hello: 'world'},
//     plain: {_key: 'banana', hello: 'world'}
//   }
//   var results = {}
//   results.install = pu.run.arangoshCmd(options, instanceInfo, {
//     'configuration': 'etc/testing/foxx-manager.conf'
//   }, [
//     'install',
//     'js/common/test-data/apps/queue-legacy-test',
//     queueAppMountPath
//   ])

//   print('Restarting without foxx-queues-warmup-exports...')
//   pu.shutdownInstance(instanceInfo, options)
//   instanceInfo = pu.startInstance('tcp', options, {
//     'server.foxx-queues-warmup-exports': 'false'
//   }, 'queue_legacy', instanceInfo.flatTmpDataDir)
//   if (instanceInfo === false) {
//     return {status: false, message: 'failed to restart server!'}
//   }
//   print('done.')

//   var res, body
//   startTime = time()
//   try {
//     res = download(
//       instanceInfo.url + queueAppMountPath + '/',
//       JSON.stringify(data.naive),
//       {method: 'POST'}
//     )
//     body = JSON.parse(res.body)
//     results.naive = {status: body.success === false, message: JSON.stringify({body: res.body, code: res.code})}
//   } catch (e) {
//     results.naive = {status: true, message: JSON.stringify({body: res.body, code: res.code})}
//   }
//   results.naive.duration = time() - startTime

//   startTime = time()
//   try {
//     res = download(
//       instanceInfo.url + queueAppMountPath + '/?allowUnknown=true',
//       JSON.stringify(data.forced),
//       {method: 'POST'}
//     )
//     body = JSON.parse(res.body)
//     results.forced = (
//       body.success
//       ? {status: true}
//       : {status: false, message: body.error, stacktrace: body.stacktrace}
//      )
//   } catch (e) {
//     results.forced = {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//   }
//   results.forced.duration = time() - startTime

//   print('Restarting with foxx-queues-warmup-exports...')
//   pu.shutdownInstance(instanceInfo, options)
//   instanceInfo = pu.startInstance('tcp', options, {
//     'server.foxx-queues-warmup-exports': 'true'
//   }, 'queue_legacy', instanceInfo.flatTmpDataDir)
//   if (instanceInfo === false) {
//     return {status: false, message: 'failed to restart server!'}
//   }
//   print('done.')

//   startTime = time()
//   try {
//     res = download(instanceInfo.url + queueAppMountPath + '/', JSON.stringify(data.plain), {method: 'POST'})
//     body = JSON.parse(res.body)
//     results.plain = (
//       body.success
//       ? {status: true}
//       : {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//     )
//   } catch (e) {
//     results.plain = {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//   }
//   results.plain.duration = time() - startTime

//   startTime = time()
//   try {
//     for (var i = 0; i < 60; i++) {
//       wait(1)
//       res = download(instanceInfo.url + queueAppMountPath + '/')
//       body = JSON.parse(res.body)
//       if (body.length === 2) {
//         break
//       }
//     }
//     results.final = (
//       body.length === 2 && body[0]._key === data.forced._key && body[1]._key === data.plain._key
//       ? {status: true}
//       : {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//     )
//   } catch (e) {
//     results.final = {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//   }
//   results.final.duration = time() - startTime

//   results.uninstall = pu.run.arangoshCmd(options, instanceInfo, {
//     'configuration': 'etc/testing/foxx-manager.conf'
//   }, [
//     'uninstall',
//     queueAppMountPath
//   ])
//   print('Shutting down...')
//   pu.shutdownInstance(instanceInfo, options)
//   print('done.')
//   if ((!options.skipLogAnalysis) &&
//       instanceInfo.hasOwnProperty('importantLogLines') &&
//       Object.keys(instanceInfo.importantLogLines).length > 0) {
//     print('Found messages in the server logs: \n' + yaml.safeDump(instanceInfo.importantLogLines))
//   }
//   return results
// }
