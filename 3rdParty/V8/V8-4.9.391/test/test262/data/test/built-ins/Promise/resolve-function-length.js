// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 25.4.1.3.2
description: The `length` property of Promise Resolve functions
info: >
  The length property of a promise resolve function is 1.

  17 ECMAScript Standard Built-in Objects:
    Unless otherwise specified, the length property of a built-in Function
    object has the attributes { [[Writable]]: false, [[Enumerable]]: false,
    [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

var resolveFunction;
new Promise(function(resolve, reject) {
  resolveFunction = resolve;
});

assert.sameValue(resolveFunction.length, 1);

verifyNotEnumerable(resolveFunction, "length");
verifyNotWritable(resolveFunction, "length");
verifyConfigurable(resolveFunction, "length");
