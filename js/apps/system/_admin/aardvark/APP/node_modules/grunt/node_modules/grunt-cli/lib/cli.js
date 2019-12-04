/*
 * grunt-cli
 * http://gruntjs.com/
 *
 * Copyright (c) 2016 Tyler Kellen, contributors
 * Licensed under the MIT license.
 * https://github.com/gruntjs/grunt-init/blob/master/LICENSE-MIT
 */

'use strict';

// External lib.
var nopt = require('nopt');
var gruntOptions = require('grunt-known-options');

// Parse `gruntOptions` into a form that nopt can handle.
exports.aliases = {};
exports.known = {};

Object.keys(gruntOptions).forEach(function(key) {
  var short = gruntOptions[key].short;
  if (short) {
    exports.aliases[short] = '--' + key;
  }
  exports.known[key] = gruntOptions[key].type;
});

// Parse them and return an options object.
Object.defineProperty(exports, 'options', {
  get: function() {
    return nopt(exports.known, exports.aliases, process.argv, 2);
  }
});
