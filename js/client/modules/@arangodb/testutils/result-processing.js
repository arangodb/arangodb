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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief internal members of the results
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const inspect = internal.inspect;
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const cu = require('@arangodb/testutils/crash-utils');
const { getNumSanitizerReports } = require('@arangodb/testutils/san-file-handler');
const AsciiTable = require('ascii-table');
const yaml = require('js-yaml');
const _ = require('lodash');

/* Constants: */
const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
const YELLOW = internal.COLORS.COLOR_YELLOW;

const internalMembers = [
  'crashreport',
  'code',
  'exitCode',
  'error',
  'status',
  'skipped',
  'duration',
  'failed',
  'total',
  'crashed',
  'ok',
  'message',
  'suiteName',
  'processStats',
  'startupTime',
  'testDuration',
  'timeout',
  'shutdownTime',
  'totalSetUp',
  'totalTearDown',
  'setUpDuration',
  'setUpAllDuration',
  'teardownAllDuration'
];

function skipInternalMember (r, a) {
  return !r.hasOwnProperty(a) || internalMembers.indexOf(a) !== -1;
}

function makePathGeneric (path) {
  return path.split(fs.pathSeparator);
}

let failedRuns = {
};

function gatherFailed(result) {
  return Object.values(result).reduce(function(prev, testCase) {
    if (testCase instanceof Object) {
      return prev + !testCase.status;
    } else {
      return prev;
    }
  }, 0
  );
}

function gatherStatus(result) {
  return Object.values(result).reduce(function(prev, testCase) {
    if (testCase instanceof Object) {
      return prev && testCase.status;
    } else {
      return prev;
    }
  }, true
  );
}

function fancyTimeFormat(time)
{   
    // Hours, minutes and seconds
    var hrs = ~~(time / 3600);
    var mins = ~~((time % 3600) / 60);
    var secs = ~~time % 60;

    // Output like "1:01" or "4:03:59" or "123:03:59"
    var ret = "";

    if (hrs > 0) {
        ret += "" + hrs + ":" + (mins < 10 ? "0" : "");
    }

    ret += "" + mins + ":" + (secs < 10 ? "0" : "");
    ret += "" + secs;
    return ret;
}


function iterateTestResults(options, results, state, handlers) {
  let bucketName = "";
  if (options.testBuckets) {
    let n = options.testBuckets.split('/');
    bucketName = "_" + n[1];
  }
  try {
    /* jshint forin: false */
    // i.e. shell_server_aql
    for (let testRunName in results) {
      if (skipInternalMember(results, testRunName)) {
        continue;
      }
      let testRun = results[testRunName];

      if (handlers.hasOwnProperty('testRun')) {
        handlers.testRun(options, state, testRun, testRunName);
      }

      // i.e. tests/js/server/aql/aql-cross.js
      for (let testSuiteName in testRun) {
        if (skipInternalMember(testRun, testSuiteName)) {
          continue;
        }
        let testSuite = testRun[testSuiteName];

        if (handlers.hasOwnProperty('testSuite')) {
          handlers.testSuite(options, state, testSuite, testSuiteName);
        }

        // i.e. testDocument1
        for (let testName in testSuite) {
          if (skipInternalMember(testSuite, testName)) {
            continue;
          }
          let testCase = testSuite[testName];
          
          if (handlers.hasOwnProperty('testCase')) {
            handlers.testCase(options, state, testCase, testName);
          }
        }
        if (handlers.hasOwnProperty('endTestSuite')) {
          handlers.endTestSuite(options, state, testSuite, testSuiteName);
        }
      }
      if (handlers.hasOwnProperty('endTestRun')) {
        handlers.endTestRun(options, state, testRun, testRunName);
      }
    }
  } catch (x) {
    print("Exception occured in running tests: " + x.message + "\n" + x.stack);
  }
}

function locateFailState(options, results) {
  let failedStates = {
    state: true,
    failCount: 0,
    thisFailedTestCount: 0,
    runFailedTestCount: 0,
    currentSuccess: true,
    failedTests: {},
    thisFailedTests: []
  };
  iterateTestResults(options, results, failedStates, {
    testRun: function(options, state, testRun, testRunName) {
      if (!testRun.state) {
        state.state = false;
        state.thisFailedTestCount = testRun.failed;
      }
    },
    testCase: function(options, state, testCase, testCaseName) {
      if (!testCase.state) {
        state.state = false;
        state.runFailedTestCount ++;
        state.thisFailedTests.push(testCaseName);
      }
    },
    endTestRun: function(options, state, testRun, testRunName) {
      if (state.thisFailedTestCount !== 0) {
        if (state.thisFailedTestCount !== state.runFailedTestCount) {
          // todo error message
        }
        state.failedTests[testRunName] = state.thisFailedTests;
        state.failCount += state.runFailedTestCount;

        state.thisFailedTests = [];
        state.thisFailedTestCount = 0;
      }
    }
  });
}

