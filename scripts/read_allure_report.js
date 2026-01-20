#!bin/arangosh --javascript.execute
/* jshint globalstrict:false, unused:false */
/* global print, start_pretty_print, ARGUMENTS */
'use strict';
const _ = require('lodash');
const yaml = require('js-yaml');
const fs = require('fs');
const internal = require('internal');
const trs = require('@arangodb/testutils/testrunners');
/* Constants: */
const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
const YELLOW = internal.COLORS.COLOR_YELLOW;

function main (argv) {
  start_pretty_print();
  let options = {};
  let status = true; // e.g all, http_server, recovery, ...
  let results = {};
  let testResultsDir = argv[0];
  let tr = new trs.runWithAllureReport(options);
  let rc = tr.getAllureResults(testResultsDir, results, status);
  print(yaml.safeDump(results));
  return rc;
}

let result = main(ARGUMENTS);

if (!result) {
  process.exit(1);
}
