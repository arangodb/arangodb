/* jshint globalstrict:false, unused:false */
/* global print, start_pretty_print, ARGUMENTS */
'use strict';

const _ = require('lodash');

const UnitTest = require('@arangodb/testing');

const internalMembers = UnitTest.internalMembers;
const fs = require('fs');
const internal = require('internal'); // js/common/bootstrap/modules/internal.js
const inspect = internal.inspect;
const abortSignal = 6;

let testOutputDirectory;

function makePathGeneric (path) {
  return path.split(fs.pathSeparator);
}

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

// //////////////////////////////////////////////////////////////////////////////
//  @brief converts results to XML representation
// //////////////////////////////////////////////////////////////////////////////

function resultsToXml (results, baseName, cluster, isRocksDb) {
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

          fs.write(testOutputDirectory + fn, xml.join(''));
        }
      }
    }
  }
}

// //////////////////////////////////////////////////////////////////////////////
//  @brief runs the test using testing.js
// //////////////////////////////////////////////////////////////////////////////

function main (argv) {
  start_pretty_print();

  // parse arguments
  let testSuits = []; // e.g all, http_server, recovery, ...
  let options = {};

  while (argv.length >= 1) {
    if (argv[0].slice(0, 1) === '{') { // stop parsing if there is a json document
      break;
    }

    if (argv[0].slice(0, 1) === '-') { // break parsing if we hit some -option
      break;
    }

    testSuits.push(argv[0]); // append first arg to test suits
    argv = argv.slice(1);    // and remove first arg (c++:pop_front/bash:shift)
  }

  // convert arguments
  if (argv.length >= 1) {
    try {
      if (argv[0].slice(0, 1) === '{') {
        options = JSON.parse(argv[0]); // parse options form json
      } else {
        options = internal.parseArgv(argv, 0); // parse option with parseArgv function
      }
    } catch (x) {
      print('failed to parse the json options: ' + x.message + '\n' + String(x.stack));
      print('argv: ', argv);
      return -1;
    }
  }

  if (options.hasOwnProperty('testOutput')) {
    testOutputDirectory = options.testOutput + '/';
  } else {
    testOutputDirectory = 'out/';
  }

  options.testOutputDirectory = testOutputDirectory;

  // force json reply
  options.jsonReply = true;

  // create output directory
  try {
    fs.makeDirectoryRecursive(testOutputDirectory);
  } catch (x) {
    print("failed to create test directory - " + x.message);
    throw x;
  }

  // by default we set this to error, so we always have a proper result for the caller
  try {
    fs.write(testOutputDirectory + '/UNITTEST_RESULT_EXECUTIVE_SUMMARY.json', "false", true);
    fs.write(testOutputDirectory + '/UNITTEST_RESULT_CRASHED.json', "true", true);
    let testFailureText = 'testfailures.txt';
    if (options.hasOwnProperty('testFailureText')) {
      testFailureText = options.testFailureText;
    }
    fs.write(fs.join(testOutputDirectory, testFailureText),
             "Incomplete testrun with these testsuites: '" + testSuits +
             "'\nand these options: " + JSON.stringify(options) + "\n");
  } catch (x) {
    print('failed to write default test result: ' + x.message);
    throw(x);
  }

  if (options.hasOwnProperty('cluster') && options.cluster) {
    // cluster beats resilient single server
    options.singleresilient = false;
  }

  if (options.hasOwnProperty('blacklist')) {
    UnitTest.loadBlacklist(options.blacklist);
  }

  // run the test and store the result
  let res = {}; // result
  try {
    // run tests
    res = UnitTest.unitTest(testSuits, options, testOutputDirectory) || {};
  } catch (x) {
    print('caught exception during test execution!');

    if (x.message !== undefined) {
      print(x.message);
    }

    if (x.stack !== undefined) {
      print(x.stack);
    } else {
      print(x);
    }

    print(JSON.stringify(res));
  }

  _.defaults(res, {
    status: false,
    crashed: true
  });

  let running = require("internal").getExternalSpawned();
  let i = 0;
  for (i = 0; i < running.length; i++) {
    let status = require("internal").statusExternal(running[i].pid, false);
    if (status.status === "TERMINATED") {
      print("process exited without us joining it (marking crashy): " + JSON.stringify(running[i]) + JSON.stringify(status));
    }
    else {
      print("Killing remaining process & marking crashy: " + JSON.stringify(running[i]));
      print(require("internal").killExternal(running[i].pid, abortSignal));
    }
    res.crashed = true;
  };

  // whether or not there was an error
  try {
    fs.write(testOutputDirectory + '/UNITTEST_RESULT_EXECUTIVE_SUMMARY.json', String(res.status), true);
    fs.write(testOutputDirectory + '/UNITTEST_RESULT_CRASHED.json', String(res.crashed), true);
  } catch (x) {
    print('failed to write test result: ' + x.message);
  }

  if (options.writeXmlReport) {
    let j;

    try {
      j = JSON.stringify(res);
    } catch (err) {
      j = inspect(res);
    }

    fs.write(testOutputDirectory + '/UNITTEST_RESULT.json', j, true);

    try {
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

      resultsToXml(res, 'UNITTEST_RESULT_' + prefix, isCluster, isRocksDb);
    } catch (x) {
      print('exception while serializing status xml!');
      print(x.message);
      print(x.stack);
      print(inspect(res));
    }
  }

  // creates yaml like dump at the end
  UnitTest.unitTestPrettyPrintResults(res, testOutputDirectory, options);

  return res.status && running.length === 0;
}

let result = main(ARGUMENTS);

if (!result) {
  // force an error in the console
  throw 'peng!';
}

