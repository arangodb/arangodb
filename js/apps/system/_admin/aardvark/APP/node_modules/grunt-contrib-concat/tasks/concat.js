/*
 * grunt-contrib-concat
 * http://gruntjs.com/
 *
 * Copyright (c) 2016 "Cowboy" Ben Alman, contributors
 * Licensed under the MIT license.
 */

'use strict';

module.exports = function(grunt) {

  // Internal lib.
  var comment = require('./lib/comment').init(grunt);
  var chalk = require('chalk');
  var sourcemap = require('./lib/sourcemap').init(grunt);

  grunt.registerMultiTask('concat', 'Concatenate files.', function() {
    // Merge task-specific and/or target-specific options with these defaults.
    var options = this.options({
      separator: grunt.util.linefeed,
      banner: '',
      footer: '',
      stripBanners: false,
      process: false,
      sourceMap: false,
      sourceMapName: undefined,
      sourceMapStyle: 'embed'
    });

    // Normalize boolean options that accept options objects.
    if (options.stripBanners === true) {
      options.stripBanners = {};
    }
    if (options.process === true) {
      options.process = {};
    }

    // Process banner and footer.
    var banner = grunt.template.process(options.banner);
    var footer = grunt.template.process(options.footer);

    // Set a local variable for whether to build source maps or not.
    var sourceMap = options.sourceMap;

    // If content is not embedded and it will be modified, either exit or do
    // not make the source map.
    if (
      sourceMap && options.sourceMapStyle === 'link' &&
        (options.stripBanners || options.process)
    ) {
      // Warn and exit if --force isn't set.
      grunt.warn(
        'stripBanners or process option is enabled. ' +
        'Set sourceMapStyle option to \'embed\' or \'inline\'.'
      );
      // --force is set, continue on without the source map.
      grunt.log.warn('Skipping creation of source maps.');
      // Set sourceMap to false to keep maps from being constructed.
      sourceMap = false;
    }

    // Iterate over all src-dest file pairs.
    this.files.forEach(function(f) {
      // Initialize source map objects.
      var sourceMapHelper;
      if (sourceMap) {
        sourceMapHelper = sourcemap.helper(f, options);
        sourceMapHelper.add(banner);
      }

      // Concat banner + specified files + footer.
      var src = banner + f.src.filter(function(filepath) {
        // Warn on and remove invalid source files (if nonull was set).
        if (!grunt.file.exists(filepath)) {
          grunt.log.warn('Source file "' + filepath + '" not found.');
          return false;
        }
        return true;
      }).map(function(filepath, i) {
        if (grunt.file.isDir(filepath)) {
          return;
        }
        // Read file source.
        var src = grunt.file.read(filepath);
        // Process files as templates if requested.
        if (typeof options.process === 'function') {
          src = options.process(src, filepath);
        } else if (options.process) {
          src = grunt.template.process(src, options.process);
        }
        // Strip banners if requested.
        if (options.stripBanners) {
          src = comment.stripBanner(src, options.stripBanners);
        }
        // Add the lines of this file to our map.
        if (sourceMapHelper) {
          src = sourceMapHelper.addlines(src, filepath);
          if (i < f.src.length - 1) {
            sourceMapHelper.add(options.separator);
          }
        }
        return src;
      }).join(options.separator) + footer;

      if (sourceMapHelper) {
        sourceMapHelper.add(footer);
        sourceMapHelper.write();
        // Add sourceMappingURL to the end.
        src += sourceMapHelper.url();
      }

      // Write the destination file.
      grunt.file.write(f.dest, src);

      // Print a success message.
      grunt.verbose.write('File ' + chalk.cyan(f.dest) + ' created.');
    });
  });

};
