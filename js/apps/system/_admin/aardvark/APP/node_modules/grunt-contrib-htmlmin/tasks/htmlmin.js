'use strict';
var chalk = require('chalk');
var prettyBytes = require('pretty-bytes');
var minify = require('html-minifier').minify;

module.exports = function (grunt) {
  grunt.registerMultiTask('htmlmin', 'Minify HTML', function () {
    var options = this.options();
    var count = 0;

    this.files.forEach(function (file) {
      var min;
      var src = file.src[0];

      if (!src) {
        return;
      }

      var max = grunt.file.read(src);

      try {
        min = minify(max, options);
      } catch (err) {
        grunt.warn(src + '\n' + err);
        return;
      }

      count++;

      grunt.file.write(file.dest, min);
      grunt.verbose.writeln('Minified ' + chalk.cyan(file.dest) + ' ' + prettyBytes(max.length) + ' → ' + prettyBytes(min.length));
    });

    grunt.log.writeln('Minified ' + chalk.cyan(count) + ' files' + (this.files.length !== count ? ' (' + chalk.red(this.files.length - count) + ' failed)' : ''));
  });
};
