// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Objects whose specified property is not configurable do not satisfy the
    assertion outside of strict mode.
includes: [propertyHelper.js]
flags: [noStrict]
---*/

var threw = false;
var obj = {};
Object.defineProperty(obj, 'a', {
  configurable: false
});

try {
  verifyConfigurable(obj, 'a');
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
