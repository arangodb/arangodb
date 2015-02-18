'use strict';
var path = require('path');
var os = require('os');
var dargs = require('dargs');
var async = require('async');
var chalk = require('chalk');
var spawn = require('cross-spawn');
var which = require('which');
var checkFilesSyntax = require('./lib/check');
var concurrencyCount = (os.cpus().length || 1) * 4;

module.exports = function (grunt) {
  var checkBinary = function (cmd, errMsg) {
    try {
      which.sync(cmd);
    } catch (err) {
      return grunt.warn(
        '\n' + errMsg + '\n' +
        'More info: https://github.com/gruntjs/grunt-contrib-sass\n'
      );
    }
  };

  grunt.registerMultiTask('sass', 'Compile Sass to CSS', function () {
    var cb = this.async();
    var options = this.options();

    if (options.bundleExec) {
      checkBinary('bundle',
        'bundleExec options set but no Bundler executable found in your PATH.'
      );
    } else {
      checkBinary('sass',
        'You need to have Ruby and Sass installed and in your PATH for this task to work.'
      );
    }

    if (options.check) {
      options.concurrencyCount = concurrencyCount;
      checkFilesSyntax(this.filesSrc, options, cb);
      return;
    }

    var passedArgs = dargs(options, {
      excludes: ['bundleExec'],
      ignoreFalse: true
    });

    async.eachLimit(this.files, concurrencyCount, function (file, next) {
      var src = file.src[0];

      if (path.basename(src)[0] === '_') {
        return next();
      }

      var args = [
        src,
        file.dest
      ].concat(passedArgs);

      if (options.update) {
        // When the source file hasn't yet been compiled SASS will write an empty file.
        // If this is the first time the file has been written we treat it as if `update` was not passed
        if (!grunt.file.exists(file.dest)) {
          // Find where the --update flag is and remove it
          args.splice(args.indexOf('--update'), 1);
        } else {
          // The first two elements in args are the source and destination files,
          // which are used to build a path that SASS recognizes, i.e. "source:destination"
          args.push(args.shift() + ':' + args.shift());
        }
      }

      var bin = 'sass';

      if (options.bundleExec) {
        bin = 'bundle';
        args.unshift('exec', 'sass');
      }

      // If we're compiling scss or css files
      if (path.extname(src) === '.css') {
        args.push('--scss');
      }

      // Make sure grunt creates the destination folders if they don't exist
      if (!grunt.file.exists(file.dest)) {
        grunt.file.write(file.dest, '');
      }

      grunt.verbose.writeln('Command: ' + bin + ' ' + args.join(' '));

      var cp = spawn(bin, args, {stdio: 'inherit'});

      cp.on('error', grunt.warn);

      cp.on('close', function (code) {
        if (code > 0) {
          grunt.warn('Exited with error code ' + code);
          next();
          return;
        }

        grunt.verbose.writeln('File ' + chalk.cyan(file.dest) + ' created');
        next();
      });
    }, cb);
  });
};