// //////////////////////////////////////////////////////////////////////////////
//  @brief converts results to XML representation
// //////////////////////////////////////////////////////////////////////////////

function saveToJunitXML(options, results) {
  function xmlEscape (s) {
    return s.replace(/[<>&"]/g, function (c) {
      return '&' + {
        '<': 'lt',
        '>': 'gt',
        '&': 'amp',
        '"': 'quot'
      }[c] + ';';
    });
  }
  function stripAnsiColors(s) {
    return s.replace(/[\u001b\u009b][[()#;?]*(?:[0-9]{1,4}(?:;[0-9]{0,4})*)?[0-9A-ORZcf-nqry=><]/g, '');
  }
  function buildXml () {
    let xml = ['<?xml version="1.0" encoding="UTF-8"?>\n'];

    xml.text = function (s) {
      Array.prototype.push.call(this, s);
      return this;
    };

    xml.elem = function (tagName, attrs, close) {
      this.text('<').text(tagName);
      attrs = attrs || {};

      for (let a in attrs) {
        if (attrs.hasOwnProperty(a)) {
          this.text(' ').text(a).text('="')
            .text(xmlEscape(String(attrs[a]))).text('"');
        }
      }

      if (close) {
        this.text('/');
      }

      this.text('>\n');

      return this;
    };

    return xml;
  }
  let xmlState = {
    xml: undefined,
    xmlName: '',
    testRunName: '',
    seenTestCases: false,
  };
  let prefix = (options.cluster ? 'CL_' : '') + (pu.isEnterpriseClient ? 'EE_' : 'CE_');

  const addOptionalDuration = (elem, test) => {
    if (test.hasOwnProperty('duration') && test.duration !== undefined) {
      // time is in seconds
      elem['time'] =  test.duration / 1000;
    }
    return elem;
  };

  iterateTestResults(options, results, xmlState, {
    testRun: function(options, state, testRun, testRunName) {state.testRunName = testRunName;},
    testSuite: function(options, state, testSuite, testSuiteName) {
      const total = testSuite.hasOwnProperty('total') ? testSuite.total : 0;
      const failed = testSuite.hasOwnProperty('failed') ? testSuite.failed : 0;
      state.seenTestCases = false;
      state.xml = buildXml();
      state.xmlName = prefix + state.testRunName + '__' + makePathGeneric(testSuiteName).join('_');
      let errors = 0;
      if (!testSuite.status && testSuite.hasOwnProperty('message')) {
        errors = 1;
      }
      let elm = {
        errors: errors,
        failures: failed,
        tests: total,
        name: state.xmlName,
      };
      state.xml.elem('testsuite', addOptionalDuration(elm, testSuite));
    },
    testCase: function(options, state, testCase, testCaseName) {
      const success = (testCase.status === true);

      state.xml.elem('testcase', addOptionalDuration({ name: prefix + testCaseName }, testCase), success);      

      state.seenTestCases = true;
      if (!success) {
        state.xml.elem('failure');
        let msg;
        if (testCase.hasOwnProperty('message')) {
          msg = stripAnsiColors(testCase.message);
        } else {
          msg = stripAnsiColors(`testcase ${testCaseName} doesn't have a message! ${JSON.stringify(testCase)}`);
        }
        state.xml.text('<![CDATA[' + msg  + ']]>\n');
        state.xml.elem('/failure');
        state.xml.elem('/testcase');
      }
    },
    endTestSuite: function(options, state, testSuite, testSuiteName) {
      if (testSuite.hasOwnProperty('skipped') && testSuite.skipped) {
        state.xml.elem('testcase', {
          name:  state.xmlName,
            time: 0.0
        }, false);
        state.xml.elem('skipped/', true);
        state.xml.elem('/testcase');
      } else if (!state.seenTestCases) {
        const elem = addOptionalDuration({ name: 'all_tests_in_' + state.xmlName }, testSuite);
        if (testSuite.failed !== 0 && testSuite.failed !== undefined) {
          elem['failures'] = testSuite.failed;
        } 
        state.xml.elem('testcase', elem, true);
      }
      state.xml.elem('/testsuite');
      let fn;
      try {
        fn = fs.join(options.testXmlOutputDirectory,
                         'UNITTEST_RESULT_' + state.xmlName + '.xml');
        fs.write(fn, state.xml.join(''));
      } catch (x) {
        print(`Failed to write '${fn}'! - ${x.message}`);
        throw(x);
      }

    },
    endTestRun: function(options, state, testRun, testRunName) {}
  });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief pretty prints the result
// //////////////////////////////////////////////////////////////////////////////

function unitTestPrettyPrintResults (options, results) {
  let onlyFailedMessages = '';
  let failedMessages = '';
  let SuccessMessages = '';
  let SkipMessages = '';
  let failedSuiteCount = 0;
  let failedTestsCount = 0;
  let successCases = {};
  let failedCases = {};
  let failsOfOneSuite = {};
  let failsOfOneSuiteCount = 0;
  let bucketName = "";
  let testRunStatistics = "";
  let isSuccess = true;
  let isSkipped = false;
  let suiteSuccess = true;

  if (options.testBuckets) {
    let n = options.testBuckets.split('/');
    bucketName = "_" + n[1];
  }
  function testCaseMessage (test) {
    if (typeof test.message === 'object' && test.message.hasOwnProperty('body')) {
      return test.message.body;
    } else {
      return test.message;
    }
  }

  let failedStates = {
    state: true,
    failCount: 0,
    thisFailedTestCount: 0,
    runFailedTestCount: 0,
    currentSuccess: true,
    failedTests: {},
    thisFailedTests: []
  };

  iterateTestResults(options, results, failedStates, {
    testRun: function(options, state, testRun, testRunName) {
    },
    testSuite: function(options, state, testSuite, testSuiteName) {
      if (testSuite.status) {
        successCases[testSuiteName] = testSuite;
      } else {
        ++failedSuiteCount;
        isSuccess = false;
        if (testSuite.hasOwnProperty('message')) {
          failedCases[testSuiteName] = {
            test: testCaseMessage(testSuite)
          };
          failsOfOneSuite[testSuiteName + '_ALL'] = testCaseMessage(testSuite);
        }
      }
    },
    testCase: function(options, state, testCase, testCaseName) {
      if (!testCase.status) {
        failsOfOneSuite[testCaseName] = testCaseMessage(testCase);
        failsOfOneSuiteCount ++;

      }
    },
    endTestSuite: function(options, state, testSuite, testSuiteName) {
      if (failsOfOneSuiteCount !== 0) {
        failedTestsCount += failsOfOneSuiteCount;
        failedCases[testSuiteName] = failsOfOneSuite;
        failsOfOneSuite = {};
        failsOfOneSuiteCount = 0;
      }
    },
    endTestRun: function(options, state, testRun, testRunName) {
      if (isSuccess) {
        SuccessMessages += '* Test "' + testRunName + bucketName + '"\n';

        for (let name in successCases) {
          if (!successCases.hasOwnProperty(name)) {
            continue;
          }

          let details = successCases[name];
          if (details.skipped) {
            let msg = ': ' + successCases[name].message;
            SkipMessages += YELLOW + '    [SKIPPED] ' + name + msg + RESET + '\n';
          } else {
            SuccessMessages += GREEN + '    [SUCCESS] ' + name + RESET + '\n';
          }
        }
      } else {
        let m = '* Test "' + testRunName + bucketName + '"\n';
        onlyFailedMessages += m;
        failedMessages += m;

        for (let name in successCases) {
          if (!successCases.hasOwnProperty(name)) {
            continue;
          }

          let details = successCases[name];

          if (details.skipped) {
            m = '    [SKIPPED] ' + name;
            failedMessages += YELLOW + m + RESET + '\n';
            onlyFailedMessages += m + '\n';
          } else {
            failedMessages += GREEN + '    [SUCCESS] ' + name + RESET + '\n';
          }
        }
        for (let name in failedCases) {
          if (!failedCases.hasOwnProperty(name)) {
            continue;
          }

          let isSkipped = true;
          if (name === 'SKIPPED') {
            m = '    [SKIPPED]  the following tests were skipped due to the server not being in a trustworthy state anymore.';
          } else {
            m = '    [FAILED]  ' + name;
          }
          let details = failedCases[name];
          if (isSkipped && details.test === "") {
            continue;
          }

          failedMessages += RED + m + RESET + '\n\n';
          onlyFailedMessages += m + '\n\n';

          let count = 0;
          for (let one in details) {
            if (!details.hasOwnProperty(one)) {
              continue;
            }

            if (count > 0) {
              failedMessages += '\n';
              onlyFailedMessages += '\n';
            }
            m = '      "' + one + '" failed: ' + details[one];
            failedMessages += RED + m + RESET + '\n\n';
            onlyFailedMessages += m + '\n\n';
            count++;
          }
        }
      }
      isSuccess = true;
      successCases = {};
      failedCases = {};
    }
  });
  let color,statusMessage;
  let crashedText = '';
  let crashText = '';
  let failText = '';
  if (results.status === true) {
    color = GREEN;
    statusMessage = 'Success';
  } else {
    color = RED;
    statusMessage = 'Fail';
  }
  if (results.crashed === true) {
    color = RED;
    for (let failed in failedRuns) {
      crashedText += ' [' + failed + '] : ' + failedRuns[failed].replace(/^/mg, '    ');
    }
    crashedText += "\nMarking crashy!";
    crashText = color + crashedText + RESET;
  }
  if (results.status !== true) {
    failText = '\n   Suites failed: ' + failedSuiteCount + ' Tests Failed: ' + failedTestsCount;
    onlyFailedMessages += failText + '\n';
    failText = RED + failText + RESET;
  }
  if (getNumSanitizerReports() > 0) { 
    failText += YELLOW + `\n\n+++ ${getNumSanitizerReports()} sanitizer report(s) found! +++\n` + RESET;
  }
  if (cu.GDB_OUTPUT !== '' && options.crashAnalysisText === options.testFailureText) {
    // write more verbose failures to the testFailureText file
    onlyFailedMessages += '\n\n' + cu.GDB_OUTPUT;
  }
  if (!options.extremeVerbosity) {
    SkipMessages = '';
  }
  print(`
${YELLOW}================================================================================'
TEST RESULTS
================================================================================${RESET}
${SuccessMessages}
${SkipMessages}
${failedMessages}${color} * Overall state: ${statusMessage}${RESET}${crashText}${failText}`);

  onlyFailedMessages;
  if (crashedText !== '') {
    onlyFailedMessages += '\n' + crashedText;
  }
  fs.write(fs.join(options.testOutputDirectory, options.testFailureText), onlyFailedMessages);

  if (cu.GDB_OUTPUT !== '' && options.crashAnalysisText !== options.testFailureText ) {
    // write more verbose failures to the testFailureText file
    fs.write(fs.join(options.testOutputDirectory, options.crashAnalysisText), cu.GDB_OUTPUT);
  }

}


// //////////////////////////////////////////////////////////////////////////////
// / @brief pretty prints the result with tabular facts added
// //////////////////////////////////////////////////////////////////////////////

function formatNone(str) {
  return str;
}
function formatTimeMS(ts) {
  return fancyTimeFormat(ts / 1000);
}
function formatNetStat(val) {
  return JSON.stringify(summarizeStats(val['processStats']));
}
function unitTestTabularPrintResults (options, results, otherResults) {
  let tableColumns = [];
  let tableColumnVectors = [];
  let tableColumnHeaders = ['suite name'];
  let tableFormaters = [formatNone];
  let timeFormatColumns = [
    'duration',
    'total',
    'totalSetUp',
    'totalTearDown'
  ];
  
  if (options.hasOwnProperty('tableColumns')) {
    tableColumns = options.tableColumns.split(',');
  }
  tableColumns.forEach(colName => {
    if (timeFormatColumns.find(val => {return val === colName; })) {
      tableFormaters.push(fancyTimeFormat);
    } else if (colName === 'netstat') {
      tableFormaters.push(formatNetStat);
    } else {
      tableFormaters.push(formatNone);
    }
    let vec = colName.split(".");
    tableColumnVectors.push(vec);
    tableColumnHeaders.push(vec[vec.length - 1].replace(/_/g, ' '));
  });
  
  let testRunStatistics = "";
  let testRunStatisticsHeader = ['Setup', 'Run', 'tests', 'setupAll', 'suite name'];
  let sortedByDuration = [];
  let failedStates = {};
  let currentTestrun = "";
  // these suites don't produce setup duration:
  let durationBlacklist = [
    'ssl_server',
    'http_server',
    'importing',
    'gtest',
    'dump',
    'authentification',
    'arangobench',
    'arangosh',
    'export',
    'hot_backup',
    'dump_multiple',
    'config'
  ];

  let resultTable;

  iterateTestResults(options, results, failedStates, {
    testRun: function(options, state, testRun, testRunName) {
      resultTable = new AsciiTable(testRunName);
      resultTable.setHeading(tableColumnHeaders);
    },
    testSuite: function(options, state, testSuite, testSuiteName) {
      let setupAllDuration = 0;
      if (testSuite.hasOwnProperty('setUpAllDuration')) {
        Object.keys(testSuite.setUpAllDuration).forEach(testName => {
          setupAllDuration += testSuite.setUpAllDuration[testName];
        });
      }
      let hasSetupAll = setupAllDuration !== 0;
      let stats = summarizeStats(testSuite['processStats']);
      if (testSuite.hasOwnProperty('totalSetUp') &&
          testSuite.hasOwnProperty('totalTearDown')) {
        sortedByDuration.push({
          testName: testSuiteName,
          setupTearDown: testSuite.totalSetUp + testSuite.totalTearDown,
          duration: testSuite.duration,
          hasSetupAll: hasSetupAll,
          count: Object.keys(testSuite).filter(testCase => ! skipInternalMember(testSuite, testCase)).length,
          netstat: JSON.stringify(stats.netstat)
        });
      } else {
        if (!durationBlacklist.find(item => { return item === currentTestrun; }) &&
            testSuiteName.search("-spec") === -1 &&
            !testSuite.hasOwnProperty('skipped') &&
            !testSuite.skipped) {
          let details = "";
          if (options.extremeVerbosity === true) {
            details = "\n" + JSON.stringify(testSuite);
          }
          print(RED + "This test doesn't have setup a duration: " + currentTestrun + "." + testSuiteName + details + RESET);
        }
      }
      let resultLine = [testSuiteName];
      let count = 1;
      tableColumnVectors.forEach(valueVec => {
        let layer = testSuite;
        valueVec.forEach(attribute => {
          if (layer.hasOwnProperty(attribute)) {
            layer = layer[attribute];
          }
        });
        if (Array.isArray(layer)) {
          resultLine.push('n/a');
        } else {
          resultLine.push(tableFormaters[count](layer));
        }
        count += 1;
      });
      resultTable.addRow(resultLine);

    },
    testCase: function(options, state, testCase, testCaseName) {
    },
    endTestSuite: function(options, state, testSuite, testSuiteName) {
    },
    endTestRun: function(options, state, testRun, testRunName) {
      testRunStatistics += resultTable.toString();
    }
  });
  print(testRunStatistics);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a chartlist of the longest running tests
// //////////////////////////////////////////////////////////////////////////////
function summarizeStats(deltaStats) {
  let sumStats = {};
  for (let instance in deltaStats) {
    sumStats['netstat'] = {};
    if (instance === 'netstat') {
      for (let nsInstance in deltaStats[instance]) {
        for (const dir of ['in', 'out']) {
          for (let type in deltaStats[instance][nsInstance][dir]) {
            if (!sumStats['netstat'].hasOwnProperty(type)) {
              sumStats['netstat'][type] = deltaStats[instance][nsInstance][dir][type];
            } else {
              sumStats['netstat'][type] += deltaStats[instance][nsInstance][dir][type];
            }
          }
        }
      }
    } else {
      for (let key in deltaStats[instance]) {
        if (!sumStats.hasOwnProperty(key)) {
          sumStats[key] = 0;
        }
        sumStats[key] += deltaStats[instance][key];
      }
    }
  }
  return sumStats;
}

function locateLongRunning(options, results, otherResults) {
  let testRunStatistics = "";
  let sortedByDuration = [];
  let pathForJson = {};
  let failedStates = {
    state: true,
    failCount: 0,
    thisFailedTestCount: 0,
    runFailedTestCount: 0,
    currentSuccess: true,
    failedTests: {},
    thisFailedTests: []
  };
  iterateTestResults(options, results, failedStates, {
    testRun: function(options, state, testRun, testRunName) {
      let startup = testRun['startupTime'];
      let testDuration = testRun['testDuration'];
      let shutdown = testRun['shutdownTime'];
      let color = GREEN;
      if (((startup + shutdown) * 10) > testDuration) {
        color = RED;
      }

      testRunStatistics += `${color}${testRunName} - startup [${startup}] => run [${testDuration}] => shutdown [${shutdown}]${RESET}
`;
    },
    testSuite: function(options, state, testSuite, testSuiteName) {
      pathForJson['testSuite'] = testSuiteName;
      if (testSuite.hasOwnProperty('duration') && testSuite.duration !== 0.0) {
        sortedByDuration.push(
          {
            testName: testSuiteName,
            duration: testSuite.duration,
            test: testSuite,
            count: Object.keys(testSuite).filter(testCase => skipInternalMember(testSuite, testCase)).length
          });
      } else {
        if (!testSuite.hasOwnProperty('skipped') || !testSuite.skipped) {
          print(RED + "This test doesn't have a duration: " + testSuiteName + "\n" + JSON.stringify(testSuite) + RESET);
        }
      }        
    },
    testCase: function(options, state, testCase, testCaseName) {
    },
    endTestSuite: function(options, state, testSuite, testSuiteName) {
    },
    endTestRun: function(options, state, testRun, testRunName) {
      pathForJson['testRunName'] = testRunName;
      sortedByDuration.sort(function(a, b) {
        return a.duration - b.duration;
      });
      let results = {};
      for (let i = sortedByDuration.length - 1; (i >= 0) && (i > sortedByDuration.length - 31); i --) {
        let key = " - " +
            fancyTimeFormat(sortedByDuration[i].duration / 1000) + " - ^ " +
            fancyTimeFormat(sortedByDuration[i].test.totalSetUp / 1000) + " v " +
            fancyTimeFormat(sortedByDuration[i].test.totalTearDown / 1000) + " - " +
            sortedByDuration[i].count + " - [" +
            fancyTimeFormat((
              sortedByDuration[i].duration +
                sortedByDuration[i].test.totalSetUp +
                sortedByDuration[i].test.totalTearDown) / 1000) + "] - " +
            sortedByDuration[i].testName.replace('/\\/g', '/');
        let setupStatistics = [];
        if (_.size(sortedByDuration[i].test.setUpAllDuration) > 1) {
          let sortStart = [];
          let d = sortedByDuration[i].test.setUpAllDuration;
          Object.keys(d).forEach(a => {
            sortStart.push([d[a], a]);
          });
          sortStart.sort(function(a, b) {
            return b[0] - a[0];
          });
          sortStart.forEach(a => {
            setupStatistics.push(fancyTimeFormat(a[0] / 1000) + " - " + a[1]);
          });
        }

        let testCases = [];
        let thisTestSuite = sortedByDuration[i];
        for (let testName in thisTestSuite.test) {
          if (skipInternalMember(thisTestSuite.test, testName)) {
            continue;
          }
          let test = thisTestSuite.test[testName];
          let duration = 0;
          if (test.hasOwnProperty('duration')) {
            duration += test.duration;
          }
          if (test.hasOwnProperty('totalSetUp')) {
            duration += test.totalSetUp;
          }
          if (test.hasOwnProperty('setUpDuration')) {
            duration += test.setUpDuration;
          }
          if (test.hasOwnProperty('tearDownDuration')) {
            duration += test.tearDownDuration;
          }
          if (test.hasOwnProperty('totalTearDown')) {
            duration += test.totalTearDown;
          }
          testCases.push({
            testName: testName,
            duration: duration
          });
        }
        testCases.sort(function(a, b) {
          return a.duration - b.duration;
        });

        let statistics = [];
        for (let j = Object.keys(testCases).length - 1; (j >= 0) && (j > Object.keys(testCases).length - 31); j --) {
          let otherTestTime = "";
          if (otherResults) {
            otherTestTime = otherResults[pathForJson['testSuite']][pathForJson['testRunName']][testCases[j].testName]['duration'];
            otherTestTime = " - " + fancyTimeFormat(otherTestTime / 1000);
          }
          statistics.push(
            fancyTimeFormat(testCases[j].duration / 1000) +
              otherTestTime +
              " - " +
              testCases[j].testName);
        }
        results[key] = {
          'processStatistics': summarizeStats(thisTestSuite.test['processStats']),
          'stats': statistics
        };
        if (setupStatistics.length > 0) {
          results[key]['setupStatistics'] = setupStatistics;
        }
      }
      testRunStatistics +=  yaml.safeDump(results);
      sortedByDuration = [];
    }
  });
  print(testRunStatistics);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a chart list of the tests with the most excessive
//          setup/teardown usage
// //////////////////////////////////////////////////////////////////////////////

function locateLongSetupTeardown(options, results) {
  let testRunStatistics = "";
  let testRunStatisticsHeader = ['Setup', 'Run', 'tests', 'setupAll', 'suite name'];
  let sortedByDuration = [];
  let failedStates = {};
  let currentTestrun = "";
  // these suites don't produce setup duration:
  let durationBlacklist = [
    'ssl_server',
    'http_server',
    'importing',
    'gtest',
    'dump',
    'authentification',
    'arangobench',
    'arangosh',
    'export',
    'hot_backup',
    'dump_multiple',
    'config'
  ];
  let testsToShow = 11;
  if (options.hasOwnProperty('testsToShow')) {
    testsToShow = Number(options.testsToShow);
  }

  iterateTestResults(options, results, failedStates, {
    testRun: function(options, state, testRun, testRunName) {
      currentTestrun = testRunName;
    },
    testSuite: function(options, state, testSuite, testSuiteName) {
      let setupAllDuration = 0;
      if (testSuite.hasOwnProperty('setUpAllDuration')) {
        Object.keys(testSuite.setUpAllDuration).forEach(testName => {
          setupAllDuration += testSuite.setUpAllDuration[testName];
        });
      }
      let hasSetupAll = setupAllDuration !== 0;
      
      if (testSuite.hasOwnProperty('totalSetUp') &&
          testSuite.hasOwnProperty('totalTearDown')) {
        sortedByDuration.push({
          testName: testSuiteName,
          setupTearDown: testSuite.totalSetUp + testSuite.totalTearDown,
          duration: testSuite.duration,
          hasSetupAll: hasSetupAll,
          count: Object.keys(testSuite).filter(testCase => ! skipInternalMember(testSuite, testCase)).length,
        });
      } else {
        if (!durationBlacklist.find(item => { return item === currentTestrun; }) &&
            testSuiteName.search("-spec") === -1 &&
            !testSuite.hasOwnProperty('skipped') &&
            !testSuite.skipped) {
          let details = "";
          if (options.extremeVerbosity === true) {
            details = "\n" + JSON.stringify(testSuite);
          }
          print(RED + "This test doesn't have setup a duration: " + currentTestrun + "." + testSuiteName + details + RESET);
        }
      }        
    },
    testCase: function(options, state, testCase, testCaseName) {
    },
    endTestSuite: function(options, state, testSuite, testSuiteName) {
    },
    endTestRun: function(options, state, testRun, testRunName) {
      sortedByDuration.sort(function(a, b) {
        return a.setupTearDown - b.setupTearDown;
      });
      let resultTable = new AsciiTable(currentTestrun);
      resultTable.setHeading(testRunStatisticsHeader);
      for (let i = sortedByDuration.length - 1; (i >= 0) && (i > sortedByDuration.length - testsToShow); i --) {
        resultTable.addRow([
          fancyTimeFormat(sortedByDuration[i].setupTearDown / 1000),
          fancyTimeFormat(sortedByDuration[i].duration / 1000),
          sortedByDuration[i].count,
          sortedByDuration[i].hasSetupAll,
          sortedByDuration[i].testName.replace('/\\/g', '/')
        ]);
      }

      if (results.length === 0) {
        if (!durationBlacklist.find(item => { return item === currentTestrun; })) {
          print(RED + "no results for: " + currentTestrun + RESET);
        }
      } else {
        testRunStatistics += resultTable.toString();
        sortedByDuration = [];
      }
    }
  });
  print(testRunStatistics);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints the factor server startup/teardown vs. test duration
// //////////////////////////////////////////////////////////////////////////////

function locateShortServerLife(options, results) {
  let rc = true;
  let testRunStatistics = "";
  let sortedByDuration = [];
  
  let failedStates = {
    state: true,
    failCount: 0,
    thisFailedTestCount: 0,
    runFailedTestCount: 0,
    currentSuccess: true,
    failedTests: {},
    thisFailedTests: []
  };
  iterateTestResults(options, results, failedStates, {
    testRun: function(options, state, testRun, testRunName) {
      if (testRun.hasOwnProperty('startupTime') &&
          testRun.hasOwnProperty('testDuration') &&
          testRun.hasOwnProperty('shutdownTime')) {
        let startup = testRun['startupTime'];
        let testDuration = testRun['testDuration'];
        let shutdown = testRun['shutdownTime'];
        let color = GREEN;
        let OK = "YES";
        if (((startup + shutdown) * 10) > testDuration) {
          color = RED;
          OK = "NOP";
          rc = false;
        }

        testRunStatistics += `${color}[${OK}] ${testRunName} - startup [${startup}] => run [${testDuration}] => shutdown [${shutdown}]${RESET}`;
      } else {
        testRunStatistics += `${YELLOW}${testRunName} doesn't have server start statistics${RESET}`;
      }
    }
  });
  print(testRunStatistics);
  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
//  @brief simple status representations
// //////////////////////////////////////////////////////////////////////////////

function writeDefaultReports(options, testSuites) {
  fs.write(fs.join(options.testOutputDirectory, 'UNITTEST_RESULT_EXECUTIVE_SUMMARY.json'), "false", true);
  fs.write(fs.join(options.testOutputDirectory, 'UNITTEST_RESULT_CRASHED.json'), "true", true);
  let testFailureText = 'testfailures.txt';
  if (options.hasOwnProperty('testFailureText')) {
    testFailureText = options.testFailureText;
  }
  fs.write(fs.join(options.testOutputDirectory, testFailureText),
           "Incomplete testrun with these testsuites: '" + testSuites +
           "'\nand these options: " + JSON.stringify(options, null, 2) + "\n" + cu.GDB_OUTPUT);

}

function processCrashReport(result) {
  if (cu.GDB_OUTPUT !== '') {
    result['crashreport'] = cu.GDB_OUTPUT;
    result['crash'] = {
      crash_report: {
        status: false,
        failed: 1,
        all: {
          status: false,
          failed: 1,
          message: (result.crashed ? "SUT crashed: \n": "SUT was aborted: \n") + cu.GDB_OUTPUT
        }
      },
      status: false,
      failed: 1,
    };
    result.status = false;
    result.crashed = true;
  }
}

function writeReports(options, results) {
  fs.write(fs.join(options.testOutputDirectory, 'UNITTEST_RESULT_EXECUTIVE_SUMMARY.json'), String(results.status), true);
  fs.write(fs.join(options.testOutputDirectory, 'UNITTEST_RESULT_CRASHED.json'), String(results.crashed), true);
}

function dumpAllResults(options, results) {
  let j;

  try {
    j = JSON.stringify(results);
  } catch (err) {
    j = inspect(results);
  }

  try {
    fs.write(fs.join(options.testOutputDirectory, 'UNITTEST_RESULT.json'), j, true);
  } catch (x) {
    print("Failed to write ` " +
          fs.join(options.testOutputDirectory, 'UNITTEST_RESULT.json') +
          '`! - ' + x.message);
    throw(x);
  }
}


function addFailRunsMessage(testcase, message) {
  if (!failedRuns.hasOwnProperty(testcase)) {
    failedRuns[testcase] = '';
  } else if (failedRuns[testcase].length > 0) {
    failedRuns[testcase] += '\n';
  }
  failedRuns[testcase] += message;
}

function yamlDumpResults(options, results) {
  try {
    print(yaml.safeDump(JSON.parse(JSON.stringify(results))));
  } catch (err) {
    print(RED + 'cannot dump results: ' + String(err) + RESET);
    print(RED + require('internal').inspect(results) + RESET);
  }
}

function getFailedTestCases(options) {
  try {
    let resultFile = fs.join(options.testOutputDirectory, 'UNITTEST_RESULT.json');
    let lastRun = JSON.parse(fs.readFileSync(resultFile));
    let filter;
    filter = (obj) => {
      return _.pickBy(_.mapValues(obj, v => {
          if (typeof v === 'object' && v.status === false) {
            return filter(v);
          }
          return undefined;
        }),
        v => v !== undefined);
    };
    return filter(lastRun);
  } catch (err) {
    print("Failed to read previous test results");
    throw err;
  }
}

function getGTestResults(fileName, defaultResults) {
  let results = defaultResults;
  if (!fs.exists(fileName)) {
    defaultResults.failed += 1;
    print(RED + "No testresult file found at: " + fileName + RESET);
    return defaultResults;
  }
  let gTestResults = JSON.parse(fs.read(fileName));
  results.failed = gTestResults.failures + gTestResults.errors;
  results.status = (gTestResults.errors === 0) && (gTestResults.failures === 0);
  gTestResults.testsuites.forEach(function (testSuite) {
    results[testSuite.name] = {
      failed: testSuite.failures + testSuite.errors,
      status: (testSuite.failures + testSuite.errors) === 0,
      duration: parseFloat(testSuite.time) * 1000 // gtest writes sec, internally we have ms
    };
    if (testSuite.failures !== 0) {
      const message = testSuite.testsuite.flatMap(
        suite => {
          if (suite.hasOwnProperty('failures')) {
            return suite.failures.map(fail => fail.failure).join("\n");
          }
          return [];
        }).join("\n");
      results[testSuite.name].message = message;
    }
  });
  return results;
}

exports.gatherStatus = gatherStatus;
exports.gatherFailed = gatherFailed;
exports.yamlDumpResults = yamlDumpResults;
exports.addFailRunsMessage = addFailRunsMessage;
exports.processCrashReport = processCrashReport;
exports.dumpAllResults = dumpAllResults;
exports.writeDefaultReports = writeDefaultReports;
exports.writeReports = writeReports;
exports.getFailedTestCases = getFailedTestCases;
exports.getGTestResults = getGTestResults;

exports.analyze = {
  unitTestPrettyPrintResults: unitTestPrettyPrintResults,
  unitTestTabularPrintResults: unitTestTabularPrintResults,
  saveToJunitXML: saveToJunitXML,
  locateLongRunning: locateLongRunning,
  locateShortServerLife: locateShortServerLife,
  locateLongSetupTeardown: locateLongSetupTeardown,
  yaml: yamlDumpResults
};
exports.registerOptions = function(optionsDefaults, optionsDocumentation) {
  tu.CopyIntoObject(optionsDefaults, {
    'testOutputDirectory': 'out',
    'testXmlOutputDirectory': 'outXml',
    'writeXmlReport': false,
    'testFailureText': 'testfailures.txt',
    'crashAnalysisText': 'testfailures.txt',
  });

  tu.CopyIntoList(optionsDocumentation, [
    ' Test result reporting Options:',
    '   - `testFailureText`: filename of the testsummary file',
    '   - `crashAnalysisText`: output of debugger in case of crash',
    '   - `testOutputDirectory`: set the output directory for testresults, defaults to `out`',
    '   - `writeXmlReport`:  Write junit xml report files',
    '   - `testXmlOutputDirectory`: set the output directory for xml testresults, defaults to `out`',
    ''
  ]);
};
