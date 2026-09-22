/* jshint strict: false, sub: true */
/* global print db arango */
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
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a remote unittest file using /_admin/execute
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const yaml = require('js-yaml');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const ct = require('@arangodb/testutils/client-tools');
const inst = require('@arangodb/testutils/instance');
const {
  toArgv,
  download,
  time
}  = require('internal');

const SetGlobalExecutionDeadlineTo = require('internal').SetGlobalExecutionDeadlineTo;

const testRunnerBase = require('@arangodb/testutils/testrunner').testRunner;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const YELLOW = require('internal').COLORS.COLOR_YELLOW;

function getTestCode(file, options, instanceManager) {
  let filter;
  if (options.testCase) {
    filter = JSON.stringify(options.testCase);
  } else if (options.failed) {
    let failed = options.failed[file] || options.failed;
    filter = JSON.stringify(Object.keys(failed));
  }

  let runTest;
  if (file.indexOf('-spec') === -1) {
    filter = filter || '"undefined"';
    runTest = 'const runTest = require("jsunity").runTest;\n';

  } else {
    filter = filter || '';
    runTest = 'const runTest = require("@arangodb/mocha-runner");\n';
  }
  let ret = '';
  if (instanceManager != null) {
    ret = `global.instanceManager = ${JSON.stringify(instanceManager.getStructure())};\n`;
  }
  return ret + runTest + 'return runTest(' + JSON.stringify(file) + ', true, ' + filter + ');\n';
}

function readTestResult(path, rc, testCase) {
  const jsonFN = fs.join(path, 'testresult.json');
  let buf;
  try {
    buf = fs.read(jsonFN);
    fs.remove(jsonFN);
  } catch (x) {
    let msg = 'readTestResult: failed to read ' + jsonFN + " - " + x;
    print(RED + msg + RESET);
    rc.message += " - " + msg;
    rc.status = false;
    return rc;
  }

  let result;
  try {
    result = JSON.parse(buf);
  } catch (x) {
    let msg = 'readTestResult: failed to parse ' + jsonFN + "'" + buf + "' - " + x;
    print(RED + msg + RESET);
    rc.message += " - " + msg;
    rc.status = false;
    return rc;
  }

  if (Array.isArray(result)) {
    if (result.length === 0) {
      // spec-files - don't have parseable results.
      rc.failed = rc.status ? 0 : 1;
      return rc;
    } else if ((result.length >= 1) &&
               (typeof result[0] === 'object') &&
               result[0].hasOwnProperty('status')) {
      return result[0];
    } else {
      rc.failed = rc.status ? 0 : 1;
      rc.message = "don't know howto handle '" + buf + "'";
      return rc;
    }
  } else if (_.isObject(result)) {
    if ((testCase !== undefined) && result.hasOwnProperty(testCase)) {
      return result[testCase];
    } else {
      if (rc.hasOwnProperty('exitCode') && rc.exitCode !== 0) {
        result.failed += 1;
        result.status = false;
        result.message = rc.message;
      }
      return result;
    }
  } else {
    rc.failed = rc.status ? 0 : 1;
    rc.message = "readTestResult: don't know howto handle '" + buf + "'";
    return rc;
  }
}

function writeTestResult(path, data) {
  const jsonFN = fs.join(path, 'testresult.json');
  fs.write(jsonFN, JSON.stringify(data));
}


