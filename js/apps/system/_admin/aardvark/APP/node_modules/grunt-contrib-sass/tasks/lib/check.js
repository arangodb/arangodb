'use strict';
var path = require('path');
var dargs = require('dargs');
var async = require('async');
var chalk = require('chalk');
var spawn = require('cross-spawn');
var grunt = require('grunt');

module.exports = function (files, options, cb) {
  var failCount = 0;

  var filesToCheck = files.filter(function (src) {
    return path.basename(src)[0] !== '_' && grunt.file.exists(src);
  });

  async.eachLimit(filesToCheck, options.concurrencyCount, function (src, next) {
    var bin;
    var args;

    var passedArgs = dargs(options, {
      includes: ['loadPath'],
      ignoreFalse: true
    });

    if (options.bundleExec) {
      bin = 'bundle';
      args = ['exec', 'sass', '--check', src];
    } else {
      bin = 'sass';
      args = ['--check', src];
    }

    args = args.concat(passedArgs);

    grunt.verbose.writeln('Command: ' + bin + ' ' + args.join(' '));

    grunt.verbose.writeln('Checking file ' + chalk.cyan(src) + ' syntax.');
    spawn(bin, args, {stdio: 'inherit'})
      .on('error', grunt.warn)
      .on('close', function (code) {
        if (code > 0) {
          failCount++;
          grunt.log.error('Checking file ' + chalk.cyan(src) + ' - ' + chalk.red('failed') + '.');
        } else {
          grunt.verbose.ok('Checking file ' + chalk.cyan(src) + ' - ' + chalk.green('passed') + '.');
        }

        next();
      });
  }, function () {
    if (failCount > 0) {
      grunt.warn('Sass check failed for ' + failCount + ' files.');
    } else {
      grunt.log.ok('All ' + chalk.cyan(filesToCheck.length) + ' files passed.');
    }

    cb();
  });
};
