/*
 * grunt-contrib-uglify
 * http://gruntjs.com/
 *
 * Copyright (c) 2016 "Cowboy" Ben Alman, contributors
 * Licensed under the MIT license.
 */

'use strict';

var path = require('path');
var chalk = require('chalk');
var maxmin = require('maxmin');
var err;

// Return the relative path from file1 => file2
function relativePath(file1, file2) {
  var file1Dirname = path.dirname(file1);
  var file2Dirname = path.dirname(file2);

  if (file1Dirname !== file2Dirname) {
    return path.relative(file1Dirname, file2Dirname) + path.sep;
  }
  return '';
}

function reportFacility( grunt, options ){
  var reporter;
  switch( options.report ){
    case 'none':
      reporter = grunt.verbose;
      break;
    default:
    case 'min':
    case 'gzip':
      reporter = grunt.log;
  }
  return reporter;
}

// Converts \r\n to \n
function normalizeLf(string) {
  return string.replace(/\r\n/g, '\n');
}

module.exports = function(grunt) {
  // Internal lib.
  var uglify = require('./lib/uglify').init(grunt);

  grunt.registerMultiTask('uglify', 'Minify files with UglifyJS.', function() {
    // Merge task-specific and/or target-specific options with these defaults.
    var options = this.options({
      banner: '',
      footer: '',
      compress: {
        warnings: false
      },
      mangle: {},
      beautify: false,
      report: 'min',
      expression: false,
      maxLineLen: 32000,
      ASCIIOnly: false,
      screwIE8: false,
      quoteStyle: 0
    });
    var log = reportFacility( grunt, options );

    // Process banner.
    var banner = normalizeLf(options.banner);
    var footer = normalizeLf(options.footer);
    var mapNameGenerator, mapInNameGenerator;
    var createdFiles = 0;
    var createdMaps = 0;

    // Iterate over all src-dest file pairs.
    this.files.forEach(function (f) {
      var src = f.src.filter(function (filepath) {
        // Warn on and remove invalid source files (if nonull was set).
        if (!grunt.file.exists(filepath)) {
          grunt.log.warn('Source file ' + chalk.cyan(filepath) + ' not found.');
          return false;
        }
        return true;
      });

      if (src.length === 0) {
        grunt.log.warn('Destination ' + chalk.cyan(f.dest) + ' not written because src files were empty.');
        return;
      }

      // Warn on incompatible options
      if (options.expression && (options.compress || options.mangle)) {
        grunt.log.warn('Option ' + chalk.cyan('expression') + ' not compatible with ' + chalk.cyan('compress and mangle'));
        options.compress = false;
        options.mangle = false;
      }

      // function to get the name of the sourceMap
      if (typeof options.sourceMapName === 'function') {
        mapNameGenerator = options.sourceMapName;
      }

      // function to get the name of the sourceMapIn file
      if (typeof options.sourceMapIn === 'function') {
        if (src.length !== 1) {
          grunt.fail.warn('Cannot generate `sourceMapIn` for multiple source files.');
        }
        mapInNameGenerator = options.sourceMapIn;
      }

      // dynamically create destination sourcemap name
      if (mapNameGenerator) {
        try {
          options.generatedSourceMapName = mapNameGenerator(f.dest);
        } catch (e) {
          err = new Error('SourceMap failed.');
          err.origError = e;
          grunt.fail.warn(err);
        }
      // If no name is passed append .map to the filename
      } else if (!options.sourceMapName) {
        options.generatedSourceMapName = f.dest + '.map';
      } else {
        options.generatedSourceMapName = options.sourceMapName;
      }

      // Dynamically create incoming sourcemap names
      if (mapInNameGenerator) {
        try {
          options.sourceMapIn = mapInNameGenerator(src[0]);
        } catch (e) {
          err = new Error('SourceMapInName failed.');
          err.origError = e;
          grunt.fail.warn(err);
        }
      }

      // Calculate the path from the dest file to the sourcemap for the
      // sourceMappingURL reference
      // If sourceMapUrl is defined, use this instead
      if(options.sourceMap) {
        var destToSourceMapPath, sourceMapBasename;
        if (!options.sourceMapUrl) {
          destToSourceMapPath = relativePath(f.dest, options.generatedSourceMapName);
          sourceMapBasename = path.basename(options.generatedSourceMapName);
          options.destToSourceMap = destToSourceMapPath + sourceMapBasename;
        } else {
          options.destToSourceMap = options.sourceMapUrl;
        }
      }

      // Minify files, warn and fail on error.
      var result;
      try {
        result = uglify.minify(src, f.dest, options);
      } catch (e) {
        console.log(e);
        err = new Error('Uglification failed.');
        if (e.message) {
          err.message += '\n' + e.message + '. \n';
          if (e.line) {
            err.message += 'Line ' + e.line + ' in ' + src + '\n';
          }
        }
        err.origError = e;
        grunt.log.warn('Uglifying source ' + chalk.cyan(src) + ' failed.');
        grunt.fail.warn(err);
      }

      // Concat minified source + footer
      var output = result.min + footer;

      // Only prepend banner if uglify hasn't taken care of it as part of the preamble
      if (!options.sourceMap) {
        output = banner + output;
      }

      // Write the destination file.
      grunt.file.write(f.dest, output);

      // Write source map
      if (options.sourceMap) {
        grunt.file.write(options.generatedSourceMapName, result.sourceMap);
        log.writeln('File ' + chalk.cyan(options.generatedSourceMapName) + ' created (source map).');
        createdMaps++;
      }

      var outputSize = maxmin(result.max, output, options.report === 'gzip');
      log.writeln('File ' + chalk.cyan(f.dest) + ' created: ' + outputSize);

      createdFiles++;
    });

    if (createdMaps > 0) {
      grunt.log.ok(createdMaps + ' source' + grunt.util.pluralize(createdMaps, 'map/maps') + ' created.');
    }

    if (createdFiles > 0) {
      grunt.log.ok(createdFiles + ' ' + grunt.util.pluralize(this.files.length, 'file/files') + ' created.');
    } else {
      grunt.log.warn('No files created.');
    }
  });
};
