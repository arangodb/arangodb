/*
 * grunt-contrib-jshint
 * http://gruntjs.com/
 *
 * Copyright (c) 2015 "Cowboy" Ben Alman, contributors
 * Licensed under the MIT license.
 */

'use strict';

module.exports = function(grunt) {

  var path = require('path');
  var hooker = require('hooker');
  var jshint = require('./lib/jshint').init(grunt);

  grunt.registerMultiTask('jshint', 'Validate files with JSHint.', function() {
    var done = this.async();

    // Merge task-specific and/or target-specific options with these defaults.
    var options = this.options({
      force: false,
      reporterOutput: null
    });

    // Report JSHint errors but dont fail the task
    var force = options.force;
    delete options.force;

    // Whether to output the report to a file
    var reporterOutput = options.reporterOutput;

    // Hook into stdout to capture report
    var output = '';
    if (reporterOutput) {
      hooker.hook(process.stdout, 'write', {
        pre: function(out) {
          output += out;
          return hooker.preempt();
        }
      });
    }

    jshint.lint(this.filesSrc, options, function(results, data) {
      var failed = 0;
      if (results.length > 0) {
        // Fail task if errors were logged except if force was set.
        failed = force;
        if (jshint.usingGruntReporter === true) {

          var numErrors = grunt.util._.reduce(results,function(memo,result){
            return memo + (result.error ? 1 : 0);
          },0);

          var numFiles = data.length;
          grunt.log.error(numErrors + ' ' + grunt.util.pluralize(numErrors,'error/errors') + ' in ' +
                          numFiles + ' ' + grunt.util.pluralize(numFiles,'file/files'));
        }
      } else {
        if (jshint.usingGruntReporter === true && data.length > 0) {
          grunt.log.ok(data.length + ' ' + grunt.util.pluralize(data.length,'file/files') + ' lint free.');
        }
      }

      // Write the output of the reporter if wanted
      if (reporterOutput) {
        hooker.unhook(process.stdout, 'write');
        reporterOutput = grunt.template.process(reporterOutput);
        var destDir = path.dirname(reporterOutput);
        if (!grunt.file.exists(destDir)) {
          grunt.file.mkdir(destDir);
        }
        grunt.file.write(reporterOutput, output);
        grunt.log.ok('Report "' + reporterOutput + '" created.');
      }

      done(failed);
    });
  });

};
