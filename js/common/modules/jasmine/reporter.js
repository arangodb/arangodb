/*jslint indent: 2, nomen: true, maxlen: 120, regexp: true, todo: true */
/*global module, require, exports, print */

// Reporter
//  [p]rogress (default - dots)
//  [d]ocumentation (group and example names)

var Reporter,
  _ = require('underscore'),
  log = require('console').log,
  internal = require('internal'),
  inspect = internal.inspect,
  p = function (x) { log(inspect(x)); };

var repeatString = function(str, num) {
  return new Array(num + 1).join(str);
};

var indenter = function(indentation) {
  return function (str) {
    return repeatString(" ", indentation) + str;
  };
};

var printIndented = function(message, indentation) {
  var lines = message.split("\n");
  print(_.map(lines, indenter(indentation)).join("\n"));
};

Reporter = function () {
  this.failures = [];
};

_.extend(Reporter.prototype, {
  jasmineStarted: function(options) {
    this.totalSpecs = options.totalSpecsDefined || 0;
    this.failedSpecs = 0;
    this.start = new Date();
    print();
  },

  jasmineDone: function() {
    if (this.failures.length > 0) {
      this.printFailureInfo();
    }
    this.printFooter();
  },

  suiteStarted: function(result) {
    print(result.description);
  },

  suiteDone: function() {
    print();
  },

  specDone: function(result) {
    if (_.isEqual(result.status, 'passed')) {
      this.pass(result.description);
    } else {
      this.fail(result.description, result);
    }
  },

  pass: function (testName) {
    print(internal.COLORS.COLOR_GREEN + "  " + testName + internal.COLORS.COLOR_RESET);
  },

  fail: function (testName, result) {
    var failedExpectations = result.failedExpectations;
    print(internal.COLORS.COLOR_RED + "  " + testName + " [FAILED]" + internal.COLORS.COLOR_RESET);
    _.each(failedExpectations, function(failedExpectation) {
      this.failures.push({ fullName: result.fullName, failedExpectation: failedExpectation });
    }, this);
  },

  printFailureInfo: function () {
    print("\nFailures:\n");
    _.each(this.failures, function(failure, index) {
      var failedExpectation = failure.failedExpectation;

      print("  " + index + ") " + failure.fullName);
      printIndented(failedExpectation.stack, 6);
    });
  },

  printFooter: function () {
    var end = new Date(),
      timeInMilliseconds = end - this.start;
    print();
    print('Finished in ' + (timeInMilliseconds / 1000) + ' seconds');
    print(this.totalSpecs + ' example, ' + this.failedSpecs + ' failures');
    print();
  }
});

exports.Reporter = Reporter;
