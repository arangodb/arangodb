#!bin/arangosh --javascript.execute
/* jshint globalstrict:false, unused:false */
/* global print, start_pretty_print, ARGUMENTS */
'use strict';
const _ = require('lodash');
const fs = require('fs');
const internal = require('internal');
const yaml = require('js-yaml');
let cu = require('@arangodb/testutils/crash-utils');
let pu = require('@arangodb/testutils/process-utils');
const optionsDefaults = require('@arangodb/testutils/testing').optionsDefaults;

/* Constants: */
const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
const YELLOW = internal.COLORS.COLOR_YELLOW;

function main (argv) {
  start_pretty_print();

  let crashfiles = [];
  let options = {
    extremeVerbosity: false
  };

  while (argv.length >= 1) {
    if (argv[0].slice(0, 1) === '-') { // break parsing if we hit some -option
      break;
    }
    crashfiles = crashfiles.concat(argv[0].split(','));
    argv = argv.slice(1);    // and remove first arg (c++:pop_front/bash:shift)
  }
  if (crashfiles.length === 0) {
    print(RED + "No gdb output files specified."+ RESET);
    process.exit(1);
  }
  
  if (argv.length >= 1) {
    try {
      options = internal.parseArgv(argv, 0);
    } catch (x) {
      print('failed to parse the options: ' + x.message + '\n' + String(x.stack));
      print('argv: ', argv);
      throw x;
    }
  }
  _.defaults(options, optionsDefaults);
  pu.setupBinaries(options);
  crashfiles.forEach(cf => {
    cu.readGdbFileFiltered(cf, options);
  });
  print(crashfiles);
  print(cu.GDB_OUTPUT);
  return 0;
}

let result = main(ARGUMENTS);

if (!result) {
  process.exit(1);
}
