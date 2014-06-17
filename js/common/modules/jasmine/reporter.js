/*jslint indent: 2, nomen: true, maxlen: 120, regexp: true, todo: true */
/*global module, require, exports, print, printf */

// Reporter
//  progress [default]: Dots
//  documentation: Group and example names

var Reporter,
  _ = require('underscore'),
  internal = require('internal'),
  inspect = internal.inspect,
  failureColor = internal.COLORS.COLOR_RED,
  successColor = internal.COLORS.COLOR_GREEN,
  commentColor = internal.COLORS.COLOR_BLUE,
  resetColor = internal.COLORS.COLOR_RESET,
  pendingColor = internal.COLORS.COLOR_YELLOW,
  p = function (x) { 'use strict'; print(inspect(x)); };

var repeatString = function (str, num) {
  'use strict';
  return new Array(num + 1).join(str);
};

var indenter = function (indentation) {
  'use strict';
  return function (str) {
    return repeatString(" ", indentation) + str;
  };
};

var indent = function (message, indentation) {
  'use strict';
  var lines = message.split("\n");
  return _.map(lines, indenter(indentation)).join("\n");
};

  // "at Function.main (test.js:19:11)"

var fileInfoPattern = /\(([^:]+):[^)]+\)/;

var parseFileName = function (stack) {
  'use strict';
  var parsedStack = _.last(_.filter(stack.split("\n"), function (line) {
    return fileInfoPattern.test(line);
  }));
  return fileInfoPattern.exec(parsedStack)[1];
};

Reporter = function (options) {
  'use strict';
  options = options || {};
  this.format = options.format || 'progress';
  this.failures = [];
};

_.extend(Reporter.prototype, {
  jasmineStarted: function (options) {
    'use strict';
    this.totalSpecs = options.totalSpecsDefined || 0;
    this.failedSpecs = [];
    this.pendingSpecs = [];
    this.start = new Date();
    print();
  },

  hasErrors: function () {
    'use strict';
    return this.failedSpecs.length > 0;
  },

  jasmineDone: function () {
    'use strict';
    if (this.format === 'progress') {
      print('\n');
    }
    if (this.failures.length > 0) {
      this.printFailureInfo();
    }
    if (this.pendingSpecs.length > 0) {
      this.printPendingInfo();
    }
    this.printFooter();
    if (this.failures.length > 0) {
      this.printFailedExamples();
    }
    print();
  },

  suiteStarted: function (result) {
    'use strict';
    if (this.format === 'documentation') {
      print(result.description);
    }
  },

  suiteDone: function () {
    'use strict';
    if (this.format === 'documentation') {
      print();
    }
  },

  specDone: function (result) {
    'use strict';
    if (_.isEqual(result.status, 'passed')) {
      this.pass(result.description);
    } else if (_.isEqual(result.status, 'pending')) {
      this.pending(result.description, result);
    } else {
      this.fail(result.description, result);
    }
  },

  pending: function (testName, result) {
    'use strict';
    this.pendingSpecs.push(result);
    if (this.format === 'progress') {
      printf("%s", pendingColor + "*" + resetColor);
    } else if (this.format === 'documentation') {
      print(pendingColor + "  " + testName + " [PENDING]" +  resetColor);
    }
  },

  pass: function (testName) {
    'use strict';
    if (this.format === 'progress') {
      printf("%s", successColor + "." + resetColor);
    } else if (this.format === 'documentation') {
      print(successColor + "  " + testName + resetColor);
    }
  },

  printFailureMessage: function (testName) {
    'use strict';
    if (this.format === 'progress') {
      printf("%s", failureColor + "F" + resetColor);
    } else if (this.format === 'documentation') {
      print(failureColor + "  " + testName + " [FAILED]" + resetColor);
    }
  },

  fail: function (testName, result) {
    'use strict';
    var failedExpectations = result.failedExpectations;
    this.failedSpecs.push(result.fullName);
    this.printFailureMessage(testName);
    _.each(failedExpectations, function (failedExpectation) {
      this.failures.push({
        fullName: result.fullName,
        failedExpectation: failedExpectation,
        fileName: parseFileName(failedExpectation.stack)
      });
    }, this);
  },

  printFailureInfo: function () {
    'use strict';
    print("Failures:\n");
    _.each(this.failures, function (failure, index) {
      var failedExpectation = failure.failedExpectation;

      print("  " + index + ") " + failure.fullName);
      print(failureColor + indent(failedExpectation.stack, 6) + resetColor);
    });
  },

  printPendingInfo: function () {
    'use strict';
    print("Pending:\n");
    _.each(this.pendingSpecs, function (pending) {
      print(pendingColor + "  " + pending.fullName + resetColor);
    });
  },

  printFooter: function () {
    'use strict';
    var end = new Date(),
      timeInMilliseconds = end - this.start,
      color,
      message = this.totalSpecs + ' example, ' + this.failedSpecs.length + ' failures';

    if (this.failedSpecs.length > 0) {
      color = failureColor;
    } else if (this.pendingSpecs.length > 0) {
      color = pendingColor;
      message += ', ' + this.pendingSpecs.length + ' pending';
    } else {
      color = successColor;
    }

    print();
    print('Finished in ' + (timeInMilliseconds / 1000) + ' seconds');
    print(color + message + resetColor);
  },

  printFailedExamples: function () {
    'use strict';
    print("\nFailed examples:\n");
    _.each(this.failures, function (failure) {
      var repeatAction = "arangod --javascript.unit-tests " + failure.fileName + " /tmp/arangodb_test";
      print(failureColor + repeatAction + commentColor + " # " + failure.fullName + resetColor);
    });
  }
});

exports.Reporter = Reporter;
