// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.3.9
description: Array.prototype.findIndex.length value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.prototype.findIndex.length, 1,
  'The value of `Array.prototype.findIndex.length` is `1`'
);

verifyNotEnumerable(Array.prototype.findIndex, 'length');
verifyNotWritable(Array.prototype.findIndex, 'length');
verifyConfigurable(Array.prototype.findIndex, 'length');
