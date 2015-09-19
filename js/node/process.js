'use strict';

const EventEmitter = require('events').EventEmitter;
const internal = require('internal');
const path = require('path');

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
  return path.resolve(internal.appPath);
};
exports.nextTick = function (fn) {
  fn();
};
