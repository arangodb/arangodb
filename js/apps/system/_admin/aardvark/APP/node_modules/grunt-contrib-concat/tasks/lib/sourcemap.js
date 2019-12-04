/*
 * grunt-contrib-concat
 * http://gruntjs.com/
 *
 * Copyright (c) 2016 "Cowboy" Ben Alman, contributors
 * Licensed under the MIT license.
 */

'use strict';

exports.init = function(grunt) {
  var exports = {};

  // Node first party libs
  var path = require('path');

  // Third party libs
  var chalk = require('chalk');
  var SourceMap = require('source-map');
  var SourceMapConsumer = SourceMap.SourceMapConsumer;
  var SourceMapGenerator = SourceMap.SourceMapGenerator;

  var NO_OP = function(){};

  function SourceMapConcatHelper(options) {
    this.files = options.files;
    this.dest = options.dest;
    this.options = options.options;
    this.line = 1;
    this.column = 0;

    // ensure we're using forward slashes, because these are URLs
    var file = path.relative(path.dirname(this.dest), this.files.dest).replace(/\\/g, '/');
    var generator = new SourceMapGenerator({
      file: file
    });
    this.file = file;
    this.generator = generator;
    this.addMapping = function(genLine, genCol, orgLine, orgCol, source, name) {
      if (!source) {
        generator.addMapping({
          generated: {line: genLine, column: genCol}
        });
      } else {
        if (!name) {
          generator.addMapping({
            generated: {line: genLine, column: genCol},
            original: {line: orgLine, column: orgCol},
            source: source
          });
        } else {
          generator.addMapping({
            generated: {line: genLine, column: genCol},
            original: {line: orgLine, column: orgCol},
            source: source,
            name: name
          });
        }
      }
    };
  }

  // Return an object that is used to track sourcemap data between calls.
  exports.helper = function(files, options) {
    // Figure out the source map destination.
    var dest = files.dest;
    if (options.sourceMapStyle === 'inline') {
      // Leave dest as is. It will be used to compute relative sources.
    } else if (typeof options.sourceMapName === 'string') {
      dest = options.sourceMapName;
    } else if (typeof options.sourceMapName === 'function') {
      dest = options.sourceMapName(dest);
    } else {
      dest += '.map';
    }

    // Inline style and sourceMapName together doesn't work
    if (options.sourceMapStyle === 'inline' && options.sourceMapName) {
      grunt.log.warn(
        'Source map will be inlined, sourceMapName option ignored.'
      );
    }

    return new SourceMapConcatHelper({
      files: files,
      dest: dest,
      options: options
    });
  };

  // Parse only to increment the generated file's column and line count
  SourceMapConcatHelper.prototype.add = function(src) {
    this._forEachTokenPosition(src);
  };

  /**
   * Parse the source file into tokens and apply the provided callback
   * with the position of the token boundaries in the original file, and
   * in the generated file.
   *
   * @param src The sources to tokenize. Required
   * @param filename The name of the source file. Optional
   * @param callback What to do with the token position indices. Optional
   */
  SourceMapConcatHelper.prototype._forEachTokenPosition = function(src, filename, callback) {
    var genLine = this.line;
    var genCol = this.column;
    var orgLine = 1;
    var orgCol = 0;
    // Tokenize on words, new lines, and white space.
    var tokens = src.split(/(\n|[^\S\n]+|\b)/g);
    if (!callback) {
      callback = NO_OP;
    }
    for (var i = 0, len = tokens.length; i < len; i++) {
      var token = tokens[i];
      if (token) {
        // The if statement filters out empty strings.
        callback(genLine, genCol, orgLine, orgCol, filename);
        if (token === '\n') {
          ++orgLine;
          ++genLine;
          orgCol = 0;
          genCol = 0;
        } else {
          orgCol += token.length;
          genCol += token.length;
        }
      }
    }

    this.line = genLine;
    this.column = genCol;
  };

  // Add the lines of a given file to the sourcemap. If in the file, store a
  // prior sourcemap and return src with sourceMappingURL removed.
  SourceMapConcatHelper.prototype.addlines = function(src, filename) {
    var sourceMapRegEx = /\n\/[*/][@#]\s+sourceMappingURL=((?:(?!\s+\*\/).)*).*/;
    var relativeFilename = path.relative(path.dirname(this.dest), filename);
    // sourceMap path references are URLs, so ensure forward slashes are used for paths passed to sourcemap library
    relativeFilename = relativeFilename.replace(/\\/g, '/');
    if (sourceMapRegEx.test(src)) {
      var sourceMapFile = RegExp.$1;
      var sourceMapPath;

      var sourceContent;
      // Browserify, as an example, stores a datauri at sourceMappingURL.
      if (/data:application\/json;(charset:utf-8;)?base64,([^\s]+)/.test(sourceMapFile)) {
        // Set sourceMapPath to the file that the map is inlined.
        sourceMapPath = filename;
        sourceContent = new Buffer(RegExp.$2, 'base64').toString();
      } else {
        // If sourceMapPath is relative, expand relative to the file
        // referring to it.
        sourceMapPath = path.resolve(path.dirname(filename), sourceMapFile);
        sourceContent = grunt.file.read(sourceMapPath);
      }
      var sourceMapDir = path.dirname(sourceMapPath);
      var sourceMap = JSON.parse(sourceContent);
      var sourceMapConsumer = new SourceMapConsumer(sourceMap);
      // Consider the relative path from source files to new sourcemap.
      var sourcePathToSourceMapPath = path.relative(path.dirname(this.dest), sourceMapDir);
      // Transfer the existing mappings into this mapping
      var initLine = this.line;
      var initCol = this.column;
      sourceMapConsumer.eachMapping(function(args){
        var source;
        if (args.source) {
          source = path.join(sourcePathToSourceMapPath, args.source).replace(/\\/g, '/');
        } else {
          source = null;
        }
        this.line = initLine + args.generatedLine - 1;
        if (this.line === initLine) {
          this.column = initCol + args.generatedColumn;
        } else {
          this.column = args.generatedColumn;
        }
        this.addMapping(
          this.line,
          this.column,
          args.originalLine,
          args.originalColumn,
          source,
          args.name
        );
      }, this);
      if (sourceMap.sources && sourceMap.sources.length && sourceMap.sourcesContent) {
        for (var i = 0; i < sourceMap.sources.length; ++i) {
          this.generator.setSourceContent(
            path.join(sourcePathToSourceMapPath, sourceMap.sources[i]).replace(/\\/g, '/'),
            sourceMap.sourcesContent[i]
          );
        }
      }
      // Remove the old sourceMappingURL.
      src = src.replace(sourceMapRegEx, '');
    } else {
      // Otherwise perform a rudimentary tokenization of the source.
      this._forEachTokenPosition(src, relativeFilename, this.addMapping);
    }

    if (this.options.sourceMapStyle !== 'link') {
      this.generator.setSourceContent(relativeFilename, src);
    }

    return src;
  };

  // Return the comment sourceMappingURL that must be appended to the
  // concatenated file.
  SourceMapConcatHelper.prototype.url = function() {
    // Create the map filepath. Either datauri or destination path.
    var mapfilepath;
    if (this.options.sourceMapStyle === 'inline') {
      var inlineMap = new Buffer(this._write()).toString('base64');
      mapfilepath = 'data:application/json;base64,' + inlineMap;
    } else {
      // Compute relative path to source map destination.
      mapfilepath = path.relative(path.dirname(this.files.dest), this.dest);
    }
    // Create the sourceMappingURL.
    var url;
    if (/\.css$/.test(this.files.dest)) {
      url = '\n/*# sourceMappingURL=' + mapfilepath + ' */';
    } else {
      url = '\n//# sourceMappingURL=' + mapfilepath;
    }

    return url;
  };

  // Return a string for inline use or write the source map to disk.
  SourceMapConcatHelper.prototype._write = function() {
    // New sourcemap.
    var newSourceMap = this.generator.toJSON();
    // Return a string for inline use or write the map.
    if (this.options.sourceMapStyle === 'inline') {
      grunt.verbose.writeln(
        'Source map for ' + chalk.cyan(this.files.dest) + ' inlined.'
      );
      return JSON.stringify(newSourceMap, null, '');
    }
    grunt.file.write(
      this.dest,
      JSON.stringify(newSourceMap, null, '')
    );
    grunt.verbose.writeln('Source map ' + chalk.cyan(this.dest) + ' created.');

  };

  // Non-private function to write the sourcemap. Shortcuts if writing a inline
  // style map.
  SourceMapConcatHelper.prototype.write = function() {
    if (this.options.sourceMapStyle !== 'inline') {
      this._write();
    }
  };

  return exports;
};