class runOnArangodRunner extends testRunnerBase{
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    this.info = "onRemoteArangod";
    this.httpOptions = {};
  }
  preRun() {
      this.httpOptions = inst.makeAuthorizationHeaders(this.options, this.instanceManager.jwt_secret);
      this.httpOptions.method = 'POST';

      this.httpOptions.timeout = this.options.oneTestTimeout;
      if (this.options.isSan) {
        this.httpOptions.timeout *= 2;
      }
      if (this.options.valgrind) {
        this.httpOptions.timeout *= 2;
      }

      this.httpOptions.returnBodyOnError = true;
  }
  runOneTest(file) {
    try {
      let testCode = getTestCode(file, this.options, this.instanceManager);
      const reply = download(this.instanceManager.url + '/_admin/execute?returnAsJSON=true',
                             testCode,
                             this.httpOptions);
      if (!reply.error && reply.code === 200) {
        return JSON.parse(reply.body);
      } else {
        if ((reply.code === 500) &&
            reply.hasOwnProperty('message') &&
            (
              (reply.message.search('Request timeout reached') >= 0 ) ||
                (reply.message.search('timeout during read') >= 0 ) ||
                (reply.message.search('Connection closed by remote') >= 0 )
            )) {
          print(RED + Date() + " request timeout reached (" + reply.message +
                "), aborting test execution" + RESET);
          return {
            status: false,
            message: reply.message,
            forceTerminate: true
          };
        } else if (reply.hasOwnProperty('body')) {
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
}


// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a local unittest file using arangosh
// //////////////////////////////////////////////////////////////////////////////

class runInArangoshRunner extends testRunnerBase {
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    this.info = "forkedArangosh";
  }
  getEndpoint() {
    return this.instanceManager.findEndpoint();
  }
  runOneTest(file) {
    let args = ct.makeArgs.arangosh(this.options);
    args['server.endpoint'] = this.getEndpoint();
    args['server.connection-timeout'] = this.options.httpTimeout;

    args['javascript.unit-tests'] = fs.join(pu.TOP_DIR, file);

    args['javascript.unit-test-filter'] = this.options.testCase;

    args['javascript.execution-deadline'] = this.options.oneTestTimeout;

    if (this.options.forceJson) {
      args['server.force-json'] = true;
    }

    if (!this.options.verbose) {
      args['log.level'] = 'warning';
    }
    if (this.options.extremeVerbosity === true) {
      args['log.level'] = 'v8=debug';
    }
    if (this.addArgs !== undefined) {
      args = Object.assign(args, this.addArgs);
    }
    require('internal').env.INSTANCEINFO = JSON.stringify(this.instanceManager.getStructure());
    let rc = pu.executeAndWait(pu.ARANGOSH_BIN, toArgv(args), this.options, 'arangosh', this.instanceManager.rootDir, this.options.coreCheck);
    return readTestResult(this.instanceManager.rootDir, rc, args['javascript.unit-tests']);
  }
}


// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a local unittest file in the current arangosh
// //////////////////////////////////////////////////////////////////////////////

class runLocalInArangoshRunner extends testRunnerBase {
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    this.info = "localArangosh";
  }
  runOneTest(file) {
    let endpoint = arango.getEndpoint();
    if (this.options.vst || this.options.http2) {
      let newEndpoint = this.instanceManager.findEndpoint();
      if (endpoint !== newEndpoint) {
        print(`runInLocalArangosh: Reconnecting to ${newEndpoint} from ${endpoint}`);
        arango.reconnect(newEndpoint, '_system', 'root', '');
      }
    }

    let testCode = getTestCode(file, this.options, null);
    global.instanceManager = this.instanceManager;
    let testFunc;
    try {
      eval('testFunc = function () {\n' + testCode + "}");
    } catch (ex) {
      print(RED + 'test failed to parse:');
      print(ex);
      print(RESET);
      return {
        status: false,
        message: "test doesn't parse! '" + file + "' - " + ex.message || String(ex),
        stack: ex.stack
      };
    }

    let startTime = time();
    try {
      SetGlobalExecutionDeadlineTo(this.options.oneTestTimeout);
      arango.timeout(this.options.httpTimeout);
      let result = testFunc();
      let timeout = SetGlobalExecutionDeadlineTo(0.0);
      if (timeout) {
        return {
          timeout: true,
          forceTerminate: true,
          status: false,
          message: `test aborted due to >>${require('internal').getDeadlineReasonString()}<<. Original test status: ${JSON.stringify(result)}`,
          duration: (time() - startTime) * 1000,
        };
      }
      if (result === undefined) {
        return {
          timeout: true,
          status: false,
          message: "test didn't return any result at all!",
          duration: (time() - startTime) * 1000,
        };
      }
      if (!result.hasOwnProperty('duration')) {
        result.duration = (time() - startTime) * 1000;
      }
      return result;
    } catch (ex) {
      let timeout = SetGlobalExecutionDeadlineTo(0.0);
      print(RED + 'test has thrown: ' + (timeout? "because of timeout in execution":""));
      print(ex, ex.stack);
      print(RESET);
      return {
        timeout: timeout,
        forceTerminate: true,
        status: false,
        message: "test has thrown! '" + file + "' - " + ex.message || String(ex),
        stack: ex.stack,
        duration: (time() - startTime) * 1000,
      };
    }
  }
}


class shellv8Runner extends runLocalInArangoshRunner {
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    this.info = "shellv8Runner";
  }

  run(testcases) {
    let obj = this;
    let res = {failed: 0, status: true};
    let filtered = {};
    let rootDir = fs.join(fs.getTempPath(), 'shellv8Runner');
    this.instanceManager = {
      rootDir: rootDir,
      endpoint: 'tcp://127.0.0.1:8888',
      findEndpoint: function() {
        return 'tcp://127.0.0.1:8888';
      },
      getStructure: function() {
        return {
          endpoint: 'tcp://127.0.0.1:8888',
          rootDir: rootDir
        };
      }
    };
    let count = 0;
    fs.makeDirectoryRecursive(rootDir);
    testcases.forEach(function (file, i) {
      if (tu.filterTestcaseByOptions(file, obj.options, filtered)) {
        print('\n' + (new Date()).toISOString() + GREEN + " [============] RunInV8: Trying", file, '... ' + count, RESET);
        res[file] = obj.runOneTest(file);
        if (res[file].status === false) {
          res.failed += 1;
          res.status = false;
        }
      } else if (obj.options.extremeVerbosity) {
        print('Skipped ' + file + ' because of ' + filtered.filter);
      }
      count += 1;
    });
    if (count === 0) {
      res['ALLTESTS'] = {
        status: true,
        skipped: true
      };
      res.status = true;
      print(RED + 'No testcase matched the filter.' + RESET);
    }
    return res;
  }
}


