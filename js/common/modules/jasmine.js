/*jslint indent: 2, nomen: true, maxlen: 120, regexp: true, todo: true */
/*global module, require, exports, print */

/** Jasmine Wrapper
 *
 * This file is based upon Jasmine's boot.js,
 * but adjusted to work with ArangoDB
 *
 * jasmine/core: This is an unmodified copy of Jasmine's Standalone Version
 * jasmine/reporter: A reporter written for ArangoDB
 */

var jasmine = require('jasmine/core'),
  _ = require('underscore'),
  fs = require('fs'),
  Reporter = require('jasmine/reporter').Reporter;

jasmine = jasmine.core(jasmine);

exports.executeTestSuite = function (specFileNames, options) {
  'use strict';
  var sandbox = jasmine.getEnv(),
    format = options.format || 'progress';

  // Explicitly add require
  sandbox.require = require;

  /**
   * The `jsApiReporter` also receives spec results, and is used by any environment
   * that needs to extract the results  from JavaScript.
   */
  var jsApiReporter = new jasmine.JsApiReporter({
    timer: new jasmine.Timer()
  });

  sandbox.addReporter(jsApiReporter);

  /**
   * The `arangoReporter` does the reporting to the console
   */
  var arangoReporter = new Reporter({ format: format });
  sandbox.addReporter(arangoReporter);

  var _ = require('underscore'),
    internal = require('internal');

  _.each(specFileNames, function (specFileName) {
    var spec = fs.read(specFileName);
    internal.executeScript(spec, sandbox, specFileName);
  });

  sandbox.execute();
  return !arangoReporter.hasErrors();
};
