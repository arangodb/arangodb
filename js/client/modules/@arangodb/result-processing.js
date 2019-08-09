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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief internal members of the results
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const inspect = internal.inspect;
const fs = require('fs');
const pu = require('@arangodb/process-utils');
const cu = require('@arangodb/crash-utils');
const yaml = require('js-yaml');

/* Constants: */
const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
const YELLOW = internal.COLORS.COLOR_YELLOW;

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
  'suiteName',
  'processStats',
  'startupTime',
  'testDuration',
  'shutdownTime'
];

let failedRuns = {
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
function unitTestPrettyPrintResults (res, options) {
  function skipInternalMember (r, a) {
    return !r.hasOwnProperty(a) || internalMembers.indexOf(a) !== -1;
  }
  print(YELLOW + '================================================================================');
  print('TEST RESULTS');
  print('================================================================================\n' + RESET);

  let failedSuite = 0;
  let failedTests = 0;

  let onlyFailedMessages = '';
  let failedMessages = '';
  let SuccessMessages = '';
  let bucketName = "";
  let sortedByDuration = [];
  if (options.testBuckets) {
    let n = options.testBuckets.split('/');
    bucketName = "_" + n[1];
  }
  try {
    /* jshint forin: false */
    for (let testrunName in res) {
      if (skipInternalMember(res, testrunName)) {
        continue;
      }

      let testrun = res[testrunName];

      let successCases = {};
      let failedCases = {};
      let isSuccess = true;

      for (let testName in testrun) {
        if (skipInternalMember(testrun, testName)) {
          continue;
        }

        let test = testrun[testName];

        if (test.hasOwnProperty('duration') && test.duration !== 0) {
          sortedByDuration.push({ testName: testName,
                                  duration: test.duration,
                                  test: test,
                                  count: Object.keys(test).length});
        } else {
          print(RED + "This test doesn't have a duration: " + testName + "\n" + JSON.stringify(test) + RESET);
        }        

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
        SuccessMessages += '* Test "' + testrunName + bucketName + '"\n';

        for (let name in successCases) {
          if (!successCases.hasOwnProperty(name)) {
            continue;
          }

          let details = successCases[name];

          if (details.skipped) {
            SuccessMessages += YELLOW + '    [SKIPPED] ' + name + RESET + '\n';
          } else {
            SuccessMessages += GREEN + '    [SUCCESS] ' + name + RESET + '\n';
          }
        }
      } else {
        let m = '* Test "' + testrunName + bucketName + '"\n';
        onlyFailedMessages += m;
        failedMessages += m;

        for (let name in successCases) {
          if (!successCases.hasOwnProperty(name)) {
            continue;
          }

          let details = successCases[name];

          if (details.skipped) {
            failedMessages += YELLOW + '    [SKIPPED] ' + name + RESET + '\n';
            onlyFailedMessages += '    [SKIPPED] ' + name + '\n';
          } else {
            failedMessages += GREEN + '    [SUCCESS] ' + name + RESET + '\n';
          }
        }

        for (let name in failedCases) {
          if (!failedCases.hasOwnProperty(name)) {
            continue;
          }

          failedMessages += RED + '    [FAILED]  ' + name + RESET + '\n\n';
          onlyFailedMessages += '    [FAILED]  ' + name + '\n\n';

          let details = failedCases[name];

          let count = 0;
          for (let one in details) {
            if (!details.hasOwnProperty(one)) {
              continue;
            }

            if (count > 0) {
              failedMessages += '\n';
              onlyFailedMessages += '\n';
            }
            failedMessages += RED + '      "' + one + '" failed: ' + details[one] + RESET + '\n\n';
            onlyFailedMessages += '      "' + one + '" failed: ' + details[one] + '\n\n';
            count++;
          }
        }
      }
    }
    print(SuccessMessages);
    print(failedMessages);

    sortedByDuration.sort(function(a, b) {
      return a.duration - b.duration;
    });
    for (let i = sortedByDuration.length - 1; (i >= 0) && (i > sortedByDuration.length - 11); i --) {
      print(" - " + fancyTimeFormat(sortedByDuration[i].duration / 1000) + " - " +
            sortedByDuration[i].count + " - " +
            sortedByDuration[i].testName);
      let testCases = [];
      let thisTestSuite = sortedByDuration[i];
      print(pu.sumarizeStats(thisTestSuite.test['processStats']));
      for (let testName in thisTestSuite.test) {
        if (skipInternalMember(thisTestSuite.test, testName)) {
          continue;
        }
        let test = thisTestSuite.test[testName];
        let duration = 0;
        if (test.hasOwnProperty('duration')) {
          duration += test.duration;
        }
        if (test.hasOwnProperty('setUpDuration')) {
          duration += test.setUpDuration;
        }
        if (test.hasOwnProperty('tearDownDuration')) {
          duration += test.tearDownDuration;
        }
        testCases.push({
          testName: testName,
          duration: duration
        });
      }
      testCases.sort(function(a, b) {
        return a.duration - b.duration;
      });

      let testSummary = "    ";
      for (let j = testCases.length - 1; (j >= 0) && (j > testCases.length - 11); j --) {
        testSummary += fancyTimeFormat(testCases[j].duration / 1000) + " - " + testCases[j].testName + " ";
      }
      print(testSummary);
    }
    /* jshint forin: true */

    let color = (!res.crashed && res.status === true) ? GREEN : RED;
    let crashText = '';
    let crashedText = '';
    if (res.crashed === true) {
      for (let failed in failedRuns) {
        crashedText += ' [' + failed + '] : ' + failedRuns[failed].replace(/^/mg, '    ');
      }
      crashedText += "\nMarking crashy!";
      crashText = RED + crashedText + RESET;
    }
    print('\n' + color + '* Overall state: ' + ((res.status === true) ? 'Success' : 'Fail') + RESET + crashText);

    let failText = '';
    if (res.status !== true) {
      failText = '   Suites failed: ' + failedSuite + ' Tests Failed: ' + failedTests;
      print(color + failText + RESET);
    }

    failedMessages = onlyFailedMessages;
    if (crashedText !== '') {
      failedMessages += '\n' + crashedText;
    }
    if (cu.GDB_OUTPUT !== '' || failText !== '') {
      failedMessages += '\n\n' + cu.GDB_OUTPUT + failText + '\n';
    }
    fs.write(options.testOutputDirectory + options.testFailureText, failedMessages);
  } catch (x) {
    print('exception caught while pretty printing result: ');
    print(x.message);
    print(JSON.stringify(res));
  }
}



function makePathGeneric (path) {
  return path.split(fs.pathSeparator);
}

// //////////////////////////////////////////////////////////////////////////////
//  @brief converts results to XML representation
// //////////////////////////////////////////////////////////////////////////////

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

function resultsToXml (results, baseName, cluster, isRocksDb, options) {
  let clprefix = '';

  if (cluster) {
    clprefix = 'CL_';
  }

  if (isRocksDb) {
    clprefix += 'RX_';
  } else {
    clprefix += 'MM_';
  }

  const isSignificant = function (a, b) {
    return (internalMembers.indexOf(b) === -1) && a.hasOwnProperty(b);
  };

  for (let resultName in results) {
    if (isSignificant(results, resultName)) {
      let run = results[resultName];

      for (let runName in run) {
        if (isSignificant(run, runName)) {
          const xmlName = clprefix + resultName + '_' + runName;
          const current = run[runName];

          if (current.skipped) {
            continue;
          }

          let xml = buildXml();
          let total = 0;

          if (current.hasOwnProperty('total')) {
            total = current.total;
          }

          let failuresFound = current.failed;
          xml.elem('testsuite', {
            errors: 0,
            failures: failuresFound,
            tests: total,
            name: xmlName,
            time: 0 + current.duration
          });

          let seen = false;

          for (let oneTestName in current) {
            if (isSignificant(current, oneTestName)) {
              const oneTest = current[oneTestName];
              const success = (oneTest.status === true);

              seen = true;

              xml.elem('testcase', {
                name: clprefix + oneTestName,
                time: 0 + oneTest.duration
              }, success);

              if (!success) {
                xml.elem('failure');
                xml.text('<![CDATA[' + oneTest.message + ']]>\n');
                xml.elem('/failure');
                xml.elem('/testcase');
              }
            }
          }

          if (!seen) {
            if (failuresFound === 0) {
              xml.elem('testcase', {
                name: 'all_tests_in_' + xmlName,
                time: 0 + current.duration
              }, true);
            } else {
              xml.elem('testcase', {
                name: 'all_tests_in_' + xmlName,
                failures: failuresFound,
                time: 0 + current.duration
              }, true);
            }
          }

          xml.elem('/testsuite');

          const fn = makePathGeneric(baseName + xmlName + '.xml').join('_');

          fs.write(options.testOutputDirectory + fn, xml.join(''));
        }
      }
    }
  }
}

function writeXMLReports(options, results) {
  let isCluster = false;
  let isRocksDb = false;
  let prefix = '';
  if (options.hasOwnProperty('prefix')) {
    prefix = options.prefix;
  }
  if (options.hasOwnProperty('cluster') && options.cluster) {
    isCluster = true;
  }
  if (options.hasOwnProperty('storageEngine')) {
    isRocksDb = (options.storageEngine === 'rocksdb');
  }

  resultsToXml(results, 'UNITTEST_RESULT_' + prefix, isCluster, isRocksDb, options);
}

// //////////////////////////////////////////////////////////////////////////////
//  @brief simple status representations
// //////////////////////////////////////////////////////////////////////////////

function writeDefaultReports(options, testSuites) {
  fs.write(options.testOutputDirectory + '/UNITTEST_RESULT_EXECUTIVE_SUMMARY.json', "false", true);
  fs.write(options.testOutputDirectory + '/UNITTEST_RESULT_CRASHED.json', "true", true);
  let testFailureText = 'testfailures.txt';
  if (options.hasOwnProperty('testFailureText')) {
    testFailureText = options.testFailureText;
  }
  fs.write(fs.join(options.testOutputDirectory, testFailureText),
           "Incomplete testrun with these testsuites: '" + testSuites +
           "'\nand these options: " + JSON.stringify(options) + "\n");

}

function writeReports(options, results) {
  fs.write(options.testOutputDirectory + '/UNITTEST_RESULT_EXECUTIVE_SUMMARY.json', String(results.status), true);
  fs.write(options.testOutputDirectory + '/UNITTEST_RESULT_CRASHED.json', String(results.crashed), true);
}

function dumpAllResults(options, results) {
  let j;

  try {
    j = JSON.stringify(results);
  } catch (err) {
    j = inspect(results);
  }

  fs.write(options.testOutputDirectory + '/UNITTEST_RESULT.json', j, true);
}


function addFailRunsMessage(testcase, message) {
  failedRuns[testcase] = message;
}

function yamlDumpResults(options, results) {
  if (options.extremeVerbosity === true) {
    try {
      print(yaml.safeDump(JSON.parse(JSON.stringify(results))));
    } catch (err) {
      print(RED + 'cannot dump results: ' + String(err) + RESET);
      print(RED + require('internal').inspect(results) + RESET);
    }
  }
}

exports.yamlDumpResults = yamlDumpResults;
exports.addFailRunsMessage = addFailRunsMessage;
exports.dumpAllResults = dumpAllResults;
exports.writeDefaultReports = writeDefaultReports;
exports.writeReports = writeReports;
exports.unitTestPrettyPrintResults = unitTestPrettyPrintResults;
exports.writeXMLReports = writeXMLReports;
