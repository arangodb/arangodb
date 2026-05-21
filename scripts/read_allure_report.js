#!bin/arangosh --javascript.execute
/* jshint globalstrict:false, unused:false */
/* global print, start_pretty_print, ARGUMENTS */
'use strict';
const _ = require('lodash');
const yaml = require('js-yaml');
const fs = require('fs');
const internal = require('internal');
const trs = require('@arangodb/testutils/testrunners');
const rp = require('@arangodb/testutils/result-processing');
/* Constants: */
const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
const YELLOW = internal.COLORS.COLOR_YELLOW;

function main (argv) {
  start_pretty_print();
  let options = {
    testOutputDirectory: "/tmp/",
    testFailureText: "debug_testfailures.txt",
    crashAnalysisText: "crash.txt",
  };
  let rc;
  let status = true;
  let results = {};
  let testResultsDir = argv[0];
  try {
    let tr = new trs.runWithAllureReport(options);
    rc = tr.getAllureResults(testResultsDir, results, status, 'dummyTest');
    results = {
      "dummytest": results,
      "failed": results.failed,
      "status": results.status,
    };

    print('================================================================================');
    print(yaml.safeDump(results));
    print('================================================================================');
    rp.analyze.unitTestPrettyPrintResults(options, results);
  } catch (ex) {
    print(ex);
    print(ex.stack);
  }
  return rc;
}

let result = main(ARGUMENTS);

if (!result) {
  process.exit(1);
}
