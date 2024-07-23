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
// / @author Wilfried Goesgens
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
const toArgv = require('internal').toArgv;
const download = require('internal').download;
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
    ret = 'global.instanceManager = ' + JSON.stringify(instanceManager.getStructure()) + ';\n';
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
  }
  runOneTest(file) {
    try {
      let testCode = getTestCode(file, this.options, this.instanceManager);
      let httpOptions = _.clone(this.instanceManager.httpAuthOptions);
      httpOptions.method = 'POST';

      httpOptions.timeout = this.options.oneTestTimeout;
      if (this.options.isSan) {
        httpOptions.timeout *= 2;
      }
      if (this.options.valgrind) {
        httpOptions.timeout *= 2;
      }

      httpOptions.returnBodyOnError = true;
      const reply = download(this.instanceManager.url + '/_admin/execute?returnAsJSON=true',
                             testCode,
                             httpOptions);
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
    require('internal').env.INSTANCEINFO = JSON.stringify(this.instanceManager.getStructure());
    let args = ct.makeArgs.arangosh(this.options);
    args['server.endpoint'] = this.getEndpoint();

    args['javascript.unit-tests'] = fs.join(pu.TOP_DIR, file);

    args['javascript.unit-test-filter'] = this.options.testCase;

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
    // TODO require('internal').env.INSTANCEINFO = JSON.stringify(this.instanceManager);
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

    try {
      SetGlobalExecutionDeadlineTo(this.options.oneTestTimeout * 1000);
      let result = testFunc();
      let timeout = SetGlobalExecutionDeadlineTo(0.0);
      if (timeout) {
        return {
          timeout: true,
          forceTerminate: true,
          status: false,
          message: `test aborted due to ${require('internal').getDeadlineReasonString()}. Original test status: ${JSON.stringify(result)}`,
        };
      }
      if (result === undefined) {
        return {
          timeout: true,
          status: false,
          message: "test didn't return any result at all!"
        };
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
        stack: ex.stack
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


exports.runOnArangodRunner = runOnArangodRunner;
exports.runInArangoshRunner = runInArangoshRunner;
exports.runLocalInArangoshRunner = runLocalInArangoshRunner;
exports.shellv8Runner = shellv8Runner;
exports.readTestResult = readTestResult;
exports.writeTestResult = writeTestResult;
