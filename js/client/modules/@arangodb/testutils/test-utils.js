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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

/* Modules: */
const _ = require('lodash');
const fs = require('fs');

const pathForTesting = require('internal').pathForTesting;
const platform = require('internal').platform;
const setDidSplitBuckets = require('@arangodb/testutils/testrunner').setDidSplitBuckets;
const isEnterprise = require("@arangodb/test-helper").isEnterprise;

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


function CopyIntoObject(objectTarget, objectSource) {
  for (var attrname in objectSource) {
    if (objectTarget.hasOwnProperty(attrname)) {
      throw new Error(`'${attrname}' already present with value '${objectTarget[attrname]}' trying to set '${objectSource[attrname]}'`);
    }
    objectTarget[attrname] = objectSource[attrname];
  }
}
function CopyIntoList(listTarget, listSource) {
  for (var i=0; i < listSource.length; i++) {
    let val = listSource[i].split('`');
    if (val.length === 3) {
      listTarget.forEach( line => {
        if (line.search('`' + val[1] + '`') !== -1) {
          throw new Error(`This attribute '${val[1]}' is already documented: '${line}' vs. new: '${listSource[i]}'`);
        }
      });
    }
    listTarget.push(listSource[i]);
  }
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

  if (testname.indexOf('-nonmac') !== -1 && platform.substr(0, 6) === 'darwin') {
    whichFilter.filter = 'non-mac';
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

  if ((testname.indexOf('-noinstr') !== -1) && (options.isInstrumented)) {
    whichFilter.filter = 'skip when built with an instrumented build';
    return false;
  }

  if ((testname.indexOf('-noinstr_or_noncluster') !== -1) && (options.isInstrumented && options.cluster)) {
    whichFilter.filter = 'skip when built with an instrumented build and running in cluster mode';
    return false;
  }

  if ((testname.indexOf('-noasan') !== -1) && (options.isSan)) {
    whichFilter.filter = 'skip when built with asan or tsan';
    return false;
  }

  if ((testname.indexOf('-nocov') !== -1) && (options.isCov)) {
    whichFilter.filter = 'skip when built with coverage';
    return false;
  }

  if ((testname.indexOf('-needfp') !== -1) && (options.isCov)) {
    whichFilter.filter = 'skip when built without failurepoint support';
    return false;
  }

  if (options.failed) {
    return options.failed.hasOwnProperty(testname);
  }

  if (options.skipN !== false) {
    if (options.skipN > 0) {
      options.skipN -= 1;
    }
    if (options.skipN > 0) {
      whichFilter.filter = `skipN: last test skip ${options.skipN}.`;
      return false;
    }
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

  if (cases.length < r) {
    throw `We only have ${m} test cases, cannot split them into ${r} buckets`;
  }

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
                    return p.endsWith('.js');
                  })
    .map(function (x) {
      return fs.join(makePathUnix(path), x);
    }).sort();
}

function scanTestPaths (paths, options, fun) {
  // add Enterprise Edition tests
  if (isEnterprise()) {
    paths = paths.concat(paths.map(function(p) {
      return 'enterprise/' + p;
    }));
  }
  if (fun === undefined) {
    fun = function() {return true;};
  }

  let allTestCases = [];

  paths.forEach(function(p) {
    allTestCases = allTestCases.concat(doOnePathInner(p));
  });

  let allFiltered = [];
  let filteredTestCases = _.filter(allTestCases,
                                   function (p) {
                                     if (!fun(p)) {
                                       allFiltered.push(p + " Filtered by: custom filter");
                                       return false;
                                     }
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

  return filteredTestCases;
}



exports.testServerAuthInfo = testServerAuthInfo;
exports.testClientJwtAuthInfo = testClientJwtAuthInfo;


exports.makePathUnix = makePathUnix;
exports.makePathGeneric = makePathGeneric;
exports.filterTestcaseByOptions = filterTestcaseByOptions;
exports.splitBuckets = splitBuckets;
exports.doOnePathInner = doOnePathInner;
exports.scanTestPaths = scanTestPaths;
exports.diffArray = diffArray;
exports.pathForTesting = pathForTesting;
exports.CopyIntoObject = CopyIntoObject;
exports.CopyIntoList = CopyIntoList;
exports.registerOptions = function(optionsDefaults, optionsDocumentation) {
  CopyIntoObject(optionsDefaults, {
    'skipMemoryIntense': false,
    'skipNightly': true,
    'skipNondeterministic': false,
    'skipGrey': false,
    'skipN': false,
    'onlyGrey': false,
    'onlyNightly': false,
    'skipTimeCritical': false,
    'arangosearch':true,
    'test': undefined,
    'testCase': undefined,
    'testBuckets': undefined,
  });

  CopyIntoList(optionsDocumentation, [
    ' Testcase filtering:',
    '   - `skipMemoryIntense`: tests using lots of resources will be skipped.',
    '   - `skipNightly`: omit the nightly tests',
    '   - `skipRanges`: if set to true the ranges tests are skipped',
    '   - `skipTimeCritical`: if set to true, time critical tests will be skipped.',
    '   - `skipNondeterministic`: if set, nondeterministic tests are skipped.',
    '   - `skipGrey`: if set, grey tests are skipped.',
    '   - `skipN`: skip the first N tests of the suite',
    '   - `onlyGrey`: if set, only grey tests are executed.',
    '   - `arangosearch`: if set to true enable the ArangoSearch-related tests',
    '   - `test`: path to single test to execute for "single" test target, ',
    '             or pattern to filter for other suites',
    '   - `testCase`: filter a jsunity testsuite for one special test case',
    '   - `onlyNightly`: execute only the nightly tests',
    '   - `testBuckets`: split tests in to buckets and execute on, for example',
    '       10/2 will split into 10 buckets and execute the third bucket.',
    ''
  ]);
};
