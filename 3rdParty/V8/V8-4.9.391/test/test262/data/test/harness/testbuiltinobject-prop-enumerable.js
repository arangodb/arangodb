// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Objects that do not define all of the specified "own" properties as
    non-enumerable do not satisfy the assertion.
includes: [testBuiltInObject.js]
---*/

var threw = false;
var obj = {};
Object.defineProperty(obj, 'a', {
  writable: true, enumerable: false, configurable: true
});
Object.defineProperty(obj, 'b', {
  writable: true, enumerable: true, configurable: true
});
Object.defineProperty(obj, 'c', {
  writable: true, enumerable: false, configurable: true
});

try {
  testBuiltInObject(obj, false, false, ['a', 'b', 'c']);
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
