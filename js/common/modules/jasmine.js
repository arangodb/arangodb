/** Jasmine Wrapper
 *
 * This file is based upon Jasmine's boot.js,
 * but adjusted to work with ArangoDB
 *
 * jasmine/core: This is an unmodified copy of Jasmine's Standalone Version
 * jasmine/reporter: A reporter written for ArangoDB
 */

var jasmine = require('jasmine/core'),
  Reporter = require('jasmine/reporter').Reporter;

jasmine = jasmine.core(jasmine);

var env = jasmine.getEnv();

/**
 * ## The Global Interface
 */
exports.describe = function(description, specDefinitions) {
  return env.describe(description, specDefinitions);
};

exports.xdescribe = function(description, specDefinitions) {
  return env.xdescribe(description, specDefinitions);
};

exports.it = function(desc, func) {
  return env.it(desc, func);
};

exports.xit = function(desc, func) {
  return env.xit(desc, func);
};

exports.beforeEach = function(beforeEachFunction) {
  return env.beforeEach(beforeEachFunction);
};

exports.afterEach = function(afterEachFunction) {
  return env.afterEach(afterEachFunction);
};

exports.expect = function(actual) {
  return env.expect(actual);
};

exports.pending = function() {
  return env.pending();
};

exports.spyOn = function(obj, methodName) {
  return env.spyOn(obj, methodName);
};

exports.execute = function() {
  env.execute();
};

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

env.addReporter(arangoReporter);
