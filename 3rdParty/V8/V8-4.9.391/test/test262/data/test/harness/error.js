// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    The global `$ERROR` function throws an instance of the global `Test262`
    function with the specified message.
---*/

var threw = false;

try {
  $ERROR('This is a test message');
} catch(err) {
  threw = true;
  if (err.constructor !== Test262Error) {
    throw new Error(
      'Expected a Test262Error, but a "' + err.constructor.name +
      '" was thrown.'
    );
  }
  if (err.message !== 'This is a test message') {
    throw new Error('The error thrown did not define the specified message.');
  }
}

if (threw === false) {
  throw new Error('Expected a Test262Error, but no error was thrown.');
}
