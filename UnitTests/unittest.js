/* jshint globalstrict:false, unused:false */
/* global print, start_pretty_print, ARGUMENTS */
'use strict';


const fs = require('fs');

const _ = require('lodash');

const UnitTest = require('@arangodb/testing');
const pu = require('@arangodb/process-utils');
const rp = require('@arangodb/result-processing');

const internal = require('internal'); // js/common/bootstrap/modules/internal.js
const inspect = internal.inspect;
const abortSignal = 6;

// //////////////////////////////////////////////////////////////////////////////
//  @brief runs the test using testing.js
// //////////////////////////////////////////////////////////////////////////////

function main (argv) {
  start_pretty_print();

  // parse arguments
  let testSuites = []; // e.g all, http_server, recovery, ...
  let options = {};

  while (argv.length >= 1) {

    if (argv[0].slice(0, 1) === '-') { // break parsing if we hit some -option
      break;
    }

    testSuites.push(argv[0]); // append first arg to test suits
    argv = argv.slice(1);    // and remove first arg (c++:pop_front/bash:shift)
  }

  // convert arguments
  if (argv.length >= 1) {
    try {
      options = internal.parseArgv(argv, 0); // parse option with parseArgv function
    } catch (x) {
      print('failed to parse the json options: ' + x.message + '\n' + String(x.stack));
      print('argv: ', argv);
      return -1;
    }
  }

  if (options.hasOwnProperty('testOutput')) {
    options.testOutputDirectory = options.testOutput + '/';
  } else {
    options.testOutputDirectory = 'out/';
  }

  // create output directory
  try {
    fs.makeDirectoryRecursive(options.testOutputDirectory);
  } catch (x) {
    print("failed to create test directory - " + x.message);
    throw x;
  }

  // by default we set this to error, so we always have a proper result for the caller
  try {
    rp.writeDefaultReports(options, testSuites);
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
    res = UnitTest.unitTest(testSuites, options);
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

  pu.killRemainingProcesses(res);

  // whether or not there was an error
  try {
    rp.writeReports(options, res);
  } catch (x) {
    print('failed to write test result: ' + x.message);
  }

  if (options.writeXmlReport) {
    try {
      rp.dumpAllResults(options, res);
      rp.writeXMLReports(options, res);
    } catch (x) {
      print('exception while serializing status xml!');
      print(x.message);
      print(x.stack);
      print(inspect(res));
    }
  }

  // creates yaml like dump at the end
  rp.unitTestPrettyPrintResults(res, options);

  return res.status;
}

let result = main(ARGUMENTS);

if (!result) {
  // force an error in the console
  throw 'peng!';
}

