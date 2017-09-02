/* jshint globalstrict:false, unused:false */
/* global print, start_pretty_print, ARGUMENTS */
'use strict';

const _ = require('lodash');

const UnitTest = require('@arangodb/testing');

const internalMembers = UnitTest.internalMembers;
const fs = require('fs');
const internal = require('internal'); // js/common/bootstrap/modules/internal.js
const inspect = internal.inspect;

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
            xml.elem('testcase', {
              name: 'all_tests_in_' + xmlName,
              time: 0 + current.duration
            }, true);
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
  fs.makeDirectoryRecursive(testOutputDirectory);

  // run the test and store the result
  let r = {}; // result
  try {
    // run tests
    r = UnitTest.unitTest(testSuits, options, testOutputDirectory) || {};
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

    print(JSON.stringify(r));
  }

  _.defaults(r, {
    status: false,
    crashed: true
  });

  // whether or not there was an error
  fs.write(testOutputDirectory + '/UNITTEST_RESULT_EXECUTIVE_SUMMARY.json', String(r.status));

  if (options.writeXmlReport) {
    let j;

    try {
      j = JSON.stringify(r);
    } catch (err) {
      j = inspect(r);
    }

    fs.write(testOutputDirectory + '/UNITTEST_RESULT.json', j);
    fs.write(testOutputDirectory + '/UNITTEST_RESULT_CRASHED.json', String(r.crashed));

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

      resultsToXml(r, 'UNITTEST_RESULT_' + prefix, isCluster, isRocksDb);
    } catch (x) {
      print('exception while serializing status xml!');
      print(x.message);
      print(x.stack);
      print(inspect(r));
    }
  }

  // creates yaml like dump at the end
  UnitTest.unitTestPrettyPrintResults(r, testOutputDirectory, options);

  return r.status;
}

let result = main(ARGUMENTS);

if (!result) {
  // force an error in the console
  throw 'peng!';
}

