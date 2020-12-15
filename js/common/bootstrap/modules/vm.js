/* jshint -W051:true */
/* eslint-disable */
global.DEFINE_MODULE('vm', (function () {
  'use strict'
  /* eslint-enable */

  const internal = require('internal');

  var exports = {};

  exports.runInThisContext = function (script, filename) {
    return internal.executeScript(script.toString(), null, filename);
  };

  return exports;
}()));
