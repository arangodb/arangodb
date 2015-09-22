'use strict';

const EventEmitter = require('events').EventEmitter;
const internal = require('internal');
const fs = require('fs');

module.exports = exports = new EventEmitter();

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
