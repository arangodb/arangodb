/* jshint -W051:true */
/* eslint-disable */
global.DEFINE_MODULE('process', (function () {
  'use strict'
  /* eslint-enable */

  const EventEmitter = require('events').EventEmitter;
  const internal = require('internal');
  const console = require('console');
  const fs = require('fs');

  var exports = new EventEmitter();

  exports.env = internal.env;

  exports.argv = [];

  exports.stdout = {
    isTTY: internal.COLOR_OUTPUT,
    write(text) {
      console.infoLines(text);
    }
  };

  exports.stderr = {
    isTTY: internal.COLOR_OUTPUT,
    write(text) {
      console.errorLines(text);
    }
  };

  exports.cwd = function () {
    return fs.makeAbsolute('');
  };

  exports.nextTick = function (fn) {
    fn();
  };

  exports.exit = internal.exit;

  return exports;
}()));