class runWithAllureReport extends testRunnerBase {
  getAllureResults(testResultsDir, results, status, defaultName) {
    // Allure containers describe fixtures, which can be shared by many tests.
    // Test results, rather than fixture lifetimes, define the test cases.
    const allResults = new Map();
    fs.list(testResultsDir).filter(file => file.endsWith('-result.json')).sort().forEach(file => {
      const testResult = JSON.parse(fs.read(fs.join(testResultsDir, file)));
      allResults.set(testResult.uuid, testResult);
    });

    results.total = allResults.size;
    results.failed = 0;
    results.timeout = false;
    if (allResults.size === 0) {
      results.status = false;
      results.failed = 1;
      results.message = `did not find any test results in ${testResultsDir}`;
      return;
    }

    let count = 0;
    allResults.forEach(testResult => {
      const passed = testResult.status === 'passed' || testResult.status === 'skipped';
      const details = testResult.statusDetails || {};
      const message = [details.message, details.trace].filter(part => part).join('\n\n');
      // Keep parameterized tests with identical names separate, and avoid keys
      // reserved for result metadata such as "status" or "message".
      const name = `${defaultName}_${count++}: ${testResult.fullName || testResult.name || testResult.uuid}`;
      results[name] = {
        duration: testResult.stop - testResult.start,
        status: passed,
        skipped: testResult.status === 'skipped',
        message: message || (passed ? '' : `Allure test status: ${testResult.status || 'unknown'}`)
      };
      if (!passed) {
        results.failed++;
      }
    });
    results.status = status && results.failed === 0;
    if (!status && results.failed === 0 && !results.message) {
      results.message = 'Test process failed although Allure reported no failed tests.';
    }
  }
}



exports.runOnArangodRunner = runOnArangodRunner;
exports.runInArangoshRunner = runInArangoshRunner;
exports.runLocalInArangoshRunner = runLocalInArangoshRunner;
exports.shellv8Runner = shellv8Runner;
exports.readTestResult = readTestResult;
exports.writeTestResult = writeTestResult;
exports.getTestCode = getTestCode;
exports.runWithAllureReport = runWithAllureReport;
