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
  Reporter = require('jasmine/reporter').Reporter;

jasmine = jasmine.core(jasmine);

var env = jasmine.getEnv();

/**
 * ## The Global Interface
 */

var exposedFunctionality = [
  'describe',
  'xdescribe',
  'it',
  'xit',
  'beforeEach',
  'afterEach',
  'expect',
  'pending',
  'spyOn',
  'execute'
];

_.each(exposedFunctionality, function(name) {
  exports[name] = env[name];
});


/**
 * Expose the interface for adding custom equality testers.
 */
jasmine.addCustomEqualityTester = function(tester) {
  env.addCustomEqualityTester(tester);
};

/**
 * Expose the interface for adding custom expectation matchers
 */
jasmine.addMatchers = function(matchers) {
  return env.addMatchers(matchers);
};

/**
 * Expose the mock interface for the JavaScript timeout functions
 */
jasmine.clock = function() {
  return env.clock;
};

/**
 * The `jsApiReporter` also receives spec results, and is used by any environment that needs to extract the results  from JavaScript.
 */
var jsApiReporter = new jasmine.JsApiReporter({
  timer: new jasmine.Timer()
});

env.addReporter(jsApiReporter);

/**
 * The `arangoReporter` does the reporting to the console
 */
var arangoReporter = new Reporter({ format: 'progress' });

exports.status = function() {
  return !arangoReporter.hasErrors();
};

exports.executeTestSuite = function (specs) {
  var jasmine = require('jasmine'),
    _ = require('underscore'),
    describe = jasmine.describe,
    it = jasmine.it,
    expect = jasmine.expect,
    fs = require('fs'),
    file,
    status;

  _.each(specs, function (spec) {
    eval(spec);
  });

  jasmine.execute();
  return jasmine.status();
};

env.addReporter(arangoReporter);
