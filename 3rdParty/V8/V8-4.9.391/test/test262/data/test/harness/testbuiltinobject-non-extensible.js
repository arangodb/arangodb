// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Objects that are not extensible  do not satisfy the assertion.
includes: [testBuiltInObject.js]
---*/

var threw = false;
var obj = {};
Object.preventExtensions(obj);

try {
  testBuiltInObject(obj);
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
