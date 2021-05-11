#!bin/arangosh --javascript.execute
/* jshint globalstrict:false, unused:false */
/* global print, start_pretty_print, ARGUMENTS */
'use strict';
const _ = require('lodash');
const fs = require('fs');
const internal = require('internal');
const rp = require('@arangodb/testutils/result-processing');
const yaml = require('js-yaml');

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

  let analyzers = []; // e.g all, http_server, recovery, ...
  let options = {};

  while (argv.length >= 1) {
    if (argv[0].slice(0, 1) === '-') { // break parsing if we hit some -option
      break;
    }
    analyzers = _.merge(analyzers, argv[0].split(','));
    
    argv = argv.slice(1);    // and remove first arg (c++:pop_front/bash:shift)
  }

  if (analyzers.length === 0) {
    print(RED + "No analyzer specified. Please specify one or more of: \n" +
          yaml.safeDump(Object.keys(rp.analyze)) + "\n" + RESET);
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

  if (!options.hasOwnProperty('readFile')) {
    print("need a file to load!");
    process.exit(1);
  }
  let readFile;
  if (Array.isArray(options.readFile)) {
    readFile = options.readFile;
  } else {
    readFile = [ options.readFile ];
  }
  let otherFile;
  if (options.hasOwnProperty('otherFile')) {
    if (Array.isArray(options.otherFile)) {
      otherFile = options.otherFile;
    } else {
      otherFile = [ options.otherFile ];
    }
  }
  let i = 0;
  let rc = readFile.forEach(file => {
    let ret = true;
    let resultsDump;
    try {
      resultsDump = fs.read(file);
    } catch (ex) {
      print(RED + "File not found [" + file + "]: " + ex.message + RESET + "\n");
      return false;
    }
      
    let results;
    try {
      results = JSON.parse(resultsDump);
    }
    catch (ex) {
      print(RED + "Failed to parse " + file + " - " + ex.message + RESET + "\n");
      return false;
    }

    let otherResults;
    if (otherFile !== undefined) {
      let otherResultsDump;
      let f = otherFile[i];
      try {
        otherResultsDump = fs.read(f);
        i+=1;
      } catch (ex) {
        print(RED + "other File not found [" + f + "]: " + ex.message + RESET + "\n");
        return false;
      }
      try {
        otherResults = JSON.parse(otherResultsDump);
      }
      catch (ex) {
        print(RED + "Failed to parse " + f + " - " + ex.message + RESET + "\n");
        return false;
      }
    }
    _.defaults(options, optionsDefaults);
    try {
      print(YELLOW + "Analyzing: " + file + RESET);
      ret = ret && analyzers.forEach(function(which) {
        rp.analyze[which](options, results, otherResults);
      });
    } catch (ex) {
      print("Failed to analyze [" + file + "]: " + ex.message + RESET + "\n");
      return false;
    }
    return ret;
  });
  return rc;
}

let result = main(ARGUMENTS);

if (!result) {
  process.exit(1);
}
