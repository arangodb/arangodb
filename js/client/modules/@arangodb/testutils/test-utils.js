/* jshint strict: false, sub: true */
/* global print db arango */
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

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const yaml = require('js-yaml');

const toArgv = require('internal').toArgv;
const time = require('internal').time;
const sleep = require('internal').sleep;
const download = require('internal').download;
const pathForTesting = require('internal').pathForTesting;
const platform = require('internal').platform;
const SetGlobalExecutionDeadlineTo = require('internal').SetGlobalExecutionDeadlineTo;
const userManager = require("@arangodb/users");
const testRunnerBase = require('@arangodb/testutils/testrunner').testRunner;
const setDidSplitBuckets = require('@arangodb/testutils/testrunner').setDidSplitBuckets;

/* Constants: */
// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testsecret = 'haxxmann';
const testServerAuthInfo = {
  'server.authentication': 'true',
  'server.jwt-secret': testsecret
};
const testClientJwtAuthInfo = {
  jwtSecret: testsecret
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the items uniq to arr1 or arr2
// //////////////////////////////////////////////////////////////////////////////

function diffArray (arr1, arr2) {
  return arr1.concat(arr2).filter(function (val) {
    if (!(arr1.includes(val) && arr2.includes(val))) {
      return val;
    }
  });
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
// / @brief filter test-cases according to options
// //////////////////////////////////////////////////////////////////////////////

function filterTestcaseByOptions (testname, options, whichFilter) {
  // These filters require a proper setup, Even if we filter by testcase:
  if ((testname.indexOf('-cluster') !== -1) && !options.cluster) {
    whichFilter.filter = 'noncluster';
    return false;
  }

  if (testname.indexOf('-noncluster') !== -1 && options.cluster) {
    whichFilter.filter = 'cluster';
    return false;
  }

  if (testname.indexOf('-arangosearch') !== -1 && !options.arangosearch) {
    whichFilter.filter = 'arangosearch';
    return false;
  }

  // if we filter, we don't care about the other filters below:
  if (options.hasOwnProperty('test') && (typeof (options.test) !== 'undefined')) {
    whichFilter.filter = 'testcase';
    if (typeof options.test === 'string') {
      return testname.search(options.test) >= 0;
    } else {
      let found = false;
      options.test.forEach(
        filter => {
          if (testname.search(filter) >= 0) {
            found = true;
          }
        }
      );
      return found;
    }
  }

  if ((testname.indexOf('-novst') !== -1) && options.vst) {
    whichFilter.filter = 'skip when invoked with vst';
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

  if (testname.indexOf('-grey') !== -1 && options.skipGrey) {
    whichFilter.filter = 'grey';
    return false;
  }

  if (testname.indexOf('-grey') === -1 && options.onlyGrey) {
    whichFilter.filter = 'grey';
    return false;
  }

  if (testname.indexOf('-graph') !== -1 && options.skipGraph) {
    whichFilter.filter = 'graph';
    return false;
  }

// *.<ext>_DISABLED should be used instead
//  if (testname.indexOf('-disabled') !== -1) {
//    whichFilter.filter = 'disabled';
//    return false;
//  }

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

  if ((testname.indexOf('-noasan') !== -1) && 
    (global.ARANGODB_CLIENT_VERSION(true).asan === 'true') ||
    (global.ARANGODB_CLIENT_VERSION(true).tsan === 'true')) {
    whichFilter.filter = 'skip when built with asan or tsan';
    return false;
  }

  if (options.failed) {
    return options.failed.hasOwnProperty(testname);
  }
  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief split into buckets
// //////////////////////////////////////////////////////////////////////////////

function splitBuckets (options, cases) {
  if (!options.testBuckets) {
    return cases;
  }
  if (cases.length === 0) {
    setDidSplitBuckets(true);
    return cases;
  }

  setDidSplitBuckets(true);
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

  cases.sort();

  for (let i = s % m; i < cases.length; i = i + r) {
    result.push(cases[i]);
  }

  return result;
}

function doOnePathInner (path) {
  return _.filter(fs.list(makePathUnix(path)),
                  function (p) {
                    return (p.substr(-3) === '.js') || (p.substr(-3) === '.rb');;
                  })
    .map(function (x) {
      return fs.join(makePathUnix(path), x);
    }).sort();
}

function scanTestPaths (paths, options) {
  // add Enterprise Edition tests
  if (global.ARANGODB_CLIENT_VERSION(true)['enterprise-version']) {
    paths = paths.concat(paths.map(function(p) {
      return 'enterprise/' + p;
    }));
  }

  let allTestCases = [];

  paths.forEach(function(p) {
    allTestCases = allTestCases.concat(doOnePathInner(p));
  });

  let allFiltered = [];
  let filteredTestCases = _.filter(allTestCases,
                                   function (p) {
                                     let whichFilter = {};
                                     let rc = filterTestcaseByOptions(p, options, whichFilter);
                                     if (!rc) {
                                       allFiltered.push(p + " Filtered by: " + whichFilter.filter);
                                     }
                                     return rc;
                                   });
  if ((filteredTestCases.length === 0) && (options.extremeVerbosity !== 'silence')) {
    print("No testcase matched the filter: " + JSON.stringify(allFiltered));
    return [];
  }

  return allTestCases;
}


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
  return 'global.instanceManager = ' + JSON.stringify(instanceManager.getStructure()) + ';\n' + runTest +
         'return runTest(' + JSON.stringify(file) + ', true, ' + filter + ');\n';
}
// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a remote unittest file using /_admin/execute
// //////////////////////////////////////////////////////////////////////////////

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
      if (this.options.isAsan) {
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


// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a local unittest file using arangosh
// //////////////////////////////////////////////////////////////////////////////

class runInArangoshRunner extends testRunnerBase{
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    this.info = "forkedArangosh";
  }
  getEndpoint() {
    return this.instanceManager.findEndpoint();
  }
  runOneTest(file) {
    require('internal').env.INSTANCEINFO = JSON.stringify(this.instanceManager.getStructure());
    let args = pu.makeArgs.arangosh(this.options);
    args['server.endpoint'] = this.getEndpoint();

    args['javascript.unit-tests'] = fs.join(pu.TOP_DIR, file);

    args['javascript.unit-test-filter'] = this.options.testCase;

    if (this.options.forceJson) {
      args['server.force-json'] = true;
    }

    if (!this.options.verbose) {
      args['log.level'] = 'warning';
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

class runLocalInArangoshRunner extends testRunnerBase{
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

    let testCode = getTestCode(file, this.options, this.instanceManager);
    require('internal').env.INSTANCEINFO = JSON.stringify(this.instanceManager.getStructure());
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
          message: "test ran into timeout. Original test status: " + JSON.stringify(result),
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

exports.testServerAuthInfo = testServerAuthInfo;
exports.testClientJwtAuthInfo = testClientJwtAuthInfo;

exports.runOnArangodRunner = runOnArangodRunner;
exports.runInArangoshRunner = runInArangoshRunner;
exports.runLocalInArangoshRunner = runLocalInArangoshRunner;

exports.makePathUnix = makePathUnix;
exports.makePathGeneric = makePathGeneric;
exports.readTestResult = readTestResult;
exports.writeTestResult = writeTestResult;
exports.filterTestcaseByOptions = filterTestcaseByOptions;
exports.splitBuckets = splitBuckets;
exports.doOnePathInner = doOnePathInner;
exports.scanTestPaths = scanTestPaths;
exports.diffArray = diffArray;
exports.pathForTesting = pathForTesting;
