/*jslint indent: 2, nomen: true, maxlen: 120, regexp: true, todo: true */
/*global module, require, exports, print */

// Reporter
//  [p]rogress (default - dots)
//  [d]ocumentation (group and example names)

var Reporter,
  _ = require('underscore'),
  internal = require('internal'),
  inspect = internal.inspect,
  failureColor = internal.COLORS.COLOR_RED,
  successColor = internal.COLORS.COLOR_GREEN,
  commentColor = internal.COLORS.COLOR_BLUE,
  resetColor = internal.COLORS.COLOR_RESET,
  p = function (x) { print(inspect(x)); };

var repeatString = function(str, num) {
  return new Array(num + 1).join(str);
};

var indenter = function(indentation) {
  return function (str) {
    return repeatString(" ", indentation) + str;
  };
};

var indent = function(message, indentation) {
  var lines = message.split("\n");
  return _.map(lines, indenter(indentation)).join("\n");
};

  // "at Function.main (test.js:19:11)"

var fileInfoPattern = /\(([^:]+):[^)]+\)/;

var parseFileName = function(stack) {
  var parsedStack = _.last(_.filter(stack.split("\n"), function(line) {
    return fileInfoPattern.test(line);
  }));
  return fileInfoPattern.exec(parsedStack)[1];
};

Reporter = function () {
  this.failures = [];
};

_.extend(Reporter.prototype, {
  jasmineStarted: function(options) {
    this.totalSpecs = options.totalSpecsDefined || 0;
    this.failedSpecs = [];
    this.start = new Date();
    print();
  },

  jasmineDone: function() {
    if (this.failures.length > 0) {
      this.printFailureInfo();
    }
    this.printFooter();
    if (this.failures.length > 0) {
      this.printFailedExamples();
    }
    print();
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
    print(successColor + "  " + testName + resetColor);
  },

  fail: function (testName, result) {
    var failedExpectations = result.failedExpectations;
    this.failedSpecs.push(result.fullName);
    print(failureColor + "  " + testName + " [FAILED]" + resetColor);
    _.each(failedExpectations, function(failedExpectation) {
      this.failures.push({
        fullName: result.fullName,
        failedExpectation: failedExpectation,
        fileName: parseFileName(failedExpectation.stack)
      });
    }, this);
  },

  printFailureInfo: function () {
    print("Failures:\n");
    _.each(this.failures, function(failure, index) {
      var failedExpectation = failure.failedExpectation;

      print("  " + index + ") " + failure.fullName);
      print(failureColor + indent(failedExpectation.stack, 6) + resetColor);
    });
  },

  printFooter: function () {
    var end = new Date(),
      timeInMilliseconds = end - this.start,
      color = this.failedSpecs.length > 0 ? failureColor : successColor;

    print();
    print('Finished in ' + (timeInMilliseconds / 1000) + ' seconds');
    print(color + this.totalSpecs + ' example, ' + this.failedSpecs.length + ' failures' + resetColor);
  },

  printFailedExamples: function () {
    print("\nFailed examples:\n");
    _.each(this.failures, function(failure) {
      var repeatAction = "arangod --javascript.unit-tests " + failure.fileName + " /tmp/arangodb_test";
      print(failureColor + repeatAction + commentColor + " # " + failure.fullName + resetColor);
    });
  }
});

exports.Reporter = Reporter;
