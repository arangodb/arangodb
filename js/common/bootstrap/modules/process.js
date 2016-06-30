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
    isTTY: false,
    write(text) {
      console.infoLines(text);
    }
  };
  exports.cwd = function () {
    return fs.makeAbsolute('');
  };
  exports.nextTick = function (fn) {
    fn();
  };

  return exports;
}()));
