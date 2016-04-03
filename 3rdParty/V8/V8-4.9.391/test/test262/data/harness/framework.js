// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

function Test262Error(message) {
  this.message = message;
}

Test262Error.prototype.toString = function () {
  return "Test262 Error: " + this.message;
};

function testFailed(message) {
  throw new Test262Error(message);
}

function testPrint(message) {

}

/**
 * It is not yet clear that runTestCase should pass the global object
 * as the 'this' binding in the call to testcase.
 */
var runTestCase = (function(global) {
  return function(testcase) {
    if (!testcase.call(global)) {
      testFailed('test function returned falsy');
    }
  };
})(this);

function assertTruthy(value) {
  if (!value) {
    testFailed('test return falsy');
  }
}


/**
 * falsy means we expect no error.
 * truthy means we expect some error.
 * A non-empty string means we expect an error whose .name is that string.
 */
var expectedErrorName = false;

/**
 * What was thrown, or the string 'Falsy' if something falsy was thrown.
 * null if test completed normally.
 */
var actualError = null;

function testStarted(expectedErrName) {
  expectedErrorName = expectedErrName;
}

function testFinished() {
  var actualErrorName = actualError && (actualError.name ||
                                        'SomethingThrown');
  if (actualErrorName) {
    if (expectedErrorName) {
      if (typeof expectedErrorName === 'string') {
        if (expectedErrorName === actualErrorName) {
          return;
        }
        testFailed('Threw ' + actualErrorName +
                   ' instead of ' + expectedErrorName);
      }
      return;
    }
    throw actualError;
  }
  if (expectedErrorName) {
    if (typeof expectedErrorName === 'string') {
      testFailed('Completed instead of throwing ' +
                 expectedErrorName);
    }
    testFailed('Completed instead of throwing');
  }
}
