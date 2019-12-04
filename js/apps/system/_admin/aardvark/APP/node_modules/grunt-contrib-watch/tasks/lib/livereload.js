/*
 * grunt-contrib-watch
 * http://gruntjs.com/
 *
 * Copyright (c) 2016 "Cowboy" Ben Alman, contributors
 * Licensed under the MIT license.
 */

'use strict';

var tinylr = require('tiny-lr');
var _ = require('lodash');

// Holds the servers out of scope in case watch is reloaded
var servers = Object.create(null);

module.exports = function(grunt) {

  var defaults = {port: 35729};

  function LR(options) {
    if (options === true) {
      options = defaults;
    } else if (typeof options === 'number') {
      options = {port: options};
    } else {
      options = _.defaults(options, defaults);
    }

    var host = (options.host || '*') + ':' + options.port;

    if (servers[host]) {
      this.server = servers[host];
    } else {
      this.server = tinylr(options);
      this.server.server.removeAllListeners('error');
      this.server.server.on('error', function(err) {
        if (err.code === 'EADDRINUSE') {
          grunt.fatal('Port ' + options.port + ' is already in use by another process.');
        } else {
          grunt.fatal(err);
        }
        process.exit(1);
      });
      this.server.listen(options.port, options.host, function(err) {
        if (err) {
          return grunt.fatal(err);
        }
        grunt.log.verbose.writeln('Live reload server started on ' + host);
      });
      servers[host] = this.server;
    }
  }

  LR.prototype.trigger = function(files) {
    grunt.log.verbose.writeln('Live reloading ' + grunt.log.wordlist(files) + '...');
    this.server.changed({body: {files: files}});
  };

  return function(options) {
    return new LR(options);
  };
};
