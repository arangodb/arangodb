// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.1.3.5
description: >
  Map.prototype.forEach.length value and descriptor.
info: >
  Map.prototype.forEach ( callbackfn [ , thisArg ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Map.prototype.forEach.length, 1,
  'The value of `Map.prototype.forEach.length` is `1`'
);

verifyNotEnumerable(Map.prototype.forEach, 'length');
verifyNotWritable(Map.prototype.forEach, 'length');
verifyConfigurable(Map.prototype.forEach, 'length');
