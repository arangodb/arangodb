/*
 * grunt-contrib-jshint
 * http://gruntjs.com/
 *
 * Copyright (c) 2016 "Cowboy" Ben Alman, contributors
 * Licensed under the MIT license.
 */

'use strict';

var path = require('path');
var chalk = require('chalk');
var jshintcli = require('jshint/src/cli');

exports.init = function(grunt) {
  var exports = {
    usingGruntReporter: false
  };

  var pad = function(msg, length) {
    while (msg.length < length) {
      msg = ' ' + msg;
    }
    return msg;
  };

  // Select a reporter (if not using the default Grunt reporter)
  // Copied from jshint/src/cli/cli.js until that part is exposed
  exports.selectReporter = function(options) {
    switch (true) {
    // JSLint reporter
    case options.reporter === 'jslint':
    case options['jslint-reporter']:
      options.reporter = 'jshint/src/reporters/jslint_xml.js';
      break;

    // CheckStyle (XML) reporter
    case options.reporter === 'checkstyle':
    case options['checkstyle-reporter']:
      options.reporter = 'jshint/src/reporters/checkstyle.js';
      break;

    // Reporter that displays additional JSHint data
    case options['show-non-errors']:
      options.reporter = 'jshint/src/reporters/non_error.js';
      break;

    // Custom reporter
    case options.reporter !== null && options.reporter !== undefined:
      options.reporter = path.resolve(process.cwd(), options.reporter.toString());
    }

    var reporter;
    if (options.reporter) {
      try {
        reporter = require(options.reporter).reporter;
        exports.usingGruntReporter = false;
      } catch (err) {
        grunt.fatal(err);
      }
    }

    // Use the default Grunt reporter if none are found
    if (!reporter) {
      reporter = exports.reporter;
      exports.usingGruntReporter = true;
    }

    return reporter;
  };

  // Default Grunt JSHint reporter
  exports.reporter = function(results, data) {
    // Dont report empty data as its an ignored file
    if (data.length < 1) {
      grunt.log.error('0 files linted. Please check your ignored files.');
      return;
    }

    if (results.length === 0) {
      // Success!
      grunt.verbose.ok();
      return;
    }

    var options = data[0].options;

    grunt.log.writeln();

    var lastfile = null;
    // Iterate over all errors.
    results.forEach(function(result) {

      // Only print file name once per error
      if (result.file !== lastfile) {
        grunt.log.writeln(chalk.bold(result.file ? '   ' + result.file : ''));
      }
      lastfile = result.file;

      var e = result.error;

      // Sometimes there's no error object.
      if (!e) {
        return;
      }

      if (e.evidence) {
        // Manually increment errorcount since we're not using grunt.log.error().
        grunt.fail.errorcount++;

        // No idea why JSHint treats tabs as options.indent # characters wide, but it
        // does. See issue: https://github.com/jshint/jshint/issues/430
        // Replacing tabs with appropriate spaces (i.e. columns) ensures that
        // caret will line up correctly.
        var evidence = e.evidence.replace(/\t/g, grunt.util.repeat(options.indent, ' '));

        grunt.log.writeln(pad(e.line.toString(), 7) + ' |' + chalk.gray(evidence));
        grunt.log.write(grunt.util.repeat(9, ' ') + grunt.util.repeat(e.character - 1, ' ') + '^ ');
        grunt.verbose.write('[' + e.code + '] ');
        grunt.log.writeln(e.reason);

      } else {
        // Generic "Whoops, too many errors" error.
        grunt.log.error(e.reason);
      }
    });
    grunt.log.writeln();
  };

  // Run JSHint on the given files with the given options
  exports.lint = function(files, options, done) {
    var cliOptions = {
      verbose: grunt.option('verbose'),
      extensions: ''
    };

    // A list of non-dot-js extensions to check
    if (options.extensions) {
      cliOptions.extensions = options.extensions;
      delete options.extensions;
    }

    // A list ignored files
    if (options.ignores) {
      if (typeof options.ignores === 'string') {
        options.ignores = [options.ignores];
      }
      cliOptions.ignores = options.ignores;
      delete options.ignores;
    }

    // Option to extract JS from HTML file
    if (options.extract) {
      cliOptions.extract = options.extract;
      delete options.extract;
    }
    var reporterOutputDir;
    // Get reporter output directory for relative paths in reporters
    if (options.hasOwnProperty('reporterOutput')) {
      if (options.reporterOutput) {
        reporterOutputDir = path.dirname(options.reporterOutput);
      }
      delete options.reporterOutput;
    }

    // Select a reporter to use
    var reporter = exports.selectReporter(options);

    // Remove bad options that may have came in from the cli
    ['reporter', 'reporterOutputRelative', 'jslint-reporter', 'checkstyle-reporter', 'show-non-errors'].forEach(function(opt) {
      if (options.hasOwnProperty(opt)) {
        delete options[opt];
      }
    });

    if (options.jshintrc === true) {
      // let jshint find the options itself
      delete cliOptions.config;
    } else if (options.jshintrc) {
      // Read JSHint options from a specified jshintrc file.
      cliOptions.config = jshintcli.loadConfig(options.jshintrc);
    } else {
      // Enable/disable debugging if option explicitly set.
      if (grunt.option('debug') !== undefined) {
        options.devel = options.debug = grunt.option('debug');
        // Tweak a few things.
        if (grunt.option('debug')) {
          options.maxerr = Infinity;
        }
      }
      // pass all of the remaining options directly to jshint
      cliOptions.config = options;
    }

    // Run JSHint on all file and collect results/data
    var allResults = [];
    var allData = [];
    cliOptions.args = files;
    cliOptions.reporter = function(results, data) {
      if (reporterOutputDir) {
        results.forEach(function(datum) {
          datum.file = path.relative(reporterOutputDir, datum.file);
        });
      }
      reporter(results, data, options);
      allResults = allResults.concat(results);
      allData = allData.concat(data);
    };
    jshintcli.run(cliOptions);
    done(allResults, allData);
  };

  return exports;
};
