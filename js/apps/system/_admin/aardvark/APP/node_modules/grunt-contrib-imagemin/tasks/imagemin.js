'use strict';
var fs = require('fs');
var os = require('os');
var path = require('path');
var async = require('async');
var chalk = require('chalk');
var prettyBytes = require('pretty-bytes');
var Imagemin = require('imagemin');
var rename = require('gulp-rename');

/*
 * grunt-contrib-imagemin
 * http://gruntjs.com/
 *
 * Copyright (c) 2016 Sindre Sorhus, contributors
 * Licensed under the MIT license.
 */

module.exports = function (grunt) {
    grunt.registerMultiTask('imagemin', 'Minify PNG, JPEG, GIF and SVG images', function () {
        var done = this.async();
        var files = this.files;
        var totalSaved = 0;
        var options = this.options({
            interlaced: true,
            optimizationLevel: 3,
            progressive: true
        });

        async.eachLimit(files, os.cpus().length, function (file, next) {
            var msg;
            var imagemin = new Imagemin()
                .src(file.src[0])
                .dest(path.dirname(file.dest))
                .use(Imagemin.jpegtran(options))
                .use(Imagemin.gifsicle(options))
                .use(Imagemin.optipng(options))
                .use(Imagemin.svgo({plugins: options.svgoPlugins || []}));

            if (options.use) {
                options.use.forEach(imagemin.use.bind(imagemin));
            }

            if (path.basename(file.src[0]) !== path.basename(file.dest)) {
                imagemin.use(rename(path.basename(file.dest)));
            }

            fs.stat(file.src[0], function (err, stats) {
                if (err) {
                    grunt.warn(err + ' in file ' + file.src[0]);
                    return next();
                }

                imagemin.run(function (err, data) {
                    if (err) {
                        grunt.warn(err + ' in file ' + file.src[0]);
                        return next();
                    }

                    var origSize = stats.size;
                    var diffSize = origSize - ((data[0].contents && data[0].contents.length) || 0);

                    totalSaved += diffSize;

                    if (diffSize < 10) {
                        msg = 'already optimized';
                    } else {
                        msg = [
                            'saved ' + prettyBytes(diffSize) + ' -',
                            (diffSize / origSize * 100).toFixed() + '%'
                        ].join(' ');
                    }

                    grunt.verbose.writeln(chalk.green('✔ ') + file.src[0] + chalk.gray(' (' + msg + ')'));
                    process.nextTick(next);
                });
            });
        }, function (err) {
            if (err) {
                grunt.warn(err);
            }

            var msg = [
                'Minified ' + files.length,
                files.length === 1 ? 'image' : 'images',
                chalk.gray('(saved ' + prettyBytes(totalSaved) + ')')
            ].join(' ');

            grunt.log.writeln(msg);
            done();
        });
    });
};
