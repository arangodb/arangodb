// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Functions whose `length` property does not match the expected value do not
    satisfy the assertion.
includes: [testBuiltInObject.js]
---*/

var threw = false;

try {
  testBuiltInObject(function(a, b) {}, true, false, [], 3);
} catch(err) {
  threw = true;
  if (err.constructor !== Test262Error) {
    $ERROR(
      'Expected a Test262Error, but a "' + err.constructor.name +
      '" was thrown.'
    );
  }
}

if (threw === false) {
  $ERROR('Expected a Test262Error, but no error was thrown.');
}
