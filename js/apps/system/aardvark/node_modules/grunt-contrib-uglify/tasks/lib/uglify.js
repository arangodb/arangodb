/*
 * grunt-contrib-uglify
 * https://gruntjs.com/
 *
 * Copyright (c) 2014 "Cowboy" Ben Alman, contributors
 * Licensed under the MIT license.
 */

'use strict';

// External libs.
var path = require('path');
var fs = require('fs');
var UglifyJS = require('uglify-js');
var _ = require('lodash');
var uriPath = require('uri-path');

exports.init = function(grunt) {
  var exports = {};

  // Minify with UglifyJS.
  // From https://github.com/mishoo/UglifyJS2
  // API docs at http://lisperator.net/uglifyjs/
  exports.minify = function(files, dest, options) {
    options = options || {};

    grunt.verbose.write('Minifying with UglifyJS...');

    var topLevel = null;
    var totalCode = '';
    var sourcesContent = {};

    var outputOptions = getOutputOptions(options, dest);
    var output = UglifyJS.OutputStream(outputOptions);

    // Grab and parse all source files
    files.forEach(function(file){

      var code = grunt.file.read(file);
      totalCode += code;

      // The src file name must be relative to the source map for things to work
      var basename = path.basename(file);
      var fileDir = path.dirname(file);
      var sourceMapDir = path.dirname(options.generatedSourceMapName);
      var relativePath = path.relative(sourceMapDir, fileDir);
      var pathPrefix = relativePath ? (relativePath+path.sep) : '';

      // Convert paths to use forward slashes for sourcemap use in the browser
      file = uriPath(pathPrefix + basename);

      sourcesContent[file] = code;
      topLevel = UglifyJS.parse(code, {
        filename: file,
        toplevel: topLevel,
        expression: options.expression
      });
    });

    // Wrap code in a common js wrapper.
    if (options.wrap) {
      topLevel = topLevel.wrap_commonjs(options.wrap, options.exportAll);
    }

    // Wrap code in closure with configurable arguments/parameters list.
    if (options.enclose) {
      var argParamList = _.map(options.enclose, function(val, key) {
        return key + ':' + val;
      });

      topLevel = topLevel.wrap_enclose(argParamList);
    }

    // Need to call this before we mangle or compress,
    // and call after any compression or ast altering
    if (options.expression === false) {
      topLevel.figure_out_scope();
    }

    if (options.compress !== false) {
      if (options.compress.warnings !== true) {
        options.compress.warnings = false;
      }
      var compressor = UglifyJS.Compressor(options.compress);
      topLevel = topLevel.transform(compressor);

      // Need to figure out scope again after source being altered
      topLevel.figure_out_scope();
    }

    if (options.mangle !== false) {
      // disabled due to:
      //   1) preserve stable name mangling
      //   2) it increases gzipped file size, see https://github.com/mishoo/UglifyJS2#mangler-options
      // // compute_char_frequency optimizes names for compression
      // topLevel.compute_char_frequency(options.mangle);

      // Requires previous call to figure_out_scope
      // and should always be called after compressor transform
      topLevel.mangle_names(options.mangle);
    }

    if (options.sourceMap && options.sourceMapIncludeSources) {
      for (var file in sourcesContent) {
        if (sourcesContent.hasOwnProperty(file)) {
          outputOptions.source_map.get().setSourceContent(file, sourcesContent[file]);
        }
      }
    }

    // Print the ast to OutputStream
    topLevel.print(output);

    var min = output.get();

    // Add the source map reference to the end of the file
    if (options.sourceMap) {
      // Set all paths to forward slashes for use in the browser
      min += "\n//# sourceMappingURL=" + uriPath(options.destToSourceMap);
    }

    var result = {
      max: totalCode,
      min: min,
      sourceMap: outputOptions.source_map
    };

    grunt.verbose.ok();

    return result;
  };

  var getOutputOptions = function(options, dest) {
    var outputOptions = {
      beautify: false,
      source_map: null
    };

    if (options.preserveComments) {
      if (options.preserveComments === 'all' || options.preserveComments === true) {

        // preserve all the comments we can
        outputOptions.comments = true;
      } else if (options.preserveComments === 'some') {
        // preserve comments with directives or that start with a bang (!)
        outputOptions.comments = /^!|@preserve|@license|@cc_on/i;
      } else if (_.isFunction(options.preserveComments)) {

        // support custom functions passed in
        outputOptions.comments = options.preserveComments;
      }
    }

    if (options.banner && options.sourceMap) {
      outputOptions.preamble = options.banner;
    }

    if (options.beautify) {
      if (_.isObject(options.beautify)) {
        // beautify options sent as an object are merged
        // with outputOptions and passed to the OutputStream
        _.assign(outputOptions, options.beautify);
      } else {
        outputOptions.beautify = true;
      }
    }


    if (options.sourceMap) {

      var destBasename = path.basename(dest);
      var destPath     = path.dirname(dest);
      var sourceMapIn;
      if (options.sourceMapIn) {
        sourceMapIn = grunt.file.readJSON(options.sourceMapIn);
      }
      outputOptions.source_map = UglifyJS.SourceMap({
        file: destBasename,
        root: options.sourceMapRoot,
        orig: sourceMapIn
      });
      if (options.sourceMapIncludeSources && sourceMapIn && sourceMapIn.sourcesContent) {
        sourceMapIn.sourcesContent.forEach(function(content, idx) {
          outputOptions.source_map.get().setSourceContent(sourceMapIn.sources[idx], content);
        });
      }

    }

    if (options.indentLevel !== undefined) {
      outputOptions.indent_level = options.indentLevel;
    }

    if (options.maxLineLen !== undefined) {
      outputOptions.max_line_len = options.maxLineLen;
    }

    if (options.ASCIIOnly !== undefined) {
      outputOptions.ascii_only = options.ASCIIOnly;
    }

    return outputOptions;
  };

  return exports;
};
