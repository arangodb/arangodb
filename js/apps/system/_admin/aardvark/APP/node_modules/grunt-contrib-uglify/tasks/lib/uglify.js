/*
 * grunt-contrib-uglify
 * https://gruntjs.com/
 *
 * Copyright (c) 2016 "Cowboy" Ben Alman, contributors
 * Licensed under the MIT license.
 */

'use strict';

// External libs.
var path = require('path');
var UglifyJS = require('uglify-js');
var _ = require('lodash');
var uriPath = require('uri-path');
var getOutputOptions;

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
    files.forEach(function(file) {

      var code = grunt.file.read(file);
      totalCode += code;

      // The src file name must be relative to the source map for things to work
      var basename = path.basename(file);
      var fileDir = path.dirname(file);
      var sourceMapDir = path.dirname(options.generatedSourceMapName);
      var relativePath = path.relative(sourceMapDir, fileDir);
      var pathPrefix = relativePath ? relativePath + path.sep : '';
      var bare_returns = options.bare_returns || undefined;

      // Convert paths to use forward slashes for sourcemap use in the browser
      file = uriPath(pathPrefix + basename);

      sourcesContent[file] = code;
      topLevel = UglifyJS.parse(code, {
        filename: file,
        toplevel: topLevel,
        expression: options.expression,
        bare_returns: bare_returns
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

    var topLevelCache = null;
    if (options.nameCache) {
      topLevelCache = UglifyJS.readNameCache(options.nameCache, 'vars');
    }

    // Need to call this before we mangle or compress,
    // and call after any compression or ast altering
    if (options.expression === false) {
      topLevel.figure_out_scope({ screw_ie8: options.screwIE8, cache: topLevelCache });
    }

    if (options.compress !== false) {
      if (options.compress === true) {
        options.compress = {};
      }
      if (options.compress.warnings !== true) {
        options.compress.warnings = false;
      }
      if (options.screwIE8) {
        options.compress.screw_ie8 = true;
      }
      var compressor = UglifyJS.Compressor(options.compress);
      topLevel = topLevel.transform(compressor);

      // Need to figure out scope again after source being altered
      if (options.expression === false) {
        topLevel.figure_out_scope({screw_ie8: options.screwIE8, cache: topLevelCache});
      }
    }

    var mangleExclusions = { vars: [], props: [] };
    if (options.reserveDOMProperties) {
      mangleExclusions = UglifyJS.readDefaultReservedFile();
    }

    if (options.exceptionsFiles) {
      try {
        options.exceptionsFiles.forEach(function(filename) {
          mangleExclusions = UglifyJS.readReservedFile(filename, mangleExclusions);
        });
      } catch (ex) {
        grunt.warn(ex);
      }
    }

    var cache = null;
    if (options.nameCache) {
      cache = UglifyJS.readNameCache(options.nameCache, 'props');
    }

    if (typeof(options.mangleProperties) !== 'undefined' && options.mangleProperties !== false) {
      // if options.mangleProperties is a boolean (true) convert it into an object
      if (typeof options.mangleProperties !== 'object') {
        options.mangleProperties = {};
      }

      options.mangleProperties.reserved = mangleExclusions ? mangleExclusions.props : null;
      options.mangleProperties.cache = cache;

      topLevel = UglifyJS.mangle_properties(topLevel, options.mangleProperties);

      if (options.nameCache) {
        UglifyJS.writeNameCache(options.nameCache, 'props', cache);
      }

      // Need to figure out scope again since topLevel has been altered
      if (options.expression === false) {
        topLevel.figure_out_scope({screw_ie8: options.screwIE8, cache: topLevelCache});
      }
    }

    if (options.mangle !== false) {
      if (options.mangle === true) {
        options.mangle = {};
      }
      if (options.screwIE8) {
        options.mangle.screw_ie8 = true;
      }
      // disabled due to:
      //   1) preserve stable name mangling
      //   2) it increases gzipped file size, see https://github.com/mishoo/UglifyJS2#mangler-options
      // // compute_char_frequency optimizes names for compression
      // topLevel.compute_char_frequency(options.mangle);

      // if options.mangle is a boolean (true) convert it into an object
      if (typeof options.mangle !== 'object') {
        options.mangle = {};
      }

      options.mangle.cache = topLevelCache;

      options.mangle.except = options.mangle.except ? options.mangle.except : [];
      if (mangleExclusions.vars) {
        mangleExclusions.vars.forEach(function(name) {
          UglifyJS.push_uniq(options.mangle.except, name);
        });
      }

      // Requires previous call to figure_out_scope
      // and should always be called after compressor transform
      topLevel.mangle_names(options.mangle);

      UglifyJS.writeNameCache(options.nameCache, 'vars', options.mangle.cache);
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
      var sourceMappingURL;
      sourceMappingURL = options.destToSourceMap.match(/^http[s]?\:\/\//) === null ?  uriPath(options.destToSourceMap) : options.destToSourceMap;
      min += '\n//# sourceMappingURL=' + sourceMappingURL;
    }

    var result = {
      max: totalCode,
      min: min,
      sourceMap: outputOptions.source_map
    };

    grunt.verbose.ok();

    return result;
  };

  getOutputOptions = function(options, dest) {
    var outputOptions = {
      beautify: false,
      source_map: null
    };

    if (options.banner && options.sourceMap) {
      outputOptions.preamble = options.banner;
    }

    if (options.screwIE8) {
      outputOptions.screw_ie8 = true;
    }

    if (options.sourceMap) {

      var destBasename = path.basename(dest);
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

      if (options.sourceMapIn) {
        outputOptions.source_map.get()._file = destBasename;
      }
    }

    if (!_.isUndefined(options.indentLevel)) {
      outputOptions.indent_level = options.indentLevel;
    }

    if (!_.isUndefined(options.maxLineLen)) {
      outputOptions.max_line_len = options.maxLineLen;
    }

    if (!_.isUndefined(options.ASCIIOnly)) {
      outputOptions.ascii_only = options.ASCIIOnly;
    }

    if (!_.isUndefined(options.quoteStyle)) {
      outputOptions.quote_style = options.quoteStyle;
    }

    if (!_.isUndefined(options.preserveComments)) {
      outputOptions.comments = options.preserveComments;
    }

    if (options.beautify) {
      if (_.isObject(options.beautify)) {
        _.assign(outputOptions, options.beautify);
      } else {
        outputOptions.beautify = true;
      }
    }

    return outputOptions;
  };

  return exports;
};
